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

#ifndef METER_H_
#define METER_H_

#include"util.h"
#include"wmbus.h"

#include<string>
#include<vector>

using namespace std;

typedef unsigned char uchar;

struct Meter {
    virtual string id() = 0;
    virtual string name() = 0;
    virtual float totalWaterConsumption() = 0;
    virtual string datetimeOfUpdate() = 0;

    virtual void onUpdate(function<void()> cb) = 0;
};

Meter *createMultical21(WMBus *bus, const char *name, const char *id, const char *key);

#endif
