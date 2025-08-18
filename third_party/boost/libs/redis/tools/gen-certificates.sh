#!/bin/bash
# Copyright (c) 2025 Marcelo Zimbres Silva (mzimbres@gmail.com),
# Ruben Perez Hidalgo (rubenperez038 at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See
# accompanying file LICENSE.txt)
#

# Generates the ca and certificates used for CI testing.
# Run this in the directory where you want the certificates to be generated.

set -e

# CA private key
openssl genpkey -algorithm RSA -out ca.key -pkeyopt rsa_keygen_bits:2048

# CA certificate
openssl req -x509 -new -nodes -key ca.key -sha256 -days 20000 -out ca.crt \
  -subj '/C=ES/O=Boost.Redis CI CA/OU=IT/CN=boost-redis-ci-ca'

# Server private key
openssl genpkey -algorithm RSA -out server.key -pkeyopt rsa_keygen_bits:2048

# Server certificate
openssl req -new -key server.key -out server.csr \
  -subj '/C=ES/O=Boost.Redis CI CA/OU=IT/CN=redis'
openssl x509 -req -in server.csr -CA ca.crt -CAkey ca.key -CAcreateserial \
  -out server.crt -days 20000 -sha256
rm server.csr
rm ca.srl
