.phony all:
all: trainsRecovered

trainsRecovered: trainsRecovered.c
	gcc -pthread -lreadline -o trainsRecovered trainsRecovered.c

.PHONY clean:
clean:
	-rm -rf *.o *.exe
