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

struct SerialDevice
{
    virtual bool open(bool fail_if_not_ok) = 0;
    virtual void close() = 0;
    virtual void checkIfShouldReopen() = 0;
    virtual bool send(std::vector<uchar> &data) = 0;
    virtual int receive(std::vector<uchar> *data) = 0;
    virtual int fd() = 0;
    virtual bool working() = 0;
    virtual SerialCommunicationManager *manager() = 0;
    virtual ~SerialDevice() = default;
};

struct SerialCommunicationManager
{
    virtual unique_ptr<SerialDevice> createSerialDeviceTTY(string dev, int baud_rate) = 0;
    virtual unique_ptr<SerialDevice> createSerialDeviceCommand(string command, vector<string> args,
                                                               vector<string> envs,
                                                               function<void()> on_exit) = 0;
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
