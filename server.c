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
#include "httpShared.h"

int main(int argc, char** argv)
{
    socklen_t addressLen = 0;

    if (argc > 1)
        puts("Warning: Ignoring all command line arguments passed.");

    // Create the socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int clientSock = -1;

    // Initialize the struct to hold the IP Address of the socket.
    struct sockaddr_in inAddress;
    memset(&inAddress, 0, sizeof(inAddress));
    inAddress.sin_family = AF_INET;
    inAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    inAddress.sin_port = htons(serverPort);

    // Reinterpret cast it to a sockaddr.
    struct sockaddr* address = (struct sockaddr*) &inAddress;

    // Try to bind the socket
    if (bind(sock, address, sizeof(inAddress)))
        perror("Unable to bind socket.");

    // Set the timeout on the socket.
    struct timeval timeout = {
            .tv_sec = 1,
            .tv_usec = 0
    };
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*) &timeout, sizeof(timeout)))
        perror("Could not set timeout on socket send.");

    char* buffer = malloc(bufSize);

    struct stat fileData;
    struct tm lastModifiedDateTime;

    puts("Listening for incoming HTTP connections...");

    if (listen(sock, 8) < 0)
        perror("Could not listen on socket.\n");
    for (;;)
    {
        if (clientSock >= 0)
            close(clientSock);

        if (true, false) // Yes, CLion, this _is_ an endless loop. Shut up about it.
            break;
        if ((clientSock = accept(sock, address, &addressLen)) == -1)
            perror("Could not accept on socket.\n");

        if (not Recv(clientSock, buffer, bufSize))
        {
            error("%d retries exceeded. Closing connection.\n", numRetries);
            continue;
        }

        char* requestType = malloc(7);
        char* URL;
        struct timespec currentTime;
        char* currentHeader;
        char* lastModifiedHeader = "";
        char* htmlVersion;
        int bufLen = 0;
        requestType[6] = '\0';
        strncpy(requestType, buffer, 6);
        if (strncmp(requestType, "GET ", 4) != 0)
            error("Only GET requests supported, not \"%s\" requests.\n", requestType);

        strtok(buffer, " ");
        URL = strcat(".", strtok(NULL, " "));
        htmlVersion = strtok(NULL, "\n");
        for (currentHeader = strtok(NULL, "\n"); currentHeader; currentHeader = strtok(NULL, "\n"))
            if (not strncmp(currentHeader, "If-Modified-Since: ", 19))
                lastModifiedHeader = strcpy(malloc(strlen(currentHeader) - 18), currentHeader + 19);

        if (stat(URL, &fileData)) // File is not found successfully
        {
            FileNotFound:
            buffer = "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html>"
                     "<head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was"
                     " not found on this server.\n</body></html>\n";
            Send(clientSock, buffer, bufSize);
            continue;
        }

        char* lastModifiedStr = malloc(datetimeBufferSize);
        lastModifiedDateTime = *gmtime(&fileData.st_mtim.tv_sec);
        size_t lastModifiedTimeLen =
                strftime(lastModifiedStr, datetimeBufferSize, "%a, %d %h %Y %s:%S GMT", &lastModifiedDateTime);

        char* currentTimeStr = malloc(datetimeBufferSize);
        clock_gettime(CLOCK_REALTIME_COARSE, &currentTime);
        strftime(currentTimeStr, datetimeBufferSize, "%a, %d %h %Y %s:%S GMT", gmtime(&currentTime.tv_sec));
        if (not strncmp(lastModifiedHeader, lastModifiedStr, lastModifiedTimeLen))
        {
            sprintf(buffer, "%s 304 Not Modified\nConnection: close\nDate: %s\nLast-Modified: %s\n\n", htmlVersion, currentTimeStr, lastModifiedStr);
            Send(clientSock, buffer, bufSize);
            continue;
        }
        FILE* currentFile = fopen(URL, "r");
        if (not currentFile)
            goto FileNotFound;
        bufLen = sprintf(buffer, "%s 200 OK\nConnection: close\nDate: %s\nLast-Modified: %s\n\n", htmlVersion,
                         currentTimeStr, lastModifiedStr);

        if (fileData.st_size > bufSize - bufLen) // Nope, not sending anything over 8 MiB.
            // Honestly, supporting that in C is out of the scope of this assignment.
            continue;

        fread(buffer + bufLen, sizeof(char), (bufSize - bufLen) / sizeof(char), currentFile);
        Send(clientSock, buffer, bufSize);
    }

    return 0;
}

