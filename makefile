CC          = gcc
CFLAGS		= -pedantic -std=c99 -Wall -g
SHELL           = /bin/bash
EXTRAFLAGS  =  -lpthread
OBJ			= bins/
SRC			= src/
INCL			= include/
HEAD			= header/
LDFLAGS 		= -L.
OPTFLAGS		= -O3
TARGETS		= $(OBJ)server \
				$(OBJ)client




.PHONY: all clean cleanall test1 test2
.SUFFIXES: .c .h



all: $(TARGETS)


$(OBJ)client: $(OBJ)api.o $(OBJ)client.o $(OBJ)utils.o
	$(CC) $(CFLAGS) -I $(HEAD) $(OPTFLAGS) -o $@ $^ $(EXTRAFLAGS)

$(OBJ)server: $(OBJ)server.o $(OBJ)icl_hash.o $(OBJ)linkedlist.o $(OBJ)utils.o $(OBJ)shoco.o
	$(CC) $(CFLAGS) -I $(HEAD) $(OPTFLAGS) -o $@ $^ $(EXTRAFLAGS)

$(OBJ)server.o: $(SRC)server.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $< -c -o $@

$(OBJ)client.o: $(SRC)client.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $< -c -o $@

$(OBJ)api.o: $(SRC)api.c $(INCL)utils.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $< -c -o $@

$(OBJ)icl_hash.o: $(INCL)icl_hash.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $< -c -o $@

$(OBJ)linkedlist.o: $(INCL)linkedlist.c
	$(CC) $(CFLAGS) $(OPTFLAGS) $< -c -o $@

$(OBJ)utils.o: $(INCL)utils.c
	$(CC) $(CFLAGS) -Wno-pointer-arith $(OPTFLAGS) $< -c -o $@

$(OBJ)shoco.o: $(INCL)shoco.c
	$(CC) $(CFLAGS) -Wno-overflow $(OPTFLAGS) $< -c -o $@


clean		:
	rm -f $(TARGETS)
cleanall	: clean
	rm -f ./bins/*.o *~ *.a /tmp/config1.txt /tmp/config2.txt ./bins/serverLog.log
cponly :
	cp ./configs/config1.txt /tmp/config.txt

	
test1	: all
	rm -f ./serverLog.log
	cp ./configs/config1.txt /tmp/config.txt
	bash ./scripts/test1.sh;

test2	: all
	rm -f ./serverLog.log
	cp ./configs/config2.txt /tmp/config.txt
	bash ./scripts/test2.sh;
