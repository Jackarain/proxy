FROM alpine:latest as builder

RUN apk update
RUN apk upgrade
RUN apk add --no-cache fortify-headers bsd-compat-headers libgphobos libgomp libatomic binutils bash build-base make gcc musl-dev cmake ninja g++ linux-headers git bison elfutils-dev libcap-dev flex iptables-dev

ADD . /proxy

RUN cd /proxy && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static" -G Ninja && ninja

FROM alpine:latest

RUN apk add --no-cache ca-certificates bash

COPY --from=builder /proxy/build/bin/proxy_server /usr/local/bin/

WORKDIR /root
ENTRYPOINT ["proxy_server"]
