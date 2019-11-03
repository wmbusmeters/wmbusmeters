#!/bin/bash

PROG="$1"

mkdir -p testoutput

TEST=testoutput

########################################################

xxd -r -p simulations/serial_rawtty_ok.hex | \
    $PROG --format=json --listento=any stdin \
          Rummet1 lansenth 00010203 "" \
          Rummet2 rfmamb 11772288 "" \
    | grep Rummet > $TEST/test_output.txt


cat > $TEST/test_expected.txt <<EOF
{"media":"room sensor","meter":"lansenth","name":"Rummet1","id":"00010203","current_temperature_c":21.8,"current_relative_humidity_rh":43,"average_temperature_1h_c":21.79,"average_relative_humidity_1h_rh":43,"average_temperature_24h_c":21.97,"average_relative_humidity_24h_rh":42.5,"timestamp":"1111-11-11T11:11:11Z"}
{"media":"room sensor","meter":"rfmamb","name":"Rummet2","id":"11772288","current_temperature_c":22.08,"average_temperature_1h_c":21.91,"average_temperature_24h_c":22.07,"maximum_temperature_1h_c":22.08,"minimum_temperature_1h_c":21.85,"maximum_temperature_24h_c":23.47,"minimum_temperature_24h_c":21.29,"current_relative_humidity_rh":44.2,"average_relative_humidity_1h_rh":43.2,"average_relative_humidity_24h_rh":44.5,"minimum_relative_humidity_1h_rh":42.2,"maximum_relative_humidity_1h_rh":50.1,"maximum_relative_humidity_24h_rh":0,"minimum_relative_humidity_24h_rh":0,"device_date_time":"2019-10-11 19:59","timestamp":"1111-11-11T11:11:11Z"}
EOF
if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo Reading binary telegram from stdin OK
    fi
else
    echo Failure.
    exit 1
fi

########################################################

xxd -r -p simulations/serial_rawtty_ok.hex > $TEST/test_raw

$PROG --format=json --listento=any $TEST/test_raw \
          Rummet1 lansenth 00010203 "" \
          Rummet2 rfmamb 11772288 "" \
    | grep Rummet > $TEST/test_output.txt

cat > $TEST/test_expected.txt <<EOF
{"media":"room sensor","meter":"lansenth","name":"Rummet1","id":"00010203","current_temperature_c":21.8,"current_relative_humidity_rh":43,"average_temperature_1h_c":21.79,"average_relative_humidity_1h_rh":43,"average_temperature_24h_c":21.97,"average_relative_humidity_24h_rh":42.5,"timestamp":"1111-11-11T11:11:11Z"}
{"media":"room sensor","meter":"rfmamb","name":"Rummet2","id":"11772288","current_temperature_c":22.08,"average_temperature_1h_c":21.91,"average_temperature_24h_c":22.07,"maximum_temperature_1h_c":22.08,"minimum_temperature_1h_c":21.85,"maximum_temperature_24h_c":23.47,"minimum_temperature_24h_c":21.29,"current_relative_humidity_rh":44.2,"average_relative_humidity_1h_rh":43.2,"average_relative_humidity_24h_rh":44.5,"minimum_relative_humidity_1h_rh":42.2,"maximum_relative_humidity_1h_rh":50.1,"maximum_relative_humidity_24h_rh":0,"minimum_relative_humidity_24h_rh":0,"device_date_time":"2019-10-11 19:59","timestamp":"1111-11-11T11:11:11Z"}
EOF
if [ "$?" == "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp":"....-..-..T..:..:..Z"/"timestamp":"1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" == "0" ]
    then
        echo Reading binary telegram from file OK
    fi
else
    echo Failure.
    exit 1
fi

########################################################
