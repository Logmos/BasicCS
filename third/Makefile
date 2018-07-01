CC = gcc
CXX = g++
INC = -I./ 
LIB = -lstdc++
CFLAGS = -pthread -g -Wall

BIN = client server
all: $(BIN)


client: client.o
	$(CC) -o $@ $^ $(LIB) $(CFLAGS)

server: server.o
	$(CC) -o $@ $^ $(LIB) $(CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(INC) $(CFLAGS)

%.o: %.cpp
	$(CXX) -c -o $@ $< $(INC) $(CFLAGS)

clean:
	rm -rf *.gch  *.o
