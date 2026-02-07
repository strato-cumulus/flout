#include "worker.h"

/**
 * Global variables for the process.
 * Negative values are placeholders for unset values.
 */

// Sockets which are supposed to be open during worker lifetime.
int rpc_socket_fd = -1;

// Worker ID as obtained from the coordinator.
int worker_id = -1;


/**
 * Keeps the connection with coordinator up by periodically sending a heartbeat.
 */
void * flout_worker_heartbeat_fn(void * msg)
{
    static const char * log_name = "flout_worker_heartbeat_fn";
    const int cmd_id = 0;

    int counter = 0;

    log_message(INFO, log_name, "initializing heartbeat thread");

    const int char_buffer_size = 32;
    char char_buffer[32];

    flout_worker_heartbeat_fn_params * params = (flout_worker_heartbeat_fn_params *) msg;

    // For now, we'll use sleep() that has a full seconds precision.
    time_t interval_s = params->interval.tv_sec;

    while (1) {
        snprintf(char_buffer, char_buffer_size, "%d\t%d\t%d\0", cmd_id, worker_id, counter);
        log_message(DEBUG, log_name, "Sending heartbeat: %s", char_buffer);
        if (write(rpc_socket_fd, char_buffer, char_buffer_size) < 0) {
            log_message(INFO, log_name, "failed to send heartbat: %s", strerror(errno));
        }
        counter += 1;
        sleep(interval_s);
    }
}


/**
 * Register with a coordinator.
 * Establishes one-time connection with the coordinator under its registration address.
*/
int flout_register(int worker_rpc_socket_fd, struct sockaddr_in6 * worker_rpc_addr,
    struct sockaddr_in6 * coordinator_rpc_addr)
{
    const char * log_name = "flout_register";

    const int cmd_id = FLOUT_CMD_REGISTER;

    int n_read;
    int ret_value;
    struct timeval timeout = {0};

    const size_t char_buffer_size = INET6_ADDRSTRLEN;
    char char_buffer[char_buffer_size];

    timeout.tv_sec = 1;

    flout_parse_address(coordinator_rpc_addr, char_buffer, char_buffer_size);
    log_message(INFO, log_name, "connecting to coordinator at %s", char_buffer);

    rpc_socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (rpc_socket_fd < 0) {
        log_message(ERROR, log_name, "could not create a socket for coordinator communication: %s: shutting down",
            strerror(errno));
        return rpc_socket_fd;
    }

    // Set timeout for all operations on the socket.
    timeout.tv_sec = 1;
    setsockopt(rpc_socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    ret_value = connect(rpc_socket_fd, (struct sockaddr *)coordinator_rpc_addr, sizeof(*coordinator_rpc_addr));
    if (ret_value < 0) {
        log_message(ERROR, log_name, "attempt to connect to coordinator failed: %s: shutting down",
            strerror(errno));
        goto err_cleanup;
    }

    // Create and send the registration message.
    memset(char_buffer, 0, char_buffer_size);
    memcpy(char_buffer, &cmd_id, sizeof(int));

    char_buffer[sizeof(int)] = '\4';

    if (write(rpc_socket_fd, char_buffer, sizeof(int) + 1) <= 0) {
        log_message(ERROR, log_name, "failed to send registration command: %s: shutting down", strerror(errno));
        goto err_cleanup;
    }

    // Receive worker ID or error code on connection.
    ret_value = read(rpc_socket_fd, char_buffer, char_buffer_size-1);

    if (ret_value < 0) {
        log_message(ERROR, log_name, "coordinator closed the connection without responding: %s: shutting down",
            strerror(errno));
        goto err_cleanup;
    }

    // Terminate the string in case of overflow and parse the number.
    char_buffer[char_buffer_size-1] = '\0';
    ret_value = strtol(char_buffer, NULL, 10);

    if (ret_value < 0) {
        log_message(ERROR, log_name, "coordinator returned an error: %d", ret_value);
        goto err_cleanup;
    }

    // Return worker ID.
    return ret_value;

    err_cleanup:
    close(rpc_socket_fd);
    return ret_value;
}


int main(int argc, char* argv[])
{
    const char * log_name = "main";

    struct sockaddr_in6 coordinator_rpc_addr;
    flout_init_sockaddr_in6(&coordinator_rpc_addr, "::1", 8122);

    // Init an RPC socket for coordinator communication.
    // It will stay open for the entire lifetime of the worker.
    const char * worker_address = "::1";
    const int worker_port = 0; // use an ephemeral port
    struct sockaddr_in6 worker_rpc_addr;
    flout_init_sockaddr_in6(&worker_rpc_addr, worker_address, worker_port);

    rpc_socket_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (rpc_socket_fd < 0) {
        log_message(ERROR, log_name, "could not create an rpc socket: %s: shutting down",
            strerror(errno));
            return errno;
    }
    if (bind(rpc_socket_fd, (struct sockaddr *)&worker_rpc_addr, sizeof(worker_rpc_addr)) < 0) {
        log_message(ERROR, log_name, "could not bind to %s:%d: %s: shutting down",
            worker_address, worker_port, strerror(errno));
        return errno;
    }
    log_message(INFO, log_name, "initialized worker rpc socket at %s:%d", 
        worker_address, worker_port);

    worker_id = flout_register(rpc_socket_fd, &worker_rpc_addr, &coordinator_rpc_addr);

    log_message(INFO, log_name, "Successfully registered worker, ID %d", worker_id);   

    if (worker_id < 0) {
        const char* error_str = strerror(errno);
        printf("%s\n", error_str);
        return worker_id;
    }

    pthread_t worker_heartbeat_thread;
    flout_worker_heartbeat_fn_params worker_heartbeat_thread_params;

    worker_heartbeat_thread_params.interval.tv_sec = 1;
    worker_heartbeat_thread_params.interval.tv_usec = 0;

    pthread_create(&worker_heartbeat_thread, NULL, &flout_worker_heartbeat_fn, (void *) &worker_heartbeat_thread_params);

    pthread_join(worker_heartbeat_thread, NULL);

    return 0;
}
