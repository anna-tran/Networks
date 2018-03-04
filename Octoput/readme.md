# Octoput Transfer Protocol

This is simple file transfer application using UDP. The structure of the application works similarly to the TCP in that it uses timers, acknowledgement bytes (ACKs), retransmissions and a timed wait once the client has received all the requested data. This application consists of a client and server program, that works on a pull-based mechanism. A pull-based, rather than a push-based, design was chosen since the server may have various files on its machine but the client only wants a select few of those files. Although this application focuses on sending data through UDP, for simplicity and reliability, the file name (requested by the client to the server) and the file size (sent from the server to the client upon receiving the file name) are sent using TCP. The server will only send data for files that exist on the server's machine.

The server will run forever until the process is explicitly killed. Clients should only connect synchronously for the application to transfer files properly. 

Data are delivered in 'octoblocks' of 8888 bytes and each of these octoblocks consists of 'octolegs' which are 1/8 the size of an octoblock, rounded down. While octoblocks are sent sequentially, octolegs can be (duplicately) sent in any order. Once the amount of file bytes left to send to the client are less than 8888 bytes, the octoblock is reduced to that size. The size is repeatedly reduced until only 0 to 7 bytes are left to send. Each octoleg contains either the file data or a pad byte.


## Compilation
Assure that when compiling both the server and client program, octohelper.h is in the same directory. Note that the server must be running before the client starts up.
To compile and run the server program:

	> g++ octoput_server.cpp -o server
	> ./server

To compile and run the client program:

	> g++ octoput_client.cpp -o client
	> ./client <requested_file_name>

The client program will write the requested file in the the current directory as "new_<requested_file_name>".

## Configuration
The default configuration values for the address and port of the server is 'localhost:4545'.
To change the address and port of the server, modify the SERVER_PORT and SERVER_ADDR values in octohelper.h.

Also by default, the octolegs are sent in a random order repeatedly until the server has received an ACK from the client that all octolegs from the current octoblock have been received. On the client side, 10% of received packets are discarded. The program can be configured to send octolegs sequentially, and the rate at which packets are discarded can also be modified.

The timeout and timed wait duration for the server and client respectively can be configured in octohelper.h.


## UDP Transfer Structure

To mimic the reliability of the TCP structure, this program relies on a timeout mechanism on the server side to assure that an octoleg was successfully received by the client. In addition to the data sent in an octoleg, the server sends an extra two bytes: the first to indicate the octoblock sequence number, and the second to indicate the octoleg sequence number. The second byte is needed for the client to determine whether it has received that octoleg before and if so, then discards it. The first byte ensures that the last ACK for one octoblock has been sent from client to server. 

The server will only send one octoleg at a time (no pipelining). Acknowledgements from the client are cumulative and indicate the largest ocotoleg sequence number 'i' for which the client has received all of the octolegs with sequence numbers 'j' <= 'i'.

Recall that the client needs to obtain the octoleg of the next octoblock to determine if the octoblock sequence number has changed. The final ACK for the server's last octoblock, however, may or may not reach the server. If it does reach the server then both client and server should exit successfully. If it does not, however, the client may be waiting infinitely for an octoleg that will not come. Instead, the client goes into a timed wait for 5 seconds should the server not receive the ACK and sends back an octoleg. The client resends the ACK and the transaction repeats until after the client has not received any message after the 'timed wait'. Then, it assumes that the server has closed and the client shuts down, itself. It is assumed that there are no bit errors and so, no need to verify the checksum for each packet.

The implementation of these features make the code more complex but does make the application more reliable. The code is also scalable and able to transfer files up to 256KB. With the ability to retransmit data and reassemble octolegs received out of order, the application should theoretically be portable over different networks although this has not been tested. It does not, however, account for late ACKs.

## Testing

This application was tested at home on my personal laptop, on a loopback interface. The file transfer application is able to successfully send complete files, even when data packets are sent out of order and/or are lost.

