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
#include <iostream>

int listeningSocket;            // socket descriptor for proxy to listen on
const int FIXED_RANGE = 30;     // amount of bytes to increment range requests by

/*
    Model for an HTTP request, which holds other information for making range requests
*/
class HttpRequest {
    public:
        std::string method;
        std::string url;
        std::string host;
        std::string protocol;
        std::string body;
        std::map<std::string,std::string> headers;
        std::string request;

        bool canSendRangeRequests;
        std::string contentType;
        unsigned long contentLength;
        unsigned long startRange;

    /*
        Create the string form of the HTTP request with its attributes
    */
    void createRequest() {
        request = (method + " " + url + " " + protocol + "\r\n");
        for (std::map<std::string,std::string>::iterator it = headers.begin(); it != headers.end(); ++it) {
            request.append(it->first + ": " + it->second + "\r\n");
        }
        request.append("\r\n"); // blank line before request body
        request.append(body);
        request.append("\0");

    }

};

/*
    Object to model an HTTP response
*/
class HttpResponse {
    public:
        std::string protocol;
        std::string statusCode;
        std::string statusPhrase;
        std::string body;
        std::map<std::string,std::string> headers;
        std::string response;

    /*
        Create the string form of the HTTP response with its attributes
    */
    void createResponse() {
        response = (protocol + " " + statusCode + " " + statusPhrase + "\r\n");
        for (std::map<std::string,std::string>::iterator it = headers.begin(); it != headers.end(); ++it) {
            response.append(it->first + ": " + it->second + "\r\n");
        }
        response.append("\r\n"); // blank line before request body
        response.append(body);
        response.append("\0");
    }

};  

/* 
    Remove trailing whitespace (\r\n\s) of a string
*/
void trimTrailingWhitespace(char* aString) {
    int i = strlen(aString) - 1;
    while (i >= 0 && isspace(aString[i])) {
        aString[i] = '\0';
        i--;
    }
}
/*
    Create listening socket and bind socket port to server.
    @return     listening socket
*/
int setupListeningSocket(sockaddr_in *address) {
    int listeningSocket;
    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == -1) {
        perror("socket() call failed");
        exit(-1);
    }

    int status;
    status = bind(listeningSocket, (struct sockaddr*) address, sizeof(struct sockaddr_in));
    if (status == -1) {
        perror("bind() call failed");
        exit(-1);
    }
    return listeningSocket;
}

/*
    Send an array of characters (message) to the given socket
*/
void sendMessageToSocket(int socket, const char* msgToSend) {
    ssize_t c_send_status;
    c_send_status = send(socket, msgToSend, strlen(msgToSend), 0);
    if (c_send_status < 0) {
        perror("Error in send() call");
        exit(-1);
    }
}

/*
    Create a TCP connection to server and return the socket descriptor

    @return     socket descriptor to server
*/
int createAndConnectServerSocket(struct sockaddr_in* addr_server, struct hostent* server, ssize_t sizeOfAddrServer) {
    int server_sock;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("socket() call failed");
        exit(-1);
    }

    // Connect to TCP server
    int connect_status;
    connect_status = connect(server_sock, (struct sockaddr *) &(*addr_server), sizeOfAddrServer);
    if (connect_status < 0) {
        perror("Error in connect()");
        close(server_sock);
        exit(-1);
    }

    return server_sock;     
}

/*
    Add a Range header to the given HTTP request.
*/
void updateRangeInHeaders(HttpRequest* httpRequest) {
    if (httpRequest->startRange > httpRequest->contentLength) {
        return;
    }
    unsigned long maxLen = httpRequest->contentLength;

    unsigned long startRange = httpRequest->startRange;
    unsigned long endRange = startRange + FIXED_RANGE - 1;

    // endRange is min(startRange + 100, content length)
    if (endRange > httpRequest->contentLength) {
        endRange = httpRequest->contentLength-1;
    }
    std::string rangeHeader ("Range");
    std::string range = "bytes=";
    range.append(std::to_string(startRange));
    range.append("-");
    range.append(std::to_string(endRange));

    httpRequest->headers[rangeHeader] = range;
    httpRequest->startRange += FIXED_RANGE;
}


/*
    Create the HEAD request to be sent to the server, given the information in a GET
    HTTP request
*/
void createHeaderRequest(HttpRequest* httpRequest, std::string* headRequest) {
    std::string headerField = "Host";
    headRequest->append("HEAD " + httpRequest->url + " " + httpRequest->protocol + "\r\n");
    headRequest->append("Host: " + httpRequest->headers[headerField] + "\r\n");
    headRequest->append("\r\n");
}

/*
    Parse the data contained in the character buffer into an HTTP response object
*/
void parseResponse(HttpResponse* httpResponse, char* buffer) {
    // initialize the response
    std::string entireResponse (buffer);
    httpResponse->response = entireResponse;   

    char* response = strtok(buffer, "\r\n");
    char responseCopy[1024];
    strcpy(responseCopy,response);

    // store each header field and its value into the HTTP response headers map
    char* headerField = strtok(NULL, "\r\n");
    unsigned long colonPos;
    std::string headerFieldStr, header, value;
    while ((headerField != NULL) && (headerField[0] != '\0') && (strcmp(headerField,"") != 0)) {
        headerFieldStr = headerField;
        colonPos = headerFieldStr.find(':');
        header = headerFieldStr.substr(0,colonPos);
        value = headerFieldStr.substr(colonPos+2,headerFieldStr.length()-colonPos-2);
        httpResponse->headers.insert(std::make_pair(header,value));

        headerField = strtok(NULL, "\n");
        trimTrailingWhitespace(headerField);
    }

    char* remainder = strtok(NULL, "\0");

    if (remainder == NULL) {
        httpResponse->body = "";
    } else {
        httpResponse->body = remainder;
    }

    char* protocol = strtok(responseCopy, " ");
    char* statusCode = strtok(NULL, " ");
    char* statusPhrase = strtok(NULL, "\n");

    // set HTTP response protocol, status code and status phrase
    httpResponse->protocol = protocol;
    httpResponse->statusCode = statusCode;
    httpResponse->statusPhrase = statusPhrase;

}


/*
    Fill in the attributes for the HTTP request's content length, content type and 
    whether it accepts ranges in the request, based on the response from the HEAD request response
*/
void putHeadInfoIntoRequest(HttpResponse* httpResponse, HttpRequest* httpRequest) {
    // set the content length of the expected response
    unsigned long contentLength = strtoul(httpResponse->headers["Content-Length"].c_str(),NULL,0);
    httpRequest->contentLength = contentLength;
    httpRequest->startRange = 0;
    httpRequest->contentType = httpResponse->headers["Content-Type"].c_str();
    httpRequest->canSendRangeRequests = httpResponse->headers["Accept-Ranges"].compare("none") != 0;
}

/*
    Parse data in a character buffer into an HTTP request object
*/
void parseRequest(HttpRequest* httpRequest, char* buffer) {
    
    // make the initial request string what was in the buffer
    char* request = strtok(buffer, "\r\n");
    char requestCopy[1024];
    strcpy(requestCopy,request);

    // store each header field and its value into the HTTP request headers map
    char* headerField = strtok(NULL, "\r\n");
    unsigned long colonPos;
    std::string headerFieldStr, header, value;
    while ((headerField != NULL) && (headerField[0] != '\0') && (strcmp(headerField,"") != 0)) {
        headerFieldStr = headerField;
        colonPos = headerFieldStr.find(':');
        header = headerFieldStr.substr(0,colonPos);
        value = headerFieldStr.substr(colonPos+2,headerFieldStr.length()-colonPos-2);
        httpRequest->headers.insert(std::make_pair(header,value));

        headerField = strtok(NULL, "\n");
        trimTrailingWhitespace(headerField);
    }

    char* remainder = strtok(NULL, "\0");

    if (remainder == NULL) {
        httpRequest->body = "";
    } else {
        httpRequest->body = remainder;
    }

    char* method = strtok(requestCopy, " ");
    char* url = strtok(NULL, " ");
    char* protocol = strtok(NULL, "\n");

    // set HTTPRequest method, url and protocol
    httpRequest->method = method;
    httpRequest->url = url;
    httpRequest->protocol = protocol;

}

/*
    Check if a request matches the GET and has content type HTML
*/
bool matchGetHtml(HttpRequest* httpRequest) {
    std::regex httpRegex ("HTTP/\\d\\.\\d");

    return httpRequest->method.compare("GET") == 0 &&
            httpRequest->contentType.find("html") != std::string::npos &&
            std::regex_match(httpRequest->protocol,httpRegex);
}


/*
    Given a socket descriptor and a character buffer, request data from the socket
    endpoint and store that data into the buffer

    @return     Number of bytes received from the socket endpoint
*/
ssize_t receiveData(int socket, char* rcvMsg, ssize_t sizeOfRcvMsg) {
    // clear buffer first
    memset(rcvMsg,0,sizeOfRcvMsg);

    ssize_t bytes_rcv;
    bytes_rcv = recv(socket, &(*rcvMsg), sizeOfRcvMsg, 0);
    if (bytes_rcv < 0) {
        perror("Error in receiving");
        exit(-1);
    }
    return bytes_rcv;

}

/*
    Given an 206 Partial Request HTTP response, change the header fields to make
    it appear as a 200 OK response
*/
void setResponse206To200(HttpResponse* httpResponse, HttpRequest* httpRequest) {
    httpResponse->statusCode = "200";
    httpResponse->statusPhrase = "OK";
    httpResponse->headers["Content-Length"] = std::to_string(httpRequest->contentLength);
    httpResponse->headers.erase("Content-Range");
}


/*
    The proxy will listen for clients wanting to send data over the proxy's port
    and manipulate the timing of the HTTP response from the server depending on
    the requested resource
*/
void listenForClient(int listeningSocket) {
    int status;
    status = listen(listeningSocket,5); // queuelen of 5
    // 5 active participants can wait for a connection if server is busy
    if (status == -1) {
        perror("listen() call failed");
        exit(-1);
    }

    while (1) {

        // Accept a connection
        int client_sock;
        client_sock = accept(listeningSocket, NULL, NULL);
        if (client_sock < 0) {
            printf("Error in accept()\n");
            exit(-1);
        }

        printf("\n***\nAccepted a connection\n");

        // zero out the buffers used to get data from client and to send data to the server
        ssize_t count;
        char rcvFromClient[1024];
        char rcvFromServer[100000];
        memset(&rcvFromClient,0,sizeof(rcvFromClient));
        memset(&rcvFromServer,0,sizeof(rcvFromServer));


        struct sockaddr_in addr_server;
        struct hostent *server;

        //Receive data from client and parse the HTTP Request
        // If for some reason, the server did not send back a message, try the whole request-response process again        
        ssize_t bytes_rcv = receiveData(client_sock, rcvFromClient, sizeof(rcvFromClient));
        if (bytes_rcv == 0) {
            close(client_sock);
            continue;
        }
        printf("Received %lu bytes from client\n", bytes_rcv);
        std::cout << "\n## Message from client ##\n" << rcvFromClient << std::endl;

        HttpRequest httpRequest;
        parseRequest(&httpRequest, rcvFromClient);
        printf("Host: %s\n", httpRequest.headers["Host"].c_str());
        printf("URL: %s\n", httpRequest.url.c_str());


        // In case the host header is supplied with the port as well, split up the host and port
        size_t colonIndex = httpRequest.headers["Host"].find(":");
        std::string host = httpRequest.headers["Host"];
        int port = 80;
        if (colonIndex != std::string::npos) {
            size_t hostLen = host.length();
            host = httpRequest.headers["Host"].substr(0,colonIndex-1);
            port = stoi(httpRequest.headers["Host"].substr(colonIndex+1, hostLen - colonIndex -1));
            printf("Host: %s \tPort: %d\n", host.c_str(), port);
        } 

        // Initialize the TCP socket for connecting to the desired web server
        server = gethostbyname(host.c_str());

        bzero((char *) &addr_server, sizeof(addr_server));
        addr_server.sin_family = AF_INET;
        bcopy((char *) server->h_addr, (char *) &addr_server.sin_addr.s_addr, server->h_length);
        addr_server.sin_port = htons(port);

        int server_sock = createAndConnectServerSocket(&addr_server, server, sizeof(addr_server));
        printf("%s\n", "Connected to web server");

        // Modify the HTTP request from client with details from a HEAD request
        HttpResponse headResponse;
        std::string headRequest = "";
        createHeaderRequest(&httpRequest, &headRequest);
        sendMessageToSocket(server_sock, headRequest.c_str());

        // If for some reason, the server did not send back a message, try the whole request-response process again
        bytes_rcv = receiveData(server_sock, rcvFromServer, sizeof(rcvFromServer));
        if (bytes_rcv == 0) {
            close(server_sock);
            close(client_sock);
            continue;
        }

        parseResponse(&headResponse, rcvFromServer);
        putHeadInfoIntoRequest(&headResponse, &httpRequest);
        printf("Content-Type: %s\n",httpRequest.contentType.c_str());
        printf("Content-Length: %lu\n", httpRequest.contentLength);        

        std::string headResponseCode = headResponse.statusCode;

        if (headResponseCode.compare("200") == 0) {
            printf("Server responded with 200 OK so begin another HTTP request\n");
            // modify the request to include range ONLY if it matches the GET .*\.html HTTP/.* regex and
            // ONLY if it allows for range requests
            if (matchGetHtml(&httpRequest) && httpRequest.canSendRangeRequests) {
               updateRangeInHeaders(&httpRequest); 
            }

            server_sock = createAndConnectServerSocket(&addr_server, server, sizeof(addr_server));
            httpRequest.createRequest(); 

            sendMessageToSocket(server_sock,httpRequest.request.c_str());


        // If for some reason, the server did not send back a message, try the whole request-response process again
            ssize_t bytes_rcv = receiveData(server_sock, rcvFromServer, sizeof(rcvFromServer));  
            if (bytes_rcv == 0) {
                close(server_sock);
                close(client_sock);
                continue;
            }
            HttpResponse httpResponse;
            parseResponse(&httpResponse, rcvFromServer);   
            printf("Received %lu bytes from HTTP GET Request to server\n", bytes_rcv);


            HttpResponse intermediateResponse;

            // Send multiple range requests to get the rest of the full request body ONLY if it's a GET HTML request,
            // ONLY if the server can accept range requests and ONLY if the current range is less than or equal
            // to the content length
            while (matchGetHtml(&httpRequest) 
                && httpRequest.canSendRangeRequests
                && httpRequest.startRange <= httpRequest.contentLength ) {
                updateRangeInHeaders(&httpRequest);

                // reconnect socket after the connection with server closes send HTTP request with updated range to server
                server_sock = createAndConnectServerSocket(&addr_server, server, sizeof(addr_server));
                httpRequest.createRequest();
                sendMessageToSocket(server_sock,httpRequest.request.c_str());

                // Receive response from web browser and add to the HTTP response body (to be sent to the client)
                receiveData(server_sock, rcvFromServer, sizeof(rcvFromServer));    
                parseResponse(&intermediateResponse, rcvFromServer);
                httpResponse.body.append(intermediateResponse.body);
            }

            // If the proxy asked for multiple range requests, update the partial HTTP response to code 200 instead of 206
            if (httpResponse.statusCode.compare("206") == 0 && httpRequest.canSendRangeRequests) {
                setResponse206To200(&httpResponse,&httpRequest);
                printf("%s\n", "Set 206 Partial Request --> 200 OK");
            }
            httpResponse.createResponse();

            // Send HTTP response to client        
            sendMessageToSocket(client_sock,httpResponse.response.c_str());
            printf("%s\n", "Sent HTTP response back to client");

        } else {
            // Do nothing, just send back the message from the HEAD request for responses 304, 301, 302, 404
            headResponse.createResponse();            
            printf("\nServer responded with %s %s\n", headResponseCode.c_str(), headResponse.statusPhrase.c_str());
            sendMessageToSocket(client_sock,headResponse.response.c_str());
        }

        // close all sockets for this transaction
        close(server_sock);
        close(client_sock);
        printf("%s\n***\n", "Closed all connection sockets");
        
   }
}

/*
    When the user exits the program by typing CTRL-C, close the listening socket before exiting
*/
void my_handler(int s){
    close(listeningSocket);
    printf("%s\n", "closed listening socket");
    exit(0);
}

int main() {

    // Set up safe exit out of program by closing all open sockets
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = my_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    // set up the proxy address and port on which it will receive client requests
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(4545);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    // start listening for client connections
    listeningSocket = setupListeningSocket(&address);
    listenForClient(listeningSocket);

    return 0;
}


