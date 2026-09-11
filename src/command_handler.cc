/*
 Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"always.h"
#include"command_handler.h"
#include"log.h"
#include"utils/doc.h"
#include"xmq.h"

#include<string.h>

using namespace std;

string CommandHandler::processRequest(const std::string &line)
{
    // Parse JSON input: {"_": "decode", "telegram": "HEX", "key": "HEX", "driver": "auto", "format": "wmbus"}
    // Parse XMQ  input: decode{telegram=HEX key=HEX driver=auto format=wmbus}
    // Parse XML  input: <decode><telegram>HEX</telegram><key>HEX</key><driver>auto</driver><format>wmbus</format></decode>
    auto rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *req = rd.doc;

    bool ok = xmqParseBufferWithType(req, line.c_str(), line.c_str()+line.length(), NULL, XMQ_CONTENT_DETECT, 0);

    if (!ok) return xmqDocError(req);

    string s = handle(req);

    xmqFreeDoc(req);

    return s;
}

string CommandHandler::handle(XMQDoc *req)
{
    // Dispatch based on command type.
    // XMQ maps JSON {"_": "CMD", ...} so that CMD becomes the root element name.
    XMQNode *root = xmqGetRootNode(req);
    const char *cmd = root ? xmqGetName(root) : NULL;

    if (cmd && !strcmp(cmd, "decode"))
    {
        return handleDecode(req);
    }

    if (cmd && !strcmp(cmd, "list_drivers"))
    {
        return handleListDrivers();
    }

    return generateError("unknown command, expected 'decode' or 'list_drivers'", "");
}

string CommandHandler::handleDecode(XMQDoc *req)
{
    string telegram_hex, key_hex, driver_name, format_str;

    const char *telegram_hex_s = xmqGetString(req, "/decode/telegram");
    if (!telegram_hex_s) return generateError("missing 'telegram' field in JSON input", "");
    telegram_hex = telegram_hex_s;

    // Key is optional - can be empty or "NOKEY"
    const char *key_hex_s = xmqGetString(req, "/decode/key");
    if (key_hex_s == NULL || !strcmp(key_hex_s, "NOKEY")) key_hex = "";
    else key_hex = key_hex_s;

    // Driver is optional - defaults to "auto"
    const char *driver_name_s = xmqGetString(req, "/decode/driver");
    if (driver_name_s == NULL) driver_name = "auto";
    else driver_name = driver_name_s;

    // Format is optional - "wmbus", "mbus", or auto-detect if not specified
    const char *format_s = xmqGetString(req, "/decode/format");
    if (format_s == NULL) format_str = "";
    else format_str = format_s;

    // Convert hex to binary
    vector<uchar> input_frame;
    bool invalid_hex = false;
    if (!isHexStringStrict(telegram_hex, &invalid_hex))
    {
        return generateError("invalid hex string in 'telegram' field", telegram_hex);
    }
    bool ok = hex2bin(telegram_hex, &input_frame);
    if (!ok)
    {
        return generateError("failed to decode hex telegram", telegram_hex);
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
    if (!ok) return generateError("failed to parse telegram header", telegram_hex);

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

        DriverInfo di;
        bool ok = lookupDriverInfo(mi.driver_name.str(), &di);
        if (!ok) return generateError(string("failed to create meter no such driver: ")+mi.driver_name.str(), telegram_hex);

        meter = createMeter(&mi);
        if (meter == NULL) return generateError("failed to create meter", telegram_hex);

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
    string hr, fields;
    vector<string> envs, more_json, selected_fields;

    auto rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *rsp = rd.doc;

    meter->printMeter(&out_telegram, &hr, &fields, '\t', &envs, &more_json, &selected_fields, rsp);

    // Check parse quality - how much of the content was understood (in bytes)
    int content_bytes = 0, understood_bytes = 0;
    out_telegram.analyzeParse(OutputFormat::NONE, &content_bytes, &understood_bytes);

    if (!handled)
    {
        // Add error info to JSON
        if (out_telegram.decryption_failed)
        {
            xmqAddKeyValue(rsp, xmqGetRootNode(rsp), "error", "decryption failed, please check key", NS_PARENT);
        }
        else
        {
            string analyze_output = out_telegram.analyzeParse(OutputFormat::PLAIN, &content_bytes, &understood_bytes);
            xmqAddKeyValue(rsp, xmqGetRootNode(rsp), "error", "decoding failed", NS_PARENT);
            xmqAddKeyValue(rsp, xmqGetRootNode(rsp), "error_analyze", analyze_output.c_str(), NS_PARENT);
        }

        xmqAddKeyValue(rsp, xmqGetRootNode(rsp), "telegram", telegram_hex.c_str(), NS_PARENT);
    }
    else if (content_bytes > 0 && understood_bytes < content_bytes)
    {
        // Telegram was handled but not fully understood - add warning with byte counts
        string info = "telegram only partially decoded ("+to_string(understood_bytes)+" of "+to_string(content_bytes)+" bytes)";
        xmqAddKeyValue(rsp, xmqGetRootNode(rsp), "warning", info.c_str(), NS_PARENT);
        xmqAddKeyValue(rsp, xmqGetRootNode(rsp), "telegram", telegram_hex.c_str(), NS_PARENT);
    }

    string json = docToString(rsp, XMQ_CONTENT_JSON, false);
    xmqFreeDoc(rsp);

    return json;
}

string CommandHandler::generateError(const string &error_msg, const string &telegram_hex)
{
    auto rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *rsp = rd.doc;

    auto rn = xmqAddRootElement(rsp, "response", NS_NONE);
    xmqAddKeyValue(rsp, rn.node, "error", error_msg.c_str(), NS_PARENT);
    if (!telegram_hex.empty())
    {
        xmqAddKeyValue(rsp, rn.node, "telegram", telegram_hex.c_str(), NS_PARENT);
    }

    return docToString(rsp, XMQ_CONTENT_JSON, false);
}

string CommandHandler::handleListDrivers()
{
    auto rd = xmqNewDoc();
    assert(rd.status == XMQ_OK);
    XMQDoc *rsp = rd.doc;

    auto rn = xmqAddRootElement(rsp, "response", NS_NONE);
    assert(rd.status == XMQ_OK);
    XMQNode *response = rn.node;

    rn = xmqAddElementWithAttrs(rsp, response, "drivers", NS_PARENT, XMQ_ATTRS( { "A", "" } )); // A means that this will be a json array.
    assert(rd.status == XMQ_OK);
    XMQNode *drivers = rn.node;

    for (DriverInfo *di : allDrivers())
    {
        rn = xmqAddElement(rsp, drivers, "_", NS_PARENT);
        assert(rn.status == XMQ_OK);
        XMQNode *driver = rn.node;
        xmqAddKeyValue(rsp, driver, "name", di->name().str().c_str(), NS_PARENT);
        xmqAddKeyValue(rsp, driver, "type", toString(di->type()), NS_PARENT);

        vector<DriverName> &aliases_list = di->nameAliases();

        if (!aliases_list.empty())
        {
            rn = xmqAddElementWithAttrs(rsp, driver, "aliases", NS_PARENT, XMQ_ATTRS({"A",""}));
            assert(rn.status == XMQ_OK);
            XMQNode *aliases = rn.node;

            for (size_t i = 0; i < aliases_list.size(); ++i)
            {
                xmqAddKeyValue(rsp, aliases, "_", aliases_list[i].str().c_str(), NS_PARENT);
            }
        }
    }

    return docToString(rsp, XMQ_CONTENT_JSON, false);
}
