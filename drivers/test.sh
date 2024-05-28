#!/bin/sh

export PROG="$1"
export DRIVER="$2"
echo Testing $DRIVER
xmq $DRIVER for-each /driver/test --shell='./test_case.sh "$PROG" "$DRIVER" "${args}" "${telegram}" "${json}" "${fields}" "$1"'
