CC=g++
CFLAGS=-c -DDEBUG
INC=-I ./include/
LIB=-lboost_system -lboost_thread -lboost_date_time -lpthread

all:	sss

sss:	main.o
	$(CC) main.o $(LIB) -o sss

main.o:	main.cpp
	$(CC) $(CFLAGS) $(INC) main.cpp

clean:
	rm -rf *.o sss
