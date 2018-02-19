/* Simple program to demonstrate a basic UDP client.
 * Reads commands from stdin and sends them to the server. 
 * Requests random nouns, adjectives, and verbs as you wish.
 * A "quit" message from the client kills the server gracefully.
 * 
 * Compile using "gcc -o wordclient wordclient.c"
 */
 
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_BUF_LEN 512

/* Hardcode the IP address of the server (local or remote) */
/* #define SERVER_IP "127.0.0.1"   /* loopback interface */
#define SERVER_IP "136.159.5.22"  /* rsx1.cpsc.ucalgary.ca */

/* Edit as needed to match port of server */
#define SERVER_PORT 8431
    
int main(void)
  {
    struct sockaddr_in si_server;
    struct sockaddr *server;
    int s, i, len = sizeof(si_server);
    char buf[MAX_BUF_LEN];
    int readBytes;
    int quit;

    if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
      {
	printf("Could not setup a socket!\n");
	return 1;
      }
      
    memset((char *) &si_server, 0, sizeof(si_server));
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(SERVER_PORT);
    server = (struct sockaddr *) &si_server;

    if (inet_pton(AF_INET, SERVER_IP, &si_server.sin_addr)==0)
      {
	printf("inet_pton() failed\n");
	return 1;
      }

    printf("This is the client side of the wordserver demo...\n");
    printf("Your choices are 'noun', 'adjective', 'verb', or 'quit'\n");

    quit = 0;
    while(!quit)
      {
	printf("Enter a command: ");
	scanf("%s", buf);

	if(strncmp(buf, "quit", 4) == 0)
	  quit = 1;
		
	if (sendto(s, buf, strlen(buf), 0, server, sizeof(si_server)) == -1)
	  {
	    printf("sendto failed\n");
	    return 1;
	  }

	if ((readBytes=recvfrom(s, &buf, MAX_BUF_LEN, 0, server, &len))==-1)
	  {
	    printf("Read error!\n");
	    quit = 1;
	  }
	buf[readBytes] = '\0'; // padding with end of string symbol
	printf("Word received from server %s on port %d is \"%s\"\n\n",
	       inet_ntoa(si_server.sin_addr), ntohs(si_server.sin_port), buf);
      }
    close(s);
    return 0;
  }
