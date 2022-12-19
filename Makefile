all: server client

server: Sender.o
	gcc -o server Sender.o
	
Sender.o: Sender.c
	gcc -c Sender.c
	
client: Receiver.o
	gcc -o client Receiver.o
	
Receiver.o: Receiver.c
	gcc -c Receiver.c
	
.PHONY: clean all

clean:
	rm -f *.o server client recv.txt
