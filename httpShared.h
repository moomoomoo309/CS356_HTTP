
#ifndef CS356_HTTP_HTTPSHARED_H
#define CS356_HTTP_HTTPSHARED_H

#include <bits/socket.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdarg.h>

#define cachePath ".httpCache"
#define numRetries 3
#define datetimeBufferSize 30
#define serverPort 12002
#define maxPending 8

const int bufSize = 8192;

#ifndef __cplusplus //C++ compatibility
#define not !
#define and &&
#define or ||
#endif

// Error, but with printf capability!
void error(char* formatStr, ...)
{
    va_list args;
    va_start(args, formatStr);
    char* errorMsg = malloc(2048);
    vsnprintf(errorMsg, 2048, formatStr, args);
    perror(errorMsg);
}

// Same as send, but retries numRetries times, and returns a bool.
bool Send(int sock, char* buf, size_t bufSize)
{
    for (int i=0; i<numRetries; i++)
        if (send(sock, buf, bufSize - 1, 0) != -1)
            break;
        else if (i == numRetries - 1)
            return false;
    return true;
}

// Same as recv, but retries numRetries times, and returns a bool.
bool Recv(int sock, char* buf, size_t bufSize)
{
    for (int i=0; i<numRetries; i++)
        if (recv(sock, buf, bufSize - 1, 0) != -1)
            break;
        else if (i == numRetries - 1)
            return false;
    return true;
}

#endif
