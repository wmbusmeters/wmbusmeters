#!/bin/bash
# Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

TMP=$(mktemp)
OUT=$1

(cd src; grep -Ec "Copyright \(C\) (....-)?.... [^\(]+ \(.+\)" *  | grep :0 > $TMP)

if [ -s $TMP ]
then
    echo "These files do not have a proper copyright notice:"
    cat $TMP | sed 's/:0//'
    exit 1
fi

(cd src; grep -Eo "Copyright \(C\) (....-)?.... [^\(]+ \(.+\)" * | cut -f 2 -d ':' | tr -s ' ' | sed 's/(C) \([0-9][0-9][0-9][0-9]\) /(C) \1-\1 /' >  $TMP)

echo 'R"AUTHORS(' > $OUT

echo "Copyright (C) 2017-2022 Fredrik Öhrström" >> $OUT

cat $TMP | sed 's/ <.*>//' | \
    sed 's/ [0-9][0-9][0-9][0-9]-/ /' | \
    sed 's/ (.[^)].*)//g' | \
    grep -v Öhrström | \
    sort -rn  | \
    sed 's/Copyright (C) /                   /' | \
    uniq >> $OUT

echo ')AUTHORS";' >> $OUT
