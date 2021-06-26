#!/bin/sh

ID=$(id -u wmbusmeters 2>/dev/null)

if [ -f "$ROOT"/usr/sbin/nologin ]
then
    USERSHELL="$ROOT/usr/sbin/nologin"
elif [ -f "$ROOT"/sbin/nologin ]
then
    USERSHELL="$ROOT/sbin/nologin"
else
    USERSHELL="/bin/false"
fi

if [ $(getent group wmbusmeters) ]
then
    echo "group: wmbusmeters unmodified"
else
    groupadd -f wmbusmeters
    echo "group: added wmbusmeters"
fi

if [ -z "$ID" ]
then
    # Create the wmbusmeters user
    useradd --system --shell $USERSHELL -g wmbusmeters wmbusmeters
    echo "user: added wmbusmeters"
else
    echo "user: wmbusmeters unmodified"
fi

if [ $(getent group dialout) ]
then
    if [ "$(groups wmbusmeters | grep -o dialout)" = "" ]
    then
        # Add the wmbusmeters user to dialout
        usermod -a -G dialout wmbusmeters
        echo "user: added wmbusmeters to dialout group"
    else
        echo "user: wmbusmeters already added to dialout"
    fi
else
    echo "dialout group does not exist"
fi

if [ $(getent group uucp) ]
then
    if [ "$(groups wmbusmeters | grep -o uucp)" = "" ]
    then
        # Add the wmbusmeters user to uucp
        usermod -a -G uucp wmbusmeters
        echo "user: added wmbusmeters to uucp group"
    else
        echo "user: wmbusmeters already added to uucp"
    fi
else
    echo "uucp group does not exist"
fi

if [ $(getent group plugdev) ]
then
    if [ "$(groups wmbusmeters | grep -o plugdev)" = "" ]
    then
        # Add the wmbusmeters user to plugdev
        usermod -a -G plugdev wmbusmeters
        echo "user: added wmbusmeters to plugdev group"
    else
        echo user: wmbusmeters already added to plugdev
    fi
else
    echo "plugdev group does not exist"
fi
