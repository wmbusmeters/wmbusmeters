FROM alpine AS build
RUN apk add --no-cache alpine-sdk gcc linux-headers librtlsdr-dev libxml2-dev cmake libusb-dev bash samurai
RUN git clone https://github.com/wmbusmeters/wmbusmeters.git && \
    git clone https://github.com/weetmuts/rtl-wmbus.git && \
    git clone https://github.com/merbanan/rtl_433.git
WORKDIR /wmbusmeters
RUN make
WORKDIR /rtl-wmbus
RUN make release && chmod 755 build/rtl_wmbus
WORKDIR /rtl_433
RUN cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=MinSizeRel && \
    cmake --build build

FROM alpine as scratch
RUN apk add --no-cache mosquitto-clients libstdc++ curl libusb rtl-sdr libxml2 netcat-openbsd
WORKDIR /wmbusmeters
COPY --from=build /wmbusmeters/build/wmbusmeters /wmbusmeters/wmbusmeters
COPY --from=build /rtl-wmbus/build/rtl_wmbus /usr/bin/rtl_wmbus
COPY --from=build /rtl_433/build/src/rtl_433 /usr/bin/rtl_433
COPY --from=build /wmbusmeters/docker/docker-entrypoint.sh /wmbusmeters/docker-entrypoint.sh
VOLUME /wmbusmeters_data/
CMD ["sh", "/wmbusmeters/docker-entrypoint.sh"]
