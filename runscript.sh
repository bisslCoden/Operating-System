#!/bin/zsh
mkdir -p /tmp/sweb;
cd /tmp/sweb;
cmake ~/Uni/OS/real/bsw22l1;
make;
make qemu;

