#pragma once
#ifdef __EMSCRIPTEN__

#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <functional>
#include <ctime>
#include "serial.h"

using namespace std;

struct SerialDeviceImp : public SerialDevice
{
    int index;
    vector<uint8_t> rxBuffer;
    bool isOpen = false;
    bool disconnected = false; // Set true when JS reports USB disconnect
    int baudrate = 115200;
    int databits = 8;
    int stopbits = 1;
    int parity = 0;

    SerialDeviceImp(int idx);

    bool open(bool fail_if_not_ok) override;
    void close() override;
    bool send(vector<uchar>& data) override;
    int receive(vector<uchar>* out) override;
    bool working() override;
    string device() override;
    void disableCallbacks() override;
    void enableCallbacks() override;
    bool skippingCallbacks() override;
    void fill(vector<uchar>&) override;
    bool waitFor(uchar c) override;
    bool resetting() override;
    bool opened() override;
    bool isClosed() override;
    bool readonly() override;
    int fd() override;
    void resetInitiated() override;
    void resetCompleted() override;
    bool checkIfDataIsPending() override;
    SerialCommunicationManager* manager() override;

    void setBaudrate(int b) { baudrate = b; }
    void setDatabits(int d) { databits = d; }
    void setStopbits(int s) { stopbits = s; }
    void setParity(int p) { parity = p; }
};

// Simple timer for startRegularCallback
struct WebTimer {
    int id;
    int seconds;
    time_t last_call;
    function<void()> callback;
    string name;
};

struct SerialCommunicationManagerImp : public SerialCommunicationManager
{
    // All registered WebSerial devices.
    vector<shared_ptr<SerialDeviceImp>> devices;
    bool running_ = false;

    // Devices actively managed by the wmbusmeters pipeline.
    set<string> managed_devices_;

    // Listen callbacks: device → callback (fires processSerialData)
    map<SerialDevice*, function<void()>> listen_callbacks_;

    // Disappear callbacks: device → callback (fires when USB disconnected)
    map<SerialDevice*, function<void()>> disappear_callbacks_;

    // Regular timers (for detectAndConfigureWmbusDevices, regularCheckup, etc.)
    vector<WebTimer> timers_;
    int next_timer_id_ = 0;

    // SerialCommunicationManager Interface
    shared_ptr<SerialDevice> createSerialDeviceTTY(string dev, int baud_rate, PARITY parity, string purpose) override;
    shared_ptr<SerialDevice> createSerialDeviceCommand(string identifier, string command, vector<string> args,
                                                       vector<string> envs, string purpose) override;
    shared_ptr<SerialDevice> createSerialDeviceFile(string file, string purpose) override;
    shared_ptr<SerialDevice> createSerialDeviceSimulator() override;
    shared_ptr<SerialDevice> createSerialDeviceSocket(string path, string purpose) override;

    void listenTo(SerialDevice* sd, function<void()> cb) override;
    void onDisappear(SerialDevice* sd, function<void()> cb) override;

    void expectDevicesToWork() override;
    void stop() override;
    void startEventLoop() override;
    void waitForStop() override;
    bool isRunning() override;

    int startRegularCallback(string name, int seconds, function<void()> callback) override;
    void stopRegularCallback(int id) override;

    AccessCheck checkAccess(string device, shared_ptr<SerialCommunicationManager> manager,
                            string extra_info,
                            function<AccessCheck(string,shared_ptr<SerialCommunicationManager>)> extra_probe) override;

    vector<string> listSerialTTYs() override;
    shared_ptr<SerialDevice> lookup(string device) override;
    bool removeNonWorking(string device) override;

    void tickleEventLoop();

    // Called from waitForStop and Semaphore::wait — does everything:
    // drain disconnects, fire timers, check pending data
    void tickCallbacks();

    // Drain pending disconnect events from JS
    void drainDisconnects();

    // Drain pending connect events from JS (auto-reconnect)
    void drainConnects();

    // Fire any due regular timers
    void fireTimers();

    // Register a device opened by JS
    void registerWebDevice(int index);

    // Mark device as disconnected (called from drainDisconnects)
    void markDeviceDisconnected(int index);

    // Full cleanup: remove from all data structures + VFS
    void cleanupDevice(int index);

    SerialDeviceImp* findByIndex(int index);
};

// Global instance
extern SerialCommunicationManagerImp* g_web_serial_manager;

#endif
