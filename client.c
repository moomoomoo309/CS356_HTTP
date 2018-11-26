#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <memory.h>
#include <unistd.h>
#include "httpShared.h"


int main(int argc, char** argv)
{
    if (argv++, --argc != 1)
        error("1 argument expected, got %d.\nUsage: %s (URL)", argc + 1, *(argv - 1));

    char* buffer = malloc(bufSize);

    // Parse the command line argument.
    char* URL = *argv;
    char* IP = strtok(URL, ":");
    char* port = strtok(NULL, "/");
    char* filePath = strtok(NULL, "");

    // Make sure the cache exists
    fclose(fopen(cachePath, "a"));
    FILE* cache = fopen(cachePath, "r");
    if (not cache)
    {
        puts("Could not open cache in read mode. Do you have permissions to the folder it's in?");
        return -1;
    }

    // Create the socket
    int sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Initialize the struct to hold the IP Address of the socket.
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    inet_aton(IP, &serverAddress.sin_addr);
    //serverAddress.sin_port = htons(atoi(port));
    serverAddress.sin_port = htons(serverPort);

    // Set the timeout on the socket.
    struct timeval timeout = {
            .tv_sec = 1,
            .tv_usec = 0
    };
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char*) &timeout, sizeof(timeout)))
        perror("Could not set timeout on socket send");
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout)))
        perror("Could not set timeout on socket receive");

    // Connect to the server.
    if (connect(sock, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0)
        perror("Could not connect to socket");

    char* findPtr;
    char* cacheLastModifiedTime = "";
    char* cacheFilePath = "";
    size_t bufLen;
    // Check the cache for the file path and last modified time, then send the GET request.
    if (fread(buffer, sizeof(char), bufSize, cache) > 1)
    {
        findPtr = strstr(buffer, "\r\n");
        if (not findPtr)
            goto EmptyCache;
        bufLen = findPtr - buffer;
        cacheFilePath = strncpy(calloc(bufLen, sizeof(char)), buffer, bufLen);
        bufLen = strstr(findPtr + 1, "\r\n") - findPtr;
        if (bufLen <= 2)
            goto EmptyCache;
        cacheLastModifiedTime = strncpy(calloc(bufLen, sizeof(char)), findPtr + 2, bufLen - 2);
        bufLen = (size_t) sprintf(buffer, "GET %s HTTP/1.1\r\nHost: %s:%s\r\nIf-Modified-Since: %s\r\n\r\n", filePath,
                                  URL, port, cacheLastModifiedTime);
        buffer[bufLen] = 0;
        if (not send(sock, buffer, strlen(buffer) + 1, 0))
            error("Could not send request to server after %d retries.", numRetries);
    }
    else //The cache was empty
    {
        EmptyCache:
        sprintf(buffer, "GET %s HTTP/1.1\r\nHost: %s:%s\r\n\r\n", filePath, URL, port);
        if (not send(sock, buffer, strlen(buffer) + 1, 0))
            error("Could not send request to server after %d retries.", numRetries);
    }

    printf("Sent the following request to the server:\n%s\n", buffer);

    // Get the server's response.
    if (not recv(sock, buffer, bufSize, 0))
    {
        error("Could not receive request from server after %d retries.", numRetries);
        return -1;
    }

    findPtr = strstr(buffer, "\r\n\r");
    *findPtr = '\0';
    printf("Server responded with the following headers:\n%s\n\n", buffer);
    *findPtr = '\r';

    // Grab the status code.
    findPtr = strstr(buffer, " ") + 1;
    int statusCode = atoi(strncpy(calloc(4, sizeof(char)), findPtr, 3));

    // Grab the last modified header.
    findPtr = strstr(buffer, "Last-Modified: ");
    char* content;
    char* lastModifiedTime = "";
    if (findPtr != NULL) // If the header's not present, but the server returns 304, it assumes the file has changed.
    {
        findPtr += 15; // 15 is the length of "Last-Modified".
        lastModifiedTime = strstr(findPtr, "\r\n");
        size_t len = lastModifiedTime - findPtr;
        lastModifiedTime = calloc(len + 1, sizeof(char));
        strncpy(lastModifiedTime, findPtr, len);
    }
    switch (statusCode)
    {
        case 304:
            if (*lastModifiedTime and strcmp(filePath, cacheFilePath) == 0 and
                strcmp(lastModifiedTime, cacheLastModifiedTime) == 0)
            {
                puts("File already cached.");
                break;
            }
            puts("File has been modified on the server.");
            // Fall through!
        case 200:
            // Print out the contents of the file and write it to the cache.
            content = strstr(buffer, "\r\n\r") + 4;
            puts("File contents:");
            puts(content);
            fclose(cache);
            cache = fopen(cachePath, "w+");
            // Store the file path, then the modified time, then the file itself, for easy parsing.
            fputs(filePath, cache);
            fputs("\r\n", cache);
            fputs(lastModifiedTime, cache);
            fputs("\r\n", cache);
            fputs(content, cache);
            break;
        case 404:
            printf("File %s not found!\n", filePath);
            break;
        case 0: // Empty string as a response.
            puts("Server did not respond, or responded with an empty string.");
            break;
        default:
            error("HTTP Status code %d not supported!", statusCode);
            break;
    }

    close(sock);
    return 0;
}
