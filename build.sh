#!/bin/sh

cd libgphoto2_port && \
./configure "$@" && \
cd .. && \
./configure "$@" &&\
make

echo 
echo Now run \"make install\" to install gPhoto 2.0
echo
