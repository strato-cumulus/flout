#include "coordinator.h"

// Max value since last command received from a worker
// before it gets disconnected from this coordinator.
const int worker_timeout_ms = 5000;

// Open connections to workers are being stored here.
// Free slots are set to zero.
flout_worker_slot_t connected_workers[MAX_CONNECTED_WORKERS];

struct sockaddr_in6 rpc_addr;
struct sockaddr_in6 ui_addr;

int rpc_socket_fd = -1;


/**
 * Initialize global state.
 */
#define log_name "flout_coordinator_init"
int flout_coordinator_init()
{
    memset(connected_workers, 0, (sizeof (struct _flout_worker_slot_t)) * MAX_CONNECTED_WORKERS);

    int ret_code = 0;
    char address_buffer[INET6_ADDRSTRLEN];
    int port;

    const int socket_queue_size = 8;
    char char_buffer[FLOUT_MAX_CMD_SIZE];

    flout_init_sockaddr_in6(&rpc_addr, "::1", 8122);
    flout_init_sockaddr_in6(&ui_addr, "::1", 8080);

    rpc_socket_fd = flout_create_outbound_socket((struct sockaddr *)&rpc_addr, socket_queue_size, char_buffer, FLOUT_MAX_CMD_SIZE);
    if (rpc_socket_fd < 0) {
        log_message(INFO, log_name, "Failed to create outbound socket: %s", strerror(errno));
        return rpc_socket_fd;
    }

    for (int i = 0; i < MAX_CONNECTED_WORKERS; ++i) {
        flout_worker_slot_t * worker_slot = &connected_workers[i];

        pthread_mutex_init(&worker_slot->mutex, NULL);
    }

    log_message(INFO, log_name, "Initialized RPC socket #%d listening at port %d", rpc_socket_fd, rpc_addr.sin6_port);

    return 0;
}
#undef log_name


#define log_name "flout_coordinator_destroy"
int flout_coordinator_destroy()
{
    for (int i = 0; i < MAX_CONNECTED_WORKERS; ++i) {
        flout_worker_slot_t * worker_slot = &connected_workers[i];
        pthread_mutex_destroy(&worker_slot->mutex);

        if (worker_slot->status != SFLOUT_FREE) {
            close(worker_slot->socket_fd);
        }
    }

    close(rpc_socket_fd);

    return 0;
}
#undef log_name


/**
 * Register the worker with the coordinator once communication has been established.
 * If there is a free spot in connected_workers, it will be written into
 * if registration has been successful.
 * Returns worker ID with this coordinator, which is its position in the workers array.
 */
int flout_register_worker(int worker_rpc_socket_fd, char * char_buffer, const size_t char_buffer_size,
    struct sockaddr * addr_buffer, const size_t addr_buffer_size)
{
    const char * log_name = "flout_register_worker";

    log_message(DEBUG, log_name, "registering worker");

    flout_worker_slot_t * found_slot;
    int found_slot_id = -1;

    log_message(INFO, log_name, "registering worker");

    // Find a free spot in the connection queue and fill it with incoming connection data.
    for (int i = 0; i < MAX_CONNECTED_WORKERS; ++i) {
        if (connected_workers[i].status == SFLOUT_FREE) {
            // Found a free slot.
            found_slot_id = i;
            break;
        }
    }

    if (found_slot_id < 0) {
        // Could not find a free slot, so we close the connection without acknowledgment.
        log_message(ERROR, log_name, "no free slot found, could not register worker");
        snprintf(char_buffer, char_buffer_size, "%d", EFLOUT_NOFREESLOT);
        write(worker_rpc_socket_fd, char_buffer, char_buffer_size);
        close(worker_rpc_socket_fd);
        return -1;
    }

    log_message(INFO, log_name, "found free slot, ID %d", found_slot_id);

    // Acknowledge the request by sending worker ID,
    // which is its position in the connected_workers array.
    snprintf(char_buffer, char_buffer_size, "%d", found_slot_id);
    if (write(worker_rpc_socket_fd, char_buffer, strlen(char_buffer)) < 0) {
        // If we could not respond with an acknowledgement,
        // then the connection won't be established.
        log_message(ERROR, log_name, "could not write connection response: %s", strerror(errno));
        close(worker_rpc_socket_fd);
        return -1;
    }

    // Store worker metadata on successful connection.
    found_slot = &connected_workers[found_slot_id];
    found_slot->socket_fd = worker_rpc_socket_fd;
    found_slot->last_activity_ts = get_current_time_ms();
    found_slot->status = SFLOUT_OCCUPIED;
    log_message(INFO, log_name, "connected to worker %d", found_slot_id);

    return worker_rpc_socket_fd;
}


/**
 * Handle incoming RPC calls from worker. This function polls on a worker socket,
 * accepts RPC invocations and calls local methods with given parameters.
 */
int flout_handle_rpc(const int worker_id, flout_worker_slot_t * worker_slot, char * buffer, size_t buffer_size)
{
    const char * log_name = "flout_handle_rpc";

    pthread_mutex_lock(&worker_slot->mutex);

    if (worker_slot->status == SFLOUT_FREE) {
        return 1;
    }

    if (flout_check_socket_read(worker_slot->socket_fd, 0) >= 0) {

        int bytes_received = flout_socket_read(worker_slot->socket_fd, buffer, buffer_size);
        pthread_mutex_unlock(&worker_slot->mutex);

        if (bytes_received < 0) {
            log_message(ERROR, log_name, "failed to fetch commands from worker %d: %s",
                worker_id, strerror(errno));
            return -1;
        }
        else if (bytes_received == 0) {
            return 1;
        }

        log_message(DEBUG, log_name, "received a command from worker %d: %s", worker_id, buffer);

        // Mark this worker as alive.
        worker_slot->last_activity_ts = get_current_time_ms();

        // There are no known commands yet, so finish for now.
    }
    else {
        pthread_mutex_unlock(&worker_slot->mutex);
    }

    return 0;
}

/**
 * Check for liveness and disconnect workers which haven't been active for more than max_time_us.
 * Returns 0 if worker has been determined to be alive. If it's not, the socket gets disconnected,
 * the slot gets cleared and a 1 is returned.
 */
int flout_handle_liveness(const int worker_id, flout_worker_slot_t * worker_slot, time_t max_time_ms)
{
    const char * log_name = "flout_handle_liveness";

    suseconds_t current_ts = get_current_time_ms();
    suseconds_t delta = current_ts - worker_slot->last_activity_ts;

    // The worker is alive if the last incoming message happened less than max_time_ms ago.
    if (delta <= max_time_ms) {
        log_message(INFO, log_name, "worker %d is alive, last activity was %d ms ago", worker_id, delta);
        return 0;
    }

    // Otherwise, the connection is closed and the slot is freed.
    log_message(INFO, log_name, "worker %d is gone, last activity was %d ms ago, disconnecting", worker_id, delta);
    pthread_mutex_lock(&worker_slot->mutex);
    close(worker_slot->socket_fd);
    worker_slot->status = SFLOUT_FREE;
    pthread_mutex_unlock(&worker_slot->mutex);
    
    return 1;
}


/**
 * Body of a thread that handles registrations only.
 * 
 * All workers connect to a registration endpoint first.
 * The coordinator waits on the bound socket and accepts a request whenever there is a connection incoming.
 * The socket created by accept() is stored alongside the worker metadata in the connected_workers array,
 * and the connection is kept up while the worker stays part of the cluster.
 */
void * flout_coordinator_registration_thread_fn(void *msg)
{
    const char * log_name = "flout_coordinator_registration_thread_fn";

    char char_buffer[INET6_ADDRSTRLEN];

    int ret_code = 0;

    struct sockaddr_in6 addr_buffer = {0};
    socklen_t addr_buffer_size = sizeof(addr_buffer);
    flout_worker_slot_t * worker_slot;
    int worker_rpc_socket_fd;
    int i;

    while(1) {

        // The socket returned by accept() will be stored as worker metadata.
        worker_rpc_socket_fd = accept(rpc_socket_fd, (struct sockaddr *) &addr_buffer, &addr_buffer_size);

        // Set socket to be non-blocking - we need to handle communication with workers asynchronously.
        ret_code = fcntl(worker_rpc_socket_fd, F_SETFL, fcntl(worker_rpc_socket_fd, F_GETFL, 0) | O_NONBLOCK);
        if (ret_code < 0) {
            log_message(ERROR, log_name, "attempt to register worker failed: can't set socket to be non-blocking: %s",
                strerror(errno));
            close(worker_rpc_socket_fd);
            continue;
        }

        flout_parse_address(&addr_buffer, char_buffer, INET6_ADDRSTRLEN);
        log_message(INFO, log_name, "opening connection to a worker at %s", addr_buffer);

        flout_register_worker(worker_rpc_socket_fd, char_buffer, FLOUT_MAX_CMD_SIZE,
            (struct sockaddr *) &addr_buffer, addr_buffer_size);
    }
}


/**
 * Synchronous tasks will be executed in this thread in regular intervals.
 */
void * flout_coordinator_sync_thread_fn(void * msg)
{
    const char * log_name = "flout_coordinator_sync_thread_fn";

    flout_worker_slot_t * worker_slot;
    const int char_buffer_size = 1024;
    char char_buffer[char_buffer_size];
    int i;

    while (1) {
        // Only perform these tasks for connected workers.
        for (i = 0; i < MAX_CONNECTED_WORKERS; ++i) {
            if (connected_workers[i].status == SFLOUT_OCCUPIED) {
                worker_slot = &connected_workers[i];
                flout_handle_liveness(i, worker_slot, worker_timeout_ms);
            }
        }
        sleep(1);
    }
}


/**
 * Asynchronous communication with workers will be handled here.
 */
void * flout_coordinator_rpc_thread_fn(void * msg)
{
    const char * log_name = "flout_coordinator_rpc_thread_fn";

    flout_worker_slot_t * worker_slot;
    const int char_buffer_size = 1024;
    char char_buffer[char_buffer_size];
    int i;

    while (1) {
        // Only perform these tasks for connected workers.
        for (i = 0; i < MAX_CONNECTED_WORKERS; ++i) {
            if (connected_workers[i].status == SFLOUT_OCCUPIED) {
                worker_slot = &connected_workers[i];
                flout_handle_rpc(i, worker_slot, char_buffer, char_buffer_size);
            }
        }
    }
}

/**
 * Communication with clients is handled here.
 * Clients issue commands that control the cluster.
 */
void * flout_coordinator_ui_thread_fn(void * msg)
{
    const char * log_name = "flout_coordinator_ui_thread_fn";

    int ui_socket_fd = 0;
    int client_socket_fd = 0;

    const int socket_queue_size = 8;
    const int char_buffer_size = 1024;
    char char_buffer[char_buffer_size];

    int ret_code = 0;
    char address_buffer[INET6_ADDRSTRLEN];
    int port;

    struct sockaddr_in6 *server_addr = (struct sockaddr_in6 *) msg;

    ui_socket_fd = flout_create_outbound_socket((struct sockaddr *)server_addr, socket_queue_size, char_buffer, char_buffer_size);
    if (ui_socket_fd < 0) {
        log_message(INFO, log_name, "Failed to create outbound socket: %s", strerror(errno));
        return NULL;
    }

    struct sockaddr_in6 addr_buffer = {0};
    socklen_t addr_buffer_size = sizeof(addr_buffer);

    while (1) {
        client_socket_fd = accept(ui_socket_fd, (struct sockaddr *) &addr_buffer, &addr_buffer_size);

        flout_parse_address(&addr_buffer, char_buffer, char_buffer_size);
        printf("Incoming connection from %s\n", char_buffer);

        close(client_socket_fd);
    }
}


int main(int argc, char* argv[])
{
    if (flout_coordinator_init() < 0) {
        return -1;
    }

    pthread_t registration_thread;
    pthread_create(&registration_thread, NULL, flout_coordinator_registration_thread_fn, NULL);

    pthread_t coordinator_sync_thread;
    pthread_create(&coordinator_sync_thread, NULL, flout_coordinator_sync_thread_fn, NULL);

    pthread_t coordinator_rpc_thread;
    pthread_create(&coordinator_rpc_thread, NULL, flout_coordinator_rpc_thread_fn, NULL);

    pthread_t coordinator_ui_thread;
    pthread_create(&coordinator_ui_thread, NULL, flout_coordinator_ui_thread_fn, (void*) &ui_addr);

    pthread_join(registration_thread, NULL);
    pthread_join(coordinator_sync_thread, NULL);
    pthread_join(coordinator_rpc_thread, NULL);
    pthread_join(coordinator_ui_thread, NULL);

    flout_coordinator_destroy();

    return 0;
}
