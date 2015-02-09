all: mylib.so server
LD_FLAGS: -I../include -L../lib

server: server.c
	gcc -Wall -o server server.c -lm -ldl -ldirtree -L../lib

mylib.o: mylib.c
	gcc -Wall -fPIC -DPIC -c mylib.c

mylib.so: mylib.o
	ld -shared -o mylib.so mylib.o -ldl

clean:
	rm -f *.o *.so

