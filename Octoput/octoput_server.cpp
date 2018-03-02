/*
	Version 1.2
	Able to send out of order, retransmission and lost messages successfully

	Author: Anna Tran
	Affiliation: University of Calgary
	Student ID: 10128425
*/
#include <ctime>
#include "octohelper.h"

int server_socket;
FILE* file;


/*
	Set the port, family and address of an address struct
*/
void set_port_family_addr(struct sockaddr_in* addr) {
	memset ((char*) &(*addr),0,sizeof((*addr)));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(SERVER_PORT);	
	addr->sin_addr.s_addr = htonl(INADDR_ANY);

}


/*
	Get file size from a give file
*/
long get_file_size(FILE* file) {
	fseek(file,0,SEEK_END);
	long file_size = ftell(file);
	rewind(file);	
	return file_size;
}


/*
	Read octoblock length bytes, stored at 8 octolegs, from the file and send the data to the client
*/
int send_octolegs(FILE* file, char octoblock_seqnum, int octoleg_len, int num_reads, int a_socket, struct sockaddr *client, unsigned int len) {

    // extra byte to contain the sequence number of the octoblock
    // extra byte to contain the sequence number of the octoleg
	char octolegs[8][octoleg_len+2];

	// read from file and init octoleg data
	for (int i = 0; i < 8; i++) {
		memset(octolegs[i],' ',sizeof(octolegs[i]));	
		octolegs[i][0] = octoblock_seqnum;
		octolegs[i][1] = 1 << i;
		if (i < num_reads) {
			fread(&(octolegs[i][2]),sizeof(char),octoleg_len,file);			
		}
	}

	int total_bytes_sent = 0;
	struct sockaddr_in* ip_client;
	ip_client = (struct sockaddr_in*) client;
	int bytes_sent, bytes_recv;

	unsigned char ack = 0;
	unsigned char finished_ack = 1 << 7;

	printf("----- Start sending octolegs -----\n");
	// send (possibly duplicate) octolegs in random order
	int i = rand() % 8;
	// int i = 0;
	while (ack != finished_ack) {
		printf("sending seqnum %d\n", i);
		bytes_recv = 0;
		char higest_client_seq;		// highest seqnum received on client side

		bytes_sent = do_concurrent_send(octolegs[i], sizeof(octolegs[i]), &higest_client_seq, sizeof(higest_client_seq), a_socket, client, len);
		ack = (unsigned char) higest_client_seq;

		// i++;
		i = rand() % 8;
	}
	printf("----- Finished sending octolegs -----\n\n");


	return (bytes_sent-2)*8;
}


/*
	Read an array of data from the file and send the data in octoblocks to the client
*/
void send_octoblocks(FILE* file, long file_size, char* buf,int a_socket, struct sockaddr *client, unsigned int len) {
	int bytes_sent, bytes_recv;
	int octoblock_len = MAX_OCTOBLOCK_LEN;
	int octoleg_len = octoblock_len / 8;
	long bytes_to_read = file_size;

    // octoblock sequence number (between 0 and OCTOBLOCK_MAX_SEQNUM) to tell the client when the server 
    // has finished sending all octolegs of one octoblock and is moving to the next octoblock
	char octoblock_seqnum = 0;

	// make sure the size of the octoleg is at least 8 bytes
	while (octoleg_len >= MIN_LEG_BYTE) {
        printf("octoleg_len: %d\n", octoleg_len);

		// keep sending octolegs until remaining bytes of file to send are less than octoleg
		while (bytes_to_read >= octoblock_len) {
			bytes_sent = send_octolegs(file, octoblock_seqnum, octoleg_len, 8, a_socket, client, len);

			// decrease bytes_to_read by how many bytes were sent
			bytes_to_read -= bytes_sent;	
			printf("bytes to read = %ld\n", bytes_to_read);
			octoblock_seqnum = (octoblock_seqnum + 1) % OCTOBLOCK_MAX_SEQNUM;
		}


		// octoleg size becomes 1/8 of the remaining file bytes
		octoblock_len = bytes_to_read;
		octoleg_len = (octoblock_len / 8);
	}

	// send the remaining bytes in a padded buffer of 8 bytes
	octoleg_len = sizeof(char);
	if (bytes_to_read > 0) {	
		printf("leftover bytes to read: %ld\n", bytes_to_read);		
		bytes_sent = send_octolegs(file, octoblock_seqnum, octoleg_len, bytes_to_read, a_socket, client, len);

	}
	
}

/*
	Wait for client to send the first message, in order to get client info for sending
*/
void wait_for_client(int a_socket, struct sockaddr *client, unsigned int len) {
	char ack = 0;
	int bytes_sent = 0;
	int bytes_recv;
	bytes_recv = recv_udp_msg(a_socket, &ack, 1, client, len);

}

/*
    Closes the open sockets and file descriptors and exits the program
*/
void my_handler(int s){
    close(server_socket);
    printf("%s\n", "Closed client socket");
    exit(0);
}

int main(int argc, char* argv[]){
	
	// setup safe exit on CTRL-C
    struct sigaction sig_int_handler;

    sig_int_handler.sa_handler = my_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    sigaction(SIGINT, &sig_int_handler, NULL);



	int status;
	struct sockaddr_in ip_server, ip_client;
	struct sockaddr *server, *client;
	unsigned int len = sizeof(ip_server);
	char buf[MAX_OCTOBLOCK_LEN];

	// Setup TCP socket to receive file name and send file size
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
        perror("Error in accept()");
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


	// open file if existing, else exit the program
	file = fopen(buf,"rb");
	if (file == NULL) {
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


	// setup UDP socket
    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    set_port_family_addr(&server_address);

	server_socket = setup_udp_socket();
	bind_socket(server_socket,&server_address);

		wait_for_client(server_socket,client,len);
		printf("**** Starting file transfer ****\n");
		printf("Server %s on port %d\n", inet_ntoa(server_address.sin_addr), ntohs(server_address.sin_port));		
		printf("Client %s on port %d\n", inet_ntoa(ip_client.sin_addr), ntohs(ip_client.sin_port));		

		send_octoblocks(file, file_size, buf, server_socket, client, len);
		
		fclose(file);
		printf("**** Finished file transfer ****\n");
	close(server_socket);
	return 0;


}























