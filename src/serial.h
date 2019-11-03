/*
 Copyright (C) 2017-2019 Fredrik Öhrström

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
    virtual bool open(bool fail_if_not_ok) = 0;
    virtual void close() = 0;
    // Send will return true only if sending on a tty.
    virtual bool send(std::vector<uchar> &data) = 0;
    // Receive returns the number of bytes received.
    virtual int receive(std::vector<uchar> *data) = 0;
    virtual int fd() = 0;
    virtual bool working() = 0;

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
    virtual unique_ptr<SerialDevice> createSerialDeviceCommand(string command,
                                                               vector<string> args,
                                                               vector<string> envs,
                                                               function<void()> on_exit) = 0;
    // Read from stdin (file="stdin") or a specific file.
    virtual unique_ptr<SerialDevice> createSerialDeviceFile(string file) = 0;
    // A serial device simulator used for internal testing.
    virtual unique_ptr<SerialDevice> createSerialDeviceSimulator() = 0;

    virtual void listenTo(SerialDevice *sd, function<void()> cb) = 0;
    virtual void stop() = 0;
    virtual void waitForStop() = 0;
    virtual bool isRunning() = 0;
    virtual void setReopenAfter(int seconds) = 0;
    virtual ~SerialCommunicationManager();
};

unique_ptr<SerialCommunicationManager> createSerialCommunicationManager(time_t exit_after_seconds = 0,
                                                                        time_t reopen_after_seconds = 0);

#endif
