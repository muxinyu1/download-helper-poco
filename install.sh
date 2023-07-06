#!/bin/bash
apt update
apt install libboost-all-dev
apt install libsqlite3-dev
apt install openssl libssl-dev

git clone https://github.com/fmtlib/fmt.git ~/fmt
cd ~/fmt
mkdir cmake-build
cd cmake-build
cmake ..
make install

git clone https://github.com/pocoproject/poco.git ~/poco
cd ~/poco
mkdir cmake-build
cd cmake-build
cmake ..
cmake --build . --target install --config Release
