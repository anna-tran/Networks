/*
    Version 1.2
    Able to send out of order, retransmission and lost messages successfully
    Client has timed wait at end of file transfer in case last ACK to server wasn't sent successfully
*/


#include "octohelper.h"

int server_socket;
FILE* file;

void set_port_and_family(struct sockaddr_in* addr) {
    memset ((char*) &(*addr),0,sizeof((*addr)));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(SERVER_PORT);   
}


int get_seqnum(unsigned char identifier) {
    int seqnum = 0;
    while ((identifier & (1 << seqnum)) == 0x00) {
        seqnum++;
    }
    return seqnum;
}





void recv_octolegs(char* octoblock, char octoblock_seqnum, int octoleg_len, char* init_octoleg, int a_socket, struct sockaddr *server, unsigned int len) {
    int bytes_sent = 0;
    int bytes_recv = 0;

    // add another bit to contain the sequence number
    char octolegs[8][octoleg_len+2];
    char buf[octoleg_len+2];
    struct sockaddr_in* ip_server;
    ip_server = (struct sockaddr_in*) server;    

    // loop until all segments of message have been received
    unsigned char ack = 0;

    char identifier = init_octoleg[1];
    int seqnum = get_seqnum(identifier);

    memcpy(octolegs[seqnum],init_octoleg,octoleg_len+2);
    ack = ack | identifier;

    while ((ack & 0xFF) != 0xFF) {
        // wait for server response
        memset (&buf,0,sizeof(buf));
        
        bytes_recv = recv_udp_msg(a_socket, buf, sizeof(buf), server, len);
        //printf("bytes_recv = %d\n", bytes_recv);
        if ((rand() % 100) < 10)
           continue;

        identifier = buf[1];
        seqnum = get_seqnum(identifier);

        printf("got seqnum %d\n", seqnum);
        // copy over contents of buffer only if the response message hadn't been received yet
        if (ack != (ack | identifier)) {
            memcpy((octolegs[seqnum]),buf,sizeof(buf));
            ack = ack | identifier;
        }

        // send cumulative ACK         
        int highest_seqnum = get_highest_seqnum(ack);
        //printf("sending highest_seqnum  %d   ", highest_seqnum);
        //print_ack(ack);
        //printf("\n");
        char highest_seqnum_ack = 1 << highest_seqnum;
        bytes_sent = send_udp_msg(a_socket, &highest_seqnum_ack, 1, server, len);
  
    }

    // assemble octolegs into octoblock
    memcpy(octoblock, &(octolegs[0][2]), octoleg_len);
    for (int i = 1; i < 8; i++) {
        char* next_location = octoblock + (i * octoleg_len);
        memcpy(next_location, &(octolegs[i][2]), octoleg_len);
    }
    //printf("finished memcpy\n");
}

void assure_octoblock_ack_sent(char octoblock_seqnum, int octoleg_len, char* init_octoleg, int a_socket, struct sockaddr *server, unsigned int len) {
    char highest_seqnum_ack = 1 << 7;
    char lowest_ack = 1;

    printf("-----\n");
    printf("octoleg_len = %d\n", octoleg_len);
    printf("octoblock seqnum = %u\n", octoblock_seqnum);
    printf("-----\n");

    memset(init_octoleg,0,sizeof(*init_octoleg));
    recv_udp_msg(a_socket, init_octoleg, octoleg_len+2, server, len);
    // while the next octoleg is not part of the next octoblock to be sent
    while (init_octoleg[0] != ((octoblock_seqnum + 1) % OCTOBLOCK_MAX_SEQNUM)) {
        send_udp_msg(a_socket, &highest_seqnum_ack, 1, server, len);
        recv_udp_msg(a_socket, init_octoleg, octoleg_len+2, server, len);
    }
    send_udp_msg(a_socket, &lowest_ack, 1, server, len);
        
}




void send_start_ack(char* init_octoleg, int octoleg_len, int a_socket, struct sockaddr *server, unsigned int len) {

    // send first ack to server so server has client udp info    
    char start_ack = 1;

    do_concurrent_send(&start_ack, sizeof(start_ack), 
                        init_octoleg, sizeof(init_octoleg),
                        a_socket, server, len);
}

void recv_octoblocks(FILE* file, long file_size, int a_socket, struct sockaddr *server, unsigned int len) {
    int bytes_written;
    int octoblock_len = MAX_OCTOBLOCK_LEN;    
    int octoleg_len = octoblock_len/8;
    long bytes_to_write = file_size;

    char init_octoleg[octoleg_len+2];
    memset(init_octoleg,0,sizeof(init_octoleg));
    send_start_ack(init_octoleg, octoleg_len, a_socket, server, len);

    char octoblock_seqnum = 0;
    // make sure the size of the octoleg is at least 8 bytes
    while (octoleg_len >= MIN_LEG_BYTE) {
        printf("octoleg_len: %d\n", octoleg_len);
        // keep sending octolegs until remaining bytes of file to send are less than octoleg
        char octoblock[octoleg_len * 8];
        while (bytes_to_write >= octoblock_len) {
            memset(octoblock,0,sizeof(octoblock));
            recv_octolegs(octoblock, octoblock_seqnum, octoleg_len, init_octoleg, a_socket, server, len);

            //printf("size of octoblock: %ld\n", sizeof(octoblock));
            bytes_written = fwrite(octoblock,sizeof(char),sizeof(octoblock),file);
            printf("bytes_written = %d\n", bytes_written);
            fflush(file);
            bytes_to_write -= bytes_written;
            printf("bytes_to_write = %ld\n", bytes_to_write);

            if (bytes_to_write >= MIN_LEG_BYTE)
                assure_octoblock_ack_sent(octoblock_seqnum, octoleg_len, init_octoleg, a_socket, server, len);

            octoblock_seqnum = (octoblock_seqnum + 1) % OCTOBLOCK_MAX_SEQNUM;
        }


        // octoleg size becomes 1/8 of the remaining file bytes
        octoblock_len = bytes_to_write;
        octoleg_len = (octoblock_len / 8);

    }

    // send the remaining bytes in a padded buffer of 8 bytes
    octoleg_len = sizeof(char);
    if (bytes_to_write > 0) {
        printf("leftover bytes to write: %ld\n", bytes_to_write);
        char octoblock[8];
        memset(octoblock,0,sizeof(octoblock));
        recv_octolegs(octoblock, octoblock_seqnum, sizeof(char), init_octoleg, a_socket, server, len);

        bytes_written = fwrite(octoblock,sizeof(char),bytes_to_write,file);
        bytes_to_write -= bytes_written;

        
    }
    char highest_seqnum_ack = 1 << 7;
    char recv_byte;

    do_timed_wait(&highest_seqnum_ack, sizeof(highest_seqnum_ack), 
                    &recv_byte, sizeof(recv_byte),
                    a_socket, server, len);    
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
    unsigned int len = sizeof(struct sockaddr_in); 
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












    const char* truncated_file_name;
    std::string file_name_str = file_name;
    ssize_t last_slash_index;    
    last_slash_index = file_name_str.find_last_of("/");
    if (last_slash_index != std::string::npos) {
        truncated_file_name = file_name_str.substr(last_slash_index+1).c_str();
    } else {
        truncated_file_name = file_name;
    }

    printf("file name is %s\n", truncated_file_name);
    const char* server_name = "localhost";
    inet_pton(AF_INET, server_name, &ip_server.sin_addr);
    server = (struct sockaddr*) &ip_server;

    // setup server socket
    server_socket = setup_udp_socket();


    char new_file_name[strlen(truncated_file_name) + 4];
    strcpy(new_file_name,"new_");
    strcat(new_file_name,truncated_file_name);
    printf("new file name: %s\n", new_file_name);

    file = fopen(new_file_name, "wb+");
    if (file == NULL) {
        perror("Problem opening file");
        close(server_socket);
        exit(-1);
    }
    // send_start_ack(server_socket, server, len);

    printf("Receiving from server %s on port %d \n", inet_ntoa(ip_server.sin_addr), ntohs(ip_server.sin_port));

    recv_octoblocks(file, file_size, server_socket, server, len);              

    fclose(file);
    close(server_socket);
    

	return 0;

}
