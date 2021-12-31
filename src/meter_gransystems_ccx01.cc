/*
 Copyright (C) 2018-2021 Fredrik Öhrström

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

/*
Implemented January 2021 Xael South:
Implements GSS, CC101 and CC301 energy meter.
This T1 WM-Bus meter broadcasts:
- accumulated energy consumption
- accumulated energy consumption till yesterday
- current date
- actually measured voltage
- actually measured current
- actually measured frequency
- meter status and errors

The single-phase and three-phase send apparently the same datagram:
three-phase meter sends voltage and current values for every phase
L1 .. L3.

Meter version. Implementation tested against meters:
- CC101 one-phase with firmware version 0x01.
- CC301 three-phase with firmware version 0x01.

Encryption: None.
*/

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"util.h"

struct MeterGransystemsCCx01: public virtual ElectricityMeter, public virtual MeterCommonImplementation {
    MeterGransystemsCCx01(MeterInfo &mi);

    double totalEnergyConsumption(Unit u);

    string status();

private:

    void processContent(Telegram *t);

    static const std::size_t MAX_TARIFFS = std::size_t(4);

    double current_total_energy_kwh_{};
    std::size_t current_tariff_energy_kwh_idx_{};
    double current_tariff_energy_kwh_[MAX_TARIFFS] {};

    double last_day_total_energy_kwh_{};
    std::size_t last_day_tariff_energy_kwh_idx_{};
    double last_day_tariff_energy_kwh_[MAX_TARIFFS] {};

    double voltage_L_[3]{0, 0, 0};
    double current_L_[3]{0, 0, 0};
    double frequency_{0};

    bool single_phase_{};
    bool three_phase_{};

    uint32_t status_{};
};

shared_ptr<Meter> createCCx01(MeterInfo &mi)
{
    return shared_ptr<ElectricityMeter>(new MeterGransystemsCCx01(mi));
}

MeterGransystemsCCx01::MeterGransystemsCCx01(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterDriver::CCx01)
{
    addLinkMode(LinkMode::T1);

    addPrint("total_energy_consumption", Quantity::Energy,
             [&](Unit u){ return totalEnergyConsumption(u); },
             "The total energy consumption recorded by this meter.",
             true, true);

    addPrint("voltage_at_phase_1", Quantity::Voltage,
             [&](Unit u){ return convert(voltage_L_[0], Unit::Volt, u); },
             "Voltage at phase L1.",
             true, true);

    addPrint("voltage_at_phase_2", Quantity::Voltage,
             [&](Unit u){ return convert(voltage_L_[1], Unit::Volt, u); },
             "Voltage at phase L2.",
             true, true);

    addPrint("voltage_at_phase_3", Quantity::Voltage,
             [&](Unit u){ return convert(voltage_L_[2], Unit::Volt, u); },
             "Voltage at phase L3.",
             true, true);

    addPrint("currrent_at_phase_1", Quantity::Current,
             [&](Unit u){ return convert(current_L_[0], Unit::Ampere, u); },
             "Current at phase L1.",
             true, true);

    addPrint("currrent_at_phase_2", Quantity::Current,
             [&](Unit u){ return convert(current_L_[1], Unit::Ampere, u); },
             "Current at phase L2.",
             true, true);

    addPrint("currrent_at_phase_3", Quantity::Current,
             [&](Unit u){ return convert(current_L_[2], Unit::Ampere, u); },
             "Current at phase L3.",
             true, true);

    addPrint("frequency", Quantity::Frequency,
             [&](Unit u){ return convert(frequency_, Unit::Hz, u); },
             "Frequency.",
             true, true);

    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "The meter status.",
             true, true);
}

double MeterGransystemsCCx01::totalEnergyConsumption(Unit u)
{
    assertQuantity(u, Quantity::Energy);
    return convert(current_total_energy_kwh_, Unit::KWH, u);
}

void MeterGransystemsCCx01::processContent(Telegram *t)
{
    int offset;
    string key;

    // Single-phase or three-phase meter?
    key = "04FD17";
    if (hasKey(&t->values, key))
    {
        if (extractDVuint32(&t->values, key, &offset, &status_))
        {
            if ((status_ & 0xFFFF0000u) == 0x01020000u)
            {
                single_phase_ = true;
            }
            else if ((status_ & 0xFFFF0000u) == 0x01010000u)
            {
                three_phase_ = true;
            }
            else
            {
                error("Internal error! Can't determine phase number.\n");
                return;
            }
        }
        else
        {
            error("Internal error! Can't detect meter type.\n");
            return;
        }
    }

    if (extractDVdouble(&t->values, "0403", &offset, &current_total_energy_kwh_))
    {
        t->addMoreExplanation(offset, " total energy (%f kwh)", current_total_energy_kwh_);
    }

    for ( current_tariff_energy_kwh_idx_ = 0;
          current_tariff_energy_kwh_idx_  < MAX_TARIFFS;
          current_tariff_energy_kwh_idx_++ )
    {
        static const std::string pattern[MAX_TARIFFS] = {"841003", "842003",  "843003", "84801003"};
        if (extractDVdouble(&t->values, pattern[current_tariff_energy_kwh_idx_],
                                &offset, &current_tariff_energy_kwh_[current_tariff_energy_kwh_idx_]))
        {
            t->addMoreExplanation(offset, " tariff %zu energy (%f kwh)",
                                    current_tariff_energy_kwh_idx_ + 1, current_tariff_energy_kwh_[current_tariff_energy_kwh_idx_]);
        }
    }

    if (extractDVdouble(&t->values, "840103", &offset, &last_day_total_energy_kwh_))
    {
        t->addMoreExplanation(offset, " last day total energy (%f kwh)", last_day_total_energy_kwh_);
    }

    for ( last_day_tariff_energy_kwh_idx_ = 0;
          last_day_tariff_energy_kwh_idx_  < MAX_TARIFFS;
          last_day_tariff_energy_kwh_idx_++ )
    {
        static const std::string pattern[MAX_TARIFFS] = {"841103",  "842103", "843103", "84811003"};
        if (extractDVdouble(&t->values, pattern[last_day_tariff_energy_kwh_idx_],
                                &offset, &last_day_tariff_energy_kwh_[last_day_tariff_energy_kwh_idx_]))
        {
            t->addMoreExplanation(offset, " tariff %zu last day energy (%f kwh)",
                                    last_day_tariff_energy_kwh_idx_ + 1, last_day_tariff_energy_kwh_[last_day_tariff_energy_kwh_idx_]);
        }
    }

    voltage_L_[0] = voltage_L_[1] = voltage_L_[2] = 0;
    current_L_[0]  = current_L_[1]  = current_L_[2] = 0;

    if (single_phase_)
    {
        if (extractDVdouble(&t->values, "04FD48", &offset, &voltage_L_[0], false))
        {
            voltage_L_[0] /= 10.;
            t->addMoreExplanation(offset, " voltage (%f volts)", voltage_L_[0]);
        }

        if (extractDVdouble(&t->values, "04FD5B", &offset, &current_L_[0], false))
        {
            current_L_[0] /= 10.;
            t->addMoreExplanation(offset, " current (%f ampere)", current_L_[0]);
        }
    }
    else if (three_phase_)
    {
        for (std::size_t i = 0; i < 3; i++)
        {
            static const std::string pattern[] = {"8440FD48", "848040FD48", "84C040FD48"};
            if (extractDVdouble(&t->values, pattern[i], &offset, &voltage_L_[i], false))
            {
                voltage_L_[i] /= 10.;
                t->addMoreExplanation(offset, " voltage L%d (%f volts)", i + 1, voltage_L_[i]);
            }
        }

        for (std::size_t i = 0; i < 3; i++)
        {
            static const std::string pattern[] = {"8440FD5B", "848040FD5B", "84C040FD5B"};
            if (extractDVdouble(&t->values, pattern[i], &offset, &current_L_[i], false))
            {
                current_L_[i] /= 10.;
                t->addMoreExplanation(offset, " current L%d (%f ampere)", i + 1, current_L_[i]);
            }
        }
    }

    if (extractDVdouble(&t->values, "02FB2D", &offset, &frequency_, false))
    {
        frequency_ /= 100.;
        t->addMoreExplanation(offset, " frequency (%f hz)", frequency_);
    }
}

string MeterGransystemsCCx01::status()
{
    const uint16_t error_codes = status_ & 0xFFFFu;
    string s;

    if (error_codes == 0)
    {
        s.append("OK");
    }
    if (error_codes & 0x0001u)
    {
        s.append("METER HARDWARE ERROR");
    }

    if (error_codes & 0x0002u)
    {
        s.append("RTC ERROR");
    }

    if (error_codes & 0x0100u)
    {
        s.append("DSP COMMUNICATION ERROR");
    }
    if (error_codes & 0x0200u)
    {
        s.append("DSP HARDWARE ERROR");
    }

    if (error_codes & 0x4000u)
    {
        s.append("RAM ERROR");
    }

    if (error_codes & 0x8000u)
    {
        s.append("ROM ERROR");
    }

    if (single_phase_)
    {
        if (error_codes & 0x0008u)
        {
            s.append("DEVICE NOT CONFIGURED");
        }
        if (error_codes & 0x0010u)
        {
            s.append("INTERNAL ERROR");
        }
        if (error_codes & 0x0020u)
        {
            s.append("BATTERIE LOW");
        }
        if (error_codes & 0x0040u)
        {
            s.append("MAGNETIC FRAUD PRESENT");
        }
        if (error_codes & 0x0080u)
        {
            s.append("MAGNETIC FRAUD PAST");
        }
        if (error_codes & 0x0400u)
        {
            s.append("CALIBRATION EEPROM ERROR");
        }
        if (error_codes & 0x0800u)
        {
            s.append("EEPROM1 ERROR");
        }
    }
    else if (three_phase_)
    {
        if (error_codes & 0x0008u)
        {
            s.append("CALIBRATION EEPROM ERROR");
        }
        if (error_codes & 0x0010u)
        {
            s.append("NETWORK INTERFERENCE");
        }
        if (error_codes & 0x0800u)
        {
            s.append("CALIBRATION EEPROM ERROR");
        }
        if (error_codes & 0x1000u)
        {
            s.append("EEPROM1 ERROR");
        }
    }

    return s;
}
