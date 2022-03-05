#!/bin/bash

TMP=$(mktemp -t wmbusmeters.copyrights.XXXXXXXXXX)
TMP_AUTHORS=$(mktemp -t wmbusmeters.authors.XXXXXXXXXX)
TMP_OTHER_AUTHORS=$(mktemp -t wmbusmeters.other.authors.XXXXXXXXXX)

function finish {
    rm -f $TMP $TMP_AUTHORS $TMP_OTHER_AUTHORS
}
trap finish EXIT

cat > $TMP <<EOF
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: wmbusmeters
Source: https://github.com/weetmuts/wmbusmeters
Upstream-Contact: Fredrik Öhrström <oehrstroem@gmail.com>

Files: *
Copyright: 2017-2022 Fredrik Öhrström <oehrstroem@gmail.com>
License: GPL-3+

EOF

SOURCES=$(find src -type f | sort)

for f in $SOURCES
do
    cat $f | grep "Copyright (C)" | sed 's/.*Copyright (C) *//g' > $TMP_AUTHORS
    cat $TMP_AUTHORS | grep -v Öhrström > $TMP_OTHER_AUTHORS
    cops=$(cat $TMP_AUTHORS | tr '\n' '|' | \
               sed 's/[0-9][0-9][0-9][0-9]-//' | \
               sed 's/ ([^)]*)//g' | \
               sed -z 's/|$//g' | \
               sed 's/|/\n           /g')
    if grep -q -i "gpl-3.0-or-later" $f
    then
        license="GPL-3+"
    elif grep -q -i "CC0" $f
    then
        license="CC0"
    else
        echo "Unknown license in file: "+$f
        exit 1
    fi

    if [ -s $TMP_OTHER_AUTHORS ]
    then
        {
        echo "Files: $f"
        echo "Copyright: $cops"
        echo "License: $license"
        echo ""
        } >> $TMP
    fi
done

cat >> $TMP <<EOF
License: GPL-3+
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 .
 On Debian systems, the complete text of the GNU General Public License
 version 3 can be found in file "/usr/share/common-licenses/GPL-3".

License: CC0
 The authors, and therefore would be copyright holders, have as much
 as possible relinguished their copyright to the public domain.
EOF

mv $TMP $1
chmod 644 $1

echo "Created copyright file: $1"
