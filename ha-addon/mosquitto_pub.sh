#!/usr/bin/with-contenv bashio

TOPIC=$1
MESSAGE=$2

MQTT_HOST=$(bashio::config "mqtt.host")
[ -z "$MQTT_HOST"] && MQTT_HOST=$(bashio::services mqtt "host")
MQTT_PORT=$(bashio::config "mqtt.port")
[ -z "$MQTT_PORT"] && MQTT_PORT=$(bashio::services mqtt "port")
MQTT_USER=$(bashio::config "mqtt.username")
[ -z "$MQTT_USER"] && MQTT_USER=$(bashio::services mqtt "username")
MQTT_PASSWORD=$(bashio::config "mqtt.password")
[ -z "$MQTT_PASSWORD"] && MQTT_PASSWORD=$(bashio::services mqtt "password")

/usr/bin/mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASSWORD -t $TOPIC -m "$MESSAGE"