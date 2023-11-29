#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 104857600

char const *get_mime_type(char const *file_ext);
char *url_decode(char const *src);

void create_http_response(const char *file_name,
                          const char *file_ext,
                          char *response,
                          size_t *response_len)
{
    const char *mime_type = get_mime_type(file_ext);
    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             mime_type);

    int file_fd = open(file_name, O_RDONLY);
    if (file_fd == -1)
    {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "\r\n"
                 "404 Not Found");
        *response_len = strlen(response);
        return;
    }

    struct stat file_stat;
    fstat(file_fd, &file_stat);
    off_t file_size = file_stat.st_size;

    *response_len = 0;
    memcpy(response, header, strlen(header));
    *response_len += strlen(header);

    ssize_t bytes_read;
    while ((bytes_read = read(file_fd,
                              response + *response_len,
                              BUFFER_SIZE - *response_len)) > 0)
    {
        *response_len += bytes_read;
    }
    free(header);
    close(file_fd);
}
char const *get_file_extension(char const *file_name)
{
    char const *dot = strchr(file_name, '.');
    if (!dot || dot == file_name)
    {
        return "";
    }
    else
    {
        return dot + 1;
    }
}
char const *get_mime_type(char const *file_ext)
{
    if (strcasecmp(file_ext, "html") == 0)
    {
        return "text/html";
    }
    else if (strcasecmp(file_ext, "txt") == 0)
    {
        return "text/plain";
    }
    else
    {
        return "application/octet-stream";
    }
}
void *handle_client(void *arg)
{
    int client_file_descriptor = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    ssize_t bytes_received = recv(client_file_descriptor, buffer, BUFFER_SIZE, 0);

    if (bytes_received > 0)
    {
        regex_t regex;
        regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
        regmatch_t matches[2];

        if (regexec(&regex, buffer, 2, matches, 0) == 0)
        {
            buffer[matches[1].rm_eo] = '\0';
            char const *url_encoded_file_name = buffer + matches[1].rm_so;
            char *file_name = url_decode(url_encoded_file_name);

            char file_ext[32];
            strcpy(file_ext, get_file_extension(file_name));

            char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
            size_t response_len;
            create_http_response(file_name, file_ext, response, &response_len);

            send(client_file_descriptor, response, response_len, 0);

            free(response);
            free(file_name);
        }

        regfree(&regex);
    }

    close(client_file_descriptor);
    free(arg);
    free(buffer);
    return NULL;
}
char *url_decode(char const *src)
{
    size_t src_len = strlen(src);
    char *decoded = malloc(src_len + 1);
    size_t decoded_len = 0;

    for (size_t i = 0; i < src_len; ++i)
    {
        if (src[i] == '%' && i + 2 < src_len)
        {
            int hex_val;
            sscanf(src + i + 1, "%2x", &hex_val);
            decoded[decoded_len++] = hex_val;
            i += 2;
        }
        else
        {
            decoded[decoded_len++] = src[i];
        }
    }

    decoded[decoded_len] = '\0';
    return decoded;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    if (port == 0)
    {
        printf("Invalid port (%s) number given as a command line #1.\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    int server_file_descriptor;
    struct sockaddr_in server_addr;

    // Create a new socket with a IPv4 address family (AF) and protocol,
    // which is chosen automatically. AF_INET means IPv4.
    if ((server_file_descriptor = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Set address family to IPv4.
    server_addr.sin_family = AF_INET;
    // Allow any IP address to connect to this server.
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // Call htons (short for Host TO Network Short, short as in 16-bit integer)
    server_addr.sin_port = htons(port);

    // Bind the socket to the socket address.
    if (bind(server_file_descriptor, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections on the socket.
    if (listen(server_file_descriptor, 10) < 0)
    {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", port);
    printf("See http://localhost:%d/example.html\n", port);

    while (true)
    {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int *client_file_descriptor = malloc(sizeof(int));

        if ((*client_file_descriptor = accept(server_file_descriptor, (struct sockaddr *)&client_addr, &client_addr_len)) < 0)
        {
            perror("Accept failed");
            continue;
        }

        // Create a new thread with a specific thread ID.
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)client_file_descriptor);
        pthread_detach(thread_id);
    }

    return 0;
}