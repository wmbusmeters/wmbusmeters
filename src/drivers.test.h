/*
 Copyright (C) 2017-2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#ifndef DRIVERS_TEST_H
#define DRIVERS_TEST_H

#include"test.h"
#include"drivers.h"
#include"meters.h"
#include"wmbus.h"
#include"util.h"
#include"xmq.h"

#include<algorithm>
#include<map>
#include<sstream>
#include<string>
#include<vector>

// Normalise a flat JSON object to canonical form: compact, keys sorted A-Z.
// Handles string values (with escaped characters), numbers, booleans and null.
// Does not handle nested objects or arrays (not needed for driver test output).
// Normalise a numeric string: if it looks like a finite float, re-format via double
// so that "3e-06" and "0.000003" compare equal.
static std::string _normaliseNumber(const std::string &v)
{
    if (v.empty() || v == "null" || v == "true" || v == "false") return v;
    // Quick check: must start with digit, '-', or '+'
    if (!isdigit((unsigned char)v[0]) && v[0] != '-' && v[0] != '+') return v;
    try {
        size_t pos;
        double d = std::stod(v, &pos);
        if (pos != v.size()) return v; // not a pure number
        // Re-format with enough precision but strip trailing zeros
        char buf[64];
        snprintf(buf, sizeof(buf), "%.10g", d);
        return buf;
    } catch (...) {
        return v;
    }
}

static std::string _canonicalJson(const std::string &json)
{
    std::map<std::string,std::string> kv; // map deduplicates; last value wins
    std::vector<std::string> key_order;   // for deterministic ordering
    size_t i = 0;
    size_t n = json.size();

    auto skipWs = [&]() { while (i < n && (json[i]==' '||json[i]=='\n'||json[i]=='\t'||json[i]=='\r')) i++; };

    auto readString = [&]() -> std::string {
        std::string s = "\"";
        i++; // skip opening "
        while (i < n && json[i] != '"') {
            if (json[i] == '\\' && i+1 < n) { s += json[i]; i++; }
            s += json[i]; i++;
        }
        s += '"';
        if (i < n) i++; // skip closing "
        return s;
    };

    auto readValue = [&]() -> std::string {
        skipWs();
        if (i < n && json[i] == '"') return readString();
        // number / boolean / null — everything up to next ',' or '}'
        std::string v;
        while (i < n && json[i] != ',' && json[i] != '}') v += json[i++];
        while (!v.empty() && v.back() == ' ') v.pop_back();
        return _normaliseNumber(v);
    };

    skipWs();
    if (i >= n || json[i] != '{') return json; // not a JSON object, return as-is
    i++; // skip {

    while (i < n) {
        skipWs();
        if (i >= n || json[i] == '}') break;
        if (json[i] == ',') { i++; continue; }
        skipWs();
        if (i >= n || json[i] != '"') break;
        std::string key = readString();
        skipWs();
        if (i < n && json[i] == ':') i++; // skip colon
        std::string val = readValue();
        if (!kv.count(key)) key_order.push_back(key);
        kv[key] = val; // duplicate keys: last wins
    }

    std::sort(key_order.begin(), key_order.end());

    std::string out = "{";
    bool first = true;
    for (const auto &k : key_order) {
        if (!first) out += ',';
        first = false;
        out += k + ':' + kv[k];
    }
    out += '}';
    return out;
}

// Replace the actual timestamp value with the canonical test sentinel.
static void _normaliseTimestampJson(std::string &json)
{
    const std::string key = "\"timestamp\":\"";
    size_t p = json.find(key);
    if (p == std::string::npos) return;
    size_t vs = p + key.size();
    size_t ve = json.find('"', vs);
    if (ve == std::string::npos) return;
    json.replace(vs, ve - vs, "1111-11-11T11:11:11Z");
}

// Replace the timestamp at the end of a semicolon-separated fields string.
static void _normaliseTimestampFields(std::string &fields)
{
    size_t p = fields.rfind(';');
    if (p == std::string::npos) return;
    fields = fields.substr(0, p + 1) + "1111-11-11 11:11.11";
}

struct _GenTestEntry {
    std::string name;
    std::string args;
    std::string telegram;
    std::string expected_json;
    std::string expected_fields;
};

static XMQProceed _collect_gen_test(XMQDoc *doc, XMQNodePtr node, void *user_data)
{
    auto *entries = static_cast<std::vector<_GenTestEntry>*>(user_data);

    const char *args     = xmqGetStringRel(doc, "args",     node);
    const char *telegram = xmqGetStringRel(doc, "telegram", node);
    const char *json     = xmqGetStringRel(doc, "json",     node);
    const char *fields   = xmqGetStringRel(doc, "fields",   node);

    if (!args || !telegram) return XMQ_CONTINUE;

    _GenTestEntry e;
    e.args          = args;
    e.telegram      = telegram;  // hex2bin already strips underscores
    e.expected_json = json   ? _canonicalJson(json)   : "";
    e.expected_fields = fields ? fields : "";

    std::istringstream iss(args);
    std::string meter_name, driver;
    iss >> meter_name >> driver;
    e.name = meter_name + " (" + driver + ")";

    entries->push_back(std::move(e));
    return XMQ_CONTINUE;
}

static auto _dynamic_loading_suite = describe("dynamic_loading", []()
{
    it("resolves VIFRange names from builtin driver database", []()
    {
        VIFRange vr = toVIFRange("Date");
        if (vr != VIFRange::Date)
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "dynamic loading got %s but expected %s",
                     toString(vr), toString(VIFRange::Date));
            test_fail(buf);
        }

        vr = toVIFRange("DateTime");
        if (vr != VIFRange::DateTime)
        {
            char buf[128];
            snprintf(buf, sizeof(buf), "dynamic loading got %s but expected %s",
                     toString(vr), toString(VIFRange::DateTime));
            test_fail(buf);
        }
    });
});

static auto _generated_driver_tests_suite = describe("generated_driver_tests", []()
{
    XMQDoc *doc = xmqNewDoc();
    if (!xmqParseFile(doc, "tests/generated_tests.xmq", NULL, XMQ_FLAG_TRIM_NONE))
    {
        it("load tests/generated_tests.xmq", []()
        {
            test_fail("could not open tests/generated_tests.xmq — run from project root");
        });
        xmqFreeDoc(doc);
        return;
    }

    std::vector<_GenTestEntry> entries;
    xmqForeach(doc, "/test", _collect_gen_test, &entries);
    xmqFreeDoc(doc);

    for (const auto &e : entries)
    {
        it(e.name.c_str(), [e]()
        {
            std::istringstream iss(e.args);
            std::string meter_name, driver, id, key;
            iss >> meter_name >> driver >> id >> key;

            MeterInfo mi;
            if (!mi.parse(meter_name, driver, id, key == "NOKEY" ? "" : key))
            {
                test_fail(("cannot parse meter args: " + e.args).c_str());
                return;
            }

            loadBuiltinDriver(driver);
            auto meter = createMeter(&mi);
            if (!meter)
            {
                test_fail(("cannot create meter for driver: " + driver).c_str());
                return;
            }

            // Split on ',' for multi-telegram tests; process each against same meter.
            std::vector<std::string> parts;
            {
                std::string cur;
                for (char c : e.telegram) {
                    if (c == ',') { if (!cur.empty()) parts.push_back(cur); cur.clear(); }
                    else cur += c;
                }
                if (!cur.empty()) parts.push_back(cur);
            }

            Telegram t;
            bool any_matched = false;
            for (const auto &hex_part : parts)
            {
                std::vector<uchar> frame;
                hex2bin(hex_part, &frame);
                if (frame.empty()) continue;

                // Mirror what wmbus_simulator.cc does: strip Frame A/B DLL CRCs before parsing.
                // Without this, CRC bytes embedded in Frame B telegrams are misread as data.
                if (frame[0] != 0x68) removeAnyDLLCRCs(frame);

                Telegram pt;
                pt.about.timestamp = 0; // AboutTelegram() leaves timestamp uninitialised; 0 means "use poll time"
                // Detect MBUS vs WMBUS from first byte (0x68 = MBUS long frame start)
                if (frame[0] == 0x68) pt.about.type = FrameType::MBUS;

                std::vector<Address> addresses;
                bool id_match;
                bool matched = meter->handleTelegram(pt.about, frame, true, &addresses, &id_match, &pt);
                if (matched && pt.meter != nullptr) { t = pt; any_matched = true; }
            }

            if (!any_matched || t.meter == nullptr)
            {
                test_fail(("telegram not matched by meter: " + e.args).c_str());
                return;
            }

            std::string hr, fields_out, json_out;
            std::vector<std::string> envs, more_json, selected_fields;
            meter->printMeter(&t, &hr, &fields_out, ';', &json_out, &envs, &more_json, &selected_fields, false);

            _normaliseTimestampJson(json_out);
            _normaliseTimestampFields(fields_out);

            std::string canonical_actual = _canonicalJson(json_out);

            if (!e.expected_json.empty() && canonical_actual != e.expected_json)
            {
                std::string msg = "JSON mismatch for " + e.args
                    + "\n  got:      " + canonical_actual
                    + "\n  expected: " + e.expected_json;
                test_fail(msg.c_str());
            }

            if (!e.expected_fields.empty() && fields_out != e.expected_fields)
            {
                std::string msg = "fields mismatch for " + e.args
                    + "\n  got:      " + fields_out
                    + "\n  expected: " + e.expected_fields;
                test_fail(msg.c_str());
            }
        });
    }
});

#endif // DRIVERS_TEST_H
