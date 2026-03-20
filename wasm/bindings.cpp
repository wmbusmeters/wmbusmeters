// wmbusmeters WASM bindings
// Protokoll-Verarbeitung (AMB8465/Tarvos/RAW) + Dekodierung komplett in C++
// JS liefert nur rohe Bytes via serial_on_data, bekommt fertige JSON zurück.

#include <emscripten/emscripten.h>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cstdio>

#include "util.h"
#include "wmbus.h"
#include "meters.h"
#include "address.h"
#include "drivers.h"
#include "serial_web.h"

// ─────────────────────────────────────────────
// Result buffer
// ─────────────────────────────────────────────
static char* g_result_buf = nullptr;
static map<int, SerialDeviceImp*> g_devices;

static std::string detectedToJson(const Detected& d) {
    std::string json = "{";
    json += "\"ok\":true,";
    json += "\"detected\":true,";
    json += "\"type\":\"" + std::string(toString(d.found_type)) + "\",";
    json += "\"device_id\":\"" + d.found_device_id + "\",";
    json += "\"baud\":" + std::to_string(d.found_bps);
    json += "}";
    return json;
}

static const char* store_result(const std::string& s)
{
    free(g_result_buf);
    g_result_buf = (char*)malloc(s.size() + 1);
    memcpy(g_result_buf, s.c_str(), s.size() + 1);
    return g_result_buf;
}

static const char* error_result(const std::string& msg)
{
    std::string escaped;
    for (char c : msg) {
        if (c == '"')  escaped += "\\\"";
        else if (c == '\n') escaped += "\\n";
        else if (c == '\\') escaped += "\\\\";
        else escaped += c;
    }
    return store_result("{\"ok\":false,\"error\":\"" + escaped + "\"}");
}

static std::string escape_json(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == '"')  out += "\\\"";
        else if (c == '\n') out += "\\n";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

static std::string bytes_to_hex(const uint8_t* data, int len)
{
    std::string hex;
    hex.reserve(len * 2);
    for (int i = 0; i < len; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", data[i]);
        hex += buf;
    }
    return hex;
}

// ─────────────────────────────────────────────
// Global serial manager
// ─────────────────────────────────────────────
static SerialCommunicationManagerImp g_web_serial_manager;

// ─────────────────────────────────────────────
// Dongle-Protokoll State pro Device
// ─────────────────────────────────────────────
enum class DongleProto { AMB8465, TARVOS2, RAW };

struct ProtoConfig {
    DongleProto type;
    uint8_t sof;
    uint8_t rsp_offset;
    uint8_t cmd_data_ind;
    uint8_t cmd_set_mode_req;
    uint8_t cmd_reset_req;
    uint8_t cmd_set_req;
    uint8_t cmd_get_req;
    uint8_t cmd_serialno_req;
    uint8_t cmd_fw_version_req;
    uint8_t cmd_factory_reset_req;
    int fw_version_bytes;
    int data_offset;
};

static ProtoConfig AMB8465_PROTO = {
    DongleProto::AMB8465, 0xFF, 0x80,
    0x03, 0x04, 0x05, 0x09, 0x0A, 0x0B, 0x0C, 0x11,
    4, 3
};

static ProtoConfig TARVOS2_PROTO = {
    DongleProto::TARVOS2, 0x02, 0x40,
    0x81, 0x04, 0x05, 0x09, 0x0A, 0x0B, 0x0C, 0x12,
    3, 3
};

static ProtoConfig RAW_PROTO = {
    DongleProto::RAW, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0, 0
};

struct DeviceState {
    ProtoConfig proto;
    std::vector<uint8_t> buffer;
};

static std::map<int, DeviceState> g_device_states;

// ─────────────────────────────────────────────
// wM-Bus C-Feld Validierung
// ─────────────────────────────────────────────
static bool is_valid_wmbus_cfield(uint8_t c)
{
    static const uint8_t valid[] = {
        0x44, 0x46, 0x48, 0x4A, 0x53, 0x55, 0x5B, 0x5D,
        0x6E, 0x73, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D,
        0x7E, 0x7F, 0x80, 0x8A, 0x8B
    };
    for (auto v : valid) if (c == v) return true;
    return false;
}

static double calculate_rssi(uint8_t raw)
{
    if (raw >= 128) return (raw - 256) / 2.0 - 74.0;
    return raw / 2.0 - 74.0;
}

static uint8_t xor_checksum(const uint8_t* data, int offset, int len)
{
    uint8_t cs = 0;
    for (int i = offset; i < offset + len; i++) cs ^= data[i];
    return cs;
}

// ─────────────────────────────────────────────
// Init + Decode
// ─────────────────────────────────────────────
static bool g_initialized = false;

static void ensure_initialized() {
    if (!g_initialized) {
        prepareBuiltinDrivers();
        loadAllBuiltinDrivers();
        g_initialized = true;
    }
}

static std::string decode_telegram(const std::vector<uint8_t>& wmbus_frame,
                                    const std::string& key,
                                    const std::string& driver_hint,
                                    double rssi_dbm)
{
    ensure_initialized();

    std::vector<uchar> frame(wmbus_frame.begin(), wmbus_frame.end());
    Telegram t;

    if (!t.parseHeader(frame)) {
        return "{\"ok\":false,\"error\":\"header parse failed\"}";
    }

    std::string driver_name;
    if (driver_hint.empty() || driver_hint == "auto") {
        DriverInfo di = pickMeterDriver(&t);
        if (di.name().str().empty()) {
            std::string id = t.addresses.empty() ? "" : t.addresses.back().id;
            return "{\"ok\":false,\"error\":\"no driver found\",\"id\":\"" + id + "\"}";
        }
        driver_name = di.name().str();
    } else {
        driver_name = driver_hint;
    }

    MeterInfo mi;
    mi.key = key;
    mi.address_expressions.clear();
    mi.address_expressions.push_back(AddressExpression(t.addresses.back()));
    mi.identity_mode = IdentityMode::ID;
    mi.driver_name = DriverName(driver_name);
    mi.poll_interval = 1000 * 1000 * 1000;

    std::shared_ptr<Meter> meter = createMeter(&mi);
    if (!meter) {
        return "{\"ok\":false,\"error\":\"meter creation failed\"}";
    }

    AboutTelegram about("wasm", 0, LinkMode::Any, FrameType::WMBUS);
    bool match = false;
    std::vector<Address> addresses;
    Telegram out;

    bool handled = meter->handleTelegram(about, frame, false,
                                         &addresses, &match, &out);

    std::string hr, fields, json;
    std::vector<std::string> envs, more_json, selected_fields;

    meter->printMeter(&out, &hr, &fields, '\t', &json,
                      &envs, &more_json, &selected_fields, false);

    int content_bytes = 0, understood_bytes = 0;
    out.analyzeParse(OutputFormat::NONE, &content_bytes, &understood_bytes);

    if (!handled) {
        if (!json.empty() && json.back() == '}') json.pop_back();
        if (out.decryption_failed) {
            json += ", \"error\": \"decryption failed\"";
        } else {
            json += ", \"error\": \"decoding failed\"";
        }
        json += "}";
    }

    std::string id = t.addresses.empty() ? "" : t.addresses.back().id;
    return "{\"ok\":true,\"driver\":\"" + driver_name
         + "\",\"id\":\"" + id
         + "\",\"rssi_dbm\":" + std::to_string(rssi_dbm)
         + ",\"data\":" + json + "}";
}

// ─────────────────────────────────────────────
// Kommando-Antwort als JSON
// ─────────────────────────────────────────────
static std::string cmd_response_json(uint8_t cmd, const uint8_t* payload, int len,
                                      const ProtoConfig& proto)
{
    std::string type = "unknown";
    std::string info = "";
    uint8_t req_cmd = cmd & ~proto.rsp_offset;

    if (req_cmd == proto.cmd_reset_req) {
        type = "reset";
        info = (len > 0 && payload[0] == 0x00) ? "ok" : "error";
    }
    else if (req_cmd == proto.cmd_set_mode_req) {
        type = "set_mode";
        if (len > 0) { char b[8]; snprintf(b, sizeof(b), "0x%02X", payload[0]); info = b; }
    }
    else if (req_cmd == proto.cmd_set_req) {
        type = "set";
        info = (len > 0 && payload[0] == 0x00) ? "ok" : "error";
    }
    else if (req_cmd == proto.cmd_get_req) {
        type = "get";
        info = bytes_to_hex(payload, len);
    }
    else if (req_cmd == proto.cmd_serialno_req) {
        type = "serial_no";
        if (len >= 4) info = bytes_to_hex(payload, 4);
    }
    else if (req_cmd == proto.cmd_fw_version_req) {
        type = "fw_version";
        if (proto.fw_version_bytes == 4 && len >= 4) {
            int build = (payload[3] << 8) | payload[2];
            char b[64]; snprintf(b, sizeof(b), "v%d.%d (Build %d)", payload[0], payload[1], build);
            info = b;
        } else if (proto.fw_version_bytes == 3 && len >= 3) {
            char b[32]; snprintf(b, sizeof(b), "v%d.%d.%d", payload[0], payload[1], payload[2]);
            info = b;
        } else {
            info = bytes_to_hex(payload, len);
        }
    }
    else if (req_cmd == proto.cmd_factory_reset_req) {
        type = "factory_reset";
        info = (len > 0 && payload[0] == 0x00) ? "ok" : "error";
    }

    char cb[16]; snprintf(cb, sizeof(cb), "0x%02X", cmd);
    return "{\"cmd\":\"" + std::string(cb)
         + "\",\"type\":\"" + type
         + "\",\"info\":\"" + escape_json(info)
         + "\",\"payload\":\"" + bytes_to_hex(payload, len) + "\"}";
}


extern "C" {

// ── Basics ───────────────────────────────────────────────────
EMSCRIPTEN_KEEPALIVE const char* wm_version() { return "wmbusmeters-wasm-4.0"; }
EMSCRIPTEN_KEEPALIVE void wm_free_result() { free(g_result_buf); g_result_buf = nullptr; }

EMSCRIPTEN_KEEPALIVE
const char* wm_list_drivers()
{
    ensure_initialized();
    std::string json = "{\"ok\":true,\"drivers\":[";
    bool first = true;
    for (DriverInfo* di : allDrivers()) {
        if (!first) json += ",";
        first = false;
        json += "\"" + di->name().str() + "\"";
    }
    json += "]}";
    return store_result(json);
}

EMSCRIPTEN_KEEPALIVE
const char* wm_decode(const char* hex, const char* key, const char* driver)
{
    ensure_initialized();
    std::vector<uchar> frame;
    if (!hex2bin(hex, &frame) || frame.empty()) return error_result("Invalid hex string");
    std::vector<uint8_t> f(frame.begin(), frame.end());
    std::string k = (key && strlen(key) > 0) ? key : "";
    std::string d = (driver && strlen(driver) > 0) ? driver : "auto";
    return store_result(decode_telegram(f, k, d, 0));
}

// ── Serial ───────────────────────────────────────────────────
EMSCRIPTEN_KEEPALIVE
const char* wm_serial_list()
{
    auto ttys = g_web_serial_manager.listSerialTTYs();
    std::string json = "{\"ok\":true,\"devices\":[";
    for (size_t i = 0; i < ttys.size(); i++) {
        if (i > 0) json += ",";
        json += "\"" + ttys[i] + "\"";
    }
    json += "]}";
    return store_result(json);
}

EMSCRIPTEN_KEEPALIVE
const char* wm_serial_open(int index, int baudrate, int databits, int stopbits, int parity)
{
    std::string dev = "webserial:" + std::to_string(index);
    auto device = g_web_serial_manager.createSerialDeviceTTY(dev, baudrate, static_cast<PARITY>(parity), "webserial");
    if (!device) return error_result("Failed to create serial device");
    auto imp = dynamic_pointer_cast<SerialDeviceImp>(device);
    if (imp) {
        imp->setBaudrate(baudrate);
        imp->setDatabits(databits);
        imp->setStopbits(stopbits);
        imp->setParity(parity);
    }
    if (!device->open(false)) return error_result("Failed to open serial device");
    return store_result("{\"ok\":true,\"device\":\"" + dev + "\"}");
}

EMSCRIPTEN_KEEPALIVE
void wm_serial_close(int index)
{
    for (auto& d : g_web_serial_manager.devices) {
        if (d->index == index) {
            d->close();  // setzt isOpen = false und ruft EM_ASM closeSerialPort auf
            break;
        }
    }
    g_devices.erase(index); // Keine Callbacks mehr für dieses Device
}

// ── Protokoll konfigurieren ──────────────────────────────────
EMSCRIPTEN_KEEPALIVE
const char* wm_configure_protocol(int index, const char* protocol)
{
    DeviceState ds;
    if (strcmp(protocol, "amb8465") == 0)      ds.proto = AMB8465_PROTO;
    else if (strcmp(protocol, "tarvos2") == 0)  ds.proto = TARVOS2_PROTO;
    else if (strcmp(protocol, "raw") == 0)      ds.proto = RAW_PROTO;
    else return error_result("Unknown protocol");
    g_device_states[index] = ds;
    return store_result("{\"ok\":true}");
}

// ── Kommando senden (baut SOF+CS Frame) ─────────────────────
EMSCRIPTEN_KEEPALIVE
const char* wm_send_command(int index, int cmd, const char* payload_hex)
{
    auto sit = g_device_states.find(index);
    if (sit == g_device_states.end()) return error_result("No protocol configured");
    const ProtoConfig& proto = sit->second.proto;
    if (proto.type == DongleProto::RAW) return error_result("RAW mode has no commands");

    std::vector<uchar> payload;
    if (payload_hex && strlen(payload_hex) > 0) {
        if (!hex2bin(payload_hex, &payload)) return error_result("Invalid payload hex");
    }

    std::vector<uint8_t> frame;
    frame.push_back(proto.sof);
    frame.push_back((uint8_t)cmd);
    frame.push_back((uint8_t)payload.size());
    for (auto b : payload) frame.push_back(b);
    frame.push_back(xor_checksum(frame.data(), 0, frame.size()));

    for (auto& d : g_web_serial_manager.devices) {
        if (d->index == index && d->isOpen) {
            d->send(frame);
            return store_result("{\"ok\":true,\"frame\":\"" + bytes_to_hex(frame.data(), frame.size()) + "\"}");
        }
    }
    return error_result("Device not open");
}

// ── Kernfunktion: Buffer verarbeiten ─────────────────────────
// Liest rxBuffer → parst Dongle-Protokoll → dekodiert Telegramme
// Gibt alles als JSON zurück
EMSCRIPTEN_KEEPALIVE
const char* wm_process_buffer(int index, const char* key, const char* driver)
{
    ensure_initialized();

    auto sit = g_device_states.find(index);
    if (sit == g_device_states.end()) return error_result("No protocol configured");

    DeviceState& state = sit->second;
    const ProtoConfig& proto = state.proto;

    // Neue Bytes aus serial rxBuffer holen
    int new_bytes = 0;
    for (auto& d : g_web_serial_manager.devices) {
        if (d->index == index && d->isOpen) {
            std::vector<uint8_t> data;
            int n = d->receive(&data);
            if (n > 0) {
                state.buffer.insert(state.buffer.end(), data.begin(), data.end());
                new_bytes = n;
            }
            break;
        }
    }

    if (state.buffer.empty()) {
        return store_result("{\"ok\":true,\"bytes\":0,\"telegrams\":[],\"responses\":[]}");
    }

    std::string key_str = (key && strlen(key) > 0) ? key : "";
    std::string driver_str = (driver && strlen(driver) > 0) ? driver : "auto";

    std::string telegrams_json;
    std::string responses_json;
    bool first_t = true, first_r = true;
    int bytes_consumed = 0;
    auto& buf = state.buffer;

    // ── RAW ──
    if (proto.type == DongleProto::RAW) {
        while (buf.size() >= 2) {
            uint8_t L = buf[0];
            if (L < 9) { buf.erase(buf.begin()); bytes_consumed++; continue; }
            if (!is_valid_wmbus_cfield(buf[1])) { buf.erase(buf.begin()); bytes_consumed++; continue; }
            int total = 1 + L + 1;
            if ((int)buf.size() < total) break;

            double rssi = calculate_rssi(buf[total - 1]);
            std::vector<uint8_t> wf(buf.begin(), buf.begin() + total - 1);
            if (!first_t) telegrams_json += ",";
            first_t = false;
            telegrams_json += decode_telegram(wf, key_str, driver_str, rssi);
            buf.erase(buf.begin(), buf.begin() + total);
            bytes_consumed += total;
        }
    }
    // ── AMB8465 / Tarvos ──
    else {
        while (buf.size() >= 3) {
            if (buf[0] == proto.sof) {
                uint8_t msg_id = buf[1];
                uint8_t plen = buf[2];
                int flen = 4 + plen;
                if ((int)buf.size() < flen) break;

                if (xor_checksum(buf.data(), 0, flen - 1) != buf[flen - 1]) {
                    buf.erase(buf.begin()); bytes_consumed++; continue;
                }

                std::vector<uint8_t> frame(buf.begin(), buf.begin() + flen);
                buf.erase(buf.begin(), buf.begin() + flen);
                bytes_consumed += flen;
                int doff = proto.data_offset;

                if (msg_id == proto.cmd_data_ind && plen >= 10) {
                    double rssi = calculate_rssi(frame[doff + plen - 1]);
                    std::vector<uint8_t> wf(frame.begin() + doff, frame.begin() + doff + plen - 1);
                    if (!first_t) telegrams_json += ",";
                    first_t = false;
                    telegrams_json += decode_telegram(wf, key_str, driver_str, rssi);
                } else {
                    if (!first_r) responses_json += ",";
                    first_r = false;
                    responses_json += cmd_response_json(msg_id, frame.data() + doff, plen, proto);
                }
            }
            else if (proto.type == DongleProto::AMB8465) {
                // Transparent mode
                uint8_t tlen = buf[0];
                if (tlen < 10) { buf.erase(buf.begin()); bytes_consumed++; continue; }
                if (buf.size() >= 2 && !is_valid_wmbus_cfield(buf[1])) {
                    buf.erase(buf.begin()); bytes_consumed++; continue;
                }
                int flen = tlen + 2;
                if ((int)buf.size() < flen) break;

                double rssi = calculate_rssi(buf[flen - 1]);
                std::vector<uint8_t> wf(buf.begin(), buf.begin() + flen - 1);
                if (!first_t) telegrams_json += ",";
                first_t = false;
                telegrams_json += decode_telegram(wf, key_str, driver_str, rssi);
                buf.erase(buf.begin(), buf.begin() + flen);
                bytes_consumed += flen;
            }
            else {
                buf.erase(buf.begin()); bytes_consumed++;
            }
        }
    }

    return store_result("{\"ok\":true,\"bytes\":" + std::to_string(bytes_consumed)
        + ",\"telegrams\":[" + telegrams_json
        + "],\"responses\":[" + responses_json + "]}");
}
EMSCRIPTEN_KEEPALIVE
const char* wm_detect_device_at_baud(int index, int baudrate)
{
    // Device über den Index finden
    SerialDeviceImp* dev = nullptr;
    for (auto& d : g_web_serial_manager.devices) {
        if (d->index == index && d->isOpen) {
            dev = d.get();
            break;
        }
    }
    if (!dev) return error_result("Device not open");

    // Detected-Objekt vorbereiten
    Detected detected;
    detected.found_file = "webserial:" + std::to_string(index);
    detected.found_bps = baudrate;

    // LinkModeSet (alle Modi)
    LinkModeSet desired_linkmodes;
    desired_linkmodes.setAll();

    // Zu testende Gerätetypen
    set<BusDeviceType> probe_for = {
        DEVICE_AMB8465,
        DEVICE_IM871A,
        DEVICE_RC1180,
        DEVICE_CUL,
        DEVICE_IU891A
    };

    // Shared-Ptr für Manager (No-Op-Deleter)
    struct NoOpManagerDeleter { void operator()(SerialCommunicationManager*) const {} };
    auto manager_ptr = std::shared_ptr<SerialCommunicationManager>(&g_web_serial_manager, NoOpManagerDeleter());

    // detectBusDeviceOnTTY aufrufen (unveränderte Originalfunktion)
    Detected result = detectBusDeviceOnTTY(detected.found_file,
                                           probe_for,
                                           desired_linkmodes,
                                           manager_ptr,
                                           std::to_string(baudrate));

    if (result.found_type != DEVICE_UNKNOWN) {
        return store_result(detectedToJson(result));
    } else {
        return store_result("{\"ok\":true,\"detected\":false}");
    }
}
} // extern "C"