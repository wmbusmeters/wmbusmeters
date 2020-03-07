# wmbusmeters
The program receives and decodes C1,T1 or S1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings. The readings can then be published using
MQTT, curled to a REST api, inserted into a database or stored in a log file.

[FAQ/WIKI/MANUAL pages](https://github.com/weetmuts/wmbusmeters)

-	Supported architectures 
	-	ARMv7 32-bit (`arm32v7`)
	-	ARMv8 64-bit (`arm64v8`)
	-	Linux x86-64 (`amd64`)

## Running the wmbusmeters container
### Pre-requisite

Before running the command that creates the wmbusmeters docker container, you must add udev rules to create usb device symlink in order to have persistent link to device across host restarts and reconnect usb device:
```
cat <<EOF > /etc/udev/rules.d/99-wmbus-usb-serial.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="im871a", MODE="0660", GROUP="docker"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", SYMLINK+="amb8465", MODE="0660", GROUP="docker"
SUBSYSTEM=="usb", ATTRS{idVendor}=="2047", ATTRS{idProduct}=="0863", SYMLINK+="rfmrx2", MODE="0660", GROUP="docker"
EOF
```
> For RTLSDR symlink passing to docker image is not working due to fact that rtl_ binaries are looking for full usb bus path in system to identify device. Full usb bus path should be passed to docker `--device=/dev/bus/usb/002/117` for example and in wmbusmeters config file `device=auto` should be changed to `device=rtlwmbus`

### Command line container download and run

```
docker run -d \
    --name=wmbusmeters \
    --restart=always \
    -v /etc/localtime:/etc/localtime:ro \
    -v /opt/wmbusmeters:/wmbusmeters_data \
    --device=/dev/im871a \
    weetmuts/wmbusmeters 
```

### Command line Options
| Parameter | Description |
| ------------ | ------------- |
| --name=wmbusmeters | Names the container "wmbusmeters". |
| --restart=always | Start the container when Docker starts (i.e. on boot/reboot). |
| -v /etc/localtime:/etc/localtime:ro | Ensure the container has the correct local time. |
| -v /opt/wmbusmeters:/wmbusmeters_data | Bind mount /opt/wmbusmeters (or the directory of your choice) into the container for persistent storage that will contain configuration and log files. |
| --device=/dev/im871a | Pass the device at ttyUSB0 into the container for use by wmbusmeters (you may need to change path to your device ). |

> Specify device in configuration file if wmbusmeters does not find device by changeing `device=auto` to `device=/dev/im871a` for example.

If you are using docker-compose.yml file you may also copy/paste the following into your existing docker-compose.yml, modifying the options as required (omit the version and services lines as your docker-compose.yml will already contain these).
```
version: "2"
services:
  wmbusmeters:
    image: weetmuts/wmbusmeters
    container_name: wmbusmeters
    restart: always
    volumes:
      - /opt/wmbusmeters:/wmbusmeters_data
      - /etc/localtime:/etc/localtime:ro
    devices:
      - /dev/im871a

```
Then, you can do docker-compose pull to pull the latest weetmuts/wmbusmeters image, docker-compose up -d to start the wmbusmeters container service, and docker-compose down to stop the wmbusmeters service and delete the container. Note that these commands will also pull, start, and stop any other services defined in docker-compose.yml.

### Issues / Contributing

Please raise any issues with this container at its [GitHub repo](https://github.com/weetmuts/wmbusmeters)
