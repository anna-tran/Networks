//
// Created by Anna Tran on 2018-01-24.
//

#ifndef NETWORKS_SLOXY_H
#define NETWORKS_SLOXY_H

#endif //NETWORKS_SLOXY_H
int setupListeningSocket(sockaddr_in *address);
int setupSendingSocket(sockaddr_in *address);

void listenForClient(int listeningSocket);

void sendDataToServer(sockaddr_in *address, char* sendToServerMsg, char* rcvFromServerMsg);
HttpRequest& parseRequest(char* buffer) {