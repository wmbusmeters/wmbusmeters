# wmbusmeters
The program receives and decodes C1,T1 or S1 telegrams
(using the wireless mbus protocol) to acquire
utility meter readings. The readings can then be published using
MQTT, curled to a REST api, inserted into a database or stored in a log file.

[FAQ/WIKI/MANUAL pages](https://github.com/wmbusmeters/wmbusmeters)

-	Supported architectures
	-	ARMv7 32-bit (`armv7`)
	-	ARMv8 64-bit (`arm64`)
	-	Linux x86-64 (`amd64`)

## Running the wmbusmeters container

wmbusmeters is able to detect attachment and removal of wmbus dongles and to provide that functionality within docker image it must be started in privileged mode to have access to hosts `/dev/` content

```
docker run -d --privileged \
    --name=wmbusmeters \
    --restart=always \
    -v /etc/localtime:/etc/localtime:ro \
    -v /opt/wmbusmeters:/wmbusmeters_data \
    -v /dev/:/dev/ \
    wmbusmeters/wmbusmeters
```

### Command line Options
| Parameter | Description |
| ------------ | ------------- |
| --privileged | Give extended privileges to this container to be able to read `/dev/` content |
| --name=wmbusmeters | Names the container "wmbusmeters". |
| --restart=always | Start the container when Docker starts (i.e. on boot/reboot). |
| -v /etc/localtime:/etc/localtime:ro | Ensure the container has the correct local time. |
| -v /opt/wmbusmeters:/wmbusmeters_data | Bind mount /opt/wmbusmeters (or any other directory) into the container for persistent storage that will contain configuration and log files. |
| -v /dev/:/dev/ | This will work for both wmbus and RTLSDR dongles. If only RTLSDR is used then `-v /dev/bus/usb/:/dev/bus/usb/` can be used. |

If docker-compose.yml file is being used, then it is also possible to copy/paste the following into existing docker-compose.yml, modifying the options as required (by omitting the version and services lines as existing docker-compose.yml may already contain those).
```
version: "2"
services:
  wmbusmeters:
    image: wmbusmeters/wmbusmeters
    container_name: wmbusmeters
    restart: always
    volumes:
      - /opt/wmbusmeters:/wmbusmeters_data
      - /etc/localtime:/etc/localtime:ro
      - /dev/:/dev/
```
Then, `docker-compose pull` can be used to pull the latest wmbusmeters/wmbusmeters image, `docker-compose up -d` to start the wmbusmeters container service, and `docker-compose down` to stop the wmbusmeters service and delete the container. It must be noted that those commands will also pull, start, and stop any other services defined in docker-compose.yml.


## How to run docker in unprivileged mode

If security is of concern - it is also possible to run docker container without privileged mode with non-RTLSDR dongles. For RTLSDR symlink passing to docker image is not working due to the fact that rtl_ binaries are looking for a full usb bus path in the system to identify device.


### Command line for container download and run

```
docker run -d \
    --name=wmbusmeters \
    --restart=always \
    -v /etc/localtime:/etc/localtime:ro \
    -v /opt/wmbusmeters:/wmbusmeters_data \
    --device=/dev/ttyUSB0 \
    wmbusmeters/wmbusmeters
```

### Command line Options
| Parameter | Description |
| ------------ | ------------- |
| --name=wmbusmeters | Names the container "wmbusmeters". |
| --restart=always | Start the container when Docker starts (i.e. on boot/reboot). |
| -v /etc/localtime:/etc/localtime:ro | Ensure the container has the correct local time. |
| -v /opt/wmbusmeters:/wmbusmeters_data | Bind mount /opt/wmbusmeters (or any other directory) into the container for persistent storage that will contain configuration and log files. |
| --device=/dev/im871a | Passes the device at /dev/im871a into the container for use by wmbusmeters (path to device may need to be changed). |


If docker-compose.yml file is being used, then it is also possible to copy/paste the following into existing docker-compose.yml, modifying the options as required (by omitting the version and services lines as existing docker-compose.yml may already contain those).
```
version: "2"
services:
  wmbusmeters:
    image: wmbusmeters/wmbusmeters
    container_name: wmbusmeters
    restart: always
    volumes:
      - /opt/wmbusmeters:/wmbusmeters_data
      - /etc/localtime:/etc/localtime:ro
    devices:
      - /dev/im871a

```
Then, `docker-compose pull` can be used to pull the latest wmbusmeters/wmbusmeters image, `docker-compose up -d` to start the wmbusmeters container service, and `docker-compose down` to stop the wmbusmeters service and delete the container. It must be noted that those commands will also pull, start, and stop any other services defined in docker-compose.yml.

### Issues / Contributing

Please raise any issues with this container at its [GitHub repo](https://github.com/wmbusmeters/wmbusmeters)
