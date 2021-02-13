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

#ifndef UNITS_H
#define UNITS_H

#include<string>
#include<vector>

#define LIST_OF_QUANTITIES   \
    X(Energy,KWH)            \
    X(Reactive_Energy,KVARH) \
    X(Apparent_Energy,KVAH)  \
    X(Power,KW)              \
    X(Volume,M3)             \
    X(Flow,M3H)              \
    X(Temperature,C)         \
    X(RelativeHumidity,RH)   \
    X(HCA,HCA)               \
    X(Text,TXT)              \
    X(Counter,INT)           \
    X(Time,Hour)             \
    X(Voltage,Volt)          \
    X(Current,Ampere)        \
    X(Frequency,Hz)

#define LIST_OF_UNITS \
    X(KWH,kwh,"kWh",Energy,"kilo Watt hour")  \
    X(MJ,mj,"MJ",Energy,"Mega Joule")         \
    X(GJ,gj,"GJ",Energy,"Giga Joule")         \
    X(KVARH,kvarh,"kVARh",Reactive_Energy,"kilo volt amperes reactive hour")  \
    X(KVAH,kvah,"kVAh",Apparent_Energy,"kilo volt amperes hour")  \
    X(M3,m3,"m3",Volume,"cubic meter")        \
    X(L,l,"l",Volume,"litre")                 \
    X(KW,kw,"kW",Power,"kilo Watt")           \
    X(M3H,m3h,"m3/h",Flow,"cubic meters per hour") \
    X(LH,lh,"l/h",Flow,"liters per hour") \
    X(C,c,"°C",Temperature,"celsius")         \
    X(F,f,"°F",Temperature,"fahrenheit")      \
    X(RH,rh,"RH",RelativeHumidity,"relative humidity")      \
    X(HCA,hca,"hca",HCA,"heat cost allocation") \
    X(TXT,txt,"txt",Text,"text")              \
    X(INT,int,"int",Counter,"int")              \
    X(Second,s,"s",Time,"second")           \
    X(Minute,m,"m",Time,"minute")           \
    X(Hour,h,"h",Time,"hour") \
    X(Day,d,"d",Time,"day") \
    X(Year,y,"y",Time,"year") \
    X(Volt,v,"V",Voltage,"volt") \
    X(Ampere,a,"A",Current,"ampere") \
    X(Hz,hz,"Hz",Frequency,"hz")

enum class Unit
{
#define X(cname,lcname,hrname,quantity,explanation) cname,
LIST_OF_UNITS
#undef X
    Unknown
};

enum class Quantity
{
#define X(quantity,default_unit) quantity,
LIST_OF_QUANTITIES
#undef X
    Unknown
};

bool canConvert(Unit from, Unit to);
double convert(double v, Unit from, Unit to);
Unit toUnit(std::string s);
bool isQuantity(Unit u, Quantity q);
void assertQuantity(Unit u, Quantity q);
Unit defaultUnitForQuantity(Quantity q);
std::string unitToStringHR(Unit u);
std::string unitToStringLowerCase(Unit u);
std::string unitToStringUpperCase(Unit u);
std::string valueToString(double v, Unit u);

Unit replaceWithConversionUnit(Unit u, std::vector<Unit> cs);

#endif
