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
COPY docker-entrypoint.sh /wmbusmeters/docker-entrypoint.sh
VOLUME /wmbusmeters_data/
CMD ["sh", "/wmbusmeters/docker-entrypoint.sh"]
