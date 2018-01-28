#include <stdio.h>
#include <stdlib.h>

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


class HttpRequest {
    public:
        std::string method;
        std::string url;
        std::string host;
        std::string protocol;
        std::string body;
        std::map<std::string,std::string> headers;
        std::string request;
        unsigned long contentLength;
        unsigned long startRange;

    void createRequest() {
        request = (method + " " + url + " " + protocol + "\r\n");
        for (std::map<std::string,std::string>::iterator it = headers.begin(); it != headers.end(); ++it) {
            request.append(it->first + ": " + it->second + "\r\n");
        }
        request.append("\r\n"); // blank line before request body
        request.append(body);
        request.append("\0");

    }

    void removeHeader(std::string* header) {
        std::map<std::string,std::string>::iterator it;
        it = headers.find(*header);
        if (it != headers.end()) {
            headers.erase(*header);
        }
    }
};

class HttpResponse {
    public:
        std::string protocol;
        std::string statusCode;
        std::string statusPhrase;
        std::string body;
        std::map<std::string,std::string> headers;
        std::string response;

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

void trimTrailingWhitespace(char* aString) {
    int i = strlen(aString) - 1;
    while (i >= 0 && isspace(aString[i])) {
        aString[i] = '\0';
        i--;
    }
}

void trimTrailingNonPrintable(char* aString) {
    int i = strlen(aString) - 1;
    while (i >= 0 && !isprint(aString[i])) {
        aString[i] = '\0';
        i--;
    }
}

void trimTrailingNonPrintable(std::string* aString) {
    int i = aString->length() - 1;
    while (i >= 0 && isprint((int)(*aString)[i]) == 0) {
        aString->pop_back();
        i--;
    }
}


int setupListeningSocket(sockaddr_in *address) {
    // create listening socket
    int listeningSocket;
    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == -1) {
        perror("socket() call failed");
        exit(-1);
    }

    // bind socket port to server
    int status;
    status = bind(listeningSocket, (struct sockaddr*) address, sizeof(struct sockaddr_in));
    if (status == -1) {
        perror("bind() call failed");
        exit(-1);
    }
    return listeningSocket;

}

void sendMessageToSocket(int socket, const char* msgToSend) {
    ssize_t c_send_status;
    c_send_status = send(socket, msgToSend, strlen(msgToSend), 0);
    if (c_send_status < 0) {
        perror("Error in send() call");
        exit(-1);
    }
}

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


void updateRangeInHeaders(HttpRequest* httpRequest) {
    if (httpRequest->startRange > httpRequest->contentLength) {
        return;
    }

    unsigned long startRange = httpRequest->startRange;
    unsigned long endRange = startRange + 99;

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

    // increase the startRange to the next 100 bytes
    httpRequest->startRange += 100;
}

void removePossible304Queries(HttpRequest* httpRequest) {
    std::string ifModifiedSince ("If-Modified-Since");
    httpRequest->removeHeader(&ifModifiedSince);
    std::string ifNoneMatch ("If-None-Match");
    httpRequest->removeHeader(&ifNoneMatch);
}

void createHeaderRequest(HttpRequest* httpRequest, std::string* headRequest) {
    std::string headerField = "Host";
    headRequest->append("HEAD " + httpRequest->url + " " + httpRequest->protocol + "\r\n");
    headRequest->append("Host: " + httpRequest->headers[headerField] + "\r\n");
    headRequest->append("Connection: Keep-Alive\r\n");    
    headRequest->append("Keep-Alive: timeout=305, max=1000\r\n");
    headRequest->append("Cache-Control: max-age=0\r\n");
    headRequest->append("\r\n");
}

void parseResponse(HttpResponse* httpResponse, char* buffer) {
    // initialize the response
    std::string entireResponse (buffer);
    httpResponse->response = entireResponse;   

    char* response = strtok(buffer, "\r\n");
    char responseCopy[1024];
    strcpy(responseCopy,response);

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
        trimTrailingNonPrintable(remainder);
        httpResponse->body = remainder;
    }

    char* protocol = strtok(responseCopy, " ");
    char* statusCode = strtok(NULL, " ");
    char* statusPhrase = strtok(NULL, "\n");

    // set HTTPRequest method, url and protocol
    httpResponse->protocol = protocol;
    httpResponse->statusCode = statusCode;
    httpResponse->statusPhrase = statusPhrase;

}

void getContentLengthForRequest(HttpRequest* httpRequest, int server_sock) {
    std::string headRequest = "";
    createHeaderRequest(httpRequest, &headRequest);
    sendMessageToSocket(server_sock, headRequest.c_str());

    char serverResponse [10000];

    ssize_t bytes_rcv = recv(server_sock, &serverResponse, sizeof(serverResponse), 0);
    if (bytes_rcv < 0) {
        perror("Error in reqeusting HEAD from browser\n");
        exit(-1);
    }
    HttpResponse httpResponse;
    parseResponse(&httpResponse, serverResponse);
    unsigned long contentLength = strtoul(httpResponse.headers["Content-Length"].c_str(),NULL,0);
    httpRequest->contentLength = contentLength;
    httpRequest->startRange = 0;

}

void parseRequest(HttpRequest* httpRequest, char* buffer) {
    
    // make the initial request string what was in the buffer
    char* request = strtok(buffer, "\r\n");
    char requestCopy[1024];
    strcpy(requestCopy,request);

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

// Check if a request matches the GET .*\.html HTTP/.* regex
bool matchGetHtml(HttpRequest* httpRequest) {
    std::regex htmlRegex ("(.)*\\.html");
    std::regex httpRegex ("HTTP/\\d\\.\\d");

    return httpRequest->method.compare("GET") == 0 &&
            std::regex_match(httpRequest->url,htmlRegex) &&
            std::regex_match(httpRequest->protocol,httpRegex);
}

void addRange(HttpRequest* httpRequest, int server_sock) {
    // modify the request to include range ONLY if it matches the GET .*\.html HTTP/.* regex
    if (matchGetHtml(httpRequest)) {
        getContentLengthForRequest(httpRequest, server_sock);
        updateRangeInHeaders(httpRequest);
    }
    
}


void receiveData(int socket, char* rcvMsg, ssize_t sizeOfRcvMsg) {
    ssize_t bytes_rcv;
    bytes_rcv = recv(socket, &(*rcvMsg), sizeOfRcvMsg, 0);
    if (bytes_rcv < 0) {
        perror("Error in receiving\n");
        exit(-1);
    }

}

void setResponse206To200(HttpResponse* httpResponse, HttpRequest* httpRequest) {
    httpResponse->statusCode = "200";
    httpResponse->statusPhrase = "OK";
    httpResponse->headers["Content-Length"] = std::to_string(httpRequest->contentLength);
    httpResponse->headers.erase("Content-Range");
}

void listenForClient(int listeningSocket) {
    int status;
    status = listen(listeningSocket,5); // queuelen of 5
    // 5 active participants can wait for a connection if server is busy
    if (status == -1) {
        perror("listen() call failed");
        exit(-1);
    }
    // int i = 0;
    // while (i < 4) {

        /* Accept a connection */
        int client_sock;
        client_sock = accept(listeningSocket, NULL, NULL);
        if (client_sock < 0) {
            printf("Error in accept()\n");
            exit(-1);
        }

        ssize_t count;
        char rcvFromClient[1024];
        char rcvFromServer[10000];

        struct sockaddr_in addr_server;
        struct hostent *server;

        /* Receive data from client */
        receiveData(client_sock, rcvFromClient, sizeof(rcvFromClient));

        // Parse HTTP Request from client
        HttpRequest httpRequest;
        parseRequest(&httpRequest, rcvFromClient);

        // Creating the TCP socket for connecting to the desired web server
        // Address initialization
        server = gethostbyname(httpRequest.headers["Host"].c_str());

        bzero((char *) &addr_server, sizeof(addr_server));
        addr_server.sin_family = AF_INET;
        bcopy((char *) server->h_addr, (char *) &addr_server.sin_addr.s_addr, server->h_length);
        addr_server.sin_port = htons(80);

        int server_sock = createAndConnectServerSocket(&addr_server, server, sizeof(addr_server));

        addRange(&httpRequest,server_sock);
        removePossible304Queries(&httpRequest);

        server_sock = createAndConnectServerSocket(&addr_server, server, sizeof(addr_server));
        httpRequest.createRequest(); 

        // send HTTP request to server
        sendMessageToSocket(server_sock,httpRequest.request.c_str());

        // Receive response from web browser
        receiveData(server_sock, rcvFromServer, sizeof(rcvFromServer));    
        HttpResponse httpResponse;
        parseResponse(&httpResponse, rcvFromServer);   
        std::cout << "************\n\nappending body\n" << httpResponse.body << "\n\n************";


        // Keep asking for ranges of requests
        HttpResponse intermediateResponse;

        while (matchGetHtml(&httpRequest) && httpRequest.startRange <= httpRequest.contentLength) {
            updateRangeInHeaders(&httpRequest);
            // reconnect server socket after the connection closes in addRange
            server_sock = createAndConnectServerSocket(&addr_server, server, sizeof(addr_server));
            httpRequest.createRequest();

            // send HTTP request to server
            sendMessageToSocket(server_sock,httpRequest.request.c_str());

            // Receive response from web browser
            receiveData(server_sock, rcvFromServer, sizeof(rcvFromServer));    

            // add to the response body
            parseResponse(&intermediateResponse, rcvFromServer);
            std::cout << "************\n\nappending body\n" << intermediateResponse.body << "\n\n************";
            httpResponse.body.append(intermediateResponse.body);
        }
        setResponse206To200(&httpResponse,&httpRequest);
        httpResponse.createResponse();
        printf("httpResponse is after parsing\n%s\n", httpResponse.response.c_str());
        // Send HTTP response to client        
        sendMessageToSocket(client_sock,httpResponse.response.c_str());
        
        close(server_sock);
        close(client_sock);
    // }
}




int main() {
    // listening on port 4545 for requests from web browser
    struct sockaddr_in address;
    memset(&address,0,sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(4545);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    int listeningSocket;
    listeningSocket = setupListeningSocket(&address);
    listenForClient(listeningSocket);

    close(listeningSocket);
    return 0;
}


