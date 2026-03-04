#!/bin/bash

mkdir -p ../build/web

#########################################################

echo -n "Copying drivers"
cp src/*.xmq ../build/web

#########################################################

echo -n "Generating html for xmq files"
for i in src/*
do
    xmq $i render-html --theme=lightbg > ../build/web/$(basename $i).html
    echo -n "."
done
echo "done."

#########################################################

echo -n "Generating map mvt to driver"

OUT=../build/web/map.tmp
rm -f $OUT
for i in src/*.xmq
do
    NAME=$(basename $i)
    export NAME="${NAME%.*}"
    (xmq $i for-each /driver/detect/mvt --shell='./print_map_mvt.sh "${.}" "$NAME"') >> $OUT
    echo -n "."
done

OUT=../build/web/mvt-to-driver.xmq

echo "mvt_to_driver {" > $OUT
sort $OUT ../build/web/map.tmp >> $OUT
echo "}" >> $OUT

echo "done."

###########################################################

DATE="$(date +%Y-%m-%d_%H:%M)"
OUT=../build/web/index.htmq

echo "!DOCTYPE=html" > $OUT
echo "html {" >> $OUT
echo 'head { style = """
body {
    width: 800px;
    font-family: "Courier New", Courier, monospace;
    margin: 0 auto;
    padding: 1rem;
    color: black;
    background-color: white;
}
a { text-decoration: none; color: blue; }
a:visited { color: blue; }
a:hover { color: black; }
"""
}' >> $OUT
echo "body {" >> $OUT

echo "h2='wmbusmeters drivers $DATE'" >> $OUT

echo -n "Generating index.html"
for i in $(ls ../build/web/*xmq.html | sort)
do
    HREF=$(basename $i)
    NAME=${HREF%%.*}
    echo "a(href=${HREF})=${NAME}.xmq" >> $OUT
    echo "&nbsp;&nbsp;" >> $OUT
    echo "'(' a(href=${NAME}.xmq)=raw ')'" >> $OUT
    echo br  >> $OUT
    echo -n "."
done
echo "done."

echo "}}" >> $OUT

xmq $OUT to-html > ../build/web/index.html
