#!/bin/bash

SO=src/.libs/libarakoon-1.0.so
SYMBOLS=SYMBOLS

if ! test -f $SO; then
    echo "Library not found"
    exit 1
fi

OUT1=`mktemp`
OUT2=`mktemp`

nm $SO | grep "[0-9a-f]*\ T\ " | sed "s/[0-9a-f]*\ T\ //" | sort > $OUT1
cat $SYMBOLS | grep -v -e ^$ -e ^# | sort > $OUT2

diff -u $OUT2 $OUT1

MD51=`md5sum $OUT1 | cut -d' ' -f1`
MD52=`md5sum $OUT2 | cut -d' ' -f1`

rm $OUT1
rm $OUT2

if [ $MD51 != $MD52 ]; then
        exit 1
fi
