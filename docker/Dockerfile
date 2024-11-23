FROM alpine AS build
RUN apk add --no-cache alpine-sdk gcc linux-headers libxml2-dev cmake libusb-dev bash samurai

ADD https://api.github.com/repos/wmbusmeters/wmbusmeters/git/refs/heads/master version.json
RUN git clone --depth 1 https://github.com/steve-m/librtlsdr.git && \
    git clone --depth 1 https://github.com/wmbusmeters/wmbusmeters.git && \
    git clone --depth 1 https://github.com/weetmuts/rtl-wmbus.git && \
    git clone --depth 1 https://github.com/merbanan/rtl_433.git && \
    git clone --depth 1 https://github.com/ED6E0F17/rtl_reset.git && \
    git clone --depth 1 https://github.com/libxmq/xmq/xmq.git

WORKDIR /librtlsdr
RUN cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DCMAKE_INSTALL_PREFIX:PATH=/usr \
        -DDETACH_KERNEL_DRIVER=ON \
        -Wno-dev && \
    cmake --build build && \
    cmake --install build
WORKDIR /wmbusmeters
RUN make
WORKDIR /rtl-wmbus
RUN make release && chmod 755 build/rtl_wmbus
WORKDIR /rtl_433
RUN cmake -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=MinSizeRel && \
    cmake --build build
WORKDIR /rtl_reset
RUN make
WORKDIR /xmq
RUN ./configure && make

FROM alpine as scratch
RUN apk add --no-cache mosquitto-clients libstdc++ curl libusb libxml2 netcat-openbsd
WORKDIR /wmbusmeters
COPY --from=build /librtlsdr/build/src/librtlsdr.so.* /usr/lib/
COPY --from=build /librtlsdr/rtl-sdr.rules /usr/lib/udev/rules.d/rtl-sdr.rules
COPY --from=build /librtlsdr/build/src/rtl_* /usr/bin/
COPY --from=build /wmbusmeters/build/wmbusmeters /wmbusmeters/wmbusmeters
COPY --from=build /rtl-wmbus/build/rtl_wmbus /usr/bin/rtl_wmbus
COPY --from=build /rtl_433/build/src/rtl_433 /usr/bin/rtl_433
COPY --from=build /rtl_reset/rtl_reset /usr/bin/rtl_reset
COPY --from=build /xmq/build/default/release/xmq /usr/bin/xmq
COPY --from=build /wmbusmeters/docker/docker-entrypoint.sh /wmbusmeters/docker-entrypoint.sh
VOLUME /wmbusmeters_data/
CMD ["sh", "/wmbusmeters/docker-entrypoint.sh"]
