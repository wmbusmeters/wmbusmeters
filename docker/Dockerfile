FROM multiarch/alpine:${TARGETARCH}${TARGETVARIANT}-latest-stable AS build
RUN apk add --no-cache alpine-sdk gcc linux-headers librtlsdr-dev cmake libusb-dev bash
RUN git clone https://github.com/wmbusmeters/wmbusmeters.git && \
    git clone https://github.com/weetmuts/rtl-wmbus.git && \
    git clone https://github.com/merbanan/rtl_433.git && \
    git clone https://github.com/ED6E0F17/rtl_reset.git
WORKDIR /wmbusmeters
RUN make
WORKDIR /rtl-wmbus
RUN make release && chmod 755 build/rtl_wmbus
WORKDIR /rtl_433
RUN mkdir build && cd build && cmake ../ && make
WORKDIR /rtl_reset
RUN make

FROM multiarch/alpine:${TARGETARCH}${TARGETVARIANT}-latest-stable as scratch
ENV QEMU_EXECVE=1
RUN apk add --no-cache mosquitto-clients libstdc++ curl libusb rtl-sdr netcat-openbsd
WORKDIR /wmbusmeters
COPY --from=build /wmbusmeters/build/wmbusmeters /wmbusmeters/wmbusmeters
COPY --from=build /rtl-wmbus/build/rtl_wmbus /usr/bin/rtl_wmbus
COPY --from=build /rtl_433/build/src/rtl_433 /usr/bin/rtl_433
COPY --from=build /rtl_reset/rtl_reset /usr/bin/rtl_reset
COPY --from=build /wmbusmeters/docker/docker-entrypoint.sh /wmbusmeters/docker-entrypoint.sh
VOLUME /wmbusmeters_data/
CMD ["sh", "/wmbusmeters/docker-entrypoint.sh"]