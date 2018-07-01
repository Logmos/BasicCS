BIN =  server libfifth.so 

all: $(BIN)

server: server.c threadpool.c
	gcc $^ -rdynamic -o $@ -lpthread -ldl

libfifth.so: lib.c
	gcc -o $@ -fPIC -shared $^ 

clean:
	-rm -f $(BIN)  *.o *.out
