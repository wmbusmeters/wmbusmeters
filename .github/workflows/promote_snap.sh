#!/bin/bash

SNAP_NAME="wmbusmeters"
ARCH_LIST="arm64 armhf amd64"

if [ -n "$(git describe --tags | grep -)" ]; then
        GIT_REV="$(git describe --tags | cut -f1,2 -d'-')"
        echo "$GIT_REV is branch not tag release, exiting.."
        exit 1
else
        GIT_VER="$(git describe --tags)"
        echo "This is release - $GIT_VER"
fi

if $(timeout 5 snapcraft status $SNAP_NAME >/dev/null); then
        echo "snapcraft login sucessfull, continuing"
else
        echo "Looks like snapcraft login is not sucessfull, exiting...."
        exit 1
fi

for arch in $ARCH_LIST
do
        snap_build_version="$(snapcraft status --arch $arch $SNAP_NAME | grep edge | awk '{print $2}')"

        c=0
        while [[ "$GIT_VER" != "$snap_build_version" && $c -lt 10 ]]; do
                echo "GIT release version "$GIT_VER" != snap latest edge version at snapcraft for $arch "$snap_build_version", iter : $c";
                ((c = $c + 1));
                sleep 300;
                snap_build_version="$(snapcraft status --arch $arch $SNAP_NAME | grep edge | awk '{print $2}')"
        done

        if [[ "$GIT_VER" != "$snap_build_version" ]]; then
                echo "GIT release version "$GIT_VER" != snap latest edge version on snapcraft for $arch "$snap_build_version", exiting..";
                exit 1
        fi
done

for arch in $ARCH_LIST
do
        snap_build_id="$(snapcraft status --arch $arch $SNAP_NAME | grep edge | awk '{print $3}')"
        echo "Snap build id for arch $arch - $snap_build_id, promoting to stable release"
        snapcraft release $SNAP_NAME $snap_build_id stable
done
