// Copyright (c) 2017 Fredrik Öhrström
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef SERIAL_H_
#define SERIAL_H_

#include"util.h"

#include<functional>
#include<string>
#include<vector>

using namespace std;

struct SerialCommunicationManager;

struct SerialDevice {
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool send(vector<uchar> &data) = 0;
    virtual int receive(vector<uchar> *data) = 0;
    virtual int fd() = 0;
    virtual SerialCommunicationManager *manager() = 0;
};

struct SerialCommunicationManager {
    virtual SerialDevice *createSerialDeviceTTY(string dev, int baud_rate) = 0;
    virtual void listenTo(SerialDevice *sd, function<void()> cb) = 0;
    virtual void stop() = 0;
    virtual void waitForStop() = 0;
    virtual bool isRunning() = 0;
};

SerialCommunicationManager *createSerialCommunicationManager();

#endif
