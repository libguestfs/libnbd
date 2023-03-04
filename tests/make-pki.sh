#!/usr/bin/env bash
# libnbd
# Copyright Red Hat
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.
#
# * Neither the name of Red Hat nor the names of its contributors may be
# used to endorse or promote products derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY RED HAT AND CONTRIBUTORS ''AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
# PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RED HAT OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
# USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
# OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

set -e

# This creates the PKI files for the TLS tests.

if [ -z "$SRCDIR" ] || [ ! -f "$SRCDIR/connect-tls.c" ]; then
    echo "$0: script is being run from the wrong directory."
    echo "Don't try to run this script by hand."
    exit 1
fi

if [ -z "$CERTTOOL" ]; then
    echo "$0: \$CERTTOOL was not set."
    echo "Don't try to run this script by hand."
    exit 1
fi

if [ -z "$1" ]; then
    echo "$0: Output directory not set."
    echo "Don't try to run this script by hand."
    exit 1
fi

mkdir -p $1

# Create the CA.
$CERTTOOL --generate-privkey > $1/ca-key.pem
chmod 0600 $1/ca-key.pem

cat > $1/ca.info <<EOF
cn = Test
ca
cert_signing_key
EOF
$CERTTOOL --generate-self-signed \
          --load-privkey $1/ca-key.pem \
          --template $1/ca.info \
          --outfile $1/ca-cert.pem

# Create the server certificate and key.
$CERTTOOL --generate-privkey > $1/server-key.pem
chmod 0600 $1/server-key.pem

cat > $1/server.info <<EOF
organization = Test
cn = localhost
dns_name = localhost
ip_address = 127.0.0.1
ip_address = ::1
tls_www_server
encryption_key
signing_key
EOF
$CERTTOOL --generate-certificate \
          --load-ca-certificate $1/ca-cert.pem \
          --load-ca-privkey $1/ca-key.pem \
          --load-privkey $1/server-key.pem \
          --template $1/server.info \
          --outfile $1/server-cert.pem

# Create a client certificate and key.
$CERTTOOL --generate-privkey > $1/client-key.pem
chmod 0600 $1/client-key.pem

cat > $1/client.info <<EOF
country = US
state = New York
locality = New York
organization = Test
cn = localhost
tls_www_client
encryption_key
signing_key
EOF
$CERTTOOL --generate-certificate \
          --load-ca-certificate $1/ca-cert.pem \
          --load-ca-privkey $1/ca-key.pem \
          --load-privkey $1/client-key.pem \
          --template $1/client.info \
          --outfile $1/client-cert.pem
