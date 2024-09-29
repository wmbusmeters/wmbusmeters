#!/bin/bash

mkdir -p ../build/web
OUT=../build/web/index.htmq

for i in src/*
do
    xmq $i render-html --theme=darkbg > ../build/web/$(basename $i).html
done

echo "!DOCTYPE=html" > $OUT
echo "html {" >> $OUT
echo "body {" >> $OUT

for i in $(ls ../build/web/*xmq.html | sort)
do
    HREF=$(basename $i)
    NAME=${HREF%%.*}
    echo "a(href=${HREF})=${NAME}.xmq" >> $OUT
    echo br  >> $OUT
done

echo "}}" >> $OUT

xmq $OUT to-html > ../build/web/index.html
