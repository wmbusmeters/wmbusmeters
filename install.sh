#!/bin/bash

if [ "$1" == "" ] || [ "$1" == "-h" ]
then
    echo "Usage: install.sh [binary] [root] [OPTIONS]
    Example: install.sh build/wmbusmeters /

    Options:
    --no-adduser        Do not add wmbusmeters user
    "
    exit 0
fi

if [ ! "$(basename "$1")" = "wmbusmeters" ]
then
    echo "Oups, please only try to install wmbusmeters using this script."
    exit 1
fi

if [ ! -x "$1" ]
then
    echo "This is not an executable."
    exit 1
fi

if [ ! -d "$2" ]
then
    echo "Oups, please supply a valid root directory."
    exit 1
fi

SRC=$1
ROOT="${2%/}"
ADDUSER=true

while [ $# -ne 0 ]
do
        ARG="$1"
        shift
        case "$ARG" in
        --no-adduser)
                ADDUSER=false
        ;;
        esac
done

SRC=$SRC ROOT=$ROOT /bin/sh ./scripts/install_binaries.sh

ROOT=$ROOT /bin/sh ./scripts/install_manpage.sh

if [ "$ADDUSER" = "true" ]
then
    ROOT=$ROOT /bin/sh ./scripts/add_wmbusmeters_user.sh
fi

ROOT=$ROOT /bin/sh ./scripts/prepare_logfiles.sh

ROOT=$ROOT /bin/sh ./scripts/install_default_configuration.sh

ROOT=$ROOT /bin/sh ./scripts/install_systemd_service.sh

ROOT=$ROOT /bin/sh ./scripts/add_myself_to_dialout.sh
