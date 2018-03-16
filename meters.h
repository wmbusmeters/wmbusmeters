// Copyright (c) 2018 Fredrik Öhrström
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

#define LIST_OF_METERS X(MULTICAL21_METER)X(MULTICAL302_METER)X(OMNIPOWER_METER)X(UNKNOWN_METER)

enum MeterType {
#define X(name) name,
LIST_OF_METERS
#undef X
};

using namespace std;

typedef unsigned char uchar;

struct Meter {
    virtual string id() = 0;
    virtual string name() = 0;
    virtual MeterType type() = 0;
    virtual int manufacturer() = 0;
    virtual int media() = 0;
    virtual WMBus *bus() = 0;

    virtual string datetimeOfUpdateHumanReadable() = 0;
    virtual string datetimeOfUpdateRobot() = 0;

    virtual void onUpdate(function<void(Meter*)> cb) = 0;
    virtual int numUpdates() = 0;

    virtual void printMeterHumanReadable(FILE *output) = 0;
    virtual void printMeterFields(FILE *output, char separator) = 0;
    virtual void printMeterJSON(FILE *output) = 0;

    virtual bool isTelegramForMe(Telegram *t) = 0;
    virtual bool useAes() = 0;
    virtual vector<uchar> key() = 0;
};

struct WaterMeter : public virtual Meter {
    virtual float totalWaterConsumption() = 0; // m3
    virtual bool  hasTotalWaterConsumption() = 0;
    virtual float targetWaterConsumption() = 0; // m3
    virtual bool  hasTargetWaterConsumption() = 0;
    virtual float maxFlow() = 0;
    virtual bool  hasMaxFlow() = 0;

    virtual string statusHumanReadable() = 0;
    virtual string status() = 0;
    virtual string timeDry() = 0;
    virtual string timeReversed() = 0;
    virtual string timeLeaking() = 0;
    virtual string timeBursting() = 0;
};

struct HeatMeter : public virtual Meter {
    virtual float totalEnergyConsumption() = 0; // kwh
    virtual float currentPowerConsumption() = 0; // kw
    virtual float totalVolume() = 0; // m3
};

struct ElectricityMeter : public virtual Meter {
    virtual float totalEnergyConsumption() = 0; // kwh
    virtual float currentPowerConsumption() = 0; // kw
};


MeterType toMeterType(const char *type);
WaterMeter *createMultical21(WMBus *bus, const char *name, const char *id, const char *key);
HeatMeter *createMultical302(WMBus *bus, const char *name, const char *id, const char *key);
ElectricityMeter *createOmnipower(WMBus *bus, const char *name, const char *id, const char *key);

#endif
