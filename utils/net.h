#ifndef FLOUT_UTIL__NET_H_INCLUDED
#define FLOUT_UTIL__NET_H_INCLUDED

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

void flout_init_sockaddr_in6(struct sockaddr_in6 * addr, const char * host, const int port);
int flout_check_socket_read(const int socket_fd, const time_t timeout);
void flout_parse_address(struct sockaddr_in6 * addr, char * buffer, socklen_t buffer_size);

#endif
