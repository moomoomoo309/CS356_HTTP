#include <stdio.h>
#include <strings.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <unistd.h>
#include "httpShared.h"


int main(int argc, char** argv)
{
    if (argv++, --argc != 1)
    {
        error("1 argument expected, got %d.\nUsage: %s (URL)", argc + 1, *(argv - 1));
    }
    char* buffer = malloc(bufSize);
    char* URL = *argv;

    FILE* cache = fopen(cachePath, "w+");
    if (not cache)
    {
        puts("Could not open cache in read/write mode. Do you have permissions to the folder it's in?");
        return -1;
    }

    // Create the socket
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    char* IP = strtok(URL, ":");
    char* port = strtok(NULL, "/");
    char* filePath = strtok(NULL, "");

    // Initialize the struct to hold the IP Address of the socket.
    struct sockaddr_in inAddress;
    bzero((char*) &inAddress, sizeof(inAddress));
    inAddress.sin_port = htons(serverPort);
    inet_aton(IP, &inAddress.sin_addr);
    inAddress.sin_family = AF_INET;

    // Reinterpret cast it to a sockaddr.
    struct sockaddr_in clientAddress_in;
    memcpy(&clientAddress_in, &inAddress, sizeof(inAddress));
    struct sockaddr serverAddress = *(struct sockaddr*) &inAddress;

    clientAddress_in.sin_port = htons(0);
    struct sockaddr clientAddress = *(struct sockaddr*) &clientAddress_in;

    // Try to bind the socket
    if (bind(sock, &clientAddress, sizeof(clientAddress)))
        perror("Unable to bind socket.");

    // Set the timeout on the socket.
    struct timeval timeout = {
            .tv_sec = 1,
            .tv_usec = 0
    };
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*) &timeout, sizeof(timeout)))
        perror("Could not set timeout on socket send.");
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout)))
        perror("Could not set timeout on socket receive.");

    char* lastModifiedTime;
    if (connect(sock, &serverAddress, sizeof(serverAddress)) == -1)
        perror("Could not connect to socket.");
    if (fread(buffer, sizeof(char), bufSize, cache))
    {
        if (not strcmp(strtok(buffer, "\n"), filePath))
        {
            lastModifiedTime = strtok(NULL, "\n");
            sprintf(buffer, "GET %s HTTP/1.1\nIf-Modified-Since: %s\n\n", filePath, lastModifiedTime);
            if (not Send(sock, buffer, bufSize))
                error("Could not send request to server after %d retries.", numRetries);
        }
        else
        {
            sprintf(buffer, "GET %s HTTP/1.1\n\n", filePath);
            if (not Send(sock, buffer, bufSize))
                error("Could not send request to server after %d retries.", numRetries);
        }
    }

    if (not Recv(sock, buffer, bufSize))
    {
        error("Could not receive request from server after %d retries", numRetries);
        return -1;
    }

    strtok(buffer, " "); // Skip the HTTP version
    int statusCode = atoi(strtok(NULL, " "));
    strtok(NULL, "Last-Modified: "); // Jump to the Last-Modified header
    lastModifiedTime = strtok(NULL, "\n"); // Grab the last modified time
    switch(statusCode) {
        case 305:
            puts("File already cached.");
            break;
        case 200:
            strtok(NULL, "\n\n"); // Skip the headers
            char* content = strtok(NULL, "");
            puts(content);
            fputs(filePath, cache);
            fputc('\n', cache);
            fputs(lastModifiedTime, cache);
            fputc('\n', cache);
            fputs(content, cache);
            fputc('\n', cache);
            break;
        default:
            error("HTTP Status code %d not supported!\n", statusCode);
            break;
    }

    close(sock);
    return 0;
}
