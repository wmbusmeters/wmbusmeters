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

struct WMBusXmqTTY : public virtual BusDeviceCommonImplementation
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

    WMBusXmqTTY(string bus_alias, shared_ptr<SerialDevice> serial,
                 shared_ptr<SerialCommunicationManager> manager);
    ~WMBusXmqTTY() { }

private:
    void processLine(const string &line);
    void outputError(const string &error_msg, const string &telegram_hex);
    void outputResult(const string &json_result);

    string line_buffer_;
    LinkModeSet link_modes_;

    // Cache entry: meter + last used key (to detect key changes)
    struct CachedMeter
    {
        shared_ptr<Meter> meter;
        string key;
    };

    // Cache meters by meter_id - driver is remembered in meter->driverName()
    map<string, CachedMeter> meter_cache_;
};

shared_ptr<BusDevice> openXmqTTY(Detected detected,
                                 shared_ptr<SerialCommunicationManager> manager,
                                 shared_ptr<SerialDevice> serial_override)
{
    string bus_alias = detected.specified_device.bus_alias;
    string device = detected.found_file;

    if (serial_override)
    {
        WMBusXmqTTY *imp = new WMBusXmqTTY(bus_alias, serial_override, manager);
        imp->markAsNoLongerSerial();
        return shared_ptr<BusDevice>(imp);
    }
    auto serial = manager->createSerialDeviceTTY(device.c_str(), 0, PARITY::NONE, "xmqtty");
    WMBusXmqTTY *imp = new WMBusXmqTTY(bus_alias, serial, manager);
    return shared_ptr<BusDevice>(imp);
}

WMBusXmqTTY::WMBusXmqTTY(string bus_alias, shared_ptr<SerialDevice> serial,
                         shared_ptr<SerialCommunicationManager> manager) :
    BusDeviceCommonImplementation(bus_alias, DEVICE_XMQTTY, manager, serial, true)
{
    reset();
    // Load all drivers once at init, not for every telegram
    loadAllBuiltinDrivers();
}

bool WMBusXmqTTY::ping()
{
    return true;
}

string WMBusXmqTTY::getDeviceId()
{
    return "?";
}

string WMBusXmqTTY::getDeviceUniqueId()
{
    return "?";
}

LinkModeSet WMBusXmqTTY::getLinkModes()
{
    return link_modes_;
}

void WMBusXmqTTY::deviceReset()
{
}

bool WMBusXmqTTY::deviceSetLinkModes(LinkModeSet lms)
{
    return true;
}

void WMBusXmqTTY::outputError(const string &error_msg, const string &telegram_hex)
{
    printf("{\"error\": \"%s\"", escapeJsonString(error_msg).c_str());
    if (!telegram_hex.empty())
    {
        printf(", \"telegram\": \"%s\"", telegram_hex.c_str());
    }
    printf("}\n");
    fflush(stdout);
}

void WMBusXmqTTY::outputResult(const string &json_result)
{
    printf("%s\n", json_result.c_str());
    fflush(stdout);
}

void WMBusXmqTTY::processLine(const string &line)
{
    // Parse JSON input: {"_": "decode", "telegram": "HEX", "key": "HEX", "driver": "auto", "format": "wmbus"}
    // Parse XMQ  input: decode{telegram=HEX key=HEX driver=auto format=wmbus}
    // Parse XML  input: <decode><telegram>HEX</telegram><key>HEX</key><driver>auto</driver><format>wmbus</format></decode>
    XMQDoc *doc = xmqNewDoc();
    bool ok = xmqParseBufferWithType(doc, line.c_str(), line.c_str()+line.length(), NULL, XMQ_CONTENT_DETECT, 0);

    if (!ok)
    {
        string error = xmqDocError(doc);
        outputError(error, "");
    }

    string telegram_hex, key_hex, driver_name, format_str;

    const char *telegram_hex_s = xmqGetString(doc, "/decode/telegram");
    if (!telegram_hex_s)
    {
        outputError("missing 'telegram' field in JSON input", "");
        xmqFreeDoc(doc);
        return;
    }
    telegram_hex = telegram_hex_s;

    // Key is optional - can be empty or "NOKEY"
    const char *key_hex_s = xmqGetString(doc, "/decode/key");
    if (key_hex_s == NULL || !strcmp(key_hex_s, "NOKEY")) key_hex = "";
    else key_hex = key_hex_s;

    // Driver is optional - defaults to "auto"
    const char *driver_name_s = xmqGetString(doc, "/decode/driver");
    if (driver_name_s == NULL) driver_name = "auto";
    else driver_name = driver_name_s;

    // Format is optional - "wmbus", "mbus", or auto-detect if not specified
    const char *format_s = xmqGetString(doc, "/decode/format");
    if (format_s == NULL) format_str = "";
    else format_str = format_s;

    xmqFreeDoc(doc);

    // Convert hex to binary
    vector<uchar> input_frame;
    bool invalid_hex = false;
    if (!isHexStringStrict(telegram_hex, &invalid_hex))
    {
        outputError("invalid hex string in 'telegram' field", telegram_hex);
        return;
    }
    ok = hex2bin(telegram_hex, &input_frame);
    if (!ok)
    {
        outputError("failed to decode hex telegram", telegram_hex);
        return;
    }

    // Determine frame type
    size_t frame_length;
    int payload_len, payload_offset;
    FrameType frame_type;

    if (format_str == "wmbus")
    {
        // Explicit WMBUS - skip detection
        frame_type = FrameType::WMBUS;
    }
    else if (format_str == "mbus")
    {
        // Explicit MBUS - skip detection
        frame_type = FrameType::MBUS;
        if (FullFrame == checkMBusFrame(input_frame, &frame_length, &payload_len, &payload_offset, true))
        {
            // Remove checksum and end marker for MBUS
            while (((size_t)payload_len) < input_frame.size()) input_frame.pop_back();
        }
    }
    else
    {
        // Auto-detect: try WMBUS first (more common), fall back to MBUS
        if (FullFrame == checkWMBusFrame(input_frame, &frame_length, &payload_len, &payload_offset, true))
        {
            frame_type = FrameType::WMBUS;
        }
        else if (FullFrame == checkMBusFrame(input_frame, &frame_length, &payload_len, &payload_offset, true))
        {
            frame_type = FrameType::MBUS;
            // Remove checksum and end marker for MBUS
            while (((size_t)payload_len) < input_frame.size()) input_frame.pop_back();
        }
        else
        {
            // Neither detected - try as WMBUS (let parser handle error)
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
        outputError("failed to parse telegram header", telegram_hex);
        return;
    }

    // Get meter ID from telegram
    string meter_id = t.addresses.back().id;

    // Try to find meter in cache by meter_id
    shared_ptr<Meter> meter;
    auto it = meter_cache_.find(meter_id);

    bool need_new_meter = (it == meter_cache_.end());

    if (!need_new_meter)
    {
        // Found cached meter - check if key changed
        if (it->second.key == key_hex)
        {
            // Same key, reuse meter (driver is already resolved)
            meter = it->second.meter;
        }
        else
        {
            // Key changed, need to create new meter
            need_new_meter = true;
        }
    }

    if (need_new_meter)
    {
        // Find best driver if auto (only when creating new meter)
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

        // Create new meter and cache it
        MeterInfo mi;
        mi.key = key_hex;
        mi.address_expressions.clear();
        mi.address_expressions.push_back(AddressExpression(t.addresses.back()));
        mi.identity_mode = IdentityMode::ID;
        mi.driver_name = DriverName(driver_name);
        mi.poll_interval = 1000*1000*1000;  // Fake a high value to silence warning

        meter = createMeter(&mi);
        if (meter == NULL)
        {
            outputError("failed to create meter", telegram_hex);
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

    // Generate output
    string hr, fields, json;
    vector<string> envs, more_json, selected_fields;
    meter->printMeter(&out_telegram, &hr, &fields, '\t', &json, &envs, &more_json, &selected_fields, true);

    // Check parse quality - how much of the content was understood (in bytes)
    int content_bytes = 0, understood_bytes = 0;
    out_telegram.analyzeParse(OutputFormat::NONE, &content_bytes, &understood_bytes);

    if (!handled)
    {
        // Add error info to JSON
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
        // Telegram was handled but not fully understood - add warning with byte counts
        if (!json.empty() && json.back() == '}')
        {
            json.pop_back();
            json += ", \"warning\": \"telegram only partially decoded (" +
                    to_string(understood_bytes) + " of " + to_string(content_bytes) + " bytes)\"";
            json += ", \"telegram\": \"" + telegram_hex + "\"}";
        }
    }

    outputResult(json);
}

void WMBusXmqTTY::processSerialData()
{
    vector<uchar> data;

    // Receive serial data
    serial()->receive(&data);

    // Append to line buffer
    for (uchar c : data)
    {
        if (c == '\n')
        {
            // Process complete line
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
