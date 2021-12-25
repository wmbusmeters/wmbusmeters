#!/bin/bash

rm -f /tmp/tmpcopyrights
cat > /tmp/tmpcopyrights <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: wmbusmeters
Source: https://github.com/weetmuts/wmbusmeters
Upstream-Contact: Fredrik Öhrström <oehrstroem@gmail.com>

Files: *
Copyright: 2017-2021 Fredrik Öhrström <oehrstroem@gmail.com>
License: GPL-3+

EOF

SOURCES=$(find src -type f | sort)

for f in $SOURCES
do
    cat $f | grep "Copyright (C)" | sed 's/.*Copyright (C) *//g' > /tmp/authors
    cat /tmp/authors | grep -v Öhrström > /tmp/other_authors
    cops=$(cat /tmp/authors | tr '\n' '|' | sed -z 's/|$//g' | sed 's/|/,\n /g')
    license="GPL-3+"
    if grep -q -i "public domain" $f
    then
        license="Public Domain"
    fi
    if [ -s /tmp/other_authors ]
    then
        {
        echo "Files: $f"
        echo "Copyright: $cops"
        echo "License: $license"
        echo ""
        } >> /tmp/tmpcopyrights
    fi
done

cat >> /tmp/tmpcopyrights <<EOF
License: GPL-3+
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 On Debian systems, the complete text of the GNU General Public License
 version 3 can be found in file "/usr/share/common-licenses/GPL-3".

License: Public Domain
 The authors, and therefore would be copyright holders, have as much
 as possible relinguished their copyright to the public domain.
EOF
