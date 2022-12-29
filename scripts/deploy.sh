#!/bin/bash
# Copyright (C) 2022 Fredrik Öhrström (gpl-3.0-or-later)

# Grab all text up to the "Version x.y.z-RC1 <date>" line
# There should be no text.
CHANGES=$(sed '/Version /q' CHANGES | grep -v ^Version | sed '/./,$!d' | \
          tac | sed -e '/./,$!d' | tac | sed -e '/./,$!d' > /tmp/release_changes)

if [ -s /tmp/release_changes ]
then
    echo "Oups! There are changes declared in the CHANGES file. There should not be if you are going to deploy."
    exit 0
fi

# Grab all text between the Version RC and the previous VERSION.
sed -n '/^Version.*-RC[0-9] /,/^Version .*\.[0-9]\+:/{p;/^Version .*\.[0-9]\+ /q}' CHANGES \
              | grep -v "^Version " | sed '/./,$!d' \
              | tac | sed -e '/./,$!d' | tac | sed -e '/./,$!d' > /tmp/release_changes

if [ ! -s /tmp/release_changes ]
then
    echo "Oups! There should be changes declared in the CHANGES file between the RC version and the previous released version."
    exit 0
fi

# Grab the line from CHANGES which says: Version 1.2.3-RC1 2022-12-22
OLD_MESSAGE=$(grep -m 1 ^Version CHANGES)
# Now extract the major, minor and patch.
PARTS=$(grep -m 1 ^Version CHANGES  | sed 's/Version \([0-9]\+\)\.\([0-9]\+\)\.\([0-9]\+\).*/\1 \2 \3/')

MAJOR=$(echo "$PARTS" | cut -f 1 -d ' ')
MINOR=$(echo "$PARTS" | cut -f 2 -d ' ')
PATCH=$(echo "$PARTS" | cut -f 3 -d ' ')

NEW_VERSION="$MAJOR.$MINOR.$PATCH"

if git tag | grep -q "^${NEW_VERSION}\$"
then
    echo "Oups! The new version tag $NEW_VERSION already exists!"
    exit 0
fi

NEW_MESSAGE="Version $NEW_VERSION $(date +'%Y-%m-%d')"

PREV_GIT_MESSAGE=$(git log -1 --pretty=%B)

if [ "$PREV_GIT_MESSAGE" != "$OLD_MESSAGE" ]
then
    echo "Oups! Something is wrong in the git log."
    echo "Expected last commit to say \"$OLD_MESSAGE\" but it doesn't, it says: \"$PREV_GIT_MESSAGE\""
    exit 0
fi

echo
echo "Deploying >>$NEW_MESSAGE<< with changelog:"
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

# Update the ha-addon version number. This will trigger rebuilds at ha installs with auto-update.
CMD="s/\"version\": \"[\.0-9]*\"/\"version\": \"${NEW_VERSION}\"/"
sed -i "$CMD" ha-addon/config.json
echo "Updated version number in ha-addon/config.json to $NEW_VERSION"

# Update the CHANGES file
CMD="1i $NEW_MESSAGE"
sed -i "$CMD" CHANGES
echo "Updated version string in CHANGES"

CMD="s/wmbusmeters version:.*/wmbusmeters version: $NEW_VERSION/g"
sed -i "$CMD" README.md
echo "Updated version string in README"

git commit -am "$NEW_MESSAGE"

git tag "$NEW_VERSION"

echo "Now do: git push --followtags"
