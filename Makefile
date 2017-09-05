all: server client

server :
	gcc -o server.exe server.c -lrt -lpthread

client:
	gcc -o client.exe client.c -lrt -lpthread
