/* Nicholas DeLello
 *
 *
 */

#include <stdio.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/time.h>
#include <time.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <zconf.h>
#include <netdb.h>
#include <math.h>
#include <signal.h>
#include "httpShared.h"

int clientSock;
int serverSock;

static void closeSocket(int signalNum)
{
    if (signalNum == SIGINT)
    {
        puts("Closing sockets...");
        close(serverSock);
        close(clientSock);
        serverSock = clientSock = -1;
    }
}


int main(int argc, char** argv)
{

    if (argc > 1)
        puts("Warning: Ignoring all command line arguments passed.");

    struct sigaction sa;
    sa.sa_handler = closeSocket;
    sigemptyset(&sa.sa_mask);
    // Close the socket if you get a SIGINT.
    if (sigaction(SIGINT, &sa, NULL) == -1)
    {
        puts("Closing sockets...");
        close(serverSock);
        close(clientSock);
        serverSock = clientSock = -1;
        sleep(1);
        return SIGINT;
    }

    clientSock = -1;
    struct sockaddr_in serverAddress;
    struct sockaddr_in clientAddress;
    socklen_t clientLen;

    // Create the socket
    serverSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Initialize the struct to hold the IP Address of the socket.
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(serverPort);

    // Try to bind the socket
    if (bind(serverSock, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0)
        perror("Unable to bind socket.");

    // Set the timeout on the socket.
    struct timeval timeout = {
            .tv_sec = 1,
            .tv_usec = 0
    };
    if (setsockopt(serverSock, SOL_SOCKET, SO_SNDTIMEO, (char*) &timeout, sizeof(timeout)))
        perror("Could not set timeout on socket send.");

    // Allocate the buffer for later.
    char* buffer = malloc(bufSize);
    buffer[bufSize - 1] = '\0';


    struct stat fileData;
    struct tm lastModifiedDateTime;

    puts("Listening for incoming HTTP connections...");
    if (listen(serverSock, maxPending) < 0)
        perror("Could not listen on socket.\r\n");

    for (;;) // K&R infinite loop :^)
    {
        if (serverSock == -1)
            return SIGINT;
        // Make sure the socket is closed if it wasn't closed before.
        if (clientSock >= 0)
            close(clientSock);

        // Accept the next connection
        clientLen = sizeof(struct sockaddr_in);
        if ((clientSock = accept(serverSock, (struct sockaddr*) &clientAddress, &clientLen)) < 0)
        {
            if (serverSock == -1)
                return SIGINT;
            perror("Could not accept on socket.\r\n");
        }

        // Grab the client's response and put it in the buffer.
        if (not Recv(clientSock, buffer, bufSize))
        {
            error("%d retries exceeded. Closing connection.\r\n", numRetries);
            continue;
        }
        // Print out which client you're responding to, so you know the server is working.
        struct timespec currentTime;
        char* currentTimeStr = calloc(datetimeBufferSize, sizeof(char));
        clock_gettime(CLOCK_REALTIME_COARSE, &currentTime);
        strftime(currentTimeStr, datetimeBufferSize, "%a, %d %h %Y %H:%M:%S GMT", gmtime(&currentTime.tv_sec));
        char* clientIPStr = calloc(INET_ADDRSTRLEN + 1, sizeof(char));
        getnameinfo((struct sockaddr*) &clientAddress, clientLen, clientIPStr, INET_ADDRSTRLEN + 1, 0, 0,
                    NI_NUMERICHOST);
        printf("[%s] Responding to %s...\n", currentTimeStr, clientIPStr);

        char* URL;
        char* currentHeader;
        char* lastModifiedHeader = "";
        size_t bufLen;

        // Since only GET requests work, check if it's a GET request and error otherwise.
        char* requestType = strncpy(calloc(5, sizeof(char)), buffer, 4);
        if (strncmp(requestType, "GET ", 4) != 0)
            error("Only GET requests supported, not \"%s\" requests.\r\n", requestType);

        // Parse the name of the file the client wants, prepending ./ to it so it's from the current directory.
        currentHeader = strstr(buffer, " ") + 1;
        bufLen = strstr(currentHeader, " ") - currentHeader;
        URL = calloc(bufLen + 3, sizeof(char));
        URL[0] = '.';
        URL[1] = '/';
        URL = strncpy(URL + 2, currentHeader, bufLen) - 2;

        // Parse the headers.
        {
            char* lastHeader;
            for (currentHeader = strstr(currentHeader, "\r\n"); *currentHeader and currentHeader[0] != currentHeader[2]; currentHeader = lastHeader)
            {
                // Pull out the If-Modified-Since header, since we're interested in it.
                lastHeader = strstr(currentHeader + 1, "\r\n");
                if (not strncmp(currentHeader + 2, "If-Modified-Since: ", 19))
                {
                    // The "magic" 19 is the length of "If-Modified-Since".
                    currentHeader += 2;
                    bufLen = lastHeader - currentHeader - 19;
                    lastModifiedHeader = strncpy(calloc(bufLen + 1, sizeof(char)), currentHeader + 19, bufLen);
                    break;
                }
            }
        }

        if (stat(URL, &fileData) == -1) // The client requested a file that doesn't exist, so return a 404.
        {
            FileNotFound:
            strcpy(buffer,
                   "HTTP/1.1 404 Not Found\r\nContent-Length: 136\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html>"
                   "<head>\r\n<title>404 Not Found</title>\r\n</head><body>\r\n<h1>Not Found</h1>\r\nThe requested URL was"
                   " not found on this server.\r\n</body></html>\r\n\0");
            Send(clientSock, buffer, strlen(buffer));
            continue;
        }

        // Grab the time the file was last modified for later.
        char* findPtr;
        char* lastModifiedStr = calloc(datetimeBufferSize, sizeof(char));
        lastModifiedDateTime = *gmtime(&fileData.st_mtim.tv_sec);
        size_t lastModifiedTimeLen = strftime(lastModifiedStr, datetimeBufferSize, "%a, %d %h %Y %H:%M:%S GMT", &lastModifiedDateTime);

        if (*lastModifiedHeader) // Check if it's a conditional GET request or not.
        {
            bufLen = (size_t) sprintf(buffer,
                    "HTTP/1.1 304 Not Modified\r\nDate: %s\r\nConnection: close\r\nLast-Modified: %s\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: %lu\r\n\r\n",
                    currentTimeStr, lastModifiedStr, fileData.st_size - 1);
            // Check if the file has been modified at a time different than what the client says.
            if (strncmp(lastModifiedHeader, lastModifiedStr, lastModifiedTimeLen) != 0)
            {
                FILE* currentFile = fopen(URL, "r");
                if (not currentFile)
                    goto FileNotFound;
                // Store the total buffer length for the Content-Length field.
                fread(buffer + bufLen, sizeof(char), (bufSize - bufLen) / sizeof(char), currentFile);
            }
            else
            {
                puts("Conditional GET!");
                findPtr = strstr(buffer, "Content-Length");
                findPtr[0] = '\r';
                findPtr[1] = '\n';
                findPtr[2] = '\0';
                Send(clientSock, buffer, strlen(buffer));
                continue;
            }
        }
        else
        {
            FILE* currentFile = fopen(URL, "r");
            if (not currentFile)
                goto FileNotFound;
            bufLen = (size_t) sprintf(buffer, "HTTP/1.1 200 OK\r\nDate: %s\r\nConnection: close\r\nLast-Modified: %s\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: %lu\r\n\r\n",
                                      currentTimeStr, lastModifiedStr, fileData.st_size - 1);

            if (fileData.st_size > bufSize - bufLen - 1) // Nope, not sending anything over 8 MiB.
            {   // Honestly, supporting that in C is out of the scope of this assignment.
                puts("This server will not serve files larger than 8MiB.");
                continue;
            }

            // Store the total buffer length for the Content-Length field.
            fread(buffer + bufLen, sizeof(char), (bufSize - bufLen) / sizeof(char), currentFile);
        }
        Send(clientSock, buffer, strlen(buffer));
    }
}

