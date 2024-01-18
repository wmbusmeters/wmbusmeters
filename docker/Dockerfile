FROM asymworks/multiarch-alpine:${TARGETARCH}${TARGETVARIANT}-latest-stable AS build
RUN apk add --no-cache alpine-sdk gcc linux-headers librtlsdr-dev libxml2-dev cmake libusb-dev bash
RUN git clone https://github.com/wmbusmeters/wmbusmeters.git && \
    git clone https://github.com/weetmuts/rtl-wmbus.git
WORKDIR /wmbusmeters
RUN make
WORKDIR /rtl-wmbus
RUN make release && chmod 755 build/rtl_wmbus

FROM asymworks/multiarch-alpine:${TARGETARCH}${TARGETVARIANT}-latest-stable as scratch
ENV QEMU_EXECVE=1
RUN apk add --no-cache mosquitto-clients libstdc++ curl libusb rtl-sdr libxml2 netcat-openbsd rtl_433
WORKDIR /wmbusmeters
COPY --from=build /wmbusmeters/build/wmbusmeters /wmbusmeters/wmbusmeters
COPY --from=build /rtl-wmbus/build/rtl_wmbus /usr/bin/rtl_wmbus
COPY --from=build /wmbusmeters/docker/docker-entrypoint.sh /wmbusmeters/docker-entrypoint.sh
VOLUME /wmbusmeters_data/
CMD ["sh", "/wmbusmeters/docker-entrypoint.sh"]
