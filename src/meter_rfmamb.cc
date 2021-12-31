/*
 Copyright (C) 2019-2020 Fredrik Öhrström

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

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"

struct MeterRfmAmb : public virtual MeterCommonImplementation {
    MeterRfmAmb(MeterInfo &mi);

    double currentTemperature(Unit u);
    double maximumTemperature(Unit u);
    double minimumTemperature(Unit u);
    double maximumTemperatureAtSetDate1(Unit u);
    double minimumTemperatureAtSetDate1(Unit u);
    double currentRelativeHumidity();
    double maximumRelativeHumidity();
    double minimumRelativeHumidity();
    double maximumRelativeHumidityAtSetDate1();
    double minimumRelativeHumidityAtSetDate1();

private:

    void processContent(Telegram *t);

    double current_temperature_c_ {};
    double average_temperature_1h_c_ {};
    double average_temperature_24h_c_ {};
    double minimum_temperature_1h_c_ {};
    double maximum_temperature_1h_c_ {};
    double minimum_temperature_24h_c_ {};
    double maximum_temperature_24h_c_ {};

    double current_relative_humidity_rh_ {};
    double average_relative_humidity_1h_rh_ {};
    double average_relative_humidity_24h_rh_ {};
    double minimum_relative_humidity_1h_rh_ {};
    double maximum_relative_humidity_1h_rh_ {};
    double minimum_relative_humidity_24h_rh_ {};
    double maximum_relative_humidity_24h_rh_ {};

    string device_date_time_;
};

MeterRfmAmb::MeterRfmAmb(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::RFMAMB)
{
    setMeterType(MeterType::TempHygroMeter);

    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::T1);

    addPrint("current_temperature", Quantity::Temperature,
             [&](Unit u){ return currentTemperature(u); },
             "The current temperature.",
             true, true);

    addPrint("average_temperature_1h", Quantity::Temperature,
             [this](Unit u){ return convert(average_temperature_1h_c_, Unit::C, u); },
             "The average temperature for the last hour.",
             false, true);

    addPrint("average_temperature_24h", Quantity::Temperature,
             [this](Unit u){ return convert(average_temperature_24h_c_, Unit::C, u); },
             "The average temperature for the last 24 hours",
             false, true);

    addPrint("maximum_temperature_1h", Quantity::Temperature,
             [&](Unit u){ return maximumTemperature(u); },
             "The maximum temperature.",
             false, true);

    addPrint("minimum_temperature_1h", Quantity::Temperature,
             [&](Unit u){ return minimumTemperature(u); },
             "The minimum temperature.",
             false, true);

    addPrint("maximum_temperature_24h", Quantity::Temperature,
             [&](Unit u){ return maximumTemperatureAtSetDate1(u); },
             "The maximum temperature at set date 1.",
             false, true);

    addPrint("minimum_temperature_24h", Quantity::Temperature,
             [&](Unit u){ return minimumTemperatureAtSetDate1(u); },
             "The minimum temperature at set date 1.",
             false, true);

    addPrint("current_relative_humidity", Quantity::RelativeHumidity,
             [&](Unit u){ return currentRelativeHumidity(); },
             "The current relative humidity.",
             true, true);

    addPrint("average_relative_humidity_1h", Quantity::RelativeHumidity,
             [this](Unit u){ return convert(average_relative_humidity_1h_rh_, Unit::RH, u); },
             "The averate relative humidity for the last hours.",
             false, true);

    addPrint("average_relative_humidity_24h", Quantity::RelativeHumidity,
             [this](Unit u){ return convert(average_relative_humidity_24h_rh_, Unit::RH, u); },
             "The average relative humidity for the last 24 hours.",
             false, true);

    addPrint("minimum_relative_humidity_1h", Quantity::RelativeHumidity,
             [&](Unit u){ return minimumRelativeHumidity(); },
             "The minimum relative humidity.",
             false, true);

    addPrint("maximum_relative_humidity_1h", Quantity::RelativeHumidity,
             [&](Unit u){ return maximumRelativeHumidity(); },
             "The maximum relative humidity.",
             false, true);

    addPrint("maximum_relative_humidity_24h", Quantity::RelativeHumidity,
             [&](Unit u){ return maximumRelativeHumidityAtSetDate1(); },
             "The maximum relative humidity at set date 1.",
             false, true);

    addPrint("minimum_relative_humidity_24h", Quantity::RelativeHumidity,
             [&](Unit u){ return minimumRelativeHumidityAtSetDate1(); },
             "The minimum relative humidity at set date 1.",
             false, true);

    addPrint("device_date_time", Quantity::Text,
             [&](){ return device_date_time_; },
             "Device date time.",
             false, true);
}

shared_ptr<Meter> createRfmAmb(MeterInfo &mi)
{
    return shared_ptr<Meter>(new MeterRfmAmb(mi));
}

double MeterRfmAmb::currentTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(current_temperature_c_, Unit::C, u);
}

double MeterRfmAmb::maximumTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(maximum_temperature_1h_c_, Unit::C, u);
}

double MeterRfmAmb::minimumTemperature(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(minimum_temperature_1h_c_, Unit::C, u);
}

double MeterRfmAmb::maximumTemperatureAtSetDate1(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(maximum_temperature_24h_c_, Unit::C, u);
}

double MeterRfmAmb::minimumTemperatureAtSetDate1(Unit u)
{
    assertQuantity(u, Quantity::Temperature);
    return convert(minimum_temperature_24h_c_, Unit::C, u);
}

double MeterRfmAmb::currentRelativeHumidity()
{
    return current_relative_humidity_rh_;
}

double MeterRfmAmb::maximumRelativeHumidity()
{
    return maximum_relative_humidity_1h_rh_;
}

double MeterRfmAmb::minimumRelativeHumidity()
{
    return minimum_relative_humidity_1h_rh_;
}

double MeterRfmAmb::maximumRelativeHumidityAtSetDate1()
{
    return maximum_relative_humidity_24h_rh_;
}

double MeterRfmAmb::minimumRelativeHumidityAtSetDate1()
{
    return minimum_relative_humidity_24h_rh_;
}

void MeterRfmAmb::processContent(Telegram *t)
{
    /*
      (rfmamb) 0f: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (rfmamb) 10: 65 vif (External temperature 10⁻² °C)
      (rfmamb) 11: * A008 current temperature (22.080000 C)
      (rfmamb) 13: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (rfmamb) 14: 65 vif (External temperature 10⁻² °C)
      (rfmamb) 15: * 8F08 temperature at set date 1 (21.910000 c)
      (rfmamb) 17: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (rfmamb) 18: 01 dife (subunit=0 tariff=0 storagenr=2)
      (rfmamb) 19: 65 vif (External temperature 10⁻² °C)
      (rfmamb) 1a: * 9F08 temperature at set date 2 (22.070000 c)
      (rfmamb) 1c: 22 dif (16 Bit Integer/Binary Minimum value)
      (rfmamb) 1d: 65 vif (External temperature 10⁻² °C)
      (rfmamb) 1e: * 8908 minimum temperature (21.850000 C)
      (rfmamb) 20: 12 dif (16 Bit Integer/Binary Maximum value)
      (rfmamb) 21: 65 vif (External temperature 10⁻² °C)
      (rfmamb) 22: * A008 maximum temperature (22.080000 C)
      (rfmamb) 24: 62 dif (16 Bit Integer/Binary Minimum value storagenr=1)
      (rfmamb) 25: 65 vif (External temperature 10⁻² °C)
      (rfmamb) 26: * 5108 minimum temperature at set date 1 (21.290000 C)
      (rfmamb) 28: 52 dif (16 Bit Integer/Binary Maximum value storagenr=1)
      (rfmamb) 29: 65 vif (External temperature 10⁻² °C)
      (rfmamb) 2a: * 2B09 maximum temperature at set date 1 (23.470000 C)
      (rfmamb) 2c: 02 dif (16 Bit Integer/Binary Instantaneous value)
      (rfmamb) 2d: FB vif (First extension of VIF-codes)
      (rfmamb) 2e: 1A vife (?)
      (rfmamb) 2f: * BA01 current relative humidity (44.200000 RH)
      (rfmamb) 31: 42 dif (16 Bit Integer/Binary Instantaneous value storagenr=1)
      (rfmamb) 32: FB vif (First extension of VIF-codes)
      (rfmamb) 33: 1A vife (?)
      (rfmamb) 34: * B001 relative humidity at set date 1 (43.200000 RH)
      (rfmamb) 36: 82 dif (16 Bit Integer/Binary Instantaneous value)
      (rfmamb) 37: 01 dife (subunit=0 tariff=0 storagenr=2)
      (rfmamb) 38: FB vif (First extension of VIF-codes)
      (rfmamb) 39: 1A vife (?)
      (rfmamb) 3a: * BD01 relative humidity at set date 2 (44.500000 RH)
      (rfmamb) 3c: 22 dif (16 Bit Integer/Binary Minimum value)
      (rfmamb) 3d: FB vif (First extension of VIF-codes)
      (rfmamb) 3e: 1A vife (?)
      (rfmamb) 3f: * A901 minimum relative humidity (42.500000 RH)
      (rfmamb) 41: 12 dif (16 Bit Integer/Binary Maximum value)
      (rfmamb) 42: FB vif (First extension of VIF-codes)
      (rfmamb) 43: 1A vife (?)
      (rfmamb) 44: * BA01 maximum relative humidity (44.200000 RH)
      (rfmamb) 46: 62 dif (16 Bit Integer/Binary Minimum value storagenr=1)
      (rfmamb) 47: FB vif (First extension of VIF-codes)
      (rfmamb) 48: 1A vife (?)
      (rfmamb) 49: * A601 minimum relative humidity at set date 1 (42.200000 RH)
      (rfmamb) 4b: 52 dif (16 Bit Integer/Binary Maximum value storagenr=1)
      (rfmamb) 4c: FB vif (First extension of VIF-codes)
      (rfmamb) 4d: 1A vife (?)
      (rfmamb) 4e: * F501 maximum relative humidity at set date 1 (50.100000 RH)
      (rfmamb) 50: 06 dif (48 Bit Integer/Binary Instantaneous value)
      (rfmamb) 51: 6D vif (Date and time type)
      (rfmamb) 52: * 3B3BB36B2A00 device datetime (2019-10-11 19:59)
    */
    int offset;
    string key;

    if (findKey(MeasurementType::Instantaneous, ValueInformation::ExternalTemperature, 0, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &current_temperature_c_);
        t->addMoreExplanation(offset, " current temperature (%f C)", current_temperature_c_);
    }

    if (findKey(MeasurementType::Maximum, ValueInformation::ExternalTemperature, 0, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &maximum_temperature_1h_c_);
        t->addMoreExplanation(offset, " maximum temperature 1h (%f C)", maximum_temperature_1h_c_);
    }

    if (findKey(MeasurementType::Minimum, ValueInformation::ExternalTemperature, 0, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &minimum_temperature_1h_c_);
        t->addMoreExplanation(offset, " minimum temperature 1h (%f C)", minimum_temperature_1h_c_);
    }

    if (findKey(MeasurementType::Maximum, ValueInformation::ExternalTemperature, 1, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &maximum_temperature_24h_c_);
        t->addMoreExplanation(offset, " maximum temperature 24h (%f C)",
                              maximum_temperature_24h_c_);
    }

    if (findKey(MeasurementType::Minimum, ValueInformation::ExternalTemperature, 1, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &minimum_temperature_24h_c_);
        t->addMoreExplanation(offset, " minimum temperature 24h (%f C)",
                              minimum_temperature_24h_c_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::ExternalTemperature, 1, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &average_temperature_1h_c_);
        t->addMoreExplanation(offset, " average temperature 1h (%f C)", average_temperature_1h_c_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::ExternalTemperature, 2, 0, &key, &t->values))
    {
        extractDVdouble(&t->values, key, &offset, &average_temperature_24h_c_);
        t->addMoreExplanation(offset, " average temperature 24h (%f C)", average_temperature_24h_c_);
    }

    // Temporarily silly solution until the dvparser is upgraded with support for extension

    key = "02FB1A"; // 02=current 16bit, 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        current_relative_humidity_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " current relative humidity (%f RH)", current_relative_humidity_rh_);
    }
    key = "22FB1A"; // 22=minimum 16bit, 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        minimum_relative_humidity_1h_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " minimum relative humidity 1h (%f RH)", minimum_relative_humidity_1h_rh_);
    }
    key = "12FB1A"; // 12=maximum 16bit, 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        maximum_relative_humidity_1h_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " maximum relative humidity 1h (%f RH)", maximum_relative_humidity_1h_rh_);
    }
    key = "42FB1A"; // 42=instant 1storage 16bit, 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        average_relative_humidity_1h_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " average relative humidity 1h (%f RH)", average_relative_humidity_1h_rh_);
    }
    key = "62FB1A"; // 62=minimum 1storage 16bit, 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        minimum_relative_humidity_1h_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " minimum relative humidity 1h (%f RH)", minimum_relative_humidity_1h_rh_);
    }
    key = "52FB1A"; // 52=maximum 1storage 16bit, 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        maximum_relative_humidity_1h_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " maximum relative humidity 1h (%f RH)", maximum_relative_humidity_1h_rh_);
    }
    key = "8201FB1A"; // 8201=instant 2storage 16bit, 1A = 0001 1010 = First extension vif code Relative Humidity 10^-1
    if (hasKey(&t->values, key))
    {
        double tmp;
        extractDVdouble(&t->values, key, &offset, &tmp, false);
        average_relative_humidity_24h_rh_ = tmp / (double)10.0;
        t->addMoreExplanation(offset, " relative humidity 24h (%f RH)", average_relative_humidity_24h_rh_);
    }

    if (findKey(MeasurementType::Unknown, ValueInformation::DateTime, 0, 0, &key, &t->values)) {
        struct tm datetime;
        extractDVdate(&t->values, key, &offset, &datetime);
        device_date_time_ = strdatetime(&datetime);
        t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
    }

}
