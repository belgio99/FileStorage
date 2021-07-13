#!/bin/bash
valgrind --leak-check=full ./bins/server /tmp/config.txt &
pid=$!

sleep 3s
prefix="./bins/client -f /tmp/LSOSocket.sk -p -t 200"

#stampa help
$prefix -h

#scrittura di 1 file

$prefix -W ./tests/test1.txt

#scrittura di più file separati da virgola

$prefix -W ./tests/test1.txt,./tests/test2.txt

#scrittura di N file in modo ricorsivo
$prefix -w ./tests/dirfile1,n=3

#append di una stringa in un file del server
$prefix -a test1.txt,provaappend

#chiusura (e dunque compressione) di un file del server
$prefix -C test2.txt

#apertura (e dunque decompressione) di un file del server
$prefix -O test2.txt

# lettura di 2 file dal server, con scrittura nella cartella dell'opzione -d
$prefix -r test1.txt,test2.txt -d ./replacementdir

# lock e unlock di file
$prefix -l test1.txt -u test1.txt

#rimozione di file dal server
$prefix -c test1.txt
sleep 1

kill -s SIGHUP $pid
wait $pid
