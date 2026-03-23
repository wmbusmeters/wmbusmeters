#ifdef __EMSCRIPTEN__

#include "serial_web.h"
#include <emscripten.h>

// Global instance
SerialCommunicationManagerImp* g_web_serial_manager = nullptr;

static map<int, SerialDeviceImp*> g_devices;

// ═══════════════════════════════════════════════════════
// SerialDeviceImp
// ═══════════════════════════════════════════════════════

SerialDeviceImp::SerialDeviceImp(int idx) : index(idx) {}

bool SerialDeviceImp::open(bool) {
    isOpen = true;
    // Don't call JS openDeviceFull — the WebSerial port was already
    // opened by requestAndOpen() and stays open for the lifetime.
    // C++ open/close just manages the internal device state for
    // the detection probe cycle (open → probe → close → next probe).
    return true;
}

void SerialDeviceImp::close() {
    if (!isOpen) return;
    isOpen = false;
    rxBuffer.clear();
    // Don't call JS closeSerialPort — keep the WebSerial port open.
    // The detection cycle calls close() between probes, but we don't
    // want to actually close/reopen the browser serial port each time.
}

bool SerialDeviceImp::send(vector<uchar>& data) {
    if (!isOpen || data.empty()) return false;
    EM_ASM({ writeSerial($0, $1, $2); }, index, data.data(), (int)data.size());
    return true;
}

// Helper: pull any pending data from JS read-loop buffer into rxBuffer.
// This is safe to call at any time — no ASYNCIFY re-entrancy issues
// because the call goes C++ → EM_ASM → JS → serial_on_data() → C++,
// all in one synchronous chain.
static void drainFromJS(int index) {
    EM_ASM({
        if (typeof drainSerialData === 'function') drainSerialData($0);
    }, index);
}

int SerialDeviceImp::receive(vector<uint8_t>* out) {
    // Pull any data already buffered in JS
    drainFromJS(index);

    // Phase 1: Wait up to 500ms for ANY data to arrive
    int timeout = 0;
    while (rxBuffer.empty() && timeout < 50) {
        usleep(10000); // 10ms — yields to JS event loop via Asyncify
        drainFromJS(index);
        timeout++;
    }
    if (rxBuffer.empty()) return 0;

    // Phase 2: Data started arriving. Wait a bit more for the rest.
    // AMB8465 config response is 126 bytes at 9600 baud = ~130ms.
    // Keep waiting as long as new data keeps arriving.
    size_t prev_size = 0;
    int idle_count = 0;
    while (idle_count < 5) { // 5 * 20ms = 100ms of silence = done
        usleep(20000); // 20ms
        drainFromJS(index);
        if (rxBuffer.size() > prev_size) {
            prev_size = rxBuffer.size();
            idle_count = 0; // reset — more data is still coming
        } else {
            idle_count++;
        }
    }

    if (out) {
        out->insert(out->end(), rxBuffer.begin(), rxBuffer.end());
    }
    int n = rxBuffer.size();
    rxBuffer.clear();
    return n;
}

bool SerialDeviceImp::working() { return isOpen; }
string SerialDeviceImp::device() { return "webserial:" + to_string(index); }
void SerialDeviceImp::disableCallbacks() {}
void SerialDeviceImp::enableCallbacks() {}
bool SerialDeviceImp::skippingCallbacks() { return false; }
void SerialDeviceImp::fill(vector<uchar>& data) {
    rxBuffer.insert(rxBuffer.end(), data.begin(), data.end());
}
bool SerialDeviceImp::waitFor(uchar) { return false; }
bool SerialDeviceImp::resetting() { return false; }
bool SerialDeviceImp::opened() { return isOpen; }
bool SerialDeviceImp::isClosed() { return !isOpen; }
bool SerialDeviceImp::readonly() { return false; }
int SerialDeviceImp::fd() { return -1; }
void SerialDeviceImp::resetInitiated() {}
void SerialDeviceImp::resetCompleted() {}
bool SerialDeviceImp::checkIfDataIsPending() {
    drainFromJS(index);
    return !rxBuffer.empty();
}
SerialCommunicationManager* SerialDeviceImp::manager() { return g_web_serial_manager; }

// ═══════════════════════════════════════════════════════
// SerialCommunicationManagerImp
// ═══════════════════════════════════════════════════════

shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceTTY(
    string dev, int baud_rate, PARITY parity, string purpose)
{
    shared_ptr<SerialDeviceImp> found;

    // Look for existing device by exact name
    for (auto& d : devices) {
        if (d->device() == dev) { found = d; break; }
    }

    // Map /dev/ttyUSB0 or /tmp/ttyUSB0 → device by index
    if (!found && (dev.find("/dev/ttyUSB") == 0 || dev.find("/tmp/ttyUSB") == 0)) {
        size_t pos = dev.rfind("ttyUSB");
        string idx_str = dev.substr(pos + 6);
        try {
            int idx = stoi(idx_str);
            for (auto& d : devices) {
                if (d->index == idx) { found = d; break; }
            }
        } catch (...) {}
    }

    // Map webserial:N
    if (!found && dev.find("webserial:") == 0) {
        try {
            int idx = stoi(dev.substr(10));
            for (auto& d : devices) {
                if (d->index == idx) { found = d; break; }
            }
        } catch (...) {}
    }

    // Fallback: return first available device
    if (!found && !devices.empty()) found = devices[0];

    // Apply serial parameters so open() can propagate them to JS
    if (found) {
        found->setBaudrate(baud_rate);
        int p = 0;
        if (parity == PARITY::EVEN) p = 1;
        else if (parity == PARITY::ODD) p = 2;
        found->setParity(p);
    }

    return found;
}

shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceCommand(
    string, string, vector<string>, vector<string>, string) { return nullptr; }
shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceFile(string, string) { return nullptr; }
shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceSimulator() { return nullptr; }
shared_ptr<SerialDevice> SerialCommunicationManagerImp::createSerialDeviceSocket(string, string) { return nullptr; }

void SerialCommunicationManagerImp::listenTo(SerialDevice* sd, function<void()> cb) {
    if (sd && cb) {
        listen_callbacks_[sd] = cb;
    }
}

void SerialCommunicationManagerImp::onDisappear(SerialDevice*, function<void()>) {}
void SerialCommunicationManagerImp::expectDevicesToWork() {}

void SerialCommunicationManagerImp::stop() {
    running_ = false;
}

void SerialCommunicationManagerImp::startEventLoop() {
    running_ = true;
}

// ── THE EVENT LOOP ────────────────────────────────────
// This is the heart: main.cc calls waitForStop() and expects
// to block here while telegrams are being processed.
// With ASYNCIFY, emscripten_sleep() yields to JS, letting the
// bridge read loop push data into rxBuffers. We then check if
// any device has pending data and fire the registered callbacks.
void SerialCommunicationManagerImp::waitForStop() {
    while (running_) {
        tickCallbacks();
        // Yield to JS event loop — lets WebSerial bridge deliver data
        emscripten_sleep(20);
    }
}

bool SerialCommunicationManagerImp::isRunning() { return running_; }

int SerialCommunicationManagerImp::startRegularCallback(string, int, function<void()>) { return 0; }
void SerialCommunicationManagerImp::stopRegularCallback(int) {}

AccessCheck SerialCommunicationManagerImp::checkAccess(
    string, shared_ptr<SerialCommunicationManager>, string,
    function<AccessCheck(string,shared_ptr<SerialCommunicationManager>)>)
{
    // In the browser we always have access
    return AccessCheck::AccessOK;
}

vector<string> SerialCommunicationManagerImp::listSerialTTYs() {
    vector<string> result;
    for (auto& d : devices) {
        result.push_back("/dev/ttyUSB" + to_string(d->index));
    }
    return result;
}

shared_ptr<SerialDevice> SerialCommunicationManagerImp::lookup(string device) {
    for (auto& d : devices) {
        if (d->device() == device) return d;
    }
    return nullptr;
}

bool SerialCommunicationManagerImp::removeNonWorking(string) { return false; }
void SerialCommunicationManagerImp::tickleEventLoop() {}

void SerialCommunicationManagerImp::tickCallbacks() {
    for (auto& kv : listen_callbacks_) {
        SerialDevice* sd = kv.first;
        if (sd && sd->checkIfDataIsPending()) {
            kv.second(); // Fires processSerialData()
        }
    }
}

// ── Register a device opened by JS ────────────────────
void SerialCommunicationManagerImp::registerWebDevice(int index) {
    for (auto& d : devices) {
        if (d->index == index) return; // already registered
    }
    auto dev = make_shared<SerialDeviceImp>(index);
    dev->isOpen = true;
    devices.push_back(dev);
    g_devices[index] = dev.get();

    // Create fake ttyUSB file in emscripten VFS so stat()/access() checks pass.
    // /dev is read-only in emscripten, use /tmp instead.
    string path = "/tmp/ttyUSB" + to_string(index);
    FILE* f = fopen(path.c_str(), "w");
    if (f) fclose(f);
}

SerialDeviceImp* SerialCommunicationManagerImp::findByIndex(int index) {
    for (auto& d : devices) {
        if (d->index == index) return d.get();
    }
    return nullptr;
}

// ═══════════════════════════════════════════════════════
// JS → C++ Callback
// ═══════════════════════════════════════════════════════
extern "C" {
void serial_on_data(int index, uint8_t* data, int len) {
    auto it = g_devices.find(index);
    if (it == g_devices.end()) return;
    it->second->rxBuffer.insert(it->second->rxBuffer.end(), data, data + len);
}
}

// ═══════════════════════════════════════════════════════
// Factory: createSerialCommunicationManager()
// Called by main.cc → start(). Returns our global manager
// that already has the WebSerial device registered.
// ═══════════════════════════════════════════════════════
shared_ptr<SerialCommunicationManager> createSerialCommunicationManager(long long, bool)
{
    if (!g_web_serial_manager) {
        g_web_serial_manager = new SerialCommunicationManagerImp();
    }
    // Custom deleter: don't delete the global
    return shared_ptr<SerialCommunicationManager>(
        g_web_serial_manager, [](SerialCommunicationManager*) {}
    );
}

// Base class destructor
SerialCommunicationManager::~SerialCommunicationManager() {}

#endif