/*
 Copyright (C) 2020-2022 Fredrik Öhrström (gpl-3.0-or-later)

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

#include"meters_common_implementation.h"

namespace
{
    struct Driver : public virtual MeterCommonImplementation
    {
        Driver(MeterInfo &mi, DriverInfo &di);

        static int const HISTORY_MONTHS = 15;

        double consumption_at_history_date_m3_[HISTORY_MONTHS];
        string history_date_[HISTORY_MONTHS];

    private:
        void processContent(Telegram *t);

        string device_date_time_;
        struct tm device_datetime_;
    };

    static bool ok = registerDriver([](DriverInfo&di)
    {
        di.setName("waterstarm");
        di.setDefaultFields("name,id,total_m3,set_date,consumption_at_set_date_m3,current_status,timestamp");
        di.setMeterType(MeterType::WaterMeter);
        di.addLinkMode(LinkMode::T1);
        di.addLinkMode(LinkMode::C1);
        di.addDetection(MANUFACTURER_DWZ, 0x06, 0x00);      // warm water
        di.addDetection(MANUFACTURER_DWZ,  0x06,  0x02);    // warm water
        di.addDetection(MANUFACTURER_DWZ,  0x07,  0x02);
        di.addDetection(MANUFACTURER_EFE,  0x07,  0x03);
        di.addDetection(MANUFACTURER_DWZ,  0x07,  0x00);    // water meter

        di.setConstructor([](MeterInfo& mi, DriverInfo& di){ return shared_ptr<Meter>(new Driver(mi, di)); });
    });

    Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(mi, di)
    {
        addStringFieldWithExtractor(
            "meter_timestamp",
            "Device date time.",
            PrintProperty::JSON,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::DateTime)
            );

        addNumericFieldWithExtractor(
            "total",
            "The total water consumption recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD | PrintProperty::IMPORTANT,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            );

        addNumericFieldWithExtractor(
            "total_backwards",
            "The total backward water volume recorded by this meter.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::AnyVolumeVIF)
            .add(VIFCombinable::BackwardFlow)
            );

        addStringFieldWithExtractorAndLookup(
            "current_status",
            "Status and error flags.",
            PrintProperty::JSON | PrintProperty::FIELD | JOIN_TPL_STATUS,
            FieldMatcher::build()
            .set(VIFRange::ErrorFlags),
            {
                {
                    {
                        "ERROR_FLAGS",
                        Translate::Type::BitToString,
                        0xffff,
                        "OK",
                        {
                            { 0x01, "SW_ERROR" },
                            { 0x02, "CRC_ERROR" },
                            { 0x04, "SENSOR_ERROR" },
                            { 0x08, "MEASUREMENT_ERROR" },
                            { 0x10, "BATTERY_VOLTAGE_ERROR" },
                            { 0x20, "MANIPULATION" },
                            { 0x40, "LEAKAGE_OR_NO_USAGE" },
                            { 0x80, "REVERSE_FLOW" },
                            { 0x100, "OVERLOAD" },
                        }
                    },
                },
            });

        addStringFieldWithExtractor(
            "set_date",
            "The most recent billing period date.",
            PrintProperty::JSON | PrintProperty::FIELD,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Date)
            .set(StorageNr(1))
            );

        addNumericFieldWithExtractor(
            "consumption_at_set_date",
            "The total water consumption at the most recent billing period date.",
            PrintProperty::JSON | PrintProperty::FIELD,
            Quantity::Volume,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Volume)
            .set(StorageNr(1))
            );


        addStringFieldWithExtractor(
            "meter_version",
            "Meter model/version.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ModelVersion)
            );

        addStringFieldWithExtractor(
            "parameter_set",
            "Parameter set.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::ParameterSet)
            );

        addNumericFieldWithExtractor(
            "battery",
            "The battery voltage.",
            PrintProperty::JSON | PrintProperty::OPTIONAL,
            Quantity::Voltage,
            VifScaling::Auto,
            FieldMatcher::build()
            .set(MeasurementType::Instantaneous)
            .set(VIFRange::Voltage)
            );

        addPrint("device_date_time", Quantity::Text,
                [&](){ return device_date_time_; },
                "Device date time.",
                PrintProperty::JSON);

        for (int i=1; i<=HISTORY_MONTHS; ++i)
        {
            string msg, info;
            strprintf(msg, "consumption_at_history_%d", i);
            strprintf(info, "The total water consumption at the history date %d.", i);
            addNumericFieldWithExtractor(
                msg,
                Quantity::Volume,
                NoDifVifKey,
                VifScaling::Auto,
                MeasurementType::Instantaneous,
                VIFRange::Volume,
                StorageNr(i+1),
                TariffNr(0),
                IndexNr(1),
                PrintProperty::JSON,
                info,
                SET_FUNC(consumption_at_history_date_m3_[i-1], Unit::M3),
                GET_FUNC(consumption_at_history_date_m3_[i-1], Unit::M3));
        }

        for (int i=1; i<=HISTORY_MONTHS; ++i)
        {
            // string key = tostrprintf("consumption_at_history_%d", i);
            // string epl = tostrprintf("The total water consumption at the history date %d.", i);

            // addPrint(key, Quantity::Volume,
            //     [this,i](Unit u){ assertQuantity(u, Quantity::Volume); return convert(consumption_at_history_date_m3_[i-1], Unit::M3, u); },
            //     epl,
            //     PrintProperty::JSON);
            string key = tostrprintf("history_%d_date", i);
            string epl = tostrprintf("The history date %d.", i);

            addPrint(key, Quantity::Text,
                    [this,i](){ return history_date_[i-1]; },
                    epl,
                    PrintProperty::JSON);
        }
    }

    void Driver::processContent(Telegram *t)
    {
        int offset;
        string key;

        if (findKey(MeasurementType::Instantaneous, VIFRange::DateTime, 0, 0, &key, &t->dv_entries)) {
            extractDVdate(&t->dv_entries, key, &offset, &device_datetime_);
            device_date_time_ = strdatetime(&device_datetime_);
            t->addMoreExplanation(offset, " device datetime (%s)", device_date_time_.c_str());
        }

        struct tm date_start_of_history = device_datetime_; // history starts with the last month before device date
        date_start_of_history.tm_mday = 1;

        // 15 months of historical data, starting in storage 2
        for (int i=0; i<HISTORY_MONTHS; ++i)
        {
            // if(findKey(MeasurementType::Instantaneous, VIFRange::Volume, i+2, 0, &key, &t->dv_entries)) {
            //     extractDVdouble(&t->dv_entries, key, &offset, &consumption_at_history_date_m3_[i]);
            //     t->addMoreExplanation(offset, " consumption at history %d (%f m3)", i+1, consumption_at_history_date_m3_[i]);
                struct tm d = date_start_of_history;
                addMonths(&d, -i);
                history_date_[i] = strdate(&d);
            // }
        }
    }    
}

// Test: Woter waterstarm 20096221 BEDB81B52C29B5C143388CBB0D15A051
// telegram=|3944FA122162092002067A3600202567C94D48D00DC47B11213E23383DB51968A705AAFA60C60E263D50CD259D7C9A03FD0C08000002FD0B0011|
// {"media":"warm water","meter":"waterstarm","name":"Woter","id":"20096221","meter_timestamp":"2020-07-30 10:40","total_m3":0.106,"total_backwards_m3":0,"current_status":"OK","set_date":null,"consumption_at_set_date_m3":null,"meter_version":"000008","parameter_set":"1100","device_date_time":"2020-07-30 10:40","consumption_at_history_1_m3":0,"history_1_date":"","consumption_at_history_2_m3":0,"history_2_date":"","consumption_at_history_3_m3":0,"history_3_date":"","consumption_at_history_4_m3":0,"history_4_date":"","consumption_at_history_5_m3":0,"history_5_date":"","consumption_at_history_6_m3":0,"history_6_date":"","consumption_at_history_7_m3":0,"history_7_date":"","consumption_at_history_8_m3":0,"history_8_date":"","consumption_at_history_9_m3":0,"history_9_date":"","consumption_at_history_10_m3":0,"history_10_date":"","consumption_at_history_11_m3":0,"history_11_date":"","consumption_at_history_12_m3":0,"history_12_date":"","consumption_at_history_13_m3":0,"history_13_date":"","consumption_at_history_14_m3":0,"history_14_date":"","consumption_at_history_15_m3":0,"history_15_date":"","timestamp":"1111-11-11T11:11:11Z"}
// |Woter;20096221;0.106;null;null;OK;1111-11-11 11:11.11

// telegram=|3944FA122162092002067A3604202567C94D48D00DC47B11213E23383DB51968A705AAFA60C60E263D50CD259D7C9A03FD0C08000002FD0B0011|
// {"media":"warm water","meter":"waterstarm","name":"Woter","id":"20096221","meter_timestamp":"2020-07-30 10:40","total_m3":0.106,"total_backwards_m3":0,"current_status":"POWER_LOW","set_date":null,"consumption_at_set_date_m3":null,"meter_version":"000008","parameter_set":"1100","device_date_time":"2020-07-30 10:40","consumption_at_history_1_m3":0,"history_1_date":"","consumption_at_history_2_m3":0,"history_2_date":"","consumption_at_history_3_m3":0,"history_3_date":"","consumption_at_history_4_m3":0,"history_4_date":"","consumption_at_history_5_m3":0,"history_5_date":"","consumption_at_history_6_m3":0,"history_6_date":"","consumption_at_history_7_m3":0,"history_7_date":"","consumption_at_history_8_m3":0,"history_8_date":"","consumption_at_history_9_m3":0,"history_9_date":"","consumption_at_history_10_m3":0,"history_10_date":"","consumption_at_history_11_m3":0,"history_11_date":"","consumption_at_history_12_m3":0,"history_12_date":"","consumption_at_history_13_m3":0,"history_13_date":"","consumption_at_history_14_m3":0,"history_14_date":"","consumption_at_history_15_m3":0,"history_15_date":"","timestamp":"1111-11-11T11:11:11Z"}
// |Woter;20096221;0.106;null;null;POWER_LOW;1111-11-11 11:11.11

// Test: Water waterstarm 22996221 NOKEY
// telegram=|3944FA122162992202067A360420252F2F_046D282A9E2704136A00000002FD17400004933C000000002F2F2F2F2F2F03FD0C08000002FD0B0011|
// {"media":"warm water","meter":"waterstarm","name":"Water","id":"22996221","meter_timestamp":"2020-07-30 10:40","total_m3":0.106,"total_backwards_m3":0,"current_status":"LEAKAGE_OR_NO_USAGE POWER_LOW","set_date":null,"consumption_at_set_date_m3":null,"meter_version":"000008","parameter_set":"1100","device_date_time":"2020-07-30 10:40","consumption_at_history_1_m3":0,"history_1_date":"","consumption_at_history_2_m3":0,"history_2_date":"","consumption_at_history_3_m3":0,"history_3_date":"","consumption_at_history_4_m3":0,"history_4_date":"","consumption_at_history_5_m3":0,"history_5_date":"","consumption_at_history_6_m3":0,"history_6_date":"","consumption_at_history_7_m3":0,"history_7_date":"","consumption_at_history_8_m3":0,"history_8_date":"","consumption_at_history_9_m3":0,"history_9_date":"","consumption_at_history_10_m3":0,"history_10_date":"","consumption_at_history_11_m3":0,"history_11_date":"","consumption_at_history_12_m3":0,"history_12_date":"","consumption_at_history_13_m3":0,"history_13_date":"","consumption_at_history_14_m3":0,"history_14_date":"","consumption_at_history_15_m3":0,"history_15_date":"","timestamp":"1111-11-11T11:11:11Z"}
// |Water;22996221;0.106;null;null;LEAKAGE_OR_NO_USAGE POWER_LOW;1111-11-11 11:11.11


// Test: Water waterstarm 11559999 NOKEY
// telegram=|2E44FA129999551100077A070020252F2F_046D0F28C22404139540000002FD17000001FD481D2F2F2F2F2F2F2F2F2F|
// {"media":"water","meter":"waterstarm","name":"Water","id":"11559999","meter_timestamp":"2022-04-02 08:15","total_m3":16.533,"total_backwards_m3":null,"current_status":"OK","set_date":null,"consumption_at_set_date_m3":null,"battery_v":2.9,"device_date_time":"2022-04-02 08:15","consumption_at_history_1_m3":0,"history_1_date":"","consumption_at_history_2_m3":0,"history_2_date":"","consumption_at_history_3_m3":0,"history_3_date":"","consumption_at_history_4_m3":0,"history_4_date":"","consumption_at_history_5_m3":0,"history_5_date":"","consumption_at_history_6_m3":0,"history_6_date":"","consumption_at_history_7_m3":0,"history_7_date":"","consumption_at_history_8_m3":0,"history_8_date":"","consumption_at_history_9_m3":0,"history_9_date":"","consumption_at_history_10_m3":0,"history_10_date":"","consumption_at_history_11_m3":0,"history_11_date":"","consumption_at_history_12_m3":0,"history_12_date":"","consumption_at_history_13_m3":0,"history_13_date":"","consumption_at_history_14_m3":0,"history_14_date":"","consumption_at_history_15_m3":0,"history_15_date":"","timestamp":"1111-11-11T11:11:11Z"}
// |Water;11559999;16.533;null;null;OK;1111-11-11 11:11.11


// Test: WarmLorenz waterstarm 20050666 NOKEY
// telegram=|9644FA126606052000067A1E000020_046D3B2ED729041340D8000002FD17000001FD481D426CBF2C4413026C000084011348D20000C40113F3CB0000840213DCC40000C40213B8B60000840313849B0000C403138B8C0000840413E3800000C4041337770000840513026C0000C40513D65F00008406134F560000C40613604700008407139D370000C407137F3300008408135B2C0000|
// {"media":"warm water","meter":"waterstarm","name":"WarmLorenz","id":"20050666","meter_timestamp":"2022-09-23 14:59","total_m3":55.36,"total_backwards_m3":null,"current_status":"OK","set_date":"2021-12-31","consumption_at_set_date_m3":27.65,"battery_v":2.9,"device_date_time":"2022-09-23 14:59","consumption_at_history_1_m3":53.832,"history_1_date":"2022-09-01","consumption_at_history_2_m3":52.211,"history_2_date":"2022-08-01","consumption_at_history_3_m3":50.396,"history_3_date":"2022-07-01","consumption_at_history_4_m3":46.776,"history_4_date":"2022-06-01","consumption_at_history_5_m3":39.812,"history_5_date":"2022-05-01","consumption_at_history_6_m3":35.979,"history_6_date":"2022-04-01","consumption_at_history_7_m3":32.995,"history_7_date":"2022-03-01","consumption_at_history_8_m3":30.519,"history_8_date":"2022-02-01","consumption_at_history_9_m3":27.65,"history_9_date":"2022-01-01","consumption_at_history_10_m3":24.534,"history_10_date":"2021-12-01","consumption_at_history_11_m3":22.095,"history_11_date":"2021-11-01","consumption_at_history_12_m3":18.272,"history_12_date":"2021-10-01","consumption_at_history_13_m3":14.237,"history_13_date":"2021-09-01","consumption_at_history_14_m3":13.183,"history_14_date":"2021-08-01","consumption_at_history_15_m3":11.355,"history_15_date":"2021-07-01","timestamp":"1111-11-11T11:11:11Z"}
// |WarmLorenz;20050666;55.36;2021-12-31;27.65;OK;1111-11-11 11:11.11


// Test: ColdLorenz waterstarm 20050665 NOKEY
// telegram=|9644FA126051062000077A78000020_046D392DD7290413901A000002FD17000001FD481D426CBF2C4413D312000084011399190000C40113841800008402130C180000C40213EC16000084031395150000C40313E3140000840413BD130000C404134C130000840513D3120000C4051322120000840613AF110000C4061397100000840713D00F0000C40713890E0000840813980C0000|
// {"media":"water","meter":"waterstarm","name":"ColdLorenz","id":"20065160","meter_timestamp":"2022-09-23 13:57","total_m3":6.8,"total_backwards_m3":null,"current_status":"OK","set_date":"2021-12-31","consumption_at_set_date_m3":4.819,"battery_v":2.9,"device_date_time":"2022-09-23 13:57","consumption_at_history_1_m3":6.553,"history_1_date":"2022-09-01","consumption_at_history_2_m3":6.276,"history_2_date":"2022-08-01","consumption_at_history_3_m3":6.156,"history_3_date":"2022-07-01","consumption_at_history_4_m3":5.868,"history_4_date":"2022-06-01","consumption_at_history_5_m3":5.525,"history_5_date":"2022-05-01","consumption_at_history_6_m3":5.347,"history_6_date":"2022-04-01","consumption_at_history_7_m3":5.053,"history_7_date":"2022-03-01","consumption_at_history_8_m3":4.94,"history_8_date":"2022-02-01","consumption_at_history_9_m3":4.819,"history_9_date":"2022-01-01","consumption_at_history_10_m3":4.642,"history_10_date":"2021-12-01","consumption_at_history_11_m3":4.527,"history_11_date":"2021-11-01","consumption_at_history_12_m3":4.247,"history_12_date":"2021-10-01","consumption_at_history_13_m3":4.048,"history_13_date":"2021-09-01","consumption_at_history_14_m3":3.721,"history_14_date":"2021-08-01","consumption_at_history_15_m3":3.224,"history_15_date":"2021-07-01","timestamp":"1111-11-11T11:11:11Z"}
// |ColdLorenz;20050665;6.8;2021-12-31;4.819;OK;1111-11-11 11:11.11