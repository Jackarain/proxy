CXX=g++
CFLAGS=-c -DDEBUG
INC=-I ./include/
LIB=-lboost_system -lboost_thread -lboost_date_time -lpthread

all:	ss

ss:	main.o
	$(CXX) main.o $(LIB) -o ss

main.o:	main.cpp
	$(CXX) $(CFLAGS) $(INC) main.cpp

clean:
	rm -rf *.o ss
