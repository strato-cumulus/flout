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

int flout_create_outbound_socket(struct sockaddr * server_addr, const int queue_size, char * err_buf, const int err_buf_len)
{
    const char * log_name = "flout_create_outbound_socket";

    int socket_fd;
    int ret_code;

    // Set up an outbound socket to communicate with workers requesting registration.
    socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        snprintf(err_buf, err_buf_len, "outbound socket creation failed: %s", strerror(errno));
        return socket_fd;
    }
    ret_code = bind(socket_fd, server_addr, sizeof(struct sockaddr_in6));
    if (ret_code < 0) {
        snprintf(err_buf, err_buf_len, "outbound socket failed to bind at %s: %s", server_addr, strerror(errno));
        close(socket_fd);
        return ret_code;
    }

    ret_code = listen(socket_fd, queue_size);
    if (ret_code < 0) {
        snprintf(err_buf, err_buf_len, "listening on %s failed: %s", server_addr, strerror(errno));
        close(socket_fd);
        return ret_code;
    }

    return socket_fd;
}


int flout_socket_read(int socket_fd, char * buffer, size_t buffer_size)
{
    #define log_name "flout_socket_read"

    int bytes_read = 0;
    int bytes_written = 0;
    int bytes_left;

    do {
        bytes_left = buffer_size - bytes_written;
        if (bytes_left <= 0) {
            bytes_written = buffer_size;
            break;
        }
        bytes_read = read(socket_fd, buffer + bytes_written, bytes_left);
        if (bytes_read < 0) {
            if (errno == EAGAIN) {
                // There is no data, so read throws because it cannot block.
                // We interpret this as a regular end-of-stream in a non-blocking socket.
                break;
            }
            return bytes_read;
        }
        bytes_written += bytes_read;
    } while (bytes_read > 0);

    // Put a guardian null, so that we can safely treat the buffer contents as a string.
    buffer[(bytes_written >= buffer_size ? buffer_size - 1 : bytes_written)] = '\0';

    return bytes_written;

    #undef log_name
}
