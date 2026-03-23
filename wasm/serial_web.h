#pragma once
#ifdef __EMSCRIPTEN__

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include "serial.h"

using namespace std;

struct SerialDeviceImp : public SerialDevice
{
    int index;
    vector<uint8_t> rxBuffer;
    bool isOpen = false;
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

struct SerialCommunicationManagerImp : public SerialCommunicationManager
{
    vector<shared_ptr<SerialDeviceImp>> devices;
    bool running_ = false;

    // Listen callbacks: device → callback
    map<SerialDevice*, function<void()>> listen_callbacks_;

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
    void waitForStop() override;  // Event loop with emscripten_sleep
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

    // Tick listen callbacks once — called from Semaphore::wait() polling
    // so that processSerialData() can run during command/response exchanges.
    void tickCallbacks();

    // Register a device opened by JS
    void registerWebDevice(int index);
    SerialDeviceImp* findByIndex(int index);
};

// Global instance — shared between bindings.cc and the pipeline
extern SerialCommunicationManagerImp* g_web_serial_manager;

#endif