/*
 Copyright (C) 2019-2023 Fredrik Öhrström (gpl-3.0-or-later)

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

namespace {
	struct Driver: public virtual MeterCommonImplementation {
			Driver(MeterInfo &mi, DriverInfo &di);
			void processContent(Telegram *t);
	};

	static bool ok = registerDriver([](DriverInfo &di) {
		di.setName("hydrodigit");
		di.setDefaultFields("name,id,total_m3,meter_datetime,timestamp");
		di.setMeterType(MeterType::WaterMeter);
		di.addLinkMode(LinkMode::T1);
		di.addDetection(MANUFACTURER_BMT, 0x06, 0x13);
		di.addDetection(MANUFACTURER_BMT, 0x06, 0x17);
		di.addDetection(MANUFACTURER_BMT, 0x07, 0x13);
		di.addDetection(MANUFACTURER_BMT, 0x07, 0x15);
		di.usesProcessContent();
		di.setConstructor([](MeterInfo &mi, DriverInfo &di) {
			return shared_ptr<Meter>(new Driver(mi, di));
		});
	});

	Driver::Driver(MeterInfo &mi, DriverInfo &di) : MeterCommonImplementation(
			mi, di) {
		addNumericFieldWithExtractor("total",
				"The total water consumption recorded by this meter.",
				DEFAULT_PRINT_PROPERTIES, Quantity::Volume, VifScaling::Auto,
				DifSignedness::Signed,
				FieldMatcher::build().set(MeasurementType::Instantaneous).set(
						VIFRange::Volume));

		addNumericFieldWithExtractor("meter",
				"Meter timestamp for measurement.",
				DEFAULT_PRINT_PROPERTIES, Quantity::PointInTime,
				VifScaling::Auto, DifSignedness::Signed,
				FieldMatcher::build().set(MeasurementType::Instantaneous).set(
						VIFRange::DateTime), Unit::DateTimeLT);
		addStringField("contents", "Contents of this telegrams",
				DEFAULT_PRINT_PROPERTIES);
		addNumericField("voltage", Quantity::Voltage, DEFAULT_PRINT_PROPERTIES,
				"Voltage of the battery inside the meter", Unit::Volt);
		addStringField("leak_date", "Date of leakage detected by meter",
				DEFAULT_PRINT_PROPERTIES);
		addNumericField("backflow", Quantity::Volume, DEFAULT_PRINT_PROPERTIES,
				"Backflow detected by the meter", Unit::M3);
		addNumericField("January_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of January",
				Unit::M3);
		addNumericField("February_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of February",
				Unit::M3);
		addNumericField("March_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of March",
				Unit::M3);
		addNumericField("April_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of April",
				Unit::M3);
		addNumericField("May_total", Quantity::Volume, DEFAULT_PRINT_PROPERTIES,
				"Total value at the end of May", Unit::M3);
		addNumericField("June_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of June",
				Unit::M3);
		addNumericField("July_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of July",
				Unit::M3);
		addNumericField("August_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of August",
				Unit::M3);
		addNumericField("September_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of September",
				Unit::M3);
		addNumericField("October_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of October",
				Unit::M3);
		addNumericField("November_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of November",
				Unit::M3);
		addNumericField("December_total", Quantity::Volume,
				DEFAULT_PRINT_PROPERTIES, "Total value at the end of December",
				Unit::M3);

	}

	double fromMonthly(uchar first_byte, uchar second_byte, uchar third_byte) {
		return ((double) (third_byte << 16 | second_byte << 8 | first_byte))
				/ 100.0;
	}

	double fromBackflow(uchar first_byte, uchar second_byte, uchar third_byte,
			uchar fourth_byte) {
		return ((double) (fourth_byte << 24 | third_byte << 16
				| second_byte << 8 | first_byte)) / 1000.0;
	}

	string getMonth(int month) {
		switch (month) {
			case 1:
				return "January";
			case 2:
				return "February";
			case 3:
				return "March";
			case 4:
				return "April";
			case 5:
				return "May";
			case 6:
				return "June";
			case 7:
				return "July";
			case 8:
				return "August";
			case 9:
				return "September";
			case 10:
				return "October";
			case 11:
				return "November";
			case 12:
				return "December";
			default:
				return "Month out of range :)";
		}
	}

	double getVoltage(uchar voltage_byte) {
		uchar relevant_half = voltage_byte & 0x0F; // ensure dumping of top half
		switch (relevant_half) {
			case 0x01:
				return 1.9;
			case 0x02:
				return 2.1;
			case 0x03:
				return 2.2;
			case 0x04:
				return 2.3;
			case 0x05:
				return 2.4;
			case 0x06:
				return 2.5;
			case 0x07:
				return 2.65;
			case 0x08:
				return 2.8;
			case 0x09:
				return 2.9;
			case 0x0A:
				return 3.05;
			case 0x0B:
				return 3.2;
			case 0x0C:
				return 3.35;
			case 0x0D:
				return 3.5;
			default:
				return 3.7; // 0, E and F all are 3.7
		}

	}

	void Driver::processContent(Telegram *t) {
		if (t->mfct_0f_index == -1) return; // Check that there is mfct data.
		int offset = t->header_size + t->mfct_0f_index;

		vector<uchar> bytes;
		t->extractMfctData(&bytes); // Extract raw frame data after the DIF 0x0F.

		debugPayload("(hydrodigit mfct)", bytes);

		int i = 0;
		int len = bytes.size();

		if (i >= len) return;
		uchar frame_identifier = bytes[i];
		if (frame_identifier == 0x15) {
			t->addSpecialExplanation(i + offset, 1, KindOfData::PROTOCOL,
					Understanding::FULL, "*** %02X frame content: %s",
					frame_identifier, "Backflow, alarms and monthly data");
			setStringValue("contents", "Backflow, alarms and monthly data");
		} else if (frame_identifier == 0x95) {
			t->addSpecialExplanation(i + offset, 1, KindOfData::PROTOCOL,
					Understanding::FULL, "*** %02X frame content: %s",
					frame_identifier,
					"Backflow, leak date, alarms and monthly data");
			setStringValue("contents",
					"Backflow, leak date, alarms and monthly data");
		} else {
			t->addSpecialExplanation(i + offset, 1, KindOfData::PROTOCOL,
					Understanding::NONE,
					"*** %02X frame content unknown, please open issue with this telegram for driver improvement",
					frame_identifier);
			setStringValue("contents",
					"unknown, please open issue with this telegram for driver improvement");
			return;
		}
		i++;


		if (i >= len) return;
		// only the bottom half changes the voltage, top half's purpose is unknown
		// values obtained from software by changing value from 0x00-0F
		// changing the top half didn't seem to change anything, but it might require a combination with other bytes
		// my meters have 0x0A and 0x2A but the data seems same
		uchar somethingAndVoltage = bytes[i];
		double voltage = getVoltage(somethingAndVoltage & 0x0F);
		t->addSpecialExplanation(i + offset, 1, KindOfData::CONTENT,
				Understanding::FULL,
				"*** %02X voltage of battery %0.2f V",
				somethingAndVoltage, voltage);
		setNumericValue("voltage", Unit::Volt, voltage);
		i++;


		if (frame_identifier == 0x95) {
			if (i + 2 >= len) return;
			uchar leak_year = bytes[i];
			uchar leak_month = bytes[i + 1];
			uchar leak_day = bytes[i + 2];
			// its BCD so I just print it as is
			t->addSpecialExplanation(i + offset, 3, KindOfData::CONTENT,
					Understanding::FULL,
					"*** %02X%02X%02X date of leakage: %02X.%02X.20%02X",
					bytes[i],
					bytes[i + 1], bytes[i + 2], leak_day, leak_month,
					leak_year);
			char buffer[16];
			sprintf(buffer, "%02X.%02X.20%02X", leak_day, leak_month,
					leak_year);
			setStringValue("leak_date", buffer);

			i += 3;
		}


		if (i + 3 >= len) return;
		// backflow detected by the meter, has higher precision than normal values
		double backflow = fromBackflow(bytes[i], bytes[i + 1], bytes[i + 2],
				bytes[i + 3]);
		t->addSpecialExplanation(i + offset, 4, KindOfData::CONTENT,
				Understanding::FULL, "*** %02X%02X%02X%02X backflow: %0.3f m3",
				bytes[i], bytes[i + 1], bytes[i + 2], bytes[i + 3],
				backflow);
		setNumericValue("backflow", Unit::M3, backflow);
		
		i += 4;

		double monthData;
		string month_field_name;
		for (int month = 1; month <= 12; ++month) {
			if (i + 2 >= len) return;
			month_field_name.clear();
			month_field_name.append(getMonth(month)).append("_total");
			monthData = fromMonthly(bytes[i], bytes[i + 1], bytes[i + 2]);
			// one of our meters was installed backwards so we know the max value is 99999,99
			// anything above (which should only be FFFFFF, more on that below) should be 0
			// FFFFFF (167772,15) means module was not active back then (before installation) - translates to 0
			if (monthData >= 100000) monthData = 0;
			t->addSpecialExplanation(i + offset, 3, KindOfData::CONTENT,
					Understanding::FULL,
					"*** %02X%02X%02X total consumption at the end of %s: %0.2f m3",
					bytes[i], bytes[i + 1], bytes[i + 2],
					getMonth(month).c_str(),
					monthData);
			setNumericValue(month_field_name, Unit::M3, monthData);
			i += 3;
		}

		if (i >= len) return;
		// changing this byte didn't seem to do anything
		// and it's always 00 on my meters
		uchar unknown = bytes[i];
		t->addSpecialExplanation(i + offset, 1, KindOfData::CONTENT,
				Understanding::NONE,
				"*** %02X unknown data",
				unknown);
		i++;
		// there should not be any more data
	}
}

// Test: HydrodigitWater hydrodigit 86868686 NOKEY
// telegram=|4E44B4098686868613077AF0004005_2F2F0C1366380000046D27287E2A0F150E00000000C10000D10000E60000FD00000C01002F0100410100540100680100890000A00000B30000002F2F2F2F2F2F|
// {"media":"water","meter":"hydrodigit","name":"HydrodigitWater","id":"86868686","total_m3":3.866,"meter_datetime":"2019-10-30 08:39","timestamp":"1111-11-11T11:11:11Z"}
// |HydrodigitWater;86868686;3.866;2019-10-30 08:39;1111-11-11 11:11.11

// Test: HydridigitWaterr hydrodigit 03245501 NOKEY
// telegram=|2444B4090155240317068C00487AC0000000_0C1335670000046D172EEA280F030000000000|
// {"id": "03245501","media": "warm water","meter": "hydrodigit","meter_datetime": "2023-08-10 14:23","name": "HydridigitWaterr","timestamp": "1111-11-11T11:11:11Z","total_m3": 6.735}
// |HydridigitWaterr;03245501;6.735;2023-08-10 14:23;1111-11-11 11:11.11
