#!/bin/sh
# The Redis container entrypoint. Runs the server with the required
# flags and makes the socket accessible

set -e

chmod 777 /tmp/redis-socks

redis-server \
    --tls-port 6380 \
    --tls-cert-file /docker/tls/server.crt \
    --tls-key-file /docker/tls/server.key \
    --tls-ca-cert-file /docker/tls/ca.crt \
    --tls-auth-clients no \
    --unixsocket /tmp/redis-socks/redis.sock \
    --unixsocketperm 777
