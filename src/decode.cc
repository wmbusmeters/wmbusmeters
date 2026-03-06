/*
 Copyright (C) 2024-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"decode.h"
#include"drivers.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"xmq.h"

#include<iomanip>
#include<sstream>
#include<string.h>

using namespace std;

string escapeJsonString(const string &input)
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

static string formatError(const string &error_msg, const string &telegram_hex)
{
    string result = "{\"error\": \"" + escapeJsonString(error_msg) + "\"";
    if (!telegram_hex.empty())
    {
        result += ", \"telegram\": \"" + telegram_hex + "\"";
    }
    result += "}";
    return result;
}

string decodeLine(DecoderSession &session, const string &line)
{
    // Parse JSON input: {"_": "decode", "telegram": "HEX", "key": "HEX", "driver": "auto", "format": "wmbus"}
    // Parse XMQ  input: decode{telegram=HEX key=HEX driver=auto format=wmbus}
    // Parse XML  input: <decode><telegram>HEX</telegram><key>HEX</key><driver>auto</driver><format>wmbus</format></decode>
    XMQDoc *doc = xmqNewDoc();
    bool ok = xmqParseBufferWithType(doc, line.c_str(), line.c_str()+line.length(), NULL, XMQ_CONTENT_DETECT, 0);

    if (!ok)
    {
        string error = xmqDocError(doc);
        string result = formatError(error, "");
        xmqFreeDoc(doc);
        return result;
    }

    string telegram_hex, key_hex, driver_name, format_str;

    const char *telegram_hex_s = xmqGetString(doc, "/decode/telegram");
    if (!telegram_hex_s)
    {
        xmqFreeDoc(doc);
        return formatError("missing 'telegram' field in JSON input", "");
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
        return formatError("invalid hex string in 'telegram' field", telegram_hex);
    }
    ok = hex2bin(telegram_hex, &input_frame);
    if (!ok)
    {
        return formatError("failed to decode hex telegram", telegram_hex);
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
        return formatError("failed to parse telegram header", telegram_hex);
    }

    // Get meter ID from telegram
    string meter_id = t.addresses.back().id;

    // Try to find meter in cache by meter_id
    shared_ptr<Meter> meter;
    auto it = session.meter_cache.find(meter_id);

    bool need_new_meter = (it == session.meter_cache.end());

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
            return formatError("failed to create meter", telegram_hex);
        }

        CachedMeter cached;
        cached.meter = meter;
        cached.key = key_hex;
        session.meter_cache[meter_id] = cached;
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

    return json;
}
