/*
 Copyright (C) 2017-2020 Fredrik Öhrström

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

#ifndef SERIAL_H_
#define SERIAL_H_

#include"util.h"

#include<functional>
#include<memory>
#include<string>
#include<vector>

using namespace std;

struct SerialCommunicationManager;

/**
  A SerialDevice can be connected to a tty with a baudrate.
  But can also be connected to stdin, a file, or the output from a subshell.
  If you try to do send bytes to such a non-tty, then send will return false.
*/
struct SerialDevice
{
    // If fail_if_not_ok then forcefully exit the program if cannot be opened.
    virtual AccessCheck open(bool fail_if_not_ok) = 0;
    virtual void close() = 0;
    // Send will return true only if sending on a tty.
    virtual bool send(std::vector<uchar> &data) = 0;
    // Receive returns the number of bytes received.
    virtual int receive(std::vector<uchar> *data) = 0;
    virtual int fd() = 0;
    virtual bool working() = 0;
    // Used when connecting stdin to a tty driver for testing.
    virtual bool readonly() = 0;
    // Mark this device so that it is ignored by the select/callback event loop.
    virtual void doNotUseCallbacks() = 0;
    virtual bool skippingCallbacks() = 0;

    // Return underlying device as string.
    virtual std::string device() = 0;

    virtual void checkIfShouldReopen() = 0;
    virtual void fill(std::vector<uchar> &data) = 0; // Fill buffer with raw data.
    virtual SerialCommunicationManager *manager() = 0;
    virtual ~SerialDevice() = default;
};

struct SerialCommunicationManager
{
    // Read from a /dev/ttyUSB0 or /dev/ttyACM0 device with baud settings.
    virtual unique_ptr<SerialDevice> createSerialDeviceTTY(string dev, int baud_rate) = 0;
    // Read from a sub shell.
    virtual unique_ptr<SerialDevice> createSerialDeviceCommand(string device,
                                                               string command,
                                                               vector<string> args,
                                                               vector<string> envs,
                                                               function<void()> on_exit) = 0;
    // Read from stdin (file="stdin") or a specific file.
    virtual unique_ptr<SerialDevice> createSerialDeviceFile(string file) = 0;
    // A serial device simulator used for internal testing.
    virtual unique_ptr<SerialDevice> createSerialDeviceSimulator() = 0;

    // Invoke cb callback when data arrives on the serial device.
    virtual void listenTo(SerialDevice *sd, function<void()> cb) = 0;
    // Invoke cb callback when the serial device has disappeared!
    virtual void onDisappear(SerialDevice *sd, function<void()> cb) = 0;
    // Normally the communication mananager runs for ever.
    // But if you expect configured devices to work, then
    // the manager will exit when there are no working devices.
    virtual void expectDevicesToWork() = 0;
    virtual void stop() = 0;
    virtual void startEventLoop() = 0;
    virtual void waitForStop() = 0;
    virtual bool isRunning() = 0;
    virtual void setReopenAfter(int seconds) = 0;
    // Register a new timer that regularly, every seconds, invokes the callback.
    // Returns an id for the timer.
    virtual int startRegularCallback(std::string name, int seconds, function<void()> callback) = 0;
    virtual void stopRegularCallback(int id) = 0;

    virtual void resetInitiated() = 0;
    virtual void resetCompleted() = 0;

    // List all real serial devices (avoid pseudo ttys)
    virtual std::vector<std::string> listSerialDevices() = 0;
    // List all all rtlsdr swradio devices
    virtual std::vector<std::string> listSWRadioDevices() = 0;
    // Return a serial device for the given device, if it exists! Otherwise NULL.
    virtual SerialDevice *lookup(std::string device) = 0;
    virtual ~SerialCommunicationManager();
};

unique_ptr<SerialCommunicationManager> createSerialCommunicationManager(time_t exit_after_seconds = 0,
                                                                        time_t reopen_after_seconds = 0,
                                                                        bool start_event_loop = true);

#endif
