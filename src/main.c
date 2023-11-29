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
void *handle_client_request(void *arg);
char *url_decode(char const *src);

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
        pthread_create(&thread_id, NULL, handle_client_request, (void *)client_file_descriptor);

        // Deallocation note: when the thread is detached, client file descriptor is freed.
        pthread_detach(thread_id);
    }

    return 0;
}

/**
 * Creates an HTTP response for a given file.
 * @return The length of the response.
 */
size_t create_http_response_from_file(const char *file_name,
                                      const char *file_ext,
                                      char *response)
{
    // Write the HTTP response header that fits in the response buffer
    // according to output format.
    char const *mime_type = get_mime_type(file_ext);
    char *header = (char *)malloc(BUFFER_SIZE * sizeof(char));
    snprintf(header, BUFFER_SIZE,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "\r\n",
             mime_type);

    // Copy the header bytes to the response buffer.
    size_t response_len = 0;
    memcpy(response, header, strlen(header));
    response_len += strlen(header);

    // Header no longer needed, free it.
    free(header);

    // Open the file for reading. If the file does not exist,
    // return a response with status code 404 (Not Found).
    int file = open(file_name, O_RDONLY);
    if (file == -1)
    {
        snprintf(response, BUFFER_SIZE,
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/plain\r\n"
                 "\r\n"
                 "404 Not Found");
        return strlen(response);
    }

    // Get the file size based on the file opened
    // and use it to read the file contents into the response buffer.
    struct stat file_stat;
    fstat(file, &file_stat);
    off_t file_size = file_stat.st_size;

    // Read all file bytes into the response buffer
    // for as long as there are bytes to read.
    ssize_t bytes_read;
    while ((bytes_read = read(file,
                              response + response_len,
                              BUFFER_SIZE - response_len)) > 0)
    {
        response_len += bytes_read;
    }

    close(file);

    return response_len;
}
/**
 * Gets the file extension from a file name.
 * @return A pointer to the file extension in the given string or
 * an empty string if no file extension is found.
 */
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
/**
 * Gets the MIME type of a file based on its extension.
 * Currently only supports HTML and plain text,
 * all other files are treated as binary data.
 * @return The MIME type.
 */
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
void *handle_client_request(void *arg)
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
            char *path = url_decode(url_encoded_file_name);
            printf("Path: %s\n", path);

            if (strcmp(path, "api/books") == 0)
            {
                char *response = (char *)malloc(BUFFER_SIZE * sizeof(char));
                snprintf(response, BUFFER_SIZE,
                         "HTTP/1.1 200 OK\r\n"
                         "Content-Type: application/json\r\n"
                         "\r\n"
                         "[{\"title\": \"The Great Gatsby\", \"author\": \"F. Scott Fitzgerald\"},"
                         "{\"title\": \"The Grapes of Wrath\", \"author\": \"John Steinbeck\"},"
                         "{\"title\": \"Nineteen Eighty-Four\", \"author\": \"George Orwell\"},"
                         "{\"title\": \"Ulysses\", \"author\": \"James Joyce\"},"
                         "{\"title\": \"Lolita\", \"author\": \"Vladimir Nabokov\"}]");

                send(client_file_descriptor, response, strlen(response), 0);

                free(response);
            }
            else if (strncmp(path, "assets/", 7) == 0)
            {
                char file_ext[32];
                strcpy(file_ext, get_file_extension(path));

                char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
                size_t response_len = create_http_response_from_file(path, file_ext, response);

                send(client_file_descriptor, response, response_len, 0);

                free(response);
            }

            free(path);
        }

        regfree(&regex);
    }

    close(client_file_descriptor);
    free(arg);
    free(buffer);
}
/**
 * Decodes a URL-encoded string.
 * @return The decoded string. Note that the returned string is allocated, so it must be freed using `free`.
 */
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