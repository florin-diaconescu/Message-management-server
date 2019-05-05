# Protocoale de comunicatii
# Laborator 7 - TCP
# Echo Server
# Makefile

CFLAGS = -Wall -g

# Portul pe care asculta serverul
PORT = 

# Adresa IP a serverului
IP_SERVER = 

all: server subscriber 

# Compileaza server.c
server: server.cpp

# Compileaza client.c
subscriber: subscriber.cpp

.PHONY: clean

clean:
	rm -f server client
