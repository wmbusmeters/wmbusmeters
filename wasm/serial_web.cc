#ifdef __EMSCRIPTEN__

#include "serial_web.h"
#include <emscripten.h>

static map<int, SerialDeviceImp*> g_devices; // Für Callback

// Debug-Makro, das in JavaScript console.log ausgibt
#define LOG_DEBUG(msg) EM_ASM({ console.log("[C++] " + UTF8ToString($0)); }, msg.c_str());

// ── SerialDeviceImp ─────────────────────────────────────────────────────────

SerialDeviceImp::SerialDeviceImp(int idx) : index(idx) {}

bool SerialDeviceImp::open(bool) {
    // Port ist bereits durch wm_serialOpen geöffnet, also nichts tun
    isOpen = true;
    return true;
}

void SerialDeviceImp::close() {
    if (!isOpen) return;
    // In JavaScript: closeSerialPort(index) aufrufen
    EM_ASM({ closeSerialPort($0); }, index);
    isOpen = false;
}

bool SerialDeviceImp::send(vector<uchar>& data) {
    EM_ASM({ writeSerial($0, $1, $2); }, index, data.data(), data.size());
    return true;
}

int SerialDeviceImp::receive(vector<uint8_t>* out) {
    int timeout = 0;
    while (rxBuffer.empty() && timeout < 50) {  // max. 500 ms warten
        emscripten_sleep(10);
        timeout++;
    }
    if (rxBuffer.empty()) return 0;
    *out = rxBuffer;
    int n = rxBuffer.size();
    rxBuffer.clear();
    return n;
}

bool SerialDeviceImp::working() {
    return isOpen;
}

string SerialDeviceImp::device() {
    return "webserial:" + to_string(index);
}

// Die meisten Methoden sind in der Web-Umgebung irrelevant
void SerialDeviceImp::disableCallbacks() {}
void SerialDeviceImp::enableCallbacks() {}
bool SerialDeviceImp::skippingCallbacks() { return false; }
void SerialDeviceImp::fill(vector<uchar>&) {}
bool SerialDeviceImp::waitFor(uchar) { return false; }
bool SerialDeviceImp::resetting() { return false; }
bool SerialDeviceImp::opened() { return isOpen; }
bool SerialDeviceImp::isClosed() { return !isOpen; }
bool SerialDeviceImp::readonly() { return false; }
int SerialDeviceImp::fd() { return -1; }
void SerialDeviceImp::resetInitiated() {}
void SerialDeviceImp::resetCompleted() {}
bool SerialDeviceImp::checkIfDataIsPending() { return !rxBuffer.empty(); }
SerialCommunicationManager* SerialDeviceImp::manager() { return nullptr; }

// ── SerialCommunicationManagerImp ───────────────────────────────────────────

shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceTTY(string dev, int baud_rate, PARITY parity, string purpose) {
    // dev ist z.B. "webserial:0"
    if (dev.find("webserial:") != 0) return nullptr;
    int index = stoi(dev.substr(10));

    // Bereits vorhandenes Device suchen (sollte durch wm_serialOpen angelegt sein)
    for (auto& d : devices) {
        if (d->index == index) {
            return d;
        }
    }

    // Falls nicht vorhanden (sollte nicht passieren), neu anlegen
    auto d = make_shared<SerialDeviceImp>(index);
    d->setBaudrate(baud_rate);
    d->setDatabits(8);
    d->setStopbits(1);
    d->setParity(static_cast<int>(parity));
    devices.push_back(d);
    g_devices[index] = d.get();
    return d;
}

// Die anderen create-Methoden werden nicht benötigt
shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceCommand(string, string, vector<string>, vector<string>, string) {
    return nullptr;
}
shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceFile(string, string) {
    return nullptr;
}
shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceSimulator() {
    return nullptr;
}
shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceSocket(string, string) {
    return nullptr;
}

void SerialCommunicationManagerImp::listenTo(SerialDevice*, function<void()>) {}
void SerialCommunicationManagerImp::onDisappear(SerialDevice*, function<void()>) {}
void SerialCommunicationManagerImp::expectDevicesToWork() {}
void SerialCommunicationManagerImp::stop() {}
void SerialCommunicationManagerImp::startEventLoop() {}
void SerialCommunicationManagerImp::waitForStop() {}
bool SerialCommunicationManagerImp::isRunning() { return true; }

int SerialCommunicationManagerImp::startRegularCallback(string, int, function<void()>) { return 0; }
void SerialCommunicationManagerImp::stopRegularCallback(int) {}

AccessCheck SerialCommunicationManagerImp::checkAccess(string, shared_ptr<SerialCommunicationManager>, string,
                                                        function<AccessCheck(string,shared_ptr<SerialCommunicationManager>)>) {
    return AccessCheck::AccessOK; // In der Web-Umgebung gehen wir von Zugriff aus
}

vector<string> SerialCommunicationManagerImp::listSerialTTYs() {
    vector<string> result;
    int count = EM_ASM_INT({ return getDeviceCount(); });
    for (int i = 0; i < count; i++)
        result.push_back("webserial:" + to_string(i));
    return result;
}

shared_ptr<SerialDevice> SerialCommunicationManagerImp::lookup(string device) {
    for (auto& d : devices) {
        if (d->device() == device) return d;
    }
    return nullptr;
}

bool SerialCommunicationManagerImp::removeNonWorking(string) {
    return false;
}

void SerialCommunicationManagerImp::tickleEventLoop() {}

// ── JS → C++ Callback ──────────────────────────────────────────────────────


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

extern "C" {
void serial_on_data(int index, uint8_t* data, int len) {
    auto it = g_devices.find(index);
    if (it == g_devices.end()) return;
    // Debug: Zeige empfangene Bytes
    string hex = bytes_to_hex(data, len);
    it->second->rxBuffer.insert(it->second->rxBuffer.end(), data, data + len);
}
}

// Destruktor der Basisklasse (wird vom Linker benötigt)
SerialCommunicationManager::~SerialCommunicationManager() {}

#endif