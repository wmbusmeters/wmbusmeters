#!/usr/bin/with-contenv bashio

CONFIG_PATH=/data/options.json

CONFIG_DATA_PATH=$(bashio::config 'data_path')
CONFIG_CONF="$(jq --raw-output -c -M '.conf' $CONFIG_PATH)"
CONFIG_METERS="$(jq --raw-output -c -M '.meters' $CONFIG_PATH)"

echo "Syncing wmbusmeters configuration ..."
[ ! -d $CONFIG_DATA_PATH/logs/meter_readings ] && mkdir -p $CONFIG_DATA_PATH/logs/meter_readings
[ ! -d $CONFIG_DATA_PATH/etc/wmbusmeters.d ] && mkdir -p $CONFIG_DATA_PATH/etc/wmbusmeters.d
echo -e "$CONFIG_CONF" > $CONFIG_DATA_PATH/etc/wmbusmeters.conf

echo "Registering meters ..."
rm -f $CONFIG_DATA_PATH/etc/wmbusmeters.d/*
meter_no=0
IFS=$'\n'
for meter in $(jq -c -M '.meters[]' $CONFIG_PATH)
do
    meter_no=$(( meter_no+1 ))
    METER_NAME=$(printf 'meter-%04d' "$(( meter_no ))")
    echo "Adding $METER_NAME ..."
    METER_DATA=$(printf '%s\n' $meter | jq --raw-output -c -M '.') 
    echo -e "$METER_DATA" > $CONFIG_DATA_PATH/etc/wmbusmeters.d/$METER_NAME
done

echo "Generating MQTT configuration ... "
if bashio::config.exists "mqtt.host"
then
  MQTT_HOST=$(bashio::config "mqtt.host")
  if bashio::config.exists "mqtt.port"; then MQTT_PORT=$(bashio::config "mqtt.port"); fi
  if bashio::config.exists "mqtt.user"; then MQTT_USER=$(bashio::config "mqtt.user"); fi
  if bashio::config.exists "mqtt.password"; then MQTT_PASSWORD=$(bashio::config "mqtt.password"); fi
else
  MQTT_HOST=$(bashio::services mqtt "host")
  MQTT_PORT=$(bashio::services mqtt "port")
  MQTT_USER=$(bashio::services mqtt "username")
  MQTT_PASSWORD=$(bashio::services mqtt "password")
fi

echo "Broker $MQTT_HOST will be used."
pub_args=('-h' $MQTT_HOST )
pub_args_quoted=('-h' \'$MQTT_HOST\' )
[[ ! -z ${MQTT_PORT+x} ]] && pub_args+=( '-p' $MQTT_PORT ) && pub_args_quoted+=( '-p' \'$MQTT_PORT\' )
[[ ! -z ${MQTT_USER+x} ]] && pub_args+=( '-u' $MQTT_USER ) && pub_args_quoted+=( '-u' \'$MQTT_USER\' )
[[ ! -z ${MQTT_PASSWORD+x} ]] && pub_args+=( '-P' $MQTT_PASSWORD ) && pub_args_quoted+=( '-P' \'$MQTT_PASSWORD\' )

cat > /wmbusmeters/mosquitto_pub.sh << EOL
#!/usr/bin/with-contenv bashio
TOPIC=\$1
MESSAGE=\$2
/usr/bin/mosquitto_pub ${pub_args_quoted[@]} -r -t "\$TOPIC" -m "\$MESSAGE"
EOL
chmod a+x /wmbusmeters/mosquitto_pub.sh

# Running MQTT discovery
/mqtt_discovery.sh ${pub_args[@]} -c $CONFIG_PATH -w $CONFIG_DATA_PATH || true

echo "Running wmbusmeters ..."
/wmbusmeters/wmbusmeters --useconfig=$CONFIG_DATA_PATH
