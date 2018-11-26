project(CS356_HTTP C)

set(CMAKE_C_STANDARD 99)

add_executable(CS356_HTTP_Server server.c httpShared.h)
add_executable(CS356_HTTP_Client client.c httpShared.h)
