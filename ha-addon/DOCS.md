# Home Assistant Community Add-on: wmbusmeters (W-MBus to MQTT)

This add-on allows you to acquire utility meter readings **without** the vendors bridge or gateway as long as they support C1, T1 or S1 telegrams using the wireless mbus protocol (WMBUS).



## Installation

The installation of this add-on is pretty straightforward and not different in
comparison to installing any other community-driven Home Assistant add-on.

1. Navigate to Supervisor > Add-on Store > Repositories
1. Add https://github.com/weetmuts/wmbusmeters
1. Install Wmbusmeters
1. Plug-in your radio receiver USB dongle
1. Start the addon container
1. In the logs you should see all the W-Mbus telegrams that wmbusmeter is able to receive. <br> _If you don't see anything, check the logs carefully. <br> If you configure it late evening or in the night, please note the radio modules often send telegrams less frequently than in typical working hours or don't send them at all. <br> If your antenna is in a distance to the radio module, try to locate it closer._
1. You are ready to configure!
1. Finally, don't forget about adding the MQTT sensor into your Home Assistant.

## Configuration

Once the wmbusmeters is receiving the telegrams you need to configure your meter using `meters` option (see below) to pass the readings to MQTT topic.

#### Option: `meters`

Specify your meters usign wmbusmeters meter file format. The `driver` and `id` values can be read from the add-on logs after the initial start (with empty `meters` configuration). The `name` is your label for the meter and `key` is the encryption key to decrypt telegrams (if your meter use any).

See [project README for more information][github]

```yaml
meters:
  - |-
    name=MainWater
    driver=apator162
    id=01234567
    key=00000000000000000000000000000000
```

#### Option: `conf`

The wmbusmeters configuration file. It will be used as `wmbusmeters.conf`.

See [project README for more information][github]


#### Option: `data_path`

Path relative for add-on where wmbusmeters files are stored:
```
/logs/meter_readings/
/etc/wmbusmeters.conf
/etc/wmbusmeters.d/
```

#### Option: `mqtt`

By default it is empty `{}` and leverages then the _Moquitto broker_ addon details provided by supervisor. However, you can specify the custom mqtt broker connection details here
```yaml
mqtt:
  host: your-broker-host
  port: 1883
  user: your-username
  password: your-password
``` 

## Home Assistant integration

Finally, you need to tell Home Assistant how to extract the readings from the MQTT. You can add the following sensor definition into your `sensor:` section of `configuration.yaml`.

```yaml
sensor:
  - platform: mqtt
    state_topic: "wmbusmeters/MainWater"
    json_attributes_topic: "wmbusmeters/MainWater"
    unit_of_measurement: "m3"
    value_template: "{{ value_json.total_m3 }}"
    name: Water usage
    icon: "mdi:gauge"
```

_Please note: `MainWater` is the water meter name used in `meters` configuration._


## Support

Got questions?

You have several options to get them answered:

- [Open an issue here][issue] in project GitHub
- The Home Assistant [Community Forum][forum].
- Join the [Reddit subreddit][reddit] in [/r/homeassistant][reddit]

## Authors & contributors

[Wmbusmeters contributors][contributors]

[contributors]: https://github.com/weetmuts/wmbusmeters/graphs/contributors
[forum]: https://community.home-assistant.io/c/home-assistant-os/25
[github]: https://github.com/weetmuts/wmbusmeters
[issue]: https://github.com/weetmuts/wmbusmeters/issues
[reddit]: https://reddit.com/r/homeassistant
