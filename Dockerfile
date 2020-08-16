FROM alpine:3.12 AS build
ARG AGONES

RUN apk add --update alpine-sdk cmake \
                     qt5-qtbase qt5-qttools-dev

# Build Agones, if it was set as an argument
RUN test -z "$AGONES" || wget -o agones.tgz https://github.com/googleforgames/agones/archive/v1.7.0.tar.gz
RUN test -z "$AGONES" || tar xvf v1.7.0.tar.gz agones-1.7.0/sdks/cpp
WORKDIR /agones-1.7.0/sdks/cpp
RUN test -z "$AGONES" || make
RUN test -z "$AGONES" || make install

COPY . /src/
WORKDIR /src

RUN qmake-qt5 CONFIG="${agones:+agones} headless nosound noupcasename" DEFINES="HAVE_STDINT_H"
RUN make

FROM alpine:3.12

RUN apk add --update qt5-qtbase \
                     util-linux \
 && rm -rf /var/cache/apk/*

COPY --from=build /src/jamulus /usr/bin/

EXPOSE 22124/udp

ENTRYPOINT nice -n 20 /usr/bin/jamulus -s
