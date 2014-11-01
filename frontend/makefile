PG_INCLUDES=-I$(shell pg_config --includedir)
ifeq ($(PG_INCLUDES),-I)
	PG_INCLUDES=
endif


.PHONY: all ldebug ldebug-full debug clean

all:
	$(CC) -Wall -g $(PG_INCLUDES) garfield-pos.c -o garfield-pos -lpq

ldebug:
	valgrind --leak-check=full  ./garfield-pos -vvvv -f local.conf

ldebug-full:
	valgrind --leak-check=full  --track-origins=yes --show-reachable=yes ./garfield-pos -vvvv -f local.conf

debug:
	valgrind --leak-check=full --show-reachable=yes ./garfield-pos -vvvv -f pos.conf

clean:
	rm garfield-pos
