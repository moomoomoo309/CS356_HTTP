
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

const int bufSize = 8192;

#ifndef __cplusplus //C++ compatibility
#define not !
#define and &&
#define or ||
#endif

void error(char* formatStr, ...)
{
    va_list args;
    va_start(args, formatStr);
    char* errorMsg = malloc(2048);
    vsprintf(errorMsg, formatStr, args);
    perror(errorMsg);
}

bool Send(int sock, char* buf, size_t bufSize)
{
    for (int i=0; i<numRetries; i++)
        if (send(sock, buf, bufSize, 0) != -1)
            break;
        else if (i == numRetries - 1)
            return false;
    return true;
}

bool Recv(int sock, char* buf, size_t bufSize)
{
    for (int i=0; i<numRetries; i++)
        if (recv(sock, buf, bufSize, 0) != -1)
            break;
        else if (i == numRetries - 1)
            return false;
    return true;
}

#endif
