default: server

server: hw1.c
	gcc hw1.c libunp.a -o tftp.out

clean:
	rm -f *.o *.out
