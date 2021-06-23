#!/bin/bash
#prefix="valgrind --leak-check=full ./bin/client -f ./sock -p -t 200"
prefix="./bin/client -f /tmp/LSOSocket.txt -p -t 200"

#stampa help
$prefix -h

#scrittura di n file all'interno una directory con altre directory all'interno
$prefix -w ./bin/,4

#scrittura di tutti i file all'interno una directory con altre directory all'interno
$prefix -w ./bin/


$prefix -w ./bin/ -D $(pwd)/replacedbin1/


#scrittura di file separati da virgola
$prefix -W ./bin/text1.txt,$./bin2/prova2,$./bin2/prova3 -D $./replacedbin2/

#lettura di un file e scrittura in una cartella su disco
$prefix -d ./tests -r $(pwd)/bin/prova1.txt

#lettura di tutti i file del server
$prefix -d ./text/ -R

#lettura di N file del server
$prefix -d ./read_dir3/ -R 4

#applicazione lock a un file
$prefix -l $(pwd)/bin/text1.txt

# unlock e remove a un file
$prefix -u $(pwd)/bin/prova1 -c $(pwd)/bin/prova1


