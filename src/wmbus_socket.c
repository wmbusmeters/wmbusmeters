/*
 Copyright (C) 2024 Fredrik Öhrström (gpl-3.0-or-later)

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"wmbus.h"
#include"wmbus_common_implementation.h"
#include"wmbus_utils.h"
#include"serial.h"
#include"meters.h"
#include"drivers.h"
#include"xmq.h"

#include<assert.h>
#include<pthread.h>
#include<semaphore.h>
#include<errno.h>
#include<unistd.h>
#include<sstream>
#include<string.h>
#include<iomanip>
#include<map>

using namespace std;

// Simple JSON string escaper
static string escapeJsonString(const string &input)
{
    ostringstream ss;
    for (char c : input)
    {
        switch (c)
        {
        case '"':  ss << "\\\""; break;
        case '\\': ss << "\\\\"; break;
        case '\b': ss << "\\b"; break;
        case '\f': ss << "\\f"; break;
        case '\n': ss << "\\n"; break;
        case '\r': ss << "\\r"; break;
        case '\t': ss << "\\t"; break;
        default:
            if ('\x00' <= c && c <= '\x1f')
            {
                ss << "\\u" << hex << setw(4) << setfill('0') << (int)c;
            }
            else
            {
                ss << c;
            }
        }
    }
    return ss.str();
}

struct WMBusSocket : public virtual BusDeviceCommonImplementation
{
    bool ping();
    string getDeviceId();
    string getDeviceUniqueId();
    LinkModeSet getLinkModes();
    void deviceReset();
    bool deviceSetLinkModes(LinkModeSet lms);
    LinkModeSet supportedLinkModes() { return Any_bit; }
    int numConcurrentLinkModes() { return 0; }
    bool canSetLinkModes(LinkModeSet desired_modes) { return true; }

    void processSerialData();
    void simulate() { }

    WMBusSocket(string bus_alias, shared_ptr<SerialDevice> serial,
                shared_ptr<SerialCommunicationManager> manager);
    ~WMBusSocket() { }

private:
    void processLine(const string &line);
    void sendResponse(const string &response);
    void sendError(const string &error_msg, const string &telegram_hex);

    string line_buffer_;
    LinkModeSet link_modes_;

    struct CachedMeter
    {
        shared_ptr<Meter> meter;
        string key;
    };

    map<string, CachedMeter> meter_cache_;
};

shared_ptr<BusDevice> openSocket(Detected detected,
                                 shared_ptr<SerialCommunicationManager> manager,
                                 shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string socket_path = detected.specified_device.extras;

    if (socket_path.empty())
    {
        error("(socket) no socket path specified. Use SOCKET(/path/to/socket)\n");
    }

    auto serial = manager->createSerialDeviceSocket(socket_path, "socket");
    WMBusSocket *imp = new WMBusSocket(bus_alias, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

WMBusSocket::WMBusSocket(string bus_alias, shared_ptr<SerialDevice> serial,
                         shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(bus_alias, DEVICE_SOCKET, manager, serial, true)
{
    reset();
    loadAllBuiltinDrivers();
}

bool WMBusSocket::ping()
{
    return true;
}

string WMBusSocket::getDeviceId()
{
    return "?";
}

string WMBusSocket::getDeviceUniqueId()
{
    return "?";
}

LinkModeSet WMBusSocket::getLinkModes()
{
    return link_modes_;
}

void WMBusSocket::deviceReset()
{
}

bool WMBusSocket::deviceSetLinkModes(LinkModeSet lms)
{
    return true;
}

void WMBusSocket::sendResponse(const string &response)
{
    string line = response + "\n";
    vector<uchar> data(line.begin(), line.end());
    serial()->send(data);
}

void WMBusSocket::sendError(const string &error_msg, const string &telegram_hex)
{
    string json = "{\"error\": \"" + escapeJsonString(error_msg) + "\"";
    if (!telegram_hex.empty())
    {
        json += ", \"telegram\": \"" + telegram_hex + "\"";
    }
    json += "}";
    sendResponse(json);
}

void WMBusSocket::processLine(const string &line)
{
    // Parse JSON/XMQ/XML input: {"_": "decode", "telegram": "HEX", "key": "HEX", "driver": "auto", "format": "wmbus"}
    XMQDoc *doc = xmqNewDoc();
    bool ok = xmqParseBufferWithType(doc, line.c_str(), line.c_str()+line.length(), NULL, XMQ_CONTENT_DETECT, 0);

    if (!ok)
    {
        string error = xmqDocError(doc);
        sendError(error, "");
        xmqFreeDoc(doc);
        return;
    }

    string telegram_hex, key_hex, driver_name, format_str;

    const char *telegram_hex_s = xmqGetString(doc, "/decode/telegram");
    if (!telegram_hex_s)
    {
        sendError("missing 'telegram' field in JSON input", "");
        xmqFreeDoc(doc);
        return;
    }
    telegram_hex = telegram_hex_s;

    const char *key_hex_s = xmqGetString(doc, "/decode/key");
    if (key_hex_s == NULL || !strcmp(key_hex_s, "NOKEY")) key_hex = "";
    else key_hex = key_hex_s;

    const char *driver_name_s = xmqGetString(doc, "/decode/driver");
    if (driver_name_s == NULL) driver_name = "auto";
    else driver_name = driver_name_s;

    const char *format_s = xmqGetString(doc, "/decode/format");
    if (format_s == NULL) format_str = "";
    else format_str = format_s;

    xmqFreeDoc(doc);

    // Convert hex to binary
    vector<uchar> input_frame;
    bool invalid_hex = false;
    if (!isHexStringStrict(telegram_hex, &invalid_hex))
    {
        sendError("invalid hex string in 'telegram' field", telegram_hex);
        return;
    }
    ok = hex2bin(telegram_hex, &input_frame);
    if (!ok)
    {
        sendError("failed to decode hex telegram", telegram_hex);
        return;
    }

    // Determine frame type
    size_t frame_length;
    int payload_len, payload_offset;
    FrameType frame_type;

    if (format_str == "wmbus")
    {
        frame_type = FrameType::WMBUS;
    }
    else if (format_str == "mbus")
    {
        frame_type = FrameType::MBUS;
        if (FullFrame == checkMBusFrame(input_frame, &frame_length, &payload_len, &payload_offset, true))
        {
            while (((size_t)payload_len) < input_frame.size()) input_frame.pop_back();
        }
    }
    else
    {
        if (FullFrame == checkWMBusFrame(input_frame, &frame_length, &payload_len, &payload_offset, true))
        {
            frame_type = FrameType::WMBUS;
        }
        else if (FullFrame == checkMBusFrame(input_frame, &frame_length, &payload_len, &payload_offset, true))
        {
            frame_type = FrameType::MBUS;
            while (((size_t)payload_len) < input_frame.size()) input_frame.pop_back();
        }
        else
        {
            frame_type = FrameType::WMBUS;
        }
    }

    // Parse telegram header
    Telegram t;
    AboutTelegram about("", 0, LinkMode::UNKNOWN, frame_type);
    t.about = about;

    ok = t.parseHeader(input_frame);
    if (!ok)
    {
        sendError("failed to parse telegram header", telegram_hex);
        return;
    }

    string meter_id = t.addresses.back().id;

    shared_ptr<Meter> meter;
    auto it = meter_cache_.find(meter_id);

    bool need_new_meter = (it == meter_cache_.end());

    if (!need_new_meter)
    {
        if (it->second.key == key_hex)
        {
            meter = it->second.meter;
        }
        else
        {
            need_new_meter = true;
        }
    }

    if (need_new_meter)
    {
        if (driver_name == "auto")
        {
            DriverInfo auto_di = pickMeterDriver(&t);
            if (auto_di.name().str() != "")
            {
                driver_name = auto_di.name().str();
            }
            else
            {
                driver_name = "unknown";
            }
        }

        MeterInfo mi;
        mi.key = key_hex;
        mi.address_expressions.clear();
        mi.address_expressions.push_back(AddressExpression(t.addresses.back()));
        mi.identity_mode = IdentityMode::ID;
        mi.driver_name = DriverName(driver_name);
        mi.poll_interval = 1000*1000*1000;

        meter = createMeter(&mi);
        if (meter == NULL)
        {
            sendError("failed to create meter", telegram_hex);
            return;
        }

        CachedMeter cached;
        cached.meter = meter;
        cached.key = key_hex;
        meter_cache_[meter_id] = cached;
    }

    bool match = false;
    vector<Address> addresses;
    Telegram out_telegram;
    bool handled = meter->handleTelegram(about, input_frame, false, &addresses, &match, &out_telegram);

    string hr, fields, json;
    vector<string> envs, more_json, selected_fields;
    meter->printMeter(&out_telegram, &hr, &fields, '\t', &json, &envs, &more_json, &selected_fields, false);

    int content_bytes = 0, understood_bytes = 0;
    out_telegram.analyzeParse(OutputFormat::NONE, &content_bytes, &understood_bytes);

    if (!handled)
    {
        if (!json.empty() && json.back() == '}')
        {
            json.pop_back();
        }

        if (out_telegram.decryption_failed)
        {
            json += ", \"error\": \"decryption failed, please check key\"";
        }
        else
        {
            string analyze_output = out_telegram.analyzeParse(OutputFormat::PLAIN, &content_bytes, &understood_bytes);
            json += ", \"error\": \"decoding failed\", \"error_analyze\": \"" + escapeJsonString(analyze_output) + "\"";
        }

        json += ", \"telegram\": \"" + telegram_hex + "\"";
        json += "}";
    }
    else if (content_bytes > 0 && understood_bytes < content_bytes)
    {
        if (!json.empty() && json.back() == '}')
        {
            json.pop_back();
            json += ", \"warning\": \"telegram only partially decoded (" +
                    to_string(understood_bytes) + " of " + to_string(content_bytes) + " bytes)\"";
            json += ", \"telegram\": \"" + telegram_hex + "\"}";
        }
    }

    sendResponse(json);
}

void WMBusSocket::processSerialData()
{
    // If no client is connected, try to accept one
    if (!serial()->hasClient())
    {
        if (serial()->acceptClient())
        {
            verbose("(socket) client connected\n");
            line_buffer_.clear();
        }
        return;
    }

    // Client is connected, read data
    vector<uchar> data;
    int n = serial()->receive(&data);

    if (n == 0 && serial()->hasClient())
    {
        // read() returned 0 means EOF — client disconnected
        verbose("(socket) client disconnected\n");
        serial()->disconnectClient();
        line_buffer_.clear();
        return;
    }

    // Append to line buffer and process complete lines
    for (uchar c : data)
    {
        if (c == '\n')
        {
            if (!line_buffer_.empty())
            {
                processLine(line_buffer_);
                line_buffer_.clear();
            }
        }
        else if (c != '\r')
        {
            line_buffer_ += (char)c;
        }
    }
}
