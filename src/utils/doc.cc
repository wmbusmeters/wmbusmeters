/*
 Copyright (C) 2026 Fredrik Öhrström (gpl-3.0-or-later)

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

#include "doc.h"


std::string docToString(XMQDoc *doc, XMQContentType format, bool pretty_print)
{
    XMQOutputSettings *os = xmqNewOutputSettings();

    xmqSetCompact(os, !pretty_print);
    xmqSetUseColor(os, false);
    xmqSetFinalNewline(os, false);
    if (format == XMQ_CONTENT_UNKNOWN) format = XMQ_CONTENT_XMQ;
    xmqSetOutputFormat(os, format);
    xmqSetRenderFormat(os, XMQ_RENDER_PLAIN);

    char *start, *stop;
    xmqSetupPrintMemory(os, &start, &stop);

    xmqPrint(doc, os);

    xmqFreeOutputSettings(os);

    stop--; // Drop the final NULL.
    std::string s = std::string(start, stop);

    free(start);

    return s;
}
