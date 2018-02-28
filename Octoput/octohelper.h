/*
    Version 1.2
    Able to send out of order, retransmission and lost messages successfully
    Client has timed wait at end of file transfer in case last ACK to server wasn't sent successfully

    Author: Anna Tran
    Affiliation: University of Calgary
    Student ID: 10128425
*/


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <string.h>
#include <string>
#include <ctype.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>

#include <set>
#include <algorithm>
#include <iterator>
#include <errno.h>



#define SERVER_PORT 4545            // socket port to connect to server
#define SERVER_ADDR "localhost"     // address the server is running on
#define MAX_OCTOBLOCK_LEN 8888      // maximum size of an octoblock
#define MIN_LEG_BYTE 8              // minimum size of an octoleg
#define OCTOBLOCK_MAX_SEQNUM 3      // modulo for restarting octoblock sequence numbers
#define TIMEOUT 1                   // timeout for concurrent sending is 1s 
#define TIMED_WAIT 10               // timeout before closing a socket
#define PERCENT_THROWAWAY 10        // percentage of how many packets to discard

// struct to contain arguements for receiving data from client
struct pthread_recv_args {
	char* recv_buf;
	size_t size_of_recv;

	int a_socket;

	struct sockaddr *client;
	unsigned int size_of_client;

	int* bytes_recv;
};

// struct to contain arguments to detect timeout
struct pthread_timeout_args {
    int timeout_len;
    bool* timed_out;
};

/*
    Create a TCP socket
*/
int setup_tcp_socket() {
    int a_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (a_socket == -1) {
		perror("Could not set up socket");
        exit(-1);
    }

    return a_socket;
}

/*
    Create a UDP socket
*/
int setup_udp_socket() {
	// create socket
	int a_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (a_socket == -1) {
		perror("Could not set up socket");
		exit(-1);
	}

	return a_socket;
}

/*
    Bind a socket to a port and address
*/
void bind_socket(int a_socket, sockaddr_in *address) {
	// bind socket to PORT
	int status = bind(a_socket, (struct sockaddr*) address, sizeof(struct sockaddr_in));
	if (status == -1) {
        perror("bind() call failed");
		close(a_socket);
		exit(-1);
	}
}

/*
    Send a TCP message, exit on failure
*/
void send_tcp_msg(int a_socket, const char* msg) {
    ssize_t c_send_status;
    c_send_status = send(a_socket, msg, strlen(msg), 0);
    if (c_send_status < 0) {
        perror("Error in send() call");
        exit(-1);
    }
}

/*
    Receive a TCP message, exit on failure
*/
ssize_t recv_tcp_msg(int a_socket, char* msg, ssize_t size_msg) {
    // clear buffer first
    memset(msg,0,size_msg);

    ssize_t bytes_rcv;
    bytes_rcv = recv(a_socket, &(*msg), size_msg, 0);
    if (bytes_rcv < 0) {
        perror("Error in receiving");
        exit(-1);
    }
    return bytes_rcv;

}

/*
    Send a UDP message, exit on failure
*/
int send_udp_msg(int a_socket, char* msg, size_t msg_len, struct sockaddr *addr, unsigned int len) {
	int sent_bytes = sendto(a_socket, msg, msg_len, 0, addr, sizeof(struct sockaddr_in));
	if (sent_bytes < 0) {
		perror("Unsuccessful send");
        close(a_socket);        
		exit(-1);
	}
	return sent_bytes;
}

/*
    Receive a UDP message, exit on failure
*/
int recv_udp_msg(int a_socket, char* buf, size_t buf_len, struct sockaddr *addr, unsigned int len) {
    int read_bytes = recvfrom(a_socket, buf, buf_len, 0, addr, &len);
    if (read_bytes < 0) {
        perror("Recv error");
        close(a_socket);        
        exit(-1);
    } 	
    return read_bytes;
}


/*
    Get the highest octoleg sequence number from an ACK
*/
int get_highest_seqnum(unsigned char ack) {
    int seqnum = 0;
    while (ack & (1 << (seqnum))) {
        seqnum++;
    }
    if (seqnum == 0) {
    	return seqnum;
    } else {
    	return seqnum-1;
    }
}

/*
    For debugging. Print out the ACK string backwards starting with LSB ending with MSB
*/
void print_ack(unsigned char ack) {
    char ack_str[9];
    memset(ack_str,0,9);
    for (int i = 0; i < 8; i++) {
        if (ack & (1 << i)) {
            ack_str[i] = '1';
        } else {
            ack_str[i] = '0';
        }
    }
    printf("ack is %s", ack_str);
}

/*
    Check the thread result to ensure it was successfully created.
    Exit program otherwise.
*/
void check_pthread_ok(int pthread_result, int a_socket) {
	if (pthread_result) {
			perror("Error creating pthread");
			close(a_socket);
			exit(-1);
	}
}

/*
    Function in which the thread waits for a duration of time before timing out, setting the timeout flag and exiting.
*/
void* pthread_wait_timeout(void *void_timeout_args) {
    struct pthread_timeout_args* timeout_args = (struct pthread_timeout_args*) void_timeout_args;
	sleep(timeout_args->timeout_len);
	*(timeout_args->timed_out) = true;
    pthread_exit(0);
}

/*
    Function in which the thread waits for a message incoming on a socket
*/
void* pthread_recv_bytes(void* void_recv_args) {
	struct pthread_recv_args* args = (struct pthread_recv_args*) void_recv_args;
	*(args->bytes_recv) = recv_udp_msg(args->a_socket, args->recv_buf, args->size_of_recv, args->client, args->size_of_client);
	pthread_exit(0);
}

/*
    Send a message and wait for a response.
    If the timeout happens before a response is obtained, restart the timeout, resend the message and wait again.
*/
int do_concurrent_send(char* send_buf, size_t size_of_send, 
						char* recv_buf, size_t size_of_recv,
						int a_socket, struct sockaddr *client, unsigned int len) {
	int bytes_sent = 0;
	int bytes_recv = 0;
	bool timed_out = true;

    struct pthread_timeout_args timeout_args;
    timeout_args.timeout_len = TIMEOUT;
    timeout_args.timed_out = &timed_out;
	struct pthread_recv_args recv_args;
	recv_args.recv_buf = recv_buf;
	recv_args.size_of_recv = size_of_recv;
	recv_args.a_socket = a_socket;
	recv_args.client = client;
	recv_args.size_of_client = len;
	recv_args.bytes_recv = &bytes_recv;


    // keep looping until the timeout has not been reached, implying that a message has been received
	while (timed_out){
		timed_out = false;

		pthread_t thread1;
		check_pthread_ok(pthread_create(&thread1, NULL, pthread_wait_timeout, (void*) &timeout_args), a_socket);

        // send the message and wait for a response
		bytes_sent = send_udp_msg(a_socket, send_buf, size_of_send, client, len);

		pthread_t thread2;
		check_pthread_ok(pthread_create(&thread2, NULL, pthread_recv_bytes, (void*) &recv_args), a_socket);

        while (!timed_out) {
        	// if received something, kill the timer process
            if (bytes_recv > 0) {
                pthread_cancel(thread1);
                break;
            }
        }
        // timed out so kill receiving process and try again
        if (timed_out) {
        	pthread_cancel(thread2);
            printf("resending octoleg\n");
        }

		
	}
	return bytes_sent;
}


/*
    When client has received all of the requested data, wait a duration of time before closing the socket
    to ensure that the server has received the last ACK. If not, resend the ACK and reset the timeout.
*/
void do_timed_wait(char* send_buf, size_t size_of_send, 
                    char* recv_buf, size_t size_of_recv,
                    int a_socket, struct sockaddr *client, unsigned int len) {
    int bytes_sent = 0;
    int bytes_recv = 0;
    bool timed_out = false;

    struct pthread_timeout_args timeout_args;
    timeout_args.timeout_len = TIMED_WAIT;              // wait 30s before exiting read call
    timeout_args.timed_out = &timed_out;
    struct pthread_recv_args recv_args;
    recv_args.recv_buf = recv_buf;
    recv_args.size_of_recv = size_of_recv;
    recv_args.a_socket = a_socket;
    recv_args.client = client;
    recv_args.size_of_client = len;
    recv_args.bytes_recv = &bytes_recv;

    while (!timed_out) {
        pthread_t thread1;
        check_pthread_ok(pthread_create(&thread1, NULL, pthread_wait_timeout, (void*) &timeout_args), a_socket);

        pthread_t thread2;
        check_pthread_ok(pthread_create(&thread2, NULL, pthread_recv_bytes, (void*) &recv_args), a_socket);
        while (!timed_out) {
            
            // if a message was received, then the server doesn't know that the client has all the requested
            // data so send the FIN ACK again
            if (bytes_recv > 0) {
                bytes_sent = send_udp_msg(a_socket, send_buf, size_of_send, client, len);
                pthread_cancel(thread1);
                break;
            }
        }
        pthread_cancel(thread2);
        
    }    



}

