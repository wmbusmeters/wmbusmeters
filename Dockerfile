FROM alpine:latest AS build
RUN apk add --no-cache alpine-sdk
RUN git clone https://github.com/weetmuts/wmbusmeters.git
WORKDIR /wmbusmeters
RUN make

FROM alpine:latest as scratch
ENV QEMU_EXECVE=1
RUN apk add --no-cache mosquitto-clients libstdc++
WORKDIR /wmbusmeters
COPY --from=build /wmbusmeters/build/wmbusmeters /wmbusmeters/wmbusmeters
RUN mkdir -p /wmbusmeters_data/logs/meter_readings && mkdir -p /wmbusmeters_data/etc/wmbusmeters.d/ && echo -e "loglevel=normal\ndevice=auto\nlistento=t1\nlogtelegrams=false\nformat=json\nmeterfiles=/wmbusmeters_data/logs/meter_readings\nmeterfilesaction=overwrite\nlogfile=/wmbusmeters_data/logs/wmbusmeters.log" > /wmbusmeters_data/etc/wmbusmeters.conf
VOLUME /wmbusmeters_data/
ENTRYPOINT ["/wmbusmeters/wmbusmeters", "--useconfig=/wmbusmeters_data/"]

