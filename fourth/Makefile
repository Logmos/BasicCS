BIN =  server client

all: $(BIN)

server: server.c threadpool.c
	gcc $^ -o $@ -lpthread
client: client.c
	gcc $^ -o $@ -lpthread

clean:
	-rm -f $(BIN)  *.o *.out
