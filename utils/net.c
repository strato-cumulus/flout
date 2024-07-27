#include "net.h"


/**
 * Initialize a struct sockaddr_in6 with all the necessary format conversions.
*/
void flout_init_sockaddr_in6(struct sockaddr_in6 * addr, const char * host, const int port)
{
    addr->sin6_family = AF_INET6;
    inet_pton(addr->sin6_family, host, &addr->sin6_addr);
    addr->sin6_port = htons(port);
}


/**
 * Shorthand for poll() for POLLIN on a single socket with timeout (or 0 for no timeout).
*/
int flout_check_socket_read(const int socket_fd, const time_t timeout)
{
    const int pollfds_size = 1;
    struct pollfd pollfds[pollfds_size];
    pollfds[0].fd = socket_fd;
    pollfds[0].events = POLLIN;
    return poll(pollfds, 1, timeout);
}


/**
 * Parse an IPv6 address into textual representation and puts it into buffer.
 */
void flout_parse_address(struct sockaddr_in6 * addr, char * buffer, socklen_t buffer_size)
{
    // Put address alone in the buffer first.
    inet_ntop(AF_INET6, &addr->sin6_addr, buffer, buffer_size);

    // Add a ':' at the end of address and put port after it.
    int port = ntohs(addr->sin6_port);
    for (int i = 0; i < buffer_size; ++i) {
        if (buffer[i] == '\0') {
            buffer[i] = ':';
            snprintf(&buffer[i+1], buffer_size-i-1, "%d", port);
            break;
        }
    }
    // Make sure that it's always a valid zero-terminated string.
    buffer[buffer_size-1] = '\0';
}
