#!/usr/bin/env python
"""Generate Home Assistant discovery messages and publish them to MQTT broker."""
from argparse import ArgumentParser, Namespace
from copy import deepcopy
from json import dumps, load
from logging import INFO, Logger, basicConfig, getLevelName, getLogger
from os import walk
from os.path import join, splitext
from time import sleep
from typing import Mapping

from paho.mqtt.client import Client, MQTTMessage

DEFAULT_READINGS_PATH = "/var/lib/wmbusmeters/meter_readings"
DEFAULT_TEMPLATES_PATH = "./mqtt_discovery"
DEFAULT_DISCOVERY_PREFIX = "homeassistant"
DEFAULT_SUGGESTED_AREA = "home"
DEFAULT_MQTT_HOST = "localhost"

# copied over from logging._nameToLevel
# because the mentioned variable is marked as private
LOGGING_LEVELS = {
    "CRITICAL": 50,
    "FATAL": 50,
    "ERROR": 40,
    "WARN": 30,
    "WARNING": 30,
    "INFO": 20,
    "DEBUG": 10,
    "NOTSET": 0,
}


def configure_logging(level=INFO) -> Logger:
    color_reset = "\x1b[0m"
    cyan = "\x1b[36;20m"
    gray = "\x1b[37;20m"
    logger_format = (
        f"{gray}%(asctime)s{color_reset} "
        f"{cyan}%(levelname)7s{color_reset} "
        f"{gray}%(name)s{color_reset} %(message)s"
    )
    basicConfig(format=logger_format, level=level, force=True)
    return getLogger(__name__)


def parse_arguments() -> Namespace:
    parser = ArgumentParser(
        description="Generate Home Assistant discovery messages and publish them to MQTT broker"
    )
    parser.add_argument(
        "--readings-path",
        default=DEFAULT_READINGS_PATH,
        help="path to directory with wmbusmeters readings (default: %(default)s)",
    )
    parser.add_argument(
        "--templates-path",
        default=DEFAULT_TEMPLATES_PATH,
        help=(
            "path to directory with wmbusmeters templates "
            "for Home Assistant discovery messages (default: %(default)s)"
        ),
    )
    parser.add_argument(
        "--discovery-prefix",
        default=DEFAULT_DISCOVERY_PREFIX,
        help=(
            "prefix of MQTT topic used for Home Assistant "
            "discovery messages (default: %(default)s)"
        ),
    )
    parser.add_argument(
        "--suggested-area",
        default=DEFAULT_SUGGESTED_AREA,
        help=("suggested area of the Home Assistant devices (default: %(default)s)"),
    )
    parser.add_argument(
        "--logging-level",
        default="INFO",
        choices=LOGGING_LEVELS.keys(),
        help="logging level (default: %(default)s)",
    )
    parser.add_argument(
        "--mqtt-host",
        default=DEFAULT_MQTT_HOST,
        help="hostname of the MQTT broker (default: %(default)s)",
    )
    parser.add_argument(
        "--mqtt-username",
        help="username for connecting to the MQTT broker (default: %(default)s)",
    )
    parser.add_argument(
        "--mqtt-password",
        help="password for connecting to the MQTT broker (default: %(default)s)",
    )
    parser.add_argument(
        "--clean-old-messages",
        action="store_true",
        help="clean old retained Home Assistant discovery MQTT messages (default: %(default)s)",
    )

    return parser.parse_args()


def load_json_file(logger: Logger, filepath: str) -> any:
    """Deserialize JSON document from file at provided path and return it."""
    logger.debug("loading JSON file %s", filepath)
    with open(filepath, encoding="utf-8") as inputfile:
        return load(inputfile)


def load_json_files(logger: Logger, directory_path: str) -> dict[str, any]:
    """Return contents of JSON files at provided path indexed by their filename root."""
    contents = {}
    for dirpath, dirnames, filenames in walk(directory_path):
        dirnames.clear()  # ignore subdirectories
        for filename in filenames:
            filename_root, _filename_extension = splitext(filename)
            filepath = join(dirpath, filename)
            contents[filename_root] = load_json_file(logger, filepath)

    return contents


def generate_discovery_message(
    logger: Logger,
    meter_id: str,
    meter_name: str,
    meter_driver: str,
    attribute: str,
    suggested_area: str,
    template: Mapping[str, any],
) -> dict[str, any]:
    """Generate Home Assistant discovery message from the provided template.

    Substitute the provided values into specific parts of the template.
    """
    logger.debug("generating discovery message for meter %s-%s", meter_name, meter_id)
    message = deepcopy(template)

    value_topic = "/".join(["wmbusmeters", meter_name, meter_id])
    message["state_topic"] = value_topic
    message["json_attributes_topic"] = value_topic

    # later usage of the format method
    # needs two curly braces for every literal curly brace in output
    message["value_template"] = f'{{{{{message["value_template"]}}}}}'

    message["name"] = f"{meter_name} {meter_id} {attribute}"
    message["object_id"] = "-".join(["wmbusmeters", meter_name, meter_id, attribute])

    replacements = {
        "id": meter_id,
        "name": meter_name,
        "driver": meter_driver,
        "attribute": attribute,
    }

    for name, value in message.items():
        if isinstance(value, str):
            message[name] = value.format_map(replacements)

    device = message["device"]
    for name, value in device.items():
        if isinstance(value, str):
            device[name] = value.format_map(replacements)

    device["name"] = "-".join([meter_name, meter_id])
    device["suggested_area"] = suggested_area
    device["identifiers"] = ["-".join(["wmbusmeters", meter_id])]

    logger.debug(
        "generated discovery message for meter %s-%s: %s", meter_name, meter_id, message
    )
    return message


def generate_discovery_messages(
    logger: Logger,
    readings: Mapping[str, any],
    templates: Mapping[str, any],
    discovery_prefix: str,
    suggested_area: str,
) -> dict[str, any]:
    """Generate Home Assistant discovery messages.

    Return a dictionary of discovery messages indexed by topic.
    """
    logger.info("generating discovery messages")
    messages = {}

    for name, reading in readings.items():
        meter_id = reading["id"]
        meter_name = reading["name"]
        meter_driver = reading["meter"]
        if meter_driver not in templates:
            logger.warning(
                "unable to generate discovery messages for meter %s-%s "
                "because template for driver %s is missing",
                meter_name,
                meter_id,
                meter_driver,
            )
            continue
        meter_templates = templates[meter_driver]
        for attribute, template in meter_templates.items():
            component = template["component"]
            discovery_payload = template["discovery_payload"]
            object_id = "-".join([name, attribute])
            topic = "/".join(
                [discovery_prefix, component, "wmbusmeters", object_id, "config"]
            )

            messages[topic] = generate_discovery_message(
                logger,
                meter_id,
                meter_name,
                meter_driver,
                attribute,
                suggested_area,
                discovery_payload,
            )

    logger.info("generated %d discovery messages", len(messages))
    return messages


def on_message_callback(client: Client, userdata: dict, message: MQTTMessage):
    """Publish empty payload to all received retained messages.

    This effectively removes them from the MQTT broker.
    """
    logger = userdata["logger"]
    logger.debug("cleaning retained message from topic %s", message.topic)
    # empty payload causes the broker to delete the message
    client.publish(message.topic, retain=True)


def clean_retained_discovery_messages(
    logger: Logger,
    discovery_prefix: str,
    client: Client,
) -> dict[str, any]:
    timeout_seconds = 5
    logger.info(
        "cleaning relevant retained messages from MQTT broker at %s", client._host
    )
    client.loop_start()
    client.subscribe("/".join([discovery_prefix, "+", "wmbusmeters", "+", "config"]))
    client.on_message = on_message_callback

    logger.info(
        "waiting %d seconds to receive existing retained discovery messages",
        timeout_seconds,
    )
    sleep(timeout_seconds)

    client.loop_stop()
    logger.info(
        "cleaned relevant retained messages from MQTT broker at %s",
        client._host,
    )


def send_mqtt_messages(
    logger: Logger,
    discovery_prefix: str,
    messages: Mapping[str, any],
    mqtt_host: str = DEFAULT_MQTT_HOST,
    mqtt_username: str = None,
    mqtt_password: str = None,
    clean_old_messages: bool = False,
) -> None:
    timeout_seconds = 5

    logger.info("connecting to MQTT broker at %s", mqtt_host)
    client = Client(userdata={"logger": logger})
    if mqtt_username is not None:
        client.username_pw_set(mqtt_username, mqtt_password)
    client.connect(mqtt_host)
    logger.info("connected to MQTT broker at %s", mqtt_host)

    if clean_old_messages:
        clean_retained_discovery_messages(logger, discovery_prefix, client)

    logger.info("sending %d messages to MQTT broker", len(messages))
    client.loop_start()
    for topic, message in messages.items():
        logger.debug("sending discovery message to topic %s: %s", topic, message)
        client.publish(topic, payload=dumps(message), retain=True)

    logger.info(
        "waiting %d seconds to send new retained discovery messages", timeout_seconds
    )
    sleep(timeout_seconds)

    client.loop_stop()
    client.disconnect()
    logger.info("sent %d messages to MQTT broker", len(messages))


def work(
    readings_path: str = DEFAULT_READINGS_PATH,
    templates_path: str = DEFAULT_TEMPLATES_PATH,
    discovery_prefix: str = DEFAULT_DISCOVERY_PREFIX,
    suggested_area: str = DEFAULT_SUGGESTED_AREA,
    logging_level: int = INFO,
    mqtt_host: str = DEFAULT_MQTT_HOST,
    mqtt_username: str = None,
    mqtt_password: str = None,
    clean_old_messages: bool = False,
) -> None:
    """Generate Home Assistant discovery messages and publish them to MQTT broker."""
    logger = configure_logging(logging_level)
    readings = load_json_files(logger, readings_path)
    templates = load_json_files(logger, templates_path)

    messages = generate_discovery_messages(
        logger, readings, templates, discovery_prefix, suggested_area
    )

    send_mqtt_messages(
        logger,
        discovery_prefix,
        messages,
        mqtt_host,
        mqtt_username,
        mqtt_password,
        clean_old_messages,
    )


def do_work():
    namespace = parse_arguments()
    work(
        namespace.readings_path,
        namespace.templates_path,
        namespace.discovery_prefix,
        namespace.suggested_area,
        getLevelName(namespace.logging_level),
        namespace.mqtt_host,
        namespace.mqtt_username,
        namespace.mqtt_password,
        namespace.clean_old_messages,
    )


if __name__ == "__main__":
    do_work()
