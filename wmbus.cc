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

#include"wmbus.h"

const char *LinkModeNames[] = {
#define X(name) #name ,
LIST_OF_LINK_MODES
#undef X
};

struct Manufacturer {
    char code[4];
    int m_field;
    char name[64];
};

Manufacturer manufacturers[] = {
#define X(key,code,name) {#key,code,#name},
LIST_OF_MANUFACTURERS
#undef X
{"",0,""}
};

struct Initializer { Initializer(); };

static Initializer initializser_;

Initializer::Initializer() {
    for (auto &m : manufacturers) {
	m.m_field = \
	    (m.code[0]-64)*1024 +
	    (m.code[1]-64)*32 +
	    (m.code[2]-64);
    }
}

void Telegram::print() {
    printf("Received telegram from id: %02x%02x%02x%02x\n",
	   a_field_address[0], a_field_address[1], a_field_address[2], a_field_address[3]);
    printf("             manufacturer: %s\n",
	   manufacturer(m_field).c_str());
    printf("              device type: %s\n",
	   deviceType(m_field, a_field_device_type).c_str());    
}

string manufacturer(int m_field) {
    for (auto &m : manufacturers) {
	if (m.m_field == m_field) return m.name;
    }
    return "Unknown";
}

string deviceType(int m_field, int a_field_device_type) {
    switch (a_field_device_type) {
    case 0: return "Other";
    case 1: return "Oil meter";
    case 2: return "Electricity meter";
    case 3: return "Gas meter";
    case 4: return "Heat meter";
    case 5: return "Steam meter";
    case 6: return "Warm Water (30°C-90°C) meter";
    case 7: return "Water meter";
    case 8: return "Heat Cost Allocator";
    case 9: return "Compressed air meter";
    case 0x0a: return "Cooling load volume at outlet meter";
    case 0x0b: return "Cooling load volume at inlet meter";
    case 0x0c: return "Heat volume at inlet meter";
    case 0x0d: return "Heat/Cooling load meter";
    case 0x0e: return "Bus/System component";
    case 0x0f: return "Unknown";
    case 0x15: return "Hot water (>=90°C) meter";
    case 0x16: return "Cold water meter";
    case 0x17: return "Hot/Cold water meter";
    case 0x18: return "Pressure meter";
    case 0x19: return "A/D converter";
    }
    return "Unknown";
}

