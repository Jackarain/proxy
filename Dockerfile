FROM alpine:edge AS builder

#RUN sed -i 's#https\?://dl-cdn.alpinelinux.org/alpine#https://mirror.nju.edu.cn/alpine#g' /etc/apk/repositories
RUN echo "https://dl-cdn.alpinelinux.org/alpine/edge/main" > /etc/apk/repositories && \
    echo "https://dl-cdn.alpinelinux.org/alpine/edge/community" >> /etc/apk/repositories

RUN apk add -u alpine-keys --allow-untrusted
RUN apk update
RUN apk upgrade
RUN apk add --no-cache bash git nasm yasm pkgconfig build-base clang cmake ninja linux-headers

ADD . /proxy

RUN cd /proxy && mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-static" -G Ninja && ninja
RUN cd /proxy && mkdir -p wolfssl && cd wolfssl && cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_USE_OPENSSL=OFF -DENABLE_USE_WOLFSSL=ON -DCMAKE_EXE_LINKER_FLAGS="-static" -G Ninja && ninja

FROM alpine:edge
COPY --from=builder /proxy/build/bin/proxy_server /usr/local/bin/
COPY --from=builder /proxy/wolfssl/bin/proxy_server /usr/local/bin/proxy_server-wolfssl

WORKDIR /root
ENTRYPOINT ["proxy_server"]
