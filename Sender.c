#include <stdio.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define server_port 5060 // The port that the server listens
#define buffer_size 1024 // The size of the file we want to send.
#define ACK_size 1       // The size of the ACK we want to send.

int main()
{

    signal(SIGPIPE, SIG_IGN); // Helps preventing crashing when closing the socket later on.

    int listenSock = -1;
    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // Setting the listening socket
    if (listenSock == -1)
    {
        printf("Listen socket creation failed, error : %d", errno); //* if the creation of the socket failed,
        return 0;                                                   //* print the error and exit main.
    }

    int reuse = 1;                                                                   //* Reusing a previously created server
    int ret = setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)); //* socket, otherwise does nothing.
    if (ret < 0)
    {
        printf("setsockopt() failed, error : %d", errno);      // If the socket's reuse failed,
        return 0;                                              // print the error and exit main.
    }

    struct sockaddr_in server_address;                  // Using the imported struct of a socket address using the IPv4 module.
    memset(&server_address, 0, sizeof(server_address)); // Resetting it to default values.

    server_address.sin_family = AF_INET;                // Server address is type IPv4.
    server_address.sin_addr.s_addr = INADDR_ANY;        // Get any address that tries to connect.
    server_address.sin_port = htons(server_port);       // Set the server port to the defined 'server_port'.

    int binded = bind(listenSock, (struct sockaddr *)&server_address, sizeof(server_address)); // Bind the socket to a port and IP.
    if (binded == -1)
    {
        printf("Binding failed, error : %d", errno); // If the binding failed,
        close(listenSock);                           // print the corresponding error, close the socket and exit main.
        return -1;
    }

    printf("Binding complete!\n");

    int listenResult = listen(listenSock, 3); // Start listening, and set the max queue for awaiting client to 3.
    if (listenResult == -1)
    {
        printf("Listening failed, error : %d", errno); // If listen failed,
        close(listenSock);                             // print the corresponding error, close the socket and exit main.
        return -1;
    }

    // Accept and incoming connection
    struct sockaddr_in client_address;                 // Using the imported struct of a socket address using the IPv4 module.
    socklen_t client_add_len = sizeof(client_address); // Setting the size of the address to match what's needed.

    while (1)
    {
        printf("Waiting for a connection...\n");
        
        memset(&client_address, 0, sizeof(client_address));                                       // Resetting address struct to default values.
        client_add_len = sizeof(client_address);                                                  // Setting the size of the address to match what's needed.
        int clientSock = accept(listenSock, (struct sockaddr *)&client_address, &client_add_len); // Accepting the clients request to connect.

        if (clientSock == -1)
        {
            printf("listen failed with error code : %d", errno); // If accept failed,
            close(listenSock);                                   // print an error, close the socket and exit main.
            return -1;
        }

        printf("Connected to client!\n");

        FILE *fp = fopen("./test.txt", "r");   // Opening the file to send.
        if (fp == NULL)
        {
            printf("File open error");         // If the file failed to open, print an error and exit main.
            return -1;
        }
  
        char buffer[buffer_size] = {0};        // Setting the buffer size to the defined 'buffer_size'.
        int size = file_size(fp);              // Getting the size of the file, and returning the pointer 'fp' to the beginning of the file.
        int counter = 0;                       // Setting a counter to keep track of how many bytes have been sent.

        // Setting the congestion control algorithm to reno:

        while(1){

        if(setsockopt(listenSock, IPPROTO_TCP, TCP_CONGESTION, "reno", 6) < 0)         
        {
            printf("Error : Failed to set congestion control algorithm to reno.");    // If the setting of the congestion control algorithm failed, print an error and exit main.
            close(clientSock);
            close(listenSock);
            return -1;
        }

        // Sending the 1st part of the file:

        counter = sendFile(fp, clientSock, listenSock, size/2, counter, buffer);      

        if(counter != size/2)                                 // If the sending of the file failed, print an error and exit main.
        {
            printf("Error : File sent is corrupted.");
            close(clientSock);
            close(listenSock);
            return -1;
        }

        // Sending and receiving key packets:

        int serverKey[10] = {0};                    // Setting the server key buffer to the size of the key.
        int key = 1714 ^ 6521;                      // Calculating the key.
        sprintf(serverKey, "%d", key);              // Converting the key to a string and storing it in the keyBuffer.
        int clientKey[10] = {0};                    // Setting the client key buffer.

        int keyRequest = getKey(clientSock, listenSock, clientKey);       // Receiving the key from the client.
        
        if( (strcmp(serverKey, clientKey)) != 0 )   // Comparing the server and client's keys.
        {
            printf("Error : Keys don't match.");    // If the keys don't match, print an error and exit main.
            close(clientSock);
            close(listenSock);
            return -1;
        }
        
        // printf("Keys match!\n");

        bzero(serverKey, 10);                 // Resetting the keyBuffer to default values.
        bzero(clientKey, 10);                 // Resetting the clientKey to default values.

        printf("First part of the file sent successfully!\n");

        // Setting the congestion control algorithm to cubic:
        
        if(setsockopt(listenSock, IPPROTO_TCP, TCP_CONGESTION, "cubic", 6) < 0)        
        {
            printf("Error : Failed to set congestion control algorithm to cubic.");    // If the setting of the congestion control algorithm failed, 
            close(clientSock);                                                         // print an error, close the sockets and exit main.
            close(listenSock);
            return -1;
        }

        // Sending the 2nd part of the file:

        counter = sendFile(fp, clientSock, listenSock, size/2, counter, buffer);       // Sending the 2nd part of the file.

        if(counter != size)                                                            
        {                                                                              
            printf("Error : File sent is corrupted.");                                 // If the sending of the file failed,
            close(clientSock);                                                         // print an error, close the sockets and exit main.
            close(listenSock);
            return -1;
        }

        printf("Second part of the file sent successfully!\n");

        // Sending the client that the server is done sending the file:

        int fin = sendFIN(clientSock, listenSock, buffer);             // Sending the fin packet.
        
        if(fin == -1)                                                  
        {
            printf("Error : Failed to send fin packet.");              // If the sending of the fin packet failed, print an error and exit main.
            close(clientSock);
            close(listenSock);
            return -1;
        }

        char message[1] = {0};
        
        printf("Do you want to send the file again? (y/*)\n");
        scanf("%s", message);                                         // Asking the sender if he wants to send the file again.

        if(strcmp(message, "y") == 0)                                 // If the sender wants to send the file again, send the file again.
        {
            counter = 0;                                              // Resetting the counter to 0.
            fseek(fp, 0, SEEK_SET);                                   // Returning the pointer 'fp' to the beginning of the file.
        }

        else                                                          // If the sender doesn't want to send the file again, break the loop.
        {
            break;
        }        

        // ***Note to myself***: I should put everything in a loop, so that the sender can send the file again if he wants to.

        }



        // Finishing up:

        fclose(fp);            // Closing the file.
        close(clientSock);     // Closing the client socket.
    }

    close(listenSock);
    return 0;
}



//_____function sendFIN: This function lets the client know that the server finished sending the file, and gets the client's ACK in return_____//



int sendFIN(int clientSock, int listenSock, int *buffer)
{
    int sendRequest = send(clientSock, "FIN", 4, 0);   // Sending the buffer to the client.

    if (sendRequest == -1)
    {
        printf("Error : Sending dropped.");            // If the send failed, print an error and exit main.
        close(clientSock);
        close(listenSock);
        return -1;
    }

    else if (sendRequest == 0)
    {
        printf("Error : Client's socket is closed, couldn't send to it.");    // If the send failed, print an error and exit main.
        close(clientSock);
        return -1;
    }

    else if (sendRequest != 10)
    {
        printf("Error : Client received a corrupted buffer.");     // If the send failed, print an error and exit main.
        close(clientSock);
        return -1;
    }

    // If the program reaches here, then the FIN was sent successfully.

    //_____Receiving the client's FINACK:_____//

    int recvResult = recv(clientSock, buffer, 1, 0); // Receiving the client's response.

    if (recvResult < 0) 
    {
        printf("Error : Receiving failed.");         // If the receive failed, print an error and exit main.
        close(clientSock);
        close(listenSock);
        return -1;
    }
            
    else if (recvResult == 0) 
    {
        printf("Error : Client's socket is closed, nothing to receive.");   // If the receive failed, print an error and exit main.
        close(clientSock);
        return -1;
    }

    else if (buffer[0] != 1)   // Checking if the bytes send and the bytes received are equal.
    {
        printf("Error : Server received a corrupted buffer.");   // If they aren't, print an error and exit main.
        close(clientSock);
        return -1;
    }

    // if the program reaches here, the 'i' ACK was sent successfully.

    bzero(buffer, 1);   // Resetting the buffer to default values.
    return 0;
}



//_____getKey function: asks the client for a key and receives it_____// 



int getKey(int clientSock, int listenSock, int clientKey[10])
{
    int sendRequest = send(clientSock, "SEND KEY", 9, 0);   // Sending the buffer to the client.

    if (sendRequest == -1)
    {
        printf("Error : Sending dropped.");    // If the send failed, print an error and exit main.
        close(clientSock);
        close(listenSock);
        return -1;
    }

    else if (sendRequest == 0)
    {
        printf("Error : Client's socket is closed, couldn't send to it.");    // If the send failed, print an error and exit main.
        close(clientSock);
        return -1;
    }

    else if (sendRequest != 10)
    {
        printf("Error : Client received a corrupted buffer.");     // If the send failed, print an error and exit main.
        close(clientSock);
        return -1;
    }

    // If the program reaches here, then the request was sent successfully.

    // Receiving the client's key:

    int recvKey = recv(clientSock, clientKey, 10, 0); // Receiving the client's response.

    if (recvKey < 0) 
    {
        printf("Error : Receiving failed.");   // If the receive failed, print an error and exit main.
        close(clientSock);
        close(listenSock);
        return -1;
    }
            
    else if (recvKey == 0) 
    {
        printf("Error : Client's socket is closed, nothing to receive."); // If the receive failed, print an error and exit main.
        close(clientSock);
        return -1;
    }

    else if (recvKey != 10)   // Checking if the bytes send and the bytes received are equal.
    {
        printf("Error : Server received a corrupted buffer.");   // If they aren't, print an error and exit main.
        close(clientSock);
        return -1;
    }

    // if the program reaches here, the client's key was received, and the main will check it's validity.

    return 0;
}



//_____sendFile function: sending the file to the client and receiving an ACK from the client_____//



int sendFile(FILE *fp, int clientSock, int listenSock, int size, int counter, char buffer[buffer_size]) 
{
    int numofbytes = min(buffer_size, size - counter); // Getting the minimum of the buffer size and the remaining bytes to send.
        
    while (counter < size && fread(buffer, numofbytes, 1, fp) != NULL)
    {

        // Reading the file and storing it in the buffer:

        if ((fread(buffer, numofbytes, 1, fp)) < 0) // Reading the file and storing it in the buffer.
        {
            printf("File read error"); // If the file failed to read, print an error and exit main.
            close(clientSock);
            close(listenSock);
            return -1;
        }
            
        //_____Sending the buffer to the client:_____//

        int sendResult = send(clientSock, buffer, numofbytes, 0);   // Sending the buffer to the client.

        if (sendResult == -1)
        {
            printf("Error : Sending dropped.");    // If the send failed, print an error and exit main.
            close(clientSock);
            return -1;
        }

        else if (sendResult == 0)
        {
            printf("Error : Client's socket is closed, couldn't send to it.");    // If the send failed, print an error and exit main.
            close(clientSock);
            return -1;
        }

        else if (sendResult != numofbytes)
        {
            printf("Error : Client received a corrupted buffer.");     // If the send failed, print an error and exit main.
            close(clientSock);
            return -1;
        }

        // if the program reaches here, the 'i' buffer was sent successfully.

        bzero(buffer, numofbytes);     // Resetting the buffer.

        //_____Receiving the client's ACK:_____//

        int recvResult = recv(clientSock, buffer, 1, 0); // Receiving the client's response.

        if (recvResult < 0) 
        {
            printf("Error : Receiving failed.");   // If the receive failed, print an error and exit main.
            close(clientSock);
            close(listenSock);
            return -1;
        }
            
        else if (recvResult == 0) 
        {
            printf("Error : Client's socket is closed, nothing to receive."); // If the receive failed, print an error and exit main.
            close(clientSock);
            return -1;
        }

        else if (buffer[0] != 1)   // Checking if the bytes send and the bytes received are equal.
        {
            printf("Error : Server received a corrupted buffer.");   // If they aren't, print an error and exit main.
            close(clientSock);
            return -1;
        }

        // if the program reaches here, the 'i' ACK was sent successfully.

        bzero(buffer, 1);          // Resetting the buffer.

        counter += numofbytes;     // Adding the number of bytes sent to the counter.
    }

    return counter;
}

//_____file_size function: returns the size of the file_____//

int file_size(FILE *fp)
{
    int size;
    fseek(fp, 0, SEEK_END);
    size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    return size;
}

//_____min function: returns the minimum of two numbers_____//

int min(int a, int b)
{
    if (a < b) { return a; }
    else       { return b; }
}

