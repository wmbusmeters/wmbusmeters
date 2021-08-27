ARG BUILD_FROM
FROM $BUILD_FROM AS build

ENV LANG C.UTF-8

RUN apk add --no-cache alpine-sdk gcc linux-headers ncurses-dev librtlsdr-dev cmake libusb-dev

ADD https://api.github.com/repos/weetmuts/wmbusmeters/git/refs/heads/master version.json
RUN git clone https://github.com/weetmuts/wmbusmeters.git && \
    git clone https://github.com/weetmuts/rtl-wmbus.git && \
    git clone https://github.com/merbanan/rtl_433.git
WORKDIR /wmbusmeters
RUN make
WORKDIR /rtl-wmbus
RUN make release && chmod 755 build/rtl_wmbus
WORKDIR /rtl_433
RUN mkdir build && cd build && cmake ../ && make

FROM $BUILD_FROM AS scratch
ENV QEMU_EXECVE=1
RUN apk add --no-cache mosquitto-clients libstdc++ curl libusb ncurses rtl-sdr
WORKDIR /wmbusmeters
COPY --from=build /wmbusmeters/build/wmbusmeters /wmbusmeters/wmbusmeters
COPY --from=build /rtl-wmbus/build/rtl_wmbus /usr/bin/rtl_wmbus
COPY --from=build /rtl_433/build/src/rtl_433 /usr/bin/rtl_433

COPY run.sh /
RUN chmod a+x /run.sh

CMD ["/run.sh"]