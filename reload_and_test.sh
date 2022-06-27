#!/bin/sh

make -C tests/ all
make -C src all
sudo rmmod faketee
sudo insmod src/faketee.ko
sudo ./tests/$1