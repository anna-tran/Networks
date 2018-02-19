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
#include <regex>
#include <map>
#include <netdb.h>
#include <fcntl.h>

#define SERVER_PORT 4545
#define MAX_OCTOBLOCK_LEN 8888
#define OCTO_SIZE 8
#define MIN_LEG_BYTE 8
#define SERVER_IP "127.0.0.1" 

struct Octoleg {
	int seqnum;

	void set_seqnum(int num) {
		seqnum = num;
	}


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
	int sent_bytes = sendto(a_socket, msg, msg_len, 0, addr, len);
	if (sent_bytes < 0 && errno != EAGAIN) {
		perror("Unsuccessful send");
        close(a_socket);        
		exit(-1);
	}
	return sent_bytes;
}

int recv_udp_msg(int a_socket, char* buf, size_t buf_len, struct sockaddr *addr, unsigned int len) {
    int read_bytes = recvfrom(a_socket, buf, buf_len, 0, addr, &len);
    if (read_bytes < 0 && errno != EAGAIN) {
        perror("Recv error");
        close(a_socket);        
        exit(-1);
    } 	
    return read_bytes;
}

void set_socket_timeout(int a_socket) {
	struct timeval timeout;
	timeout.tv_sec = 1;
	int status = setsockopt(a_socket,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
	if (status < 0) {
		perror("Could not set timeout for socket");
		close(a_socket);		
		exit(-1);
	}
}

void disable_socket_timeout(int a_socket) {
	int opts = fcntl(a_socket,F_GETFL);
    if (opts < 0) {
        perror("Could not get socket options");
        exit(-1);
    }	
    opts = opts & (~O_NONBLOCK);
	int status = fcntl(a_socket,F_SETFL,opts);
	if (status < 0) {
		perror("Could not disable socket timeout");
		close(a_socket);		
		exit(-1);
	}
}