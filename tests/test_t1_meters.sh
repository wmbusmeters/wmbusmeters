#!/bin/sh

PROG="$1"

mkdir -p testoutput
TEST=testoutput

TESTNAME="Test T1 meters"
TESTRESULT="ERROR"

METERS="MyWarmWater supercom587 12345678 NOKEY
      MyColdWater supercom587 11111111 NOKEY
      MyHeatCoster sontex868  27282728 NOKEY
      MoreWater   iperl       12345699 NOKEY
      WaterWater  iperl       33225544 NOKEY
      MyElectricity1 amiplus  10101010 NOKEY
      MyElectricity2 amiplus  00254358 NOKEY
      Duschen     mkradio3    34333231 NOKEY
      Duschagain  mkradio4    02410120 NOKEY
      HeatMeter   vario451    58234965 NOKEY
      Room        fhkvdataiii 11776622 NOKEY
      Room        fhkvdataiii 11111234 NOKEY
      Rooom       fhkvdataiv  14542076 FCF41938F63432975B52505F547FCEDF
      Gran101     gransystems 18046178 NOKEY
      Gran301     gransystems 20100117 NOKEY
      HeatMeter   eurisii     88018801 NOKEY
      Smokeo      lansensm    00010204 NOKEY
      Tempoo      lansenth    00010203 NOKEY
      Dooro       lansendw    00010205 NOKEY
      Countero    lansenpu    00010206 NOKEY
      Rummet      rfmamb      11772288 NOKEY
      IzarWater   izar        21242472 NOKEY
      IzarWater2  izar        66290778 NOKEY
      IzarWater3  izar        19790778 NOKEY
      IzarWater4  izar        18001698 NOKEY
      HydrusWater hydrus      64646464 NOKEY
      HydrusVater hydrus      65656565 NOKEY
      HydrusAES   hydrus      64745666 NOKEY
      HydrusIzarRSWarm auto   60904720 NOKEY
      HydrusIzarRS auto       60897379 NOKEY
      HydrodigitWater hydrodigit 86868686 NOKEY
      HydrodigitWater2 hydrodigit 67452301 NOKEY
      Q400Water   q400        72727272 NOKEY
      AxiomaWater q400        72727273 NOKEY
      Elen1       ebzwmbe     22992299 NOKEY
      Elen2       esyswm      77997799 NOKEY
      Elen3       ehzp        55995599 NOKEY
      Uater       ultrimis    95969798 NOKEY
      Vatten      apator08    004444dd NOKEY
      Wasser      rfmtx1      74737271 NOKEY
      Woter       waterstarm  20096221 BEDB81B52C29B5C143388CBB0D15A051
      Witer       topaseskr   78563412 NOKEY
      Heater      sensostar   12345679 NOKEY
      Voda        ev200       99993030 NOKEY
      Vodda       emerlin868  95949392 NOKEY
      Smokey      tsd2        91633569 NOKEY
      Sharky774   sharky774   72615127 NOKEY
      Sharky775   sharky      68926025 NOKEY
      Heating     compact5    62626262 NOKEY
      Heating2    compact5    66336633 NOKEY
      Hetta       elf         01885619 NOKEY
      DigiWasser  dme_07      93929190 NOKEY
      Votten      aventieswm  61070071 A004EB23329A477F1DD2D7820B56EB3D
      GasMeter    unismart    00043094 00000000000000000000000000000000
      HeatCool    hydrocalm3  71727374 NOKEY
      TempoHygro  munia       00013482 NOKEY
      QHeato      qheat       67228058 NOKEY
      QSmokeo     qsmoke      45797086 NOKEY
      QSmokep     qsmoke      48128850 NOKEY"


cat simulations/simulation_t1.txt | grep '^{' | jq --sort-keys . > $TEST/test_expected.txt
$PROG --format=json simulations/simulation_t1.txt $METERS  | jq --sort-keys . > $TEST/test_output.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/"timestamp": "....-..-..T..:..:..Z"/"timestamp": "1111-11-11T11:11:11Z"/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK json: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_responses.txt
        fi
        echo ERROR: $TESTNAME
        exit 1
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi

cat simulations/simulation_t1.txt | grep '^|' | sed 's/^|//' > $TEST/test_expected.txt
$PROG --format=fields simulations/simulation_t1.txt $METERS  > $TEST/test_output.txt
if [ "$?" = "0" ]
then
    cat $TEST/test_output.txt | sed 's/[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9].[0-9][0-9]$/1111-11-11 11:11.11/' > $TEST/test_responses.txt
    diff $TEST/test_expected.txt $TEST/test_responses.txt
    if [ "$?" = "0" ]
    then
        echo OK fields: $TESTNAME
        TESTRESULT="OK"
    else
        TESTRESULT="ERROR"
        if [ "$USE_MELD" = "true" ]
        then
            meld $TEST/test_expected.txt $TEST/test_responses.txt
        fi
        echo ERROR: $TESTNAME
        exit 1
    fi
else
    echo "wmbusmeters returned error code: $?"
    cat $TEST/test_output.txt
fi


if [ "$TESTRESULT" = "ERROR" ]
then
    echo ERROR: $TESTNAME
    exit 1
fi
