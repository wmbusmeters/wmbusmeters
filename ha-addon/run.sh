#!/usr/bin/with-contenv bashio

CONFIG_PATH=/data/options.json

CONFIG_DATA_PATH=$(bashio::config 'data_path')
CONFIG_CONF=$(bashio::jq "${CONFIG_PATH}" '.conf')
CONFIG_METERS=$(bashio::jq "${CONFIG_PATH}" '.meters')

bashio::log.info "CONFIG_CONF ..."
bashio::log.info "${CONFIG_CONF}"
bashio::log.info "CONFIG_METERS ..."
bashio::log.info "${CONFIG_METERS}"

bashio::log.info "Syncing wmbusmeters configuration ..."
if ! bashio::fs.directory_exists "${CONFIG_DATA_PATH}/logs/meter_readings"; then
    mkdir -p "${CONFIG_DATA_PATH}/logs/meter_readings"
fi
if ! bashio::fs.directory_exists "${CONFIG_DATA_PATH}/etc/wmbusmeters.d"; then
    mkdir -p "${CONFIG_DATA_PATH}/etc/wmbusmeters.d"
fi

echo -e "$CONFIG_CONF" | jq -r '
  to_entries | map( 
    .key as $k |
    if ( .value | type ) == "array" then
      .value | map( { key: $k, value: . } )
    else 
      . 
    end 
  ) | flatten | map( "\(.key)=\(.value|tostring)") | .[]
' > $CONFIG_DATA_PATH/etc/wmbusmeters.conf

bashio::log.info "Registering meters ..."
rm -f $CONFIG_DATA_PATH/etc/wmbusmeters.d/*
meter_no=0
IFS=$'\n'
for meter in $(jq -c -M '.meters[]' $CONFIG_PATH)
do
    meter_no=$(( meter_no+1 ))
    METER_NAME=$(printf 'meter-%04d' "$(( meter_no ))")
    bashio::log.info "Adding $METER_NAME ..."
    METER_DATA=$(printf '%s\n' $meter | jq --raw-output -c -M '.') 
    echo -e "$METER_DATA" | jq -r 'to_entries|map("\(.key)=\(.value|tostring)")|.[]' > $CONFIG_DATA_PATH/etc/wmbusmeters.d/$METER_NAME
done

bashio::log.info "Generating MQTT configuration ... "
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

bashio::log.info "Broker $MQTT_HOST will be used."
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

bashio::log.info "Running wmbusmeters ..."
/wmbusmeters/wmbusmeters --useconfig=$CONFIG_DATA_PATH
