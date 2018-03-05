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

#include"printer.h"

using namespace std;

Printer::Printer(bool json, bool fields, char separator, bool meterfiles, const char *meterfiles_dir)
{
    json_ = json;
    fields_ = fields;
    separator_ = separator;
    meterfiles_ = meterfiles;
    meterfiles_dir_ = meterfiles_dir;
}

void Printer::print(Meter *meter)
{
    FILE *output = stdout;

    if (meterfiles_) {
	char filename[128];
	memset(filename, 0, sizeof(filename));
	snprintf(filename, 127, "%s/%s", meterfiles_dir_, meter->name().c_str());
	output = fopen(filename, "w");
    }

    if (json_) {
        meter->printMeterJSON(output);
    }
    else if (fields_) {
        meter->printMeterFields(output, separator_);
    }
    else {
        meter->printMeterHumanReadable(output);
    }

    if (output != stdout) {
	fclose(output);
    }
}
