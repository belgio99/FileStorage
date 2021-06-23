#!/bin/bash

prefix="./bin/client -f /tmp/LSOSocket.txt -p -t 200"

$prefix -w ./bin

$prefix -w ./bin -D $(pwd)/filesRimpiazzati/

$prefix -w ./bin -D $(pwd)/replacedFiles2/
