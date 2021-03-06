/*
    Version 1.2
    Able to send out of order, retransmission and lost messages successfully
    Client has timed wait at end of file transfer in case last ACK to server wasn't sent successfully

    Author: Anna Tran
    Affiliation: University of Calgary
    Student ID: 10128425
*/


#include "octohelper.h"

int server_socket;
FILE* file;

/*
    Set the port and family for the address struct
*/
void set_port_and_family(struct sockaddr_in* addr) {
    memset ((char*) &(*addr),0,sizeof((*addr)));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(SERVER_PORT);   
}


/*
    Get the octoleg sequence number from a byte
*/
int get_seqnum(unsigned char identifier) {
    int seqnum = 0;
    while ((identifier & (1 << seqnum)) == 0x00) {
        seqnum++;
    }
    return seqnum;
}



/*
    Receive the octolegs from the server and reassemble them into an octoblock
*/
void recv_octolegs(char* octoblock, char octoblock_seqnum, int octoleg_len, int a_socket, struct sockaddr *server, unsigned int len) {
    printf("looking at octoblock seqnum: %d\n", octoblock_seqnum);
    int bytes_sent = 0;
    int bytes_recv = 0;

    // extra byte to contain the sequence number of the octoblock
    // extra byte to contain the sequence number of the octoleg
    char octolegs[8][octoleg_len+2];
    char buf[octoleg_len+2];
    struct sockaddr_in* ip_server;
    ip_server = (struct sockaddr_in*) server;    

    unsigned char ack = 0;

    char identifier;
    int seqnum;

    printf("----- Start receiving octolegs -----\n");
    // loop until all segments of message have been received
    while ((ack & 0xFF) != 0xFF) {
        memset (&buf,0,sizeof(buf));
        bytes_recv = recv_udp_msg(a_socket, buf, sizeof(buf), server, len);
        // throw away 10% of packets
        

        identifier = buf[1];
        seqnum = get_seqnum(identifier);

        
        if ((rand() % 100) < PERCENT_THROWAWAY) {
            printf("discarding octoleg %d\n", seqnum);
            continue;
        }
        printf("got seqnum %d\t", seqnum);
        // copy over contents of buffer only if the response message hadn't been received yet
        if (ack != (ack | identifier)) {

            memcpy((octolegs[seqnum]),buf,sizeof(buf));
            ack = ack | identifier;
        }

        // send current cumulative ACK         
        int highest_seqnum = get_highest_seqnum(ack);
        char highest_seqnum_ack = 1 << highest_seqnum;
        print_ack(ack);
        printf("\n");
        // printf("have all octolegs upto seqnum: %d\n", highest_seqnum);
        bytes_sent = send_udp_msg(a_socket, &highest_seqnum_ack, 1, server, len);
  
    }

    printf("----- Finished receiving octolegs -----\n\n");

    // assemble octolegs into octoblock
    for (int i = 0; i < 8; i++) {
        char* next_location = octoblock + (i * octoleg_len);
        memcpy(next_location, &(octolegs[i][2]), octoleg_len);
    }
}

/*
    Assure that the ACK from the last octoleg is sent to the server by checking if the octoblock sequence number
    has moved up to the next expected value
*/
void assure_octoblock_ack_sent(char octoblock_seqnum, int octoleg_len, char* init_octoleg, 
                                int a_socket, struct sockaddr *server, unsigned int len) {
    char highest_seqnum_ack = 1 << 7;
    char lowest_ack = 1;

    memset(init_octoleg,0,sizeof(*init_octoleg));
    recv_udp_msg(a_socket, init_octoleg, octoleg_len+2, server, len);
    // while the next octoleg is not part of the next octoblock to be sent
    while (init_octoleg[0] != ((octoblock_seqnum + 1) % OCTOBLOCK_MAX_SEQNUM)) {
        send_udp_msg(a_socket, &highest_seqnum_ack, 1, server, len);
        recv_udp_msg(a_socket, init_octoleg, octoleg_len+2, server, len);
    }
    send_udp_msg(a_socket, &lowest_ack, 1, server, len);
        
}


/*
    Send the first message to the server so that the server has client udp info
*/

void send_start_ack(char* init_octoleg, int octoleg_len, int a_socket, struct sockaddr *server, unsigned int len) {

    // send first ack to server so server has client udp info 
    // save the response to be included in the first octoblock   
    char start_ack = 1;
    do_concurrent_send(&start_ack, sizeof(start_ack), 
                        init_octoleg, sizeof(init_octoleg),
                        a_socket, server, len);
}

/*
    Receive complete and partial octoblocks from the server and write them to the file given the file descriptor
*/
void recv_octoblocks(FILE* file, long file_size, int a_socket, struct sockaddr *server, unsigned int len) {
    int bytes_written;
    int octoblock_len = MAX_OCTOBLOCK_LEN;    
    int octoleg_len = octoblock_len/8;
    long bytes_to_write = file_size;

    // first octoleg received after start ACK is sent
    char init_octoleg[octoleg_len+2];
    memset(init_octoleg,0,sizeof(init_octoleg));
    send_start_ack(init_octoleg, octoleg_len, a_socket, server, len);

    // octoblock sequence number (between 0 and OCTOBLOCK_MAX_SEQNUM) to tell when the client has received all 
    // octolegs of one octoblock and is ready to move to the next octoblock
    char octoblock_seqnum = 0;

    // make sure the size of the octoleg is at least 8 bytes
    while (octoleg_len >= MIN_LEG_BYTE) {
        printf("octoleg_len: %d\n", octoleg_len);
        // keep sending octolegs until remaining bytes of file to send are less than octoleg
        char octoblock[octoleg_len * 8];
        while (bytes_to_write >= octoblock_len) {

            memset(octoblock,0,sizeof(octoblock));
            recv_octolegs(octoblock, octoblock_seqnum, octoleg_len, a_socket, server, len);

            bytes_written = fwrite(octoblock,sizeof(char),sizeof(octoblock),file);
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
        recv_octolegs(octoblock, octoblock_seqnum, sizeof(char), a_socket, server, len);

        bytes_written = fwrite(octoblock,sizeof(char),bytes_to_write,file);
        bytes_to_write -= bytes_written;

        
    }
    char highest_seqnum_ack = 1 << 7;
    char recv_byte;

    // do a timed wait in case the server didn't get the ACK for the last octoblock, so client resends the ACK
    do_timed_wait(&highest_seqnum_ack, sizeof(highest_seqnum_ack), 
                    &recv_byte, sizeof(recv_byte),
                    a_socket, server, len);    
}

/*
    Closes the open sockets and file descriptors and exits the program
*/
void my_handler(int s){
    close(server_socket);
    fclose(file);
    printf("%s\n", "Closed server socket and open file descriptors");
    exit(0);
}

int main(int argc, char* argv[]){

    // set up safe exit on CTRL-C
    struct sigaction sig_int_handler;

    sig_int_handler.sa_handler = my_handler;
    sigemptyset(&sig_int_handler.sa_mask);
    sig_int_handler.sa_flags = 0;

    sigaction(SIGINT, &sig_int_handler, NULL);

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
    const char* server_name = SERVER_ADDR;
    inet_pton(AF_INET, server_name, &ip_server.sin_addr);
    server = (struct sockaddr*) &ip_server;

    // setup UDP socket
    server_socket = setup_udp_socket();


    char new_file_name[strlen(truncated_file_name) + 4];
    strcpy(new_file_name,"new_");
    strcat(new_file_name,truncated_file_name);
    printf("new file name: %s\n", new_file_name);

    file = fopen(new_file_name, "wb");
    if (file == NULL) {
        perror("Problem opening file");
        close(server_socket);
        exit(-1);
    }

    printf("Receiving from server %s on port %d \n", inet_ntoa(ip_server.sin_addr), ntohs(ip_server.sin_port));

    recv_octoblocks(file, file_size, server_socket, server, len);              

    fclose(file);
    close(server_socket);
    

	return 0;

}
