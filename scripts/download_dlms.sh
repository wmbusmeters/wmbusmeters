#!/bin/bash
# Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

SCRIPT_DIR=$(cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd)

echo "Downloading list of flags from www.dlms.com..."
curl -s 'https://www.dlms.com/srv/FlagIdLoader.php' > tmp.flags

xmq tmp.flags transform $SCRIPT_DIR/flags.xslq to-text > tsv.tmp.flags
iconv -f utf-8 -t ascii//TRANSLIT -c tsv.tmp.flags -o tsv.flags

cat tsv.flags | grep -v ^# | cut -f 1 > list.flags
cat tsv.flags | grep -v ^# | cut -f 2 > names.flags
cat tsv.flags | grep -v ^# | cut -f 3 > countries.flags

cat countries.flags | sort -u | grep -v '^$' > uniquec.flags

cat names.flags | tr -d "'" | tr -c 'a-zA-Z0-9\n' ' ' | tr -s ' ' | sed 's/^ //g' | sed 's/ $//g' > ansi.flags
cat ansi.flags | sed 's/\(^.......[^0123456789]*\)[0123456789]\+.*/\1/g' > cleaned.flags
cat cleaned.flags | sed -e "$(sed 's:.*:s/&//Ig:' uniquec.flags)" > cleanedc.flags

cat cleanedc.flags | sed \
-e 's/ ab\( \|$\)/ /Ig' \
-e 's/ ag\( \|$\)/ /Ig' \
-e 's/ a \?s\( \|$\)/ /Ig' \
-e 's/ co\( \|$\)/ /Ig' \
-e 's/ b \?v\( \|$\)/ /Ig' \
-e 's/ bvba\( \|$\)/ /Ig' \
-e 's/ corp\( \|$\)/ /Ig' \
-e 's/ d \?o \?o\( \|$\)/ /g' \
-e 's/ d \?d\( \|$\)/ /g' \
-e 's/ gmbh//Ig' \
-e 's/ gbr//Ig' \
-e 's/ inc\( \|$\)/ /Ig' \
-e 's/ kg\( \|$\)/ /Ig' \
-e 's/ llc/ /Ig' \
-e 's/ ltd//Ig' \
-e 's/ limited//Ig' \
-e 's/ nv\( \|$\)/ /Ig' \
-e 's/ oy//Ig' \
-e 's/ ood\( \|$\)/ /Ig' \
-e 's/ooo\( \|$\)/ /Ig' \
-e 's/ pvt\( \|$\)/ /Ig' \
-e 's/ pte\( \|$\)/ /Ig' \
-e 's/ pty\( \|$\)/ /Ig' \
-e 's/ plc\( \|$\)/ /Ig' \
-e 's/ private\( \|$\)/ /Ig' \
-e 's/ s \?a\( \|$\)/ /Ig' \
-e 's/ sarl\( \|$\)/ /Ig' \
-e 's/ sagl\( \|$\)/ /Ig' \
-e 's/ s c ul//Ig' \
-e 's/ s \?l\( \|$\)/ /Ig' \
-e 's/ s \?p \?a\( \|$\)/ /Ig' \
-e 's/ sp j\( \|$\)/ /Ig' \
-e 's/ sp z o o//Ig' \
-e 's/ s r o//Ig' \
-e 's/ s \?r \?l//Ig' \
-e 's/ ug\( \|$\)/ /Ig' \
                    > trimmed.flags

cat trimmed.flags | tr -s ' ' | sed 's/^ //g' | sed 's/ $//g' > done.flags

paste -d '|,' list.flags done.flags countries.flags | sed 's/,/, /g' | sed 's/ |/|/g' > manufacturers.txt

echo "// Copyright (C) $(date +%Y) Fredrik Öhrström (CC0)" > m.h
echo '#ifndef MANUFACTURERS_H' >> m.h
echo '#define MANUFACTURERS_H' >> m.h
echo '#define MANFCODE(a,b,c) ((a-64)*1024+(b-64)*32+(c-64))' >> m.h
echo "#define LIST_OF_MANUFACTURERS \\" >> m.h
cat manufacturers.txt | sed -e "s/\(.\)\(.\)\(.\).\(.*\)/X(\1\2\3,MANFCODE('\1','\2','\3'),\"\4\")\\\\/g" | sed 's/, ")/")/' >> m.h
echo >> m.h
cat manufacturers.txt | sed -e "s/\(.\)\(.\)\(.\).*/#define MANUFACTURER_\1\2\3 MANFCODE('\1','\2','\3')/g" >> m.h
echo >> m.h
echo '#endif' >> m.h
mv m.h src/manufacturers.h

echo "Updated src/manufacturers.h"

rm -f *.flags manufacturers.txt
