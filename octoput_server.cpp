/*
	Version 1.0
	Working on localhost to transfer sequential octolegs
*/
#include <ctime>
#include "octohelper.h"

int server_socket;
FILE* file;


void send_error_to_client(int server_socket, char* buf) {
	char file_err[80];
	strcat(file_err,"Could not open file ");
	strcat(file_err,buf);	
	printf("%s\n", buf);
		
	send_tcp_msg(server_socket,file_err);		
}

void set_port_family_addr(struct sockaddr_in* addr) {
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

void update_ack(unsigned char* ack, unsigned char higest_client_seq) {
	while (higest_client_seq != 0) {
		(*ack) = (*ack) | higest_client_seq;
		higest_client_seq >>= 1;
	}
}


int send_octolegs(FILE* file, int a_socket, int octoleg_len, int num_reads, struct sockaddr *client, unsigned int len) {

	// add another bit to contain the sequence number
	char octolegs[8][octoleg_len+1];

	for (int i = 0; i < 8; i++) {
		// first byte of buf is seqnum
		memset(octolegs[i],0,sizeof(octolegs[i]));	
		octolegs[i][0] = 1 << i;
		if (i < num_reads) {
			fread(&(octolegs[i][1]),sizeof(char),octoleg_len,file);			
		}
	}
	// TODO later: send Octolegs in different order
	// send octolegs in order of seqnum
	int total_bytes_sent = 0;
	struct sockaddr_in* ip_client;
	ip_client = (struct sockaddr_in*) client;
	int bytes_sent, bytes_recv;


	unsigned char ack = 0;
	unsigned char finished_ack = 1 << 7;
	int i = rand() % 8;
	while (ack != finished_ack) {
		printf("sending seqnum %d\n", i);
		bytes_recv = 0;
		char higest_client_seq;		// highest seqnum received on client side

		bytes_sent = do_concurrent_send(octolegs[i], sizeof(octolegs[i]), &higest_client_seq, sizeof(higest_client_seq), a_socket, client, len);
		ack = (unsigned char) higest_client_seq;
		printf("highest sent bit %u\n", ack);

		i = rand() % 8;

	}


	return (bytes_sent-1)*8;
}

void send_octoblocks(FILE* file, long file_size, char* buf,int a_socket, struct sockaddr *client, unsigned int len) {
	int bytes_sent, bytes_recv;
	int octoblock_len = MAX_OCTOBLOCK_LEN;
	int octoleg_len = octoblock_len / 8;
	long bytes_to_read = file_size;


	// make sure the size of the octoleg is at least 8 bytes
	while (octoleg_len >= MIN_LEG_BYTE) {
        printf("octoleg_len: %d\n", octoleg_len);

		// keep sending octolegs until remaining bytes of file to send are less than octoleg
		while (bytes_to_read >= octoblock_len) {
			bytes_sent = send_octolegs(file, a_socket, octoleg_len, 8, client, len);
			printf("bytes_sent = %d\n", bytes_sent);

			// decrease bytes_to_read by how many bytes were sent
			bytes_to_read = bytes_to_read - bytes_sent;	
			printf("bytes to read = %ld\n", bytes_to_read);

		}


		// octoleg size becomes 1/8 of the remaining file bytes
		octoblock_len = bytes_to_read;
		octoleg_len = (octoblock_len / 8);
	}

	// send the remaining bytes in a padded buffer of 8 bytes
	octoleg_len = sizeof(char);
	if (bytes_to_read > 0) {	
		printf("leftover bytes to read: %ld\n", bytes_to_read);		
		bytes_sent = send_octolegs(file, a_socket, octoleg_len, bytes_to_read, client, len);

	}
	
}

void wait_for_client(int a_socket, struct sockaddr *client, unsigned int len) {
	char ack = 0;
	int bytes_sent = 0;
	int bytes_recv;
	bytes_recv = recv_udp_msg(a_socket, &ack, 1, client, len);

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
	set_port_family_addr(&ip_server);

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
		send_error_to_client(server_socket, buf);
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
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    set_port_family_addr(&server_address);

	server_socket = setup_udp_socket();
	bind_socket(server_socket,&server_address);

	wait_for_client(server_socket,client,len);
	printf("server %s on port %d\n", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));		
	printf("client %s on port %d\n", inet_ntoa(ip_client.sin_addr), ntohs(ip_client.sin_port));		

	// send octoblocks
	send_octoblocks(file, file_size, buf, server_socket, client, len);
	
	fclose(file);
	close(server_socket);


	return 0;


}























