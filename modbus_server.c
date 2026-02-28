#include <stdio.h>
#include <modbus/modbus.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdarg.h>

#define DEFAULT_SERVER_IP "0.0.0.0"      // Default server IP address
#define DEFAULT_SERVER_PORT 502          // Default server port
#define DEFAULT_REG_COUNT 10             // Default number of registers
#define VERSION "1.0.0"                  // Server version

/**
 * Function to display the usage/help message.
 * This function provides information about the available options and configuration
 * arguments for the Modbus server.
 */
void print_usage() {
    printf("Modbus Server - Version %s\n", VERSION);
    printf("Copyright (c) 2025 Aleksandra Matysik MikroB S.A.\n\n");
    printf("Usage: modbus_server [OPTIONS]\n\n");

    printf("General Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --version     Show version information\n");

    printf("\nServer Configuration:\n");
    printf("  -i IP             Set server IP address (default: 0.0.0.0)\n");
    printf("  -p PORT           Set server port (default: 502)\n");
    printf("  -r REG_COUNT      Set number of registers (default: 10)\n");
    printf("  --debug           Enable debug output (default: off)\n");
    
    printf("\nExample:\n");
    printf("  modbus_server -i 192.168.1.100 -p 502 -r 20 --debug\n");
}

/**
 * Function for debugging output.
 * If debugging is enabled, it prints the formatted string to stderr.
 *
 * @param debug    The debug flag (1 for enabled, 0 for disabled).
 * @param fmt      The format string for the message.
 * @param ...      The variable arguments to format the message.
 */
void debug_print_func(int debug, const char *fmt, ...) {
    if (debug) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

/**
 * Function to print the Modbus query received.
 * This function prints the received query in hexadecimal format.
 *
 * @param query    The Modbus query byte array.
 * @param length   The length of the query.
 */
void print_query(uint8_t *query, int length) {
    printf("[QUERY] Received query (Length: %d): ", length);
    for (int i = 0; i < length; i++) {
        printf("%02X ", query[i]);
    }
    printf("\n");
}

/**
 * Function to print the Modbus response being sent.
 * This function prints the response in hexadecimal format.
 *
 * @param response    The Modbus response byte array.
 * @param length      The length of the response.
 */
void print_response(uint8_t *response, int length) {
    printf("[RESPONSE] Sending response (Length: %d): ", length);
    for (int i = 0; i < length; i++) {
        printf("%02X ", response[i]);
    }
    printf("\n");
}

/**
 * Function to print the server's current settings.
 * Displays the server's IP, port, register count, and debug mode status.
 *
 * @param server_ip    The IP address of the server.
 * @param server_port  The port number the server is listening on.
 * @param reg_count    The number of Modbus registers.
 * @param debug        The debug flag indicating if debugging is enabled.
 */
void print_server_settings(char *server_ip, int server_port, int reg_count, int debug) {
    printf("[INFO] Modbus Server Settings:\n");
    printf("  IP Address: %s\n", server_ip);
    printf("  Port: %d\n", server_port);
    printf("  Register Count: %d\n", reg_count);
    printf("  Debug Mode: %s\n", debug ? "Enabled" : "Disabled");
    printf("\n");
}

/**
 * Function to initialize a Modbus TCP server context.
 * This function creates and returns a new Modbus context for the TCP server.
 *
 * @param server_ip    The IP address of the server.
 * @param server_port  The port number to listen on.
 * 
 * @return A pointer to the Modbus context if successful, NULL otherwise.
 */
modbus_t* init_modbus_server(char *server_ip, int server_port) {
    modbus_t *ctx = modbus_new_tcp(server_ip, server_port);
    if (ctx == NULL) {
        fprintf(stderr, "[ERROR] Error initializing Modbus server: %s\n", modbus_strerror(errno));
        return NULL;
    }
    return ctx;
}

/**
 * Function to initialize the Modbus mapping.
 * This function creates and returns a Modbus mapping for the server with the specified number of registers.
 *
 * @param reg_count  The number of registers for the Modbus server.
 * 
 * @return A pointer to the Modbus mapping if successful, NULL otherwise.
 */
modbus_mapping_t* init_modbus_mapping(int reg_count) {
    modbus_mapping_t *mb_mapping = modbus_mapping_new(0, 0, reg_count, 0);
    if (mb_mapping == NULL) {
        fprintf(stderr, "[ERROR] Error allocating memory for Modbus mapping: %s\n", modbus_strerror(errno));
        return NULL;
    }
    return mb_mapping;
}

/**
 * Function to start the server socket and begin listening for incoming client connections.
 * 
 * @param ctx    The Modbus context.
 * 
 * @return The server socket descriptor if successful, -1 otherwise.
 */
int start_listening(modbus_t *ctx) {
    int server_socket = modbus_tcp_listen(ctx, 1);
    if (server_socket == -1) {
        fprintf(stderr, "[ERROR] Error listening on TCP socket: %s\n", modbus_strerror(errno));
        return -1;
    }
    return server_socket;
}

/**
 * Function to handle an incoming client request.
 * This function receives a query from the client and sends a response based on the Modbus mapping.
 *
 * @param ctx          The Modbus context.
 * @param mb_mapping   The Modbus mapping containing register values.
 * @param query        The query received from the client.
 * @param debug        The debug flag indicating if debugging is enabled.
 * 
 * @return The number of bytes processed if successful, -1 if an error occurred.
 */
int handle_client_request(modbus_t *ctx, modbus_mapping_t *mb_mapping, uint8_t *query, int debug) {
    int rc = modbus_receive(ctx, query);
    if (rc > 0) {
        if (debug) print_query(query, rc);

        rc = modbus_reply(ctx, query, rc, mb_mapping);
        if (rc > 0 && debug) {
            print_response(query, rc);
        }
    } else if (rc == -1 && errno == ECONNRESET) {
        debug_print_func(debug, "[INFO] Client disconnected (Connection reset by peer).\n");
    } else if (rc == -1) {
        fprintf(stderr, "[ERROR] Error while receiving request: %s\n", modbus_strerror(errno));
    }
    return rc;
}

/**
 * Function to parse command-line arguments.
 * This function processes the command-line options and updates the server settings accordingly.
 *
 * @param argc          The number of command-line arguments.
 * @param argv          The array of command-line arguments.
 * @param server_ip     The pointer to the server IP address.
 * @param server_port   The pointer to the server port.
 * @param reg_count     The pointer to the register count.
 * @param debug         The pointer to the debug flag.
 */
void parse_arguments(int argc, char *argv[], char **server_ip, int *server_port, int *reg_count, int *debug) {
    static struct option long_options[] = {
        {"debug", no_argument, NULL, 'd'},
        {"version", no_argument, NULL, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:p:r:hv", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                *server_ip = optarg;
                break;
            case 'p':
                *server_port = atoi(optarg);
                break;
            case 'r':
                *reg_count = atoi(optarg);
                break;
            case 'd':
                *debug = 1;
                break;
            case 'v':
                printf("Modbus Server - Version %s\n", VERSION);
                exit(0);
            case 'h':
                print_usage();
                exit(0);
            default:
                fprintf(stderr, "[ERROR] Invalid option: %c\n", optopt);
                print_usage();
                exit(-1);
        }
    }
}

/**
 * Main function to run the Modbus server.
 * This function initializes the Modbus server, sets up the register mapping,
 * and enters a loop to handle client requests.
 *
 * @param argc          The number of command-line arguments.
 * @param argv          The array of command-line arguments.
 * 
 * @return 0 if the server shuts down successfully, -1 if an error occurs.
 */
int main(int argc, char *argv[]) {
    char *server_ip = DEFAULT_SERVER_IP;
    int server_port = DEFAULT_SERVER_PORT;
    int reg_count = DEFAULT_REG_COUNT;
    int debug = 0;

    parse_arguments(argc, argv, &server_ip, &server_port, &reg_count, &debug);
    print_server_settings(server_ip, server_port, reg_count, debug);

    // Initialize Modbus TCP server
    modbus_t *ctx = init_modbus_server(server_ip, server_port);
    if (ctx == NULL) return -1;

    // Create register map
    modbus_mapping_t *mb_mapping = init_modbus_mapping(reg_count);
    if (mb_mapping == NULL) {
        modbus_free(ctx);
        return -1;
    }

    // Start listening on the server socket
    int server_socket = start_listening(ctx);
    if (server_socket == -1) {
        modbus_mapping_free(mb_mapping);
        modbus_free(ctx);
        return -1;
    }

    printf("[INFO] Modbus server listening on %s:%d\n\n", server_ip, server_port);

    while (1) {
        // Accept client connections
        int client_socket = modbus_tcp_accept(ctx, &server_socket);
        if (client_socket == -1) {
            fprintf(stderr, "[ERROR] Error accepting client connection: %s\n", modbus_strerror(errno));
            continue;
        }

        debug_print_func(debug, "[INFO] Client connected successfully.\n");

        // Main loop to handle requests
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        while (1) {
            int rc = handle_client_request(ctx, mb_mapping, query, debug);
            if (rc == -1) break;  // End client handling if error or disconnect
        }
    }

    // Cleanup and shutdown
    printf("[INFO] Server shutting down gracefully...\n");
    modbus_mapping_free(mb_mapping);
    modbus_free(ctx);
    return 0;
}
