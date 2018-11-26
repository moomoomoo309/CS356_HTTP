/* Nicholas DeLello
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <memory.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "httpShared.h"

int clientSock;
int serverSock;

static void closeSocket(int signal)
{
    if (signal == SIGINT)
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

    printf("Listening for incoming HTTP connections on port %d...", serverPort);
    if (listen(serverSock, maxPending) < 0)
        perror("Could not listen on socket.\r\n");

    // Don't do any stack allocations inside of the loop.
    char* currentTimeStr = calloc(datetimeBufferSize, sizeof(char));
    char* clientIPStr = calloc(INET_ADDRSTRLEN + 1, sizeof(char));
    char* lastModifiedStr = calloc(datetimeBufferSize, sizeof(char));
    char *findPtr, *URL, *currentHeader, *lastModifiedHeader, *currentHeaderEnd, *requestType;
    size_t bufLen, lastModifiedTimeLen;
    struct timespec currentTime;
    FILE* currentFile;

    for (;;) // K&R infinite loop :^)
    {
        lastModifiedHeader = "";
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
            perror("Could not accept on socket.\n");
        }

        // Grab the client's response and put it in the buffer.
        if (recv(clientSock, buffer, bufSize, 0) == -1)
        {
            perror("Socket failure on recv.");
            continue;
        }

        // Since only GET requests work, check if it's a GET request and error otherwise.
        requestType = strncpy(calloc(5, sizeof(char)), buffer, 4);
        if (strncmp(requestType, "GET ", 4) != 0)
            error("Only GET requests supported, not \"%s\" requests.\n", requestType);

        // Parse the name of the file the client wants, prepending ./ to it so it's from the current directory.
        currentHeader = strstr(buffer, " ") + 1;
        bufLen = strstr(currentHeader, " ") - currentHeader;
        URL = calloc(bufLen + 3, sizeof(char));
        URL[0] = '.';
        URL[1] = '/';
        URL = strncpy(URL + 2, currentHeader, bufLen) - 2;

        // Parse the headers.
        for (currentHeader = strstr(currentHeader, "\r\n"); *currentHeader and currentHeader[0] != currentHeader[2]; currentHeader = currentHeaderEnd)
        {
            // Pull out the If-Modified-Since header, since we're interested in it.
            currentHeaderEnd = strstr(currentHeader + 1, "\r\n");
            if (not strncmp(currentHeader + 2, "If-Modified-Since: ", 19))
            {
                // The "magic" 19 is the length of "If-Modified-Since".
                currentHeader += 2;
                bufLen = currentHeaderEnd - currentHeader - 19;
                lastModifiedHeader = strncpy(calloc(bufLen + 1, sizeof(char)), currentHeader + 19, bufLen);
                break;
            }
        }

        if (stat(URL, &fileData) == -1) // The client requested a file that doesn't exist, so return a 404.
        {
            FileNotFound:
            strcpy(buffer,
                   "HTTP/1.1 404 Not Found\r\nContent-Length: 136\r\nConnection: close\r\nContent-Type: text/html\r\n\r\n<html>"
                   "<head>\r\n<title>404 Not Found</title>\r\n</head><body>\r\n<h1>Not Found</h1>\r\nThe requested URL was"
                   " not found on this server.\r\n</body></html>\r\n\0");
            if (send(clientSock, buffer, strlen(buffer), 0) == -1)
                perror("Socket failure on send.");
            continue;
        }

        // Print out which client you're responding to, so you know the server is working.
        clock_gettime(CLOCK_REALTIME_COARSE, &currentTime);
        strftime(currentTimeStr, datetimeBufferSize, "%a, %d %h %Y %H:%M:%S GMT", gmtime(&currentTime.tv_sec));
        getnameinfo((struct sockaddr*) &clientAddress, clientLen, clientIPStr, INET_ADDRSTRLEN + 1, 0, 0,
                    NI_NUMERICHOST);
        printf("[%s] Responding to %s's request for %s...\n", currentTimeStr, clientIPStr, URL);

        // Grab the time the file was last modified for later.
        lastModifiedDateTime = *gmtime(&fileData.st_mtim.tv_sec);
        lastModifiedTimeLen = strftime(lastModifiedStr, datetimeBufferSize, "%a, %d %h %Y %H:%M:%S GMT", &lastModifiedDateTime);

        if (*lastModifiedHeader) // Check if it's a conditional GET request or not.
        {
            bufLen = (size_t) sprintf(buffer,
                    "HTTP/1.1 304 Not Modified\r\nDate: %s\r\nConnection: close\r\nLast-Modified: %s\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: %lu\r\n\r\n",
                    currentTimeStr, lastModifiedStr, fileData.st_size - 1);
            // Check if the file has been modified at a time different than what the client says.
            if (strncmp(lastModifiedHeader, lastModifiedStr, lastModifiedTimeLen) != 0)
            {
                currentFile = fopen(URL, "r");
                if (not currentFile)
                    goto FileNotFound;
                // Read the file into the buffer so it can be sent out.
                fread(buffer + bufLen, sizeof(char), (bufSize - bufLen) / sizeof(char), currentFile);
            }
            else
            {
                findPtr = strstr(buffer, "Content-Length");
                findPtr[0] = '\r';
                findPtr[1] = '\n';
                findPtr[2] = '\0';
                if (send(clientSock, buffer, strlen(buffer), 0) == -1)
                    perror("Socket failure on send.");
                continue;
            }
        }
        else
        {
            currentFile = fopen(URL, "r");
            if (not currentFile)
                goto FileNotFound;
            bufLen = (size_t) sprintf(buffer, "HTTP/1.1 200 OK\r\nDate: %s\r\nConnection: close\r\nLast-Modified: %s\r\nContent-Type: text/html; charset=UTF-8\r\nContent-Length: %lu\r\n\r\n",
                                      currentTimeStr, lastModifiedStr, fileData.st_size - 1);

            if (fileData.st_size > bufSize - bufLen - 1) // Nope, not sending anything over 8 KiB.
            {   // Honestly, supporting that in C is out of the scope of this assignment.
                puts("This server will not serve files larger than 8KiB.");
                continue;
            }

            // Read the file into the buffer so it can be sent out.
            fread(buffer + bufLen, sizeof(char), (bufSize - bufLen) / sizeof(char), currentFile);
        }
        if (send(clientSock, buffer, strlen(buffer), 0) == -1)
            perror("Socket failure on send.");
    }
}

