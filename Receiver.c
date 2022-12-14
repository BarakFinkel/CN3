/*
        TCP/IP client
*/

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define server_port 5060
#define SERVER_IP_ADDRESS "127.0.0.1"
#define BUFFER_SIZE 1024

int main() {
    
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == -1) {
        printf("Could not create socket : %d", errno);      // If the socket creation failed, print the error and exit main.   
        return -1;
    }

    struct sockaddr_in serverAddress;                       // Using the imported struct of a socket address using the IPv4 module.
    memset(&serverAddress, 0, sizeof(serverAddress));       // Resetting it to default values.

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);                                              // Changing little endian to big endian.
    int rval = inet_pton(AF_INET, (const char *)SERVER_IP_ADDRESS, &serverAddress.sin_addr);  // convert IPv4 and IPv6 addresses from text to binary form
    // e.g. 127.0.0.1 => 0x7f000001 => 01111111.00000000.00000000.00000001 => 2130706433
    if (rval <= 0) {
        printf("inet_pton() failed");
        return -1;
    }

    // Make a connection to the server with socket SendingSocket.

    int connectResult = connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)); 
    if (connectResult == -1) {
        printf("connect() failed with error code : %d", errno);   
        close(sock);
        return -1;
    }

    printf("connected to server\n");

    // Sends some data to server
    char buffer[BUFFER_SIZE] = {'\0'};
    char message[] = "Hello, from the Client\n";
    int messageLen = strlen(message) + 1;
    
    int bytesSent = send(sock, message, messageLen, 0);

    if (bytesSent == -1) {
        printf("send() failed with error code : %d", errno);
    } else if (bytesSent == 0) {
        printf("peer has closed the TCP connection prior to send().\n");
    } else if (bytesSent < messageLen) {
        printf("sent only %d bytes from the required %d.\n", messageLen, bytesSent);
    } else {
        printf("message was successfully sent.\n");
    }
    
    // Receive data from server
    char bufferReply[BUFFER_SIZE] = {'\0'};
    int bytesReceived = recv(sock, bufferReply, BUFFER_SIZE, 0);
    if (bytesReceived == -1) {
        printf("recv() failed with error code : %d", errno);
    } else if (bytesReceived == 0) {
        printf("peer has closed the TCP connection prior to recv().\n");
    } else {
        printf("received %d bytes from server: %s\n", bytesReceived, bufferReply);
    }
    
    close(sock);
    return 0;
}