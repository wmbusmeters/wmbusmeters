#!/bin/bash

if [ $# -lt 2 ]; then
  echo "Usage: $0 [password] [kem(2)_file]"
  exit 1
fi

password="$1"
file="$2"

if unzip -v "$file" > /dev/null 2>&1
then
    rm -rf temp_extracting_kem
    mkdir temp_extracting_kem
    unzip -d temp_extracting_kem "$file"
    file=$(echo temp_extracting_kem/*.kem)
    if [ ! -f "$file" ]; then file=$(echo temp_extracting_kem/*.kem2) ; fi
    echo "Extracting from $file"
    echo
fi

# The key is the supplied password as raw bytes padded with zero bytes.
# The key is also used as the iv.
key=$(echo -n "$password" | xxd -p)
key=$(printf "%-32s" "$key" | tr ' ' '0')

echo "KEY=>$key<"
xml=$(cat "$file")

# Extract the Base64-encoded ciphertext from the XML
b64=$(echo "$xml" | sed -n 's/.*<CipherValue>\(.*\)<\/CipherValue>.*/\1/p' > gurka)

echo BASE64="$b64"
# Decrypt the ciphertext using the key and IV
plain=$(echo "$b64" | base64 -d | openssl enc -aes-128-cbc -d -K "$key" -iv "$key" -nopad)

# Extract the id and key from the decrypted XML
meter_key=$(echo "$plain" | sed -n 's/.*<DEK>\(.*\)<\/DEK>.*/\1/p')
meter_id=$(echo "$plain" | sed -n 's/.*<SerialNo>\(.*\)<\/SerialNo>.*/\1/p')

echo "id=$meter_id"
echo "key=$meter_key"
