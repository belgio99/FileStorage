SHELL           = /bin/bash
TARGETS		= $(OBJ)server \
				$(OBJ)client


.PHONY: all clean cleanall test1 test2
.SUFFIXES: .c .h


all: $(TARGETS)


$(OBJ)client:
	gcc -pedantic -Wall -g -I ./header -o binary/client src/client.c include/icl_hash.c include/utils.c include/linkedlist.c src/api.c -lpthread

$(OBJ)server:
	gcc -pedantic -Wall -g -I ./header -o binary/server src/server.c include/icl_hash.c include/utils.c include/linkedlist.c -lpthread



clean		:
	rm -f $(TARGETS)
cleanall	: clean
	\ rm -f ./bin/*.o *~ *.a ./tmp/config.txt /tmp/LSOSocket.txt

test1	:
	cp config.txt /tmp/config.txt
	cp LSOSocket.txt /tmp/LSOSocket.txt
	valgrind --leak-check=full ./binary/server /tmp/config.txt &
	sleep 3s
	bash ./scripts/test1.sh; kill -1 $$!

test2	:
	cp config.txt /tmp/config.txt
	cp LSOSocket.txt /tmp/LSOSocket.txt
	./binary/server /tmp/config.txt &
	sleep 3s
	bash ./scripts/test2.sh; kill -1 $$!
