#ifdef __EMSCRIPTEN__

#include "serial_web.h"
#include <emscripten.h>
#include <algorithm>

// Global instance
SerialCommunicationManagerImp* g_web_serial_manager = nullptr;

static map<int, SerialDeviceImp*> g_devices;

// ═══════════════════════════════════════════════════════
// SerialDeviceImp
// ═══════════════════════════════════════════════════════

SerialDeviceImp::SerialDeviceImp(int idx) : index(idx) {}

bool SerialDeviceImp::open(bool) {
    if (disconnected) return false;
    isOpen = true;

    // Tell JS to reopen the port if baudrate/parity changed.
    // Returns 1 if a change was queued, 0 if same settings (no-op).
    int changed = EM_ASM_INT({
        if (typeof queueBaudrateChange === 'function')
            return queueBaudrateChange($0, $1, $2, $3, $4);
        return 0;
    }, index, baudrate, databits, stopbits, parity);

    if (changed) {
        // Poll-wait until JS signals the reopen is complete.
        // Each usleep yields to the JS event loop, allowing the
        // async close+open to progress.
        for (int i = 0; i < 50; i++) { // max 500ms
            int ready = EM_ASM_INT({
                return (typeof serialPortReady !== 'undefined' && serialPortReady[$0]) ? 1 : 0;
            }, index);
            if (ready) {
                // Clear the flag for next time
                EM_ASM({ serialPortReady[$0] = false; }, index);
                break;
            }
            usleep(10000); // 10ms — yields to JS
        }
        // Clear any stale data from before the baudrate change
        rxBuffer.clear();
    }

    return true;
}

void SerialDeviceImp::close() {
    if (!isOpen) return;
    isOpen = false;
    rxBuffer.clear();
}

bool SerialDeviceImp::send(vector<uchar>& data) {
    if (!isOpen || disconnected || data.empty()) return false;
    EM_ASM({ writeSerial($0, $1, $2); }, index, data.data(), (int)data.size());
    return true;
}

// Helper: pull pending data from JS read-loop buffer into rxBuffer.
static void drainFromJS(int index) {
    EM_ASM({
        if (typeof drainSerialData === 'function') drainSerialData($0);
    }, index);
}

int SerialDeviceImp::receive(vector<uint8_t>* out) {
    if (disconnected) return 0;
    drainFromJS(index);

    // Wait up to 200ms for first data to arrive.
    // writeSerial initiates the write in the same JS tick, but the
    // device needs time to process and respond. Each usleep yields
    // to the JS event loop so async operations (write promises,
    // read loop) can progress.
    if (rxBuffer.empty()) {
        for (int i = 0; i < 20 && rxBuffer.empty(); i++) {
            usleep(10000); // 10ms × 20 = 200ms max
            drainFromJS(index);
        }
    }

    if (rxBuffer.empty()) return 0;

    // Data arrived — collect rest of message until 30ms of silence
    size_t prev_size = 0;
    int idle_count = 0;
    while (idle_count < 3) {
        usleep(10000);
        drainFromJS(index);
        if (rxBuffer.size() > prev_size) {
            prev_size = rxBuffer.size();
            idle_count = 0;
        } else {
            idle_count++;
        }
    }

    if (out) {
        out->clear(); // Must clear first — matches native receive() behavior
        out->insert(out->end(), rxBuffer.begin(), rxBuffer.end());
    }
    int n = rxBuffer.size();
    rxBuffer.clear();
    return n;
}

// working() is THE critical method: BusDevice::isWorking() checks this.
// Returns false when USB is disconnected → pipeline detects dead device.
bool SerialDeviceImp::working() { return isOpen && !disconnected; }

string SerialDeviceImp::device() { return "/dev/ttyWebUSB" + to_string(index); }
void SerialDeviceImp::disableCallbacks() {}
void SerialDeviceImp::enableCallbacks() {}
bool SerialDeviceImp::skippingCallbacks() { return false; }
void SerialDeviceImp::fill(vector<uchar>& data) {
    rxBuffer.insert(rxBuffer.end(), data.begin(), data.end());
}
bool SerialDeviceImp::waitFor(uchar) { return false; }
bool SerialDeviceImp::resetting() { return false; }
bool SerialDeviceImp::opened() { return isOpen; }
bool SerialDeviceImp::isClosed() { return !isOpen || disconnected; }
bool SerialDeviceImp::readonly() { return false; }
int SerialDeviceImp::fd() { return -1; }
void SerialDeviceImp::resetInitiated() {}
void SerialDeviceImp::resetCompleted() {}
bool SerialDeviceImp::checkIfDataIsPending() {
    if (disconnected) return false;
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

    for (auto& d : devices) {
        if (d->device() == dev && !d->disconnected) { found = d; break; }
    }

    if (!found && dev.find("/dev/ttyWebUSB") == 0) {
        try {
            int idx = stoi(dev.substr(11));
            for (auto& d : devices) {
                if (d->index == idx && !d->disconnected) { found = d; break; }
            }
        } catch (...) {}
    }

    if (!found) {
        for (auto& d : devices) {
            if (!d->disconnected) { found = d; break; }
        }
    }

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
        managed_devices_.insert(sd->device());
    }
}

void SerialCommunicationManagerImp::onDisappear(SerialDevice* sd, function<void()> cb) {
    if (sd) {
        if (cb) {
            disappear_callbacks_[sd] = cb;
        } else {
            disappear_callbacks_.erase(sd);
        }
    }
}

void SerialCommunicationManagerImp::expectDevicesToWork() {}

void SerialCommunicationManagerImp::stop() {
    running_ = false;
}

void SerialCommunicationManagerImp::startEventLoop() {
    running_ = true;
}

// ── THE EVENT LOOP ────────────────────────────────────
void SerialCommunicationManagerImp::waitForStop() {
    while (running_) {
        tickCallbacks();
        emscripten_sleep(20);
    }
}

bool SerialCommunicationManagerImp::isRunning() { return running_; }

int SerialCommunicationManagerImp::startRegularCallback(string name, int seconds, function<void()> callback) {
    int id = next_timer_id_++;
    timers_.push_back({ id, seconds, time(NULL), callback, name });
    return id;
}

void SerialCommunicationManagerImp::stopRegularCallback(int id) {
    timers_.erase(
        remove_if(timers_.begin(), timers_.end(),
                  [id](const WebTimer& t) { return t.id == id; }),
        timers_.end());
}

AccessCheck SerialCommunicationManagerImp::checkAccess(
    string, shared_ptr<SerialCommunicationManager>, string,
    function<AccessCheck(string,shared_ptr<SerialCommunicationManager>)>)
{
    return AccessCheck::AccessOK;
}

vector<string> SerialCommunicationManagerImp::listSerialTTYs() {
    vector<string> result;
    for (auto& d : devices) {
        if (!d->disconnected) {
            result.push_back("/dev/ttyWebUSB" + to_string(d->index));
        }
    }
    return result;
}

shared_ptr<SerialDevice> SerialCommunicationManagerImp::lookup(string device) {
    if (managed_devices_.count(device) == 0) return nullptr;
    for (auto& d : devices) {
        if (d->device() == device && !d->disconnected) return d;
    }
    return nullptr;
}

bool SerialCommunicationManagerImp::removeNonWorking(string device) {
    // Find and clean up a non-working device
    for (auto& d : devices) {
        if (d->device() == device && d->disconnected) {
            cleanupDevice(d->index);
            return true;
        }
    }
    return false;
}

void SerialCommunicationManagerImp::tickleEventLoop() {}

// ── Central tick: disconnects + timers + data callbacks ──
void SerialCommunicationManagerImp::tickCallbacks() {
    drainDisconnects();
    drainConnects();
    fireTimers();

    for (auto& kv : listen_callbacks_) {
        SerialDevice* sd = kv.first;
        if (sd && sd->working() && sd->checkIfDataIsPending()) {
            kv.second();
        }
    }
}

// ── Drain disconnect events from JS ──────────────────
// JS sets pendingDisconnects = [index, ...] when USB is removed.
// We drain it here (safe — called from C++ context, not during ASYNCIFY).
void SerialCommunicationManagerImp::drainDisconnects() {
    int count = EM_ASM_INT({
        return (typeof pendingDisconnects !== 'undefined') ? pendingDisconnects.length : 0;
    });
    if (count == 0) return;

    for (int i = 0; i < count; i++) {
        int idx = EM_ASM_INT({
            return pendingDisconnects[$0];
        }, i);
        markDeviceDisconnected(idx);
    }

    EM_ASM({
        pendingDisconnects.splice(0, $0);
    }, count);
}

// ── Drain connect events from JS (auto-connect) ───
// JS opens the port and pushes the index into pendingConnects.
// We register the device in C++ here (safe — C++ context).
void SerialCommunicationManagerImp::drainConnects() {
    int count = EM_ASM_INT({
        return (typeof pendingConnects !== 'undefined') ? pendingConnects.length : 0;
    });
    if (count == 0) return;

    for (int i = 0; i < count; i++) {
        int idx = EM_ASM_INT({
            return pendingConnects[$0];
        }, i);
        registerWebDevice(idx);
        printf("(serial) device /dev/ttyWebUSB%d connected\n", idx);
    }

    EM_ASM({
        pendingConnects.splice(0, $0);
    }, count);
    // JS portchange handler will stop the session and restart it.
}

// ── Fire due timers ──────────────────────────────────
void SerialCommunicationManagerImp::fireTimers() {
    time_t now = time(NULL);
    for (auto& t : timers_) {
        if ((now - t.last_call) >= t.seconds) {
            t.last_call = now;
            if (t.callback) t.callback();
        }
    }
}

// ── Register a device opened by JS ───────────────────
void SerialCommunicationManagerImp::registerWebDevice(int index) {
    for (auto& d : devices) {
        if (d->index == index) {
            // Re-register: reset state (connect case)
            d->isOpen = true;
            d->disconnected = false;
            d->rxBuffer.clear();
            g_devices[index] = d.get();
            return;
        }
    }
    auto dev = make_shared<SerialDeviceImp>(index);
    dev->isOpen = true;
    devices.push_back(dev);
    g_devices[index] = dev.get();

    // Create character device in VFS so stat() returns S_ISCHR=true
    EM_ASM({
        try {
            FS.createDevice('/dev', 'ttyWebUSB' + $0,
                function() { return null; },
                function(c) {}
            );
        } catch(e) {
            console.warn('[vfs] createDevice /dev/ttyWebUSB' + $0 + ' failed:', e);
        }
    }, index);
}

SerialDeviceImp* SerialCommunicationManagerImp::findByIndex(int index) {
    for (auto& d : devices) {
        if (d->index == index) return d.get();
    }
    return nullptr;
}

// ── Mark device as disconnected (soft close) ─────────
// Called when JS reports USB disconnect.
// Does NOT remove from data structures — the pipeline still holds
// shared_ptrs to the device. working() now returns false, so
// checkForDeadWmbusDevices will detect it and clean up.
void SerialCommunicationManagerImp::markDeviceDisconnected(int index) {
    auto* dev = findByIndex(index);
    if (!dev) return;
    if (dev->disconnected) return; // already processed

    dev->disconnected = true;
    dev->isOpen = false;
    dev->rxBuffer.clear();

    string dev_name = "/dev/ttyWebUSB" + to_string(index);
    printf("(serial) device %s disconnected\n", dev_name.c_str());

    // Fire onDisappear callback so BusDevice is notified immediately
    auto it = disappear_callbacks_.find(dev);
    if (it != disappear_callbacks_.end() && it->second) {
        it->second();
    }

    // Clean up pipeline state so that after connect:
    // - lookup() returns nullptr → detection re-runs
    // - listen_callbacks_ doesn't hold stale pointers
    managed_devices_.erase(dev_name);
    listen_callbacks_.erase(dev);
    disappear_callbacks_.erase(dev);
}

// ── Full cleanup: remove device from everything ──────
// Called after the pipeline has finished with the device
// (via removeNonWorking or explicit user action).
void SerialCommunicationManagerImp::cleanupDevice(int index) {
    string dev_name = "/dev/ttyWebUSB" + to_string(index);
    managed_devices_.erase(dev_name);

    for (auto it = listen_callbacks_.begin(); it != listen_callbacks_.end(); ) {
        SerialDeviceImp* sd = dynamic_cast<SerialDeviceImp*>(it->first);
        if (sd && sd->index == index) {
            it = listen_callbacks_.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = disappear_callbacks_.begin(); it != disappear_callbacks_.end(); ) {
        SerialDeviceImp* sd = dynamic_cast<SerialDeviceImp*>(it->first);
        if (sd && sd->index == index) {
            it = disappear_callbacks_.erase(it);
        } else {
            ++it;
        }
    }

    g_devices.erase(index);

    devices.erase(
        remove_if(devices.begin(), devices.end(),
                  [index](const shared_ptr<SerialDeviceImp>& d) { return d->index == index; }),
        devices.end());

    EM_ASM({
        try { FS.unlink('/dev/ttyWebUSB' + $0); } catch(e) {}
    }, index);
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
// Factory
// ═══════════════════════════════════════════════════════
shared_ptr<SerialCommunicationManager> createSerialCommunicationManager(long long, bool)
{
    if (!g_web_serial_manager) {
        g_web_serial_manager = new SerialCommunicationManagerImp();
    }
    return shared_ptr<SerialCommunicationManager>(
        g_web_serial_manager, [](SerialCommunicationManager*) {}
    );
}

SerialCommunicationManager::~SerialCommunicationManager() {}

#endif