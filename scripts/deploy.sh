#!/bin/bash

# Grab all text up to the "Version x.y.z-RC1: <date>" line
# There should be no text.
CHANGES=$(sed '/Version /q' CHANGES | grep -v ^Version | sed '/./,$!d' | \
          tac | sed -e '/./,$!d' | tac | sed -e '/./,$!d' > /tmp/release_changes)

if [ -s /tmp/release_changes ]
then
    echo "Oups! There are changes declared in the CHANGES file. There should not be if you are going to deploy."
    exit 0
fi

# Grab all text between the Version RC and the previous VERSION.
sed -n '/^Version.*-RC[0-9]:/,/^Version .*\.[0-9]\+:/{p;/^Version .*\.[0-9]\+:/q}' CHANGES \
              | grep -v "^Version " | sed '/./,$!d' \
              | tac | sed -e '/./,$!d' | tac | sed -e '/./,$!d' > /tmp/release_changes

if [ ! -s /tmp/release_changes ]
then
    echo "Oups! There should be changes declared in the CHANGES file between the RC version and the previous released version."
    exit 0
fi

OLD_MESSAGE=$(grep -m 1 ^Version CHANGES)
RC_VERSION=$(grep -m 1 ^Version CHANGES  | sed 's/Version \([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\).*/\1 \2 \3/')

MAJOR=$(echo "$RC_VERSION" | cut -f 1 -d ' ')
MINOR=$(echo "$RC_VERSION" | cut -f 2 -d ' ')
PATCH=$(echo "$RC_VERSION" | cut -f 3 -d ' ')

NEW_VERSION="$MAJOR.$MINOR.$PATCH"

NEW_MESSAGE="Version $NEW_VERSION: $(date +'%Y-%m-%d')"

PREV_GIT_MESSAGE=$(git log -1 --pretty=%B)

if [ "PREV_GIT_MESSAGE" != "$OLD_MESSAGE" ]
then
    echo "Oups! Something is wrong in the git log. Expected last commit to say \"$OLD_MESSAGE\" but it does not!"
    exit 0
fi

echo
echo "Deploying release $NEW_MESSAGE with changelog:"
echo "----------------------------------------------------------------------------------"
cat /tmp/release_changes
echo "----------------------------------------------------------------------------------"
echo
while true; do
    read -p "Ok to deploy release? y/n " yn
    case $yn in
        [Yy]* ) break;;
        [Nn]* ) exit;;
        * ) echo "Please answer yes or no.";;
    esac
done

sed 's/"version": "[\.0-9]*"/"version": "'$NEW_VERSION'"/' ha-addon/config.json > /tmp/release_haconfig
echo "Updated version number in ha-addon/config.json to $NEW_VERSION"

sed "s/$OLD_MESSAGE/$NEW_MESSAGE/" CHANGES > /tmp/release_changes
echo "Updated version string in CHANGES"

git commit -am "$NEW_MESSAGE"

git tag "$NEW_VERSION"
