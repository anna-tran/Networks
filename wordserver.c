/* Simple program to demonstrate a UDP-based server.
 * Binds to a random port between 8000 and 8999.
 * Receives a message from the client. 
 * If it receives a "noun" message, it sends a random noun back.
 * If it receives a "adj" message, it sends a random adjective back.
 * If it receives a "verb" message, it sends a random verb back.
 * If you send an unknown command, it sends a random insult back.
 * A "quit" message from client kills the server gracefully.
 * 
 * Compile using "gcc -o wordserver wordserver.c"
 */
 
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#define MAX_BUF_LEN 512
#define PORTRANGE_MIN 8000
#define MAX_NOUNS 5
#define MAX_ADJS 8
#define MAX_VERBS 6
#define MAX_DUMDUMS 3
#define MAX_LENGTH 10

int main(int argc, char *argv[])
  {
    struct sockaddr_in si_server, si_client;
    struct sockaddr *server, *client;
    unsigned int s, i, len=sizeof(si_server);
    int port;					// Port number
    char buf[MAX_BUF_LEN];
    char tosend[10];
    int readBytes;
    int choice;
    char nounlist[MAX_NOUNS][MAX_LENGTH] = { "dog", "horse", "lamb", "pony", "skunk" };
    char adjlist[MAX_ADJS][MAX_LENGTH] = { "big", "fat", "huge", "lazy", "melodious", "pretty", "stinky", "yucky" };
    char verblist[MAX_VERBS][MAX_LENGTH] = { "ate", "endured", "jumped", "ran", "sat", "puked" };
    char dummylist[MAX_DUMDUMS][MAX_LENGTH] = { "dummy", "idiot", "moron" };

    printf("Generating random input port!\n");
    srandom(time(NULL));
    port = random()%1000 + PORTRANGE_MIN;
    printf("Word server listening on port %d\n", port);

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      {
	printf("Could not setup a socket!\n");
	return 1;
      }
    
    memset((char *) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(port);
    si_server.sin_addr.s_addr = htonl(INADDR_ANY);
    server = (struct sockaddr *) &si_server;
    client = (struct sockaddr *) &si_client;
    if (bind(s, server, sizeof(si_server))==-1)
      {
	printf("Could not bind to port %d!\n", port);
	return 1;
      }
	
    int quit = 0;
    while (!quit)
      {
	if ((readBytes=recvfrom(s, &buf, MAX_BUF_LEN, 0, client, &len))==-1)
	  {
	    printf("Read error!\n");
	    quit = 1;
	  }
	buf[readBytes] = '\0'; // padding with end of string symbol

	printf("  Server received command \"%s\" from client %s on port %d\n",
	       buf, inet_ntoa(si_client.sin_addr), ntohs(si_client.sin_port));
	
	if(strncmp(buf, "quit", 4) == 0)
	    quit = 1;
	
	if( quit == 1 )
	  sprintf(tosend, "%s", "OK");
	else if(strncmp(buf, "noun", 4) == 0)
	  {
	    choice = random()%MAX_NOUNS;
	    sprintf(tosend, "%s", nounlist[choice]);
	  }
	else if(strncmp(buf, "adj", 3) == 0)
	  {
	    choice = random()%MAX_ADJS;
	    sprintf(tosend, "%s", adjlist[choice]);
	  }
	else if(strncmp(buf, "verb", 4) == 0)
	  {
	    choice = random()%MAX_VERBS;
	    sprintf(tosend, "%s", verblist[choice]);
	  }
	else
	  {
	    choice = random()%MAX_DUMDUMS;
	    sprintf(tosend, "%s", dummylist[choice]);
	  }

	printf("    Sending back \"%s\" as a response\n", tosend);
	
	sendto(s, tosend, strlen(tosend), 0, client, len);		
      }
	printf("server %s on port %d\n", inet_ntoa(si_server.sin_addr), ntohs(si_server.sin_port));		
	printf("client %s on port %d\n", inet_ntoa(si_client.sin_addr), ntohs(si_client.sin_port));		


    close(s);
    return 0;
  }
