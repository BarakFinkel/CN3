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

// **Function Headers**:

int recvFileSize(int sock, char *buffer, char *ACK);
int sendKey(int sock, char *buffer);
int sendACK(int sock, char *ACK);
int writeChunk(FILE *fp, int sock, int chunkSize, char *buffer, char *ACK);
int sendEND(int sock, char *buffer);

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
    temp = inet_pton(AF_INET, (const char *)server_ip, &serverAddress.sin_addr);          // Converting the IP address from string to binary.

    if (temp <= 0) {
        printf("Error: inet_pton() failed.\n");                                               // If the conversion failed, print the error and exit main.
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
    char *ACK = "ACK";

    printf("Receiving file size...\n");

    int size = recvFileSize(sock, buffer, ACK);

    if (size == -1) {
        printf("Error : File size receiving failed.\n");
        close(sock);
        return -1;
    }

    int counter = 0;                                // The current bit of the file that will be written.
    int chunkSize = 0;                              // The size of the chunk received from the server.

    if (size < 0) 
    {
        printf("Error : File size receiving failed.\n");
        close(sock);
        return -1;
    }

    printf("File size received successfully!\n");


    while(1)
    {

        FILE *fp = fopen("lotr.txt", "w"); 
        if (fp == NULL) 
        {
            printf("Error : File opening failed.\n");
            close(sock);
            return -1;
        }

        if(setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, "reno", 6) < 0)         
        {
            printf("Error : Failed to set congestion control algorithm to reno.\n");    // If the setting of the congestion control algorithm failed, 
            close(sock);                                                                // print an error, close  and exit main.
            return -1;
        }        

        printf("CC algorithm set to reno.\n");

        printf("Receiving the first part of the file...\n");

        while(1)
        {

            temp = recv(sock, buffer, buffer_size, 0);    // Receiving data from the server.
            

            // if temp == -1, then receiving failed with a general error.

            if(temp == -1)
            {
                printf("Error : Receive failed.\n");          // If receiving failed, 
                close(sock);                                  // print an error, close the socket and exit main.
                return -1;
            }


            // if temp == 0, then the server's socket is closed.

            else if (size == 0)
            {
                printf("Error : Server's socket is closed, couldn't receive anything.\n");    // If receiving failed,
                close(sock);                                                                  // print an error, close the socket and exit main.
                return -1;
            }


            // If the received data is "SEND KEY", then the server is sending a key:

            else if (strcmp(buffer, "SEND KEY") == 0)
            {
                printf("First part of the file received successfully!\n");
                
                printf("Sending key...\n");

                temp = sendKey(sock, buffer);

                if(temp == -1)
                {
                    printf("Error : Key sending failed.\n");                                   // if sending the key failed,
                    close(sock);                                                               // print an error, close the socket and exit main.
                    return -1;
                }

                printf("Keys matched!\n");

                // If the counter is half the size of the file, change the congestion control algorithm to cubic to match the sender's algorithm:

                if(setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, "cubic", 6) < 0)           
                {                                                                         
                    printf("Error : Failed to set congestion control algorithm to cubic.\n");    // If the setting of the congestion control algorithm failed, 
                    close(sock);                                                                 // print an error and exit main.
                    return -1;
                }     

                printf("CC algorithm set to cubic.\n");

                printf("Receiving the second part of the file...\n");
            }

            // If the received data is "FIN", then the file has been fully received, so break out of the loop:

            else if (strcmp(buffer, "FIN") == 0)
            {
                temp = sendACK(sock, ACK);

                if(temp == -1)
                {
                    close(sock);
                    return -1;
                }

                printf("Second part of the file received successfully!\n");

                break;
            }

            // If none of the above occur, then the received data is a chunk of the file:

            else 
            {

                chunkSize = temp;

                temp = writeChunk(fp, sock, chunkSize, buffer, ACK);
            
                if(temp == -1)
                {
                    printf("Error : Chunk writing failed.\n");    // If writing the chunk failed,
                    close(sock);                                  // print an error, close the socket and exit main.  
                    return -1;
                }
                
                counter += chunkSize;      // Incrementing the counter by the number of bytes received. 

                // If the counter exceeds the size of the file, print an error and exit main:

                if(counter > size)
                {
                    printf("Error : Received more bytes than the server's file size.\n");
                    close(sock);
                    return -1;
                }
            }
        } 

        printf("File received successfully!\n");


        // At this point, we expect the server to send "AGAIN" to indicate that it is ready to receive the file again,
        // or send "END" to indicate that the server won't be sending the file once more.


        temp = recv(sock, buffer, buffer_size, 0);        // Receiving data from the server.

        if(temp == -1)
        {
            printf("Error : Receive failed.\n");          // If receiving failed, 
            close(sock);                                  // print an error, close the socket and exit main.
            return -1;
        }
        else if (size == 0)
        {
            printf("Error : Server's socket is closed, couldn't receive anything.\n");    // If receiving failed,
            close(sock);                                                                  // print an error, close the socket and exit main.
            return -1;
        }

        // If we get an "AGAIN" message, we'll send an ACK and continue the loop:

        else if (strcmp(buffer, "AGAIN") == 0)
        {   
            temp = sendACK(sock, ACK);

            if(temp == -1)
            {
                close(sock);                                // If sending the ACK failed,
                return -1;                                  // close the socket and exit main.
            }

            printf("Server wishes to send the file again, deleting the file and preparing to receive it again...\n");

            // Closing and removing the file. This is done so that the file can be written from the beginning again:

            fseek(fp, 0, SEEK_SET);           
            fclose(fp);                                 
            int rmv = remove("lotr.txt");               
            counter = 0;

            if(rmv != 0)
            {
                printf("Error : File removal failed.\n");   // If the removal of the file failed,
                close(sock);                                // print an error, close the socket and exit main.
                return -1;
            }

            printf("File deleted successfully!\n");
        }
        
        // If we get an "END" message: we'll send an ACK, also ask to end the connection and break out of the loop afterwards:  

        else if (strcmp(buffer, "END") == 0)
        {
            temp = sendACK(sock, ACK);

            if(temp == -1)
            {
                close(sock);
                return -1;
            }
            
            printf("Server wishes to end the connection, sending END message and closing the socket...\n");

            temp = sendEND(sock, buffer);

            if(temp == -1)
            {
                close(sock);
                return -1;
            }

            break;
        }

        // At this point, we should have gotten either an END message or an AGAIN message. 
        // If we get anything else, print an error, close the socket and exit main:

        else 
        {
            printf("Error : Received unexpected data.\n");
            close(sock);
            return -1;
        }

    }  

    close(sock);
    
    printf("Socket closed, goodbye!\n");

    return 0;

}



// ########################## THE FUNCTIONS: #############################


// recvFileSize() receives the file size from the server and sends an ACK:

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
        printf("Error : Server's socket is closed, couldn't receive anything.\n");
        return -1;
    } 

    // If we reached here, then the server sent us the file size successfully. Send an ACK:

    int ack = sendACK(sock, ACK);

    if (ack == -1) 
    {
        return -1;                  // If sending the ACK failed, we'll return -1 to indicate that the function failed and exit main.
    } 

    char** end;
    long val = strtol(buffer, end, 10);
    int size = (int)val;

    bzero(buffer, (int)(strlen(buffer) + 1));

    return size;
}


// sendKey() sends the key to the server and receives an ACK:

int sendKey(int sock, char *buffer)
{
    int key = 1714 ^ 6521;                                                    // Calculating the key.
    sprintf(buffer, "%d", key);                                               // Converting the key into the buffer.

    int sendResult = send(sock, buffer, (int)(strlen(buffer) + 1), 0);

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

    else if (sendResult != (int)(strlen(buffer) + 1))
    {
        printf("Error : Server received a corrupted buffer.\n");
        return -1;
    } 
    
    bzero(buffer, (int)(strlen(buffer) + 1));

    int recvResult = recv(sock, buffer, 3, 0);

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
    
    bzero(buffer, (int)(strlen(buffer) + 1));

    return 0;
}


// sendACK() sends an ACK to the server:

int sendACK(int sock, char *ACK)
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

    if (sendResult != 4)
    {
        printf("Error : Server received a corrupted buffer.\n");
        return -1;
    }
}


// writeChunk() writes the chunk to the file sent by the server and sends an ACK:

int writeChunk(FILE *fp, int sock, int chunkSize, char *buffer, char *ACK) 
{        
    fwrite(buffer, chunkSize, 1, fp);    // Writing the buffer to the file.

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

    else if (sendResult != 4)
    {
        printf("Error : Server received a corrupted buffer.");     // If the send failed, print an error and exit main.
        return -1;
    }

    bzero(buffer, chunkSize);          // Resetting the buffer.

    return 0;
}


// sendEND() sends an 'END' to end the connection with the server and receives an ACK:

int sendEND(int sock, char *buffer)     
{
    int sendResult = send(sock, "END", 4, 0);
    
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

    if (sendResult != 4)
    {
        printf("Error : Server received a corrupted buffer.\n");
        return -1;
    }

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

    else if(strcmp(buffer, "ACK") != 0)
    {
        printf("Error : Server's ACK not received propperly.\n");
        close(sock);
        return -1;
    }

    bzero(buffer, (int)(strlen(buffer) + 1));

    return 0;
}