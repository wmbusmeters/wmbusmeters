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
### Prerequisite

Before running the command that creates the wmbusmeters docker container, udev rules must be added to create usb device symlink in order to have persistent link to device across host restarts and reconnects of usb device:
```
cat <<EOF > /etc/udev/rules.d/99-wmbus-usb-serial.rules
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", SYMLINK+="im871a", MODE="0660", GROUP="docker"
SUBSYSTEM=="tty", ATTRS{idVendor}=="0403", ATTRS{idProduct}=="6001", SYMLINK+="amb8465", MODE="0660", GROUP="docker"
SUBSYSTEM=="usb", ATTRS{idVendor}=="2047", ATTRS{idProduct}=="0863", SYMLINK+="rfmrx2", MODE="0660", GROUP="docker"
EOF
```

### Command line for container download and run

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
| -v /opt/wmbusmeters:/wmbusmeters_data | Bind mount /opt/wmbusmeters (or any other directory) into the container for persistent storage that will contain configuration and log files. |
| --device=/dev/im871a | Passes the device at /dev/im871a into the container for use by wmbusmeters (path to device may need to be changed). |

> Device must be specified in configuration file if wmbusmeters does not find device by changing `device=auto` to `device=/dev/im871a` for example.

If docker-compose.yml file is being used, then it is also possible to copy/paste the following into existing docker-compose.yml, modifying the options as required (by omitting the version and services lines as existing docker-compose.yml may already contain those).
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
Then, `docker-compose pull` can be used to pull the latest weetmuts/wmbusmeters image, `docker-compose up -d` to start the wmbusmeters container service, and `docker-compose down` to stop the wmbusmeters service and delete the container. It must be noted that those commands will also pull, start, and stop any other services defined in docker-compose.yml.

### Using RTLSDR dongles with container

For RTLSDR symlink passing to docker image is not working due to the fact that rtl_ binaries are looking for a full usb bus path in the system to identify device. When using RTLSDR following docker command should be used to start container and in wmbusmeters config file `device=auto` should be changed to `device=rtlwmbus`
```
docker run -d --privileged \
    --name=wmbusmeters \
    --restart=always \
    -v /etc/localtime:/etc/localtime:ro \
    -v /opt/wmbusmeters:/wmbusmeters_data \
    -v /dev/bus/usb/:/dev/bus/usb/ \
    weetmuts/wmbusmeters 
```

### Issues / Contributing

Please raise any issues with this container at its [GitHub repo](https://github.com/weetmuts/wmbusmeters)
