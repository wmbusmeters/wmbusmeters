FROM multiarch/alpine:amd64-latest-stable AS build
RUN apk add --no-cache alpine-sdk gcc linux-headers ncurses-dev
RUN git clone https://github.com/weetmuts/wmbusmeters.git && \
    git clone https://github.com/xaelsouth/rtl-wmbus.git
WORKDIR /wmbusmeters
RUN make
WORKDIR /rtl-wmbus
RUN make release && chmod 755 build/rtl_wmbus

FROM multiarch/alpine:amd64-latest-stable as scratch
ENV QEMU_EXECVE=1
RUN apk add --no-cache mosquitto-clients libstdc++ curl libusb rtl-sdr ncurses
WORKDIR /wmbusmeters
COPY --from=build /wmbusmeters/build/wmbusmeters /wmbusmeters/wmbusmeters
COPY --from=build /rtl-wmbus/build/rtl_wmbus /usr/bin/rtl_wmbus
COPY docker/docker-entrypoint.sh /wmbusmeters/docker-entrypoint.sh
VOLUME /wmbusmeters_data/
CMD ["sh", "/wmbusmeters/docker-entrypoint.sh"]
