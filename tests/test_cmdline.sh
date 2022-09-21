#!/bin/sh

PROG="$1"
TEST=testoutput
mkdir -p $TEST

TESTNAME="Test cmdline --overridedevice and --useconfig"
TESTRESULT="ERROR"

$PROG  --overridedevice=simulations/simulation_single_telegram.txt --useconfig=tests/config12 > $TEST/test_output.txt 2> $TEST/test_stderr.txt

if [ "$?" = "0" ]
then
    echo 6.408 > $TEST/test_expected.txt
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test cmdline --overridedevice without --useconfig"
TESTRESULT="ERROR"

$PROG  --overridedevice=rtlwmbus > $TEST/test_output.txt 2>&1

if [ "$?" != "0" ]
then
    echo "You can only use --overridedevice=xyz with --useconfig=xyz" > $TEST/test_expected.txt
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi


TESTNAME="Test cmdline --overridedevice with --useconfig and surplus argument"
TESTRESULT="ERROR"

$PROG  --overridedevice=rtlwmbus --useconfig=tests/config12 gurka > $TEST/test_output.txt 2>&1

if [ "$?" != "0" ]
then
    echo "Usage error: too many arguments \"gurka\" with --useconfig=..." > $TEST/test_expected.txt
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test cmdline --overridedevice with --useconfig and surplus arguments"
TESTRESULT="ERROR"

$PROG  --overridedevice=rtlwmbus --useconfig=tests/config12 gurka banan > $TEST/test_output.txt 2>&1

if [ "$?" != "0" ]
then
    echo "Usage error: too many arguments \"gurka\" with --useconfig=..." > $TEST/test_expected.txt
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test cmdline --overridedevice with --useconfig and surplus argument prefixed"
TESTRESULT="ERROR"

$PROG banan --overridedevice=rtlwmbus --useconfig=tests/config12 > $TEST/test_output.txt 2>&1

if [ "$?" != "0" ]
then
    echo "Usage error: too many arguments \"banan\" with --useconfig=..." > $TEST/test_expected.txt
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi

TESTNAME="Test cmdline --overridedevice with --useconfig and surplus argument infixed"
TESTRESULT="ERROR"

$PROG --overridedevice=rtlwmbus apple --useconfig=tests/config12 > $TEST/test_output.txt 2>&1

if [ "$?" != "0" ]
then
    echo "Usage error: too many arguments \"apple\" with --useconfig=..." > $TEST/test_expected.txt
    diff $TEST/test_expected.txt $TEST/test_output.txt
    if [ "$?" = "0" ]
    then
        echo "OK: $TESTNAME"
        TESTRESULT="OK"
    fi
fi

if [ "$TESTRESULT" = "ERROR" ]; then echo ERROR: $TESTNAME;  exit 1; fi
