#!/bin/bash -xue

sudo aptitude update || true

for PKG in autoconf check; do
    sudo aptitude install -yVDq $PKG
done
