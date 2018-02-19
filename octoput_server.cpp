/*
	Version 1.0
*/
#include <ctime>
#include "octohelper.h"

int server_socket;
FILE* file;


void send_error_to_client(char* buf, int server_socket) {
	char file_err[80];
	strcat(file_err,"Could not open file ");
	strcat(file_err,buf);	
	printf("%s\n", buf);
		
	send_tcp_msg(a_socket,file_err);		
}

void set_port_and_family(struct sockaddr_in* addr) {
	memset ((char*) &(*addr),0,sizeof((*addr)));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(SERVER_PORT);	
	addr->sin_addr.s_addr = htonl(INADDR_ANY);

}

long get_file_size(FILE* file) {
	// get file size
	fseek(file,0,SEEK_END);
	long file_size = ftell(file);
	rewind(file);	
	return file_size;
}

int send_octolegs(FILE* file, int a_socket, int octoleg_len, int num_reads, struct sockaddr *client, unsigned int len) {
	int bytes_sent, bytes_recv;

	// add another bit to contain the sequence number
	char octolegs[OCTO_SIZE][octoleg_len+1];
	for (int i = 0; i < num_reads; i++) {
		// first byte of buf is seqnum
		// read in data from file into remaining bytes
		memset(octolegs[i],0,sizeof(octolegs[i]));	
		octolegs[i][0] = 1 << i;
		fread(&(octolegs[i][1]),sizeof(char),octoleg_len,file);

	}
	char ack;
	// TODO later: send Octolegs in different order
	// send octolegs in order of seqnum
	int total_bytes_sent = 0;
	// while (ack && 0xFF != 0xFF)
	for (int i = 0; i < OCTO_SIZE; i++) {
		bytes_recv = 0;
		char temp_ack;
		while(bytes_recv <= 0 || errno != 0) {
			errno = 0;
			bytes_sent = send_udp_msg(a_socket, octolegs[i], sizeof(octolegs[i]), client, len);
			printf("sent %d bytes\n", bytes_sent);
			
			//TODO: use ACK response to determine next octoleg to send
			bytes_recv = recv_udp_msg(a_socket, &temp_ack, 1, client, len);
			printf("recv %d bytes\n", bytes_recv);

		}
		printf("bytes recv: %d\n", bytes_recv);
		ack = ack | temp_ack;
		total_bytes_sent += bytes_sent;
	}

	return total_bytes_sent;
}

void send_octoblocks(FILE* file, long bytes_to_read, char* buf,int a_socket, struct sockaddr *client, unsigned int len) {
	int bytes_sent, bytes_recv;
	int octoleg_len = MAX_OCTOBLOCK_LEN;
	// make sure the size of the octoleg is at least 8 bytes
	while (octoleg_len >= MIN_LEG_BYTE) {

		// keep sending octolegs until remaining bytes of file to send are less than octoleg
		while (bytes_to_read > octoleg_len) {
			// TODO: create list of Octolegs
			bytes_sent = send_octolegs(file, a_socket, octoleg_len, OCTO_SIZE, client, len);

			// decrease bytes_to_read by how many bytes were sent
			printf("sent %d bytes\n", bytes_sent);
			bytes_to_read -= bytes_sent;			
		}

		// octoleg size becomes 1/8 of the remaining file bytes
		octoleg_len = bytes_to_read / OCTO_SIZE;
	}

	// send the remaining bytes in a padded buffer of 8 bytes
	
	if (bytes_to_read > 0) {
		bytes_sent = send_octolegs(file, a_socket, sizeof(char), bytes_to_read, client, len);
		printf("sent %d bytes\n", bytes_sent);

	}
	
}

void wait_for_client(int a_socket, struct sockaddr *client, unsigned int len) {
	char ack;
	int bytes_sent;
	recv_udp_msg(a_socket, &ack, 1, client, len);
	bytes_sent = send_udp_msg(a_socket, &ack, 1, client, len);
	
}


void my_handler(int s){
    close(server_socket);
    printf("%s\n", "Closed client socket");
    exit(0);
}

int main(int argc, char* argv[]){
	
	// setup safe exit on CTRL-C
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);	



	int status;
	struct sockaddr_in ip_server, ip_client;
	struct sockaddr *server, *client;
	unsigned int len = sizeof(ip_server);
	char buf[MAX_OCTOBLOCK_LEN];

	// setup server socket
	set_port_and_family(&ip_server);

	server = (struct sockaddr*) &ip_server;
	client = (struct sockaddr*) &ip_client;

	server_socket = setup_tcp_socket();
	bind_socket(server_socket,&ip_server);

	// listen for a connection
    status = listen(server_socket,5); // queuelen of 5
    if (status == -1) {
        perror("listen() call failed\n");
        exit(-1);
    }

    // accept a connection
    int client_sock;
    client_sock = accept(server_socket, NULL, NULL);
    if (client_sock < 0) {
        printf("Error in accept()\n");
        exit(-1);
    }

	long file_size;
	size_t result;

	// get file name from client
	memset(buf,0,sizeof(buf));	
	int read_bytes = recv_tcp_msg(client_sock, buf, MAX_OCTOBLOCK_LEN);	
    buf[read_bytes] = '\0';
	printf("Server received file name \"%s\" from client %s on port %d\n", 
		buf, inet_ntoa(ip_client.sin_addr), ntohs(ip_client.sin_port));		


	// open file if existing, else return an error message
	file = fopen(buf,"rb");
	if (file == NULL) {
		send_error_to_client(buf, server_socket);
		close(server_socket);
		return 0;
	}


	// get file size
	file_size = get_file_size(file);
    printf("File size %ld\n", file_size);


	memset(buf,0,sizeof(buf));
	sprintf(buf,"%ld",file_size);
	send_tcp_msg(client_sock, buf);

	// close TCP socket and open UDP socket
	close(server_socket);

	// setup server socket
	server_socket = setup_udp_socket();
	bind_socket(server_socket,&ip_server);

	wait_for_client(server_socket,client,len);
	printf("server %s on port %d\n", inet_ntoa(ip_server.sin_addr), ntohs(ip_server.sin_port));		
	printf("client %s on port %d\n", inet_ntoa(ip_client.sin_addr), ntohs(ip_client.sin_port));		

	// set timeout for socket
	set_socket_timeout(server_socket);
	// send octoblocks
	send_octoblocks(file, file_size, buf, server_socket, client, len);
	
	fclose(file);
	close(server_socket);


	return 0;


}























