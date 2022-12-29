#!/bin/bash
# Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

TYPE=$1

if [ "$TYPE" != "major" ] && [ "$TYPE" != "minor" ] &&  [ "$TYPE" != "patch" ] && [ "$TYPE" != "rc" ]
then
    echo "You must supply major,minor or patch!"
    exit 0
fi

# Grab all text up to the "Version x.y.z <date>" line
# If there is no text, then we have to add some information to CHANGES
# before we make a release.
CHANGES=$(sed '/Version /q' CHANGES | grep -v ^Version | sed '/./,$!d' | \
          tac | sed -e '/./,$!d' | tac | sed -e '/./,$!d' > /tmp/release_changes)

if [ ! -s /tmp/release_changes ]
then
    echo "Oups! There are no changes declared in the CHANGES file. There should be some for a release!"
    exit 0
fi

VERSION=$(grep -m 1 ^Version CHANGES  | sed 's/Version \([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\)\(-RC[ 0-9]\?\)\? .*/\1 \2 \3 \4/')

MAJOR=$(echo "$VERSION" | cut -f 1 -d ' ')
MINOR=$(echo "$VERSION" | cut -f 2 -d ' ')
PATCH=$(echo "$VERSION" | cut -f 3 -d ' ')
RC=$(echo "$VERSION" | cut -f 4 -d ' ')

if [ ! "$RC" = "" ]
then
    if [ ! "$TYPE" = "rc" ]
    then
        echo "Previous tag was a release candidate. You should run \"make release_rc\""
        echo "if you need to create another release candidate. Or \"make deploy\" if"
        echo "the candidate passed testing."
        exit 0
    fi
    # The previous tag was an RC version, ie we are fixing a bug in a release candidate.
    # Bump to next release candidate.
    RC="${RC#-RC}"
    RC=$((RC+1))
else
    # rc can only be used when the previous tag was also an rc!
    if [ "$TYPE" = "rc" ] ; then echo "You must supply major,minor or patch! Not rc." ; exit 0; fi
    # otherwise you supply major, minor or patch.
    if [ "$TYPE" = "major" ] ; then MAJOR=$((MAJOR+1)) ; fi
    if [ "$TYPE" = "minor" ] ; then MINOR=$((MINOR+1)) ; fi
    if [ "$TYPE" = "patch" ] ; then PATCH=$((PATCH+1)) ; fi
    RC=1
fi

RC_VERSION="$MAJOR.$MINOR.$PATCH-RC$RC"

MESSAGE="Version $RC_VERSION $(date +'%Y-%m-%d')"

echo
echo "$MESSAGE"
echo "----------------------------------------------------------------------------------"
cat /tmp/release_changes
echo "----------------------------------------------------------------------------------"
echo
while true; do
    read -p "Ok to create release candidate? y/n " yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done

# Insert release candidate line in CHANGES.
CMD="1 i\$MESSAGE"
sed -i "$CMD" CHANGES

git commit -am "$MESSAGE"

git tag "$RC_VERSION"

echo "Now do: git push ; git push --tags"

#git push
#git push --tags
