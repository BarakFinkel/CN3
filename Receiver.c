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
#include <netinet/tcp.h>

#define server_port 5060
#define server_ip "127.0.0.1"
#define buffer_size 1024


int main() {
    
    int temp = 0;                                           // Setting a temporary variable to help check for errors throughout the program.

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);   // Creating a socket using the IPv4 module.

    if (sock == -1) {
        printf("Error : Listen socket creation failed.\n"); // If the socket creation failed, print the error and exit main.   
        return -1;
    }

    struct sockaddr_in serverAddress;                       // Using the imported struct of a socket address using the IPv4 module.
    memset(&serverAddress, 0, sizeof(serverAddress));       // Resetting it to default values.

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(server_port);                                              // Changing little endian to big endian.
    int temp = inet_pton(AF_INET, (const char *)server_ip, &serverAddress.sin_addr);          // Converting the IP address from string to binary.

    if (temp <= 0) {
        printf("Error: inet_pton() failed.\n");                                                         // If the conversion failed, print the error and exit main.
        return -1;
    }


    // Make a connection to the server with socket SendingSocket.

    int connectResult = connect(sock, (struct sockaddr *)&serverAddress, sizeof(serverAddress)); 
    if (connectResult == -1) {
        printf("Error: Connecting to server failed.\n");   
        close(sock);
        return -1;
    }

    printf("Connected to server!\n");

    char buffer[buffer_size] = {0};
    char ACK = "ACK";

    int size = recvFileSize(sock, buffer, ACK);
    
    if (size < 0) 
    {
        printf("Error : File size receiving failed.\n");
        close(sock);
        return -1;
    }

    printf("Size received successfully!\n");

    FILE *fp = fopen("lotr.txt", "w"); 
    if (fp == NULL) 
    {
        printf("Error : File opening failed.\n");
        close(sock);
        return -1;
    }

    while(1)
    {

        while(1)
        {
            if(setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, "reno", 6) < 0)         
            {
                printf("Error : Failed to set congestion control algorithm to reno.\n");    // If the setting of the congestion control algorithm failed, print an error and exit main.
                close(sock);
                return -1;
            }        
            
            int size = recv(sock, buffer, buffer_size, 0);    // Receiving data from the server.
            
            if(size == -1)
            {
                printf("Error : Receive failed.\n");    // If the receive failed, print an error and exit main.
                close(sock);
                return -1;
            }

            else if (size == 0)
            {
                printf("Error : Server's socket is closed, couldn't receive anything.\n");    // If the receive failed, print an error and exit main.
                close(sock);
                return -1;
            }

            else if (strcmp(buffer, "SEND KEY") == 0)
            {
                temp = sendKey(sock, buffer);

                if(temp == -1)
                {
                    printf("Error : Key sending failed.\n");
                    close(sock);
                    return -1;
                }
            }

            else if (strcmp(buffer, "FIN") == 0)
            {
                temp = getFIN(sock, buffer, ACK);

                if(temp == -1)
                {
                    close(sock);
                    return -1;
                }

                break;
            }

            else {

                temp = writeChunk(fp, sock, size, buffer, ACK);

                if(temp == -1)
                {
                    close(sock);
                    return -1;
                }
            }
            
        }

    

    close(sock);
    return 0;

    }
}


int recvFileSize(int sock, char *buffer, char *ACK)
{
    
    int bytes = recv(sock, buffer, buffer_size, 0);
    
    if (bytes == -1) 
    {
        printf("Error : Receive failed.\n");
        return -1;
    } 
    
    else if (bytes == 0) 
    {
        printf("Error : Client's socket is closed, couldn't receive anything.\n");
        return -1;
    } 

    int sendACK = send(sock, ACK, 4, 0);

    if (sendACK == -1) 
    {
        printf("Error : Ack sending failed.\n");
        return -1;
    } 
    
    else if (sendACK == 0) 
    {
        printf("Error : Client's socket is closed, couldn't send to it.\n");
        return -1;
    } 

    else if (sendACK != 4) 
    {
        printf("Error : Client received a corrupted ACK.\n");
        return -1;
    }

    char* end;
    long val = strtol(buffer, end, 10);
    int size = (int)val;

    bzero(buffer, strlen(buffer) + 1);

    return size;
}


int sendKey(int sock, char *buffer)
{
    int key = 1714 ^ 6521;                                                    // Calculating the key.
    sprintf(buffer, "%d", key);                                               // Converting the key into the buffer.

    int sendResult = send(sock, buffer, strlen(buffer) + 1, 0);

    if(sendResult == -1)
    {
        printf("Error : Sending failed.\n");
        return -1;
    }

    else if (sendResult == 0)
    {
        printf("Error : Server's socket is closed, couldn't send to it.\n");
        return -1;
    }

    else if (sendResult != strlen(buffer) + 1)
    {
        printf("Error : Server received a corrupted buffer.\n");
        return -1;
    } 
    
    bzero(buffer, strlen(buffer) + 1);

    int recvResult = recv(sock, buffer, buffer_size, 0);

    if(recvResult == -1)
    {
        printf("Error : Receive failed.\n");
        close(sock);
        return -1;
    }

    else if (recvResult == 0)
    {
        printf("Error : Server's socket is closed, couldn't receive anything.\n");
        close(sock);
        return -1;
    }

    else if(strcmp(buffer, "OK") != 0)
    {
        printf("Error : Key doesn't match the server's.\n");
        close(sock);
        return -1;
    }
    
    bzero(buffer, strlen(buffer) + 1);

    return 0;
}


int getFIN(int sock, char *buffer, char *ACK)
{
    int sendResult = send(sock, ACK, 4, 0);
    
    if (sendResult == -1) 
    {
        printf("Error : Receive failed.\n");
        return -1;
    } 
    
    else if (sendResult == 0) 
    {
        printf("Error : Client's socket is closed, couldn't receive anything.\n");
        return -1;
    } 

    if (sendResult != 1)
    {
        printf("Error : Server received a corrupted buffer.\n");
        return -1;
    }
}


int writeChunk(FILE *fp, int sock, int size, char *buffer, char *ACK) 
{        
    fwrite(buffer, size, 1, fp);    // Writing the buffer to the file.

    int sendResult = send(sock, ACK, 4, 0);    // Sending an ACK to the server.
    
    if (sendResult == -1)
    {
        printf("Error : Sending failed.");    // If the send failed, print an error and exit main.
        return -1;
    }

    else if (sendResult == 0)
    {
        printf("Error : Server's socket is closed, couldn't send to it.");    // If the send failed, print an error and exit main.
        return -1;
    }

    else if (sendResult != size)
    {
        printf("Error : Server received a corrupted buffer.");     // If the send failed, print an error and exit main.
        return -1;
    }

    bzero(buffer, size);          // Resetting the buffer.

    return 0;
}


int getEND(int sock, char *buffer)     // Note to myself : This function needs to be worked on.
{
    int bytes = recv(sock, buffer, buffer_size, 0);
    
    if (bytes == -1) 
    {
        printf("Error : Receive failed.\n");
        return -1;
    } 
    
    else if (bytes == 0) 
    {
        printf("Error : Client's socket is closed, couldn't receive anything.\n");
        return -1;
    } 

    if (strcmp(buffer, "END") == 0)
    {
        return 0;
    }

    return -1;
}