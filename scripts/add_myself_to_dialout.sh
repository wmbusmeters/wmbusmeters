#!/bin/sh
# Copyright (C) 2021 Fredrik Öhrström (gpl-3.0-or-later)

if [ ! -z "$SUDO_USER" ]
then
    if [ "$(getent group dialout | grep $SUDO_USER)" = "" ]
    then
        usermod -a -G dialout $SUDO_USER
        echo "group: added $SUDO_USER to group dialout"
    else
        echo "group: $SUDO_USER already member of dialout"
    fi

    if [ "$(groups $SUDO_USER | grep -o wmbusmeters)" = "" ]
    then
        usermod -a -G wmbusmeters $SUDO_USER
        echo "group: added $SUDO_USER to group wmbusmeters"
    else
        echo "group: user $SUDO_USER already member of wmbusmeters"
    fi
fi
