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



#define SERVER_PORT 4545
#define MAX_OCTOBLOCK_LEN 8888
#define OCTO_SIZE 8
#define MIN_LEG_BYTE 8
#define TIMEOUT 1

struct pthread_recv_args {
	char* recv_buf;
	size_t size_of_recv;

	int a_socket;

	struct sockaddr *client;
	unsigned int size_of_client;

	int* bytes_recv;
};

int setup_tcp_socket() {
    int a_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (a_socket == -1) {
		perror("Could not set up socket");
        exit(-1);
    }

    return a_socket;
}


int setup_udp_socket() {
	// create socket
	int a_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (a_socket == -1) {
		perror("Could not set up socket");
		exit(-1);
	}

	return a_socket;
}

void bind_socket(int a_socket, sockaddr_in *address) {
	// bind socket to PORT
	int status = bind(a_socket, (struct sockaddr*) address, sizeof(struct sockaddr_in));
	if (status == -1) {
        perror("bind() call failed");
		close(a_socket);
		exit(-1);
	}
}

void send_tcp_msg(int a_socket, const char* msg) {
    ssize_t c_send_status;
    c_send_status = send(a_socket, msg, strlen(msg), 0);
    if (c_send_status < 0) {
        perror("Error in send() call");
        exit(-1);
    }
}

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


int send_udp_msg(int a_socket, char* msg, size_t msg_len, struct sockaddr *addr, unsigned int len) {
	int sent_bytes = sendto(a_socket, msg, msg_len, 0, addr, sizeof(struct sockaddr_in));
	if (sent_bytes < 0) {
		perror("Unsuccessful send");
        close(a_socket);        
		exit(-1);
	}
	return sent_bytes;
}

int recv_udp_msg(int a_socket, char* buf, size_t buf_len, struct sockaddr *addr, unsigned int len) {
    int read_bytes = recvfrom(a_socket, buf, buf_len, 0, addr, &len);
    if (read_bytes < 0) {
        perror("Recv error");
        close(a_socket);        
        exit(-1);
    } 	
    return read_bytes;
}

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
// void set_socket_timeout(int a_socket) {
// 	struct timeval timeout;
// 	timeout.tv_sec = 1;
// 	timeout.tv_usec = 0;
// 	int status = setsockopt(a_socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
// 	if (status < 0) {
// 		perror("Could not set timeout for socket");
// 		close(a_socket);		
// 		exit(-1);
// 	}
// }

// void disable_socket_timeout(int a_socket) {
// 	int opts = fcntl(a_socket,F_GETFL);
//     if (opts < 0) {
//         perror("Could not get socket options");
//         close(a_socket);
//         exit(-1);
//     }	
//     opts = opts & (~O_NONBLOCK);
// 	int status = fcntl(a_socket,F_SETFL,opts);
// 	if (status < 0) {
// 		perror("Could not disable socket timeout");
// 		close(a_socket);		
// 		exit(-1);
// 	}
// }

void check_pthread_ok(int pthread_result, int a_socket) {
	if (pthread_result) {
			perror("Error creating pthread");
			close(a_socket);
			exit(-1);
	}
}

void* pthread_wait_timeout(void *void_timed_out) {
	bool* timed_out = (bool*) void_timed_out;
	sleep(TIMEOUT);
	//printf("child1 timed_out\n");
	*timed_out = true;
    pthread_exit(0);
}

void* pthread_recv_bytes(void* void_recv_args) {
	struct pthread_recv_args* args = (struct pthread_recv_args*) void_recv_args;
	//printf("child2 waiting for message\n");
	*(args->bytes_recv) = recv_udp_msg(args->a_socket, args->recv_buf, args->size_of_recv, args->client, args->size_of_client);
	pthread_exit(0);
}

int do_concurrent_send(char* send_buf, size_t size_of_send, 
						char* recv_buf, size_t size_of_recv,
						int a_socket, struct sockaddr *client, unsigned int len) {
	int bytes_sent = 0;
	int bytes_recv = 0;
	bool timed_out = true;
	struct pthread_recv_args recv_args;
	recv_args.recv_buf = recv_buf;
	recv_args.size_of_recv = size_of_recv;
	recv_args.a_socket = a_socket;
	recv_args.client = client;
	recv_args.size_of_client = len;
	recv_args.bytes_recv = &bytes_recv;

	while (timed_out){
		timed_out = false;

		pthread_t thread1;
		check_pthread_ok(pthread_create(&thread1, NULL, pthread_wait_timeout, (void*) &timed_out), a_socket);

		bytes_sent = send_udp_msg(a_socket, send_buf, size_of_send, client, len);

		pthread_t thread2;
		check_pthread_ok(pthread_create(&thread2, NULL, pthread_recv_bytes, (void*) &recv_args), a_socket);

		

        // parent process
        while (!timed_out) {
        	// if received something, kill the timer process
            if (bytes_recv > 0) {
                pthread_cancel(thread1);
                //printf("bytes_recv %d\n", bytes_recv);
                break;
            }
        }
        // timed out
        // kill receiving process and try again
        if (timed_out) {
        	pthread_cancel(thread2);
        }

		
	}
	//printf("bytes_sent on concurrent send: %d\n", bytes_sent);
	return bytes_sent;
}


