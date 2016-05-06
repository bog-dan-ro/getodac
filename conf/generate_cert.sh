#!/bin/sh

openssl req -x509 -newkey rsa:2048 -passout pass:aaaa -keyout server.pem -out server.crt -days 768
openssl rsa  -passin pass:aaaa -in server.pem -out server.key
rm server.pem

