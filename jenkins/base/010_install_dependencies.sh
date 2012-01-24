#!/bin/bash -xue

sudo aptitude update || true

for PKG in autoconf libtool pkg-config check; do
    sudo aptitude install -yVDq $PKG
done
