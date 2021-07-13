#!/bin/bash
./bins/server /tmp/config.txt &
pid=$!
sleep 3
prefix="./bins/client -f /tmp/LSOSocket.sk -p -t 200"


$prefix -w ./tests -D ./replacementdir
sleep 1

kill -s SIGHUP $pid
wait $pid
