/*
	Version 1.0
*/


#include "octohelper.h"

int server_socket;
FILE* file;

void set_port_and_family(struct sockaddr_in* addr) {
    memset ((char*) &(*addr),0,sizeof((*addr)));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(SERVER_PORT);   
}

int get_seqnum(char identifier) {
    int seqnum = 0;
    while ((identifier && (1 << seqnum)) == 0) {
        seqnum++;
    }
    return seqnum;
}

char get_highest_bit(char ack) {
    int seqnum = 0;
    while ((ack && (1 << seqnum)) > 0) {
        seqnum++;
    }
    return 1 << seqnum;
}

void recv_octolegs(FILE* file, char* octoblock, int octoleg_len, int a_socket, struct sockaddr *server, unsigned int len) {
    int bytes_sent, bytes_recv;

    // add another bit to contain the sequence number
    char octolegs[OCTO_SIZE][octoleg_len+1];
    char ack;
    
    char buf[octoleg_len+1];
    printf("have bufs\n");
    // loop until all segments of message have been received
    while ((ack & 0xFF) != 0xFF) {
        // TODO: wait for server response
        printf("waiting for server response\n");

        bytes_recv = recv_udp_msg(a_socket, buf, sizeof(buf), server, len);
        char identifier = buf[0];
        int seqnum = get_seqnum(identifier);

        // copy over contents of buffer only if the response message hadn't been received yet
        if (ack != (ack | identifier)) {
            strcpy(octolegs[seqnum],buf);
            ack = ack | identifier;

        }

        // TODO: send cumulative ACK         
        char highest_bit = get_highest_bit(ack);
        bytes_sent = send_udp_msg(a_socket, &highest_bit, 1, server, len);

  
    }

    // TODO: assemble octolegs into octoblock
    strcpy(octoblock,&(octolegs[0][1]));
    for (int i = 1; i < OCTO_SIZE; i++) {
        strcat(octoblock,(&octolegs[i][1]));
    }

}



void recv_octoblocks(FILE* file, long bytes_to_write, int a_socket, struct sockaddr *server, unsigned int len) {
    int bytes_written;
    int octoleg_len = MAX_OCTOBLOCK_LEN;
    // make sure the size of the octoleg is at least 8 bytes
    while (octoleg_len >= MIN_LEG_BYTE) {
        printf("octoleg_len: %d\n", octoleg_len);

        // keep sending octolegs until remaining bytes of file to send are less than octoleg
        while (bytes_to_write > octoleg_len) {
            printf("bytes to write %ld\n", bytes_to_write);
            char octoblock[octoleg_len * OCTO_SIZE];
            recv_octolegs(file, octoblock, octoleg_len, a_socket, server, len);
            bytes_written = fwrite(octoblock,sizeof(char),sizeof(octoblock),file);
            bytes_to_write -= bytes_written;
        }

        // octoleg size becomes 1/8 of the remaining file bytes
        octoleg_len = (bytes_to_write / OCTO_SIZE) + 1;
    }

    // send the remaining bytes in a padded buffer of 8 bytes
    
    if (bytes_to_write > 0) {
        char octoblock[octoleg_len * OCTO_SIZE];
        recv_octolegs(file, octoblock, sizeof(char), a_socket, server, len);
        bytes_written = fwrite(octoblock,sizeof(char),bytes_to_write,file);
        bytes_to_write -= bytes_written; 
    }
}

void send_start_ack(int a_socket, struct sockaddr *server, unsigned int len) {
    set_socket_timeout(a_socket);


    // send first ack to server so server has client udp info    
    char start_ack = 1;
    char server_ack;
    int bytes_recv = 0;

    while (bytes_recv <= 0) {
        errno = 0;
        send_udp_msg(a_socket, &start_ack, 1, server, len); 
        bytes_recv = recv_udp_msg(a_socket, &server_ack, 1, server, len);
    }

    disable_socket_timeout(a_socket);   

}

void my_handler(int s){
    close(server_socket);
    fclose(file);
    printf("%s\n", "Closed server socket");
    exit(0);
}

int main(int argc, char* argv[]){

    // set up safe exit on CTRL-C
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    // check for supplied file name
    if (argc != 2 ) {
        printf("To use this file transfer program, run \n./client <input file>\n");
        return 0;
    }
    char* file_name = argv[1];


    struct sockaddr_in ip_server;
    struct sockaddr *server;
    char buf[MAX_OCTOBLOCK_LEN];
    unsigned int len = sizeof(ip_server); 
    ssize_t read_bytes;   

    // setup server socket
    set_port_and_family(&ip_server);
    server = (struct sockaddr *) &ip_server;

    server_socket = setup_tcp_socket();

    // Connect to TCP server
    int status = connect(server_socket, (struct sockaddr *) &ip_server, sizeof(ip_server));
    if (status < 0) {
        perror("Error in connect()");
        close(server_socket);
        exit(-1);
    }    

    // send requested file name to server
    send_tcp_msg(server_socket, file_name);
    read_bytes = recv_tcp_msg(server_socket,buf,sizeof(buf));
    buf[read_bytes] = '\0';

    // convert file size string to long int
    long file_size = atol(buf);

    printf("Client received file size %ld from server\n", file_size);
    close(server_socket);

    // setup server socket
    server_socket = setup_udp_socket();


    if (inet_pton(AF_INET, SERVER_IP, &ip_server.sin_addr)==0) {
		perror("inet_pton() failed");
		exit(-1);
	}


    char new_file_name[strlen(file_name) + 4];
    strcpy(new_file_name,"new_");
    strcat(new_file_name,file_name);
    file = fopen(new_file_name, "wb");

    send_start_ack(server_socket, server, len);

    printf("Receiving from server %s on port %d \n",
               inet_ntoa(ip_server.sin_addr), ntohs(ip_server.sin_port));
                

    recv_octoblocks(file, file_size, server_socket, server, len);              

    fclose(file);
    close(server_socket);
    

	return 0;

}