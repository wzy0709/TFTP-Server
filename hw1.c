// #include "/mnt/c/cs/network/unpv13e/lib/unp.h"
#include "unp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
// int numchild = 0;
struct DataPacket
{
	uint16_t opcode;
	uint16_t block;
	uint8_t data[512];
};
struct AkgPacket
{
	uint16_t opcode;
	uint16_t block;
};
struct ErrorPacket
{
	uint16_t opcode;
	uint16_t ErrorCode;
	uint8_t message[512];
};
// void checkchild()
// {
// 	int stat;
//     while(waitpid(-1, &stat, WNOHANG)>0){
//         // printf("Parent sees child PID %i has terminated.\n", pid);
//         // numchild--;
// 		continue;
//     }
// }
int DoRead(char* filename, int connectsd, struct sockaddr_in servaddr, socklen_t servlen){
	// file not exist
	if (access( filename, F_OK ) != 0)
	{
		// struct ErrorPacket ep;
		// ep.opcode = htons(5);
		// ep.ErrorCode = htons(0);
		// strcpy(ep.message,"File Not exist");
		// Sendto(connectsd, &ep, sizeof(ep), 0, (SA*)&servaddr, sizeof(servaddr));
		// Close(connectsd);
		// return 0;
		struct DataPacket dp;
		dp.opcode = htons(3);
		dp.block = htons(1);
		memcpy(dp.data, "", 1);
		Sendto(connectsd, &dp, 5, 0, (SA*)&servaddr, sizeof(servaddr));
		Close(connectsd);
		return 0;
	}
	fd_set rset;
	char* buf[516];
	FILE * file = Fopen(filename, "r");
	int filefd = fileno(file);
	char contents[512] = "";
	short block = 1;
	pid_t childs[MAXLINE];
	int nbytes;
	//start to send data
	while( (nbytes = Read(filefd, contents, sizeof(contents)))!=0 ){
		// send data
		struct DataPacket dp;
		dp.opcode = htons(3);
		dp.block = htons(block);
		memcpy(dp.data, contents, nbytes);
		//end of file
		if (nbytes<512)
		{
			dp.data[nbytes] = '\0';
		}
		int count = 0;
		while(1){
			//send data
			Sendto(connectsd, &dp, 4+nbytes, 0, (SA*)&servaddr, sizeof(servaddr));
			FD_ZERO(&rset);
			FD_SET(connectsd, &rset);
			struct timeval timeout;
			timeout.tv_sec = 1;
			timeout.tv_usec = 0;
			int nready = Select(connectsd+1, &rset, NULL, NULL, &timeout);
			//received ack
			if ((FD_ISSET(connectsd, &rset)))
			{
				struct AkgPacket ap;
				Recvfrom(connectsd, &ap, sizeof(ap), 0, (SA*)&servaddr, &servlen);;
				int index = ntohs(ap.block);
				int opcode = ntohs(ap.opcode);
				if (opcode==4 && index == block)
				{
					block++;
					break;
				}
				
			}
			else{
				count++;
				//exceed time
				if (count==10)
				{
					Close(filefd);
					Close(connectsd);
					return 0;
				}
				
			}
		}
	}
	Close(filefd);
	Close(connectsd);
	return 0;
}

int DoWrite(char* filename, int connectsd, struct sockaddr_in servaddr, socklen_t servlen){
	fd_set rset;
	char* buf[516];
	FILE * file = Fopen(filename, "w");
	int filefd = fileno(file);
	short block = 0;
	int index = 0;
	int NeedStop = 0;
	//start to write
	while(1){
		//send akg
		struct AkgPacket ap;
		ap.opcode = htons(4);
		ap.block = htons(block);
		Sendto(connectsd, &ap, 4, 0, (SA*)&servaddr, sizeof(servaddr));
		//stop
		if(NeedStop ==1)
			break;
		//receive
		FD_ZERO(&rset);
		FD_SET(connectsd, &rset);
		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		int nready = Select(connectsd+1, &rset, NULL, NULL, &timeout);
		//receive
		if (FD_ISSET(connectsd,&rset))
		{
			struct DataPacket dp;
			int nbytes = Recvfrom(connectsd, &dp, sizeof(dp), 0, (SA*)&servaddr, &servlen);
			if (ntohs(dp.opcode)==3)
			{
				block++;
				if (block != ntohs(dp.block))
				{
					continue;
				}
				//end of file
				if (nbytes<516){
					NeedStop = 1;
				}
				Write(filefd, dp.data, nbytes-4);
			}
		}
		//passed time
		else
			break;
	}
	Close(filefd);
	Close(connectsd);
	return 0;
}
int main(int argc, char const *argv[])
{
    //check arguments
    if(argc!=3){ perror("WRONG ARGUMENT NUMBERS");}
    int StartPortRange = atoi(argv[1]);
    int EndPortRange = atoi(argv[2]);
    //socket
    int listensd = Socket(AF_INET, SOCK_DGRAM, 0);

    //bind
    struct sockaddr_in	servaddr;
	socklen_t servlen = sizeof(servaddr);
    bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(StartPortRange);
    Bind(listensd, (SA *) &servaddr, sizeof(servaddr));
    //listen
    Getsockname(listensd, (SA *)&servaddr, &servlen);
    //initialize
    fd_set rset;
	
    while(1){
		FD_ZERO(&rset);
		FD_SET(listensd, &rset);
		struct timeval timeout;
		timeout.tv_sec = 10;
		timeout.tv_usec = 0;
		int nready = Select(listensd+1, &rset, NULL, NULL, &timeout);
		//check if wait too long
		if (FD_ISSET(listensd, &rset))
		{
			//read
			char buf[516];
			// short *intptr = (short*) buf;
			Recvfrom(listensd, &buf, sizeof(buf), 0, (SA*)&servaddr, &servlen);
			//get opcode
			short opcode = ntohs(*(short*)buf);
			//get filename
			char filename[512];
			strcpy(filename, buf+2);
			//can't process
			if (opcode!=1 && opcode!=2)
			{
				continue;
			}
			else{
				//connect
				struct sockaddr_in	connectaddr;
				socklen_t connectlen = sizeof(connectaddr);
				int connectsd;
				//able to connect
				if (EndPortRange>StartPortRange)
				{
					//socket
					connectsd = Socket(AF_INET, SOCK_DGRAM, 0);

					//bind
					bzero(&connectaddr, sizeof(connectaddr));
					connectaddr.sin_family      = AF_INET;
					connectaddr.sin_addr.s_addr = htonl(INADDR_ANY);
					connectaddr.sin_port = htons(EndPortRange);
					Bind(connectsd, (SA *) &connectaddr, sizeof(connectaddr));
					//getsockname
					Getsockname(connectsd, (SA *)&connectaddr, &connectlen);
					EndPortRange--;
				}
				else{
					perror("Too many connections\n");
					return EXIT_FAILURE;
				}
				// start to process
				pid_t pid = fork();
				//child
				if (pid==0)
				{
					// close listen end
					Close(listensd);
					//RRQ
					if (opcode == 1)
					{
						printf("Reading %s ...\n", filename);
						DoRead(filename, connectsd, servaddr, servlen);
						printf("Finished Reading %s\n", filename);
					}
					//WRQ
					else if(opcode == 2)
					{
						printf("Writing %s ...\n", filename);
						DoWrite(filename, connectsd, servaddr, servlen);
						printf("Finished Writing %s\n", filename);
					}
					exit(0);
				}
				//parent
				else{
					// numchild++;
					//close client end
					Close(connectsd);
					// Signal(SIGCHLD, checkchild);
					int stat;
					while(waitpid(-1, &stat, WNOHANG)>0){
						continue;
					}
					continue;
				}
			}
		}
		//end
		else{
			kill(0, SIGKILL);
			return 0;
		}

	}
    
}
