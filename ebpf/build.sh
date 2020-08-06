#!/bin/bash

C_FLAGS="-g -ggdb -std=gnu89 -Winline -Wno-write-strings -Wreturn-type -fms-extensions"
L_FLAGS=""

SRC=`pwd`
mkdir -p build
rm -fr build/*
pushd build
#gcc $C_FLAGS -I. -o test test.c -L . -lbpf -lelf -lz
#gcc $C_FLAGS -I. -o sock_example sock_example.c -L . -lbpf -lelf -lz

LINUX_INCLUDE=-I/usr/include/x86_64-linux-gnu

#clang $LINUX_INCLUDE -O2 -Wall -target bpf -c xdp_example_drop.c -o xdp_example_drop.o

clang $LINUX_INCLUDE -O2 -Wall -target bpf -c $SRC/xdp_redirect_user_kern.c -o xdp_redirect_user_kern.o
clang -g -ggdb $LINUX_INCLUDE -O2 -Wall -c $SRC/bpf_load.c -o bpf_load.o
clang -g -ggdb -static $LINUX_INCLUDE -O2 -Wall $SRC/xdp_redirect_user.c -o xdp_redirect_user -L $SRC bpf_load.o -lbpf -lelf -lz

#clang $LINUX_INCLUDE -O2 -Wall -target bpf -c xdp_parse_kern.c -o xdp_parse_kern.o

clang $LINUX_INCLUDE -O2 -Wall -target bpf -c $SRC/count_packets_kern.c -o count_packets_kern.o
clang -g -ggdb -static $LINUX_INCLUDE -O2 -Wall $SRC/count_packets.c -o count_packets -L $SRC bpf_load.o -lbpf -lelf -lz
popd
