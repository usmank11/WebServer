#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>

#define PORT 8080


const char *get_file_extension(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

void set_content_type(const char *file_extension, char *content_type) {
    if (strcmp(file_extension, "html") == 0) {
        strcpy(content_type, "text/html");
    } 
    else if (strcmp(file_extension, "css") == 0) {
        strcpy(content_type, "text/css");
    } 
    else if (strcmp(file_extension, "js") == 0) {
        strcpy(content_type, "application/javascript");
    } 
    else if (strcmp(file_extension, "png") == 0) {
        strcpy(content_type, "image/png");
    } 
    else if (strcmp(file_extension, "jpg") == 0) {
        strcpy(content_type, "image/jpeg");
    } 
    else if (strcmp(file_extension, "gif") == 0) {
        strcpy(content_type, "image/gif");
    } 
    else {
        strcpy(content_type, "application/octet-stream");
    }
}

typedef struct KeyValue KeyValue;

typedef struct {
    const char *path;
    void (*handler)(int, const char *, KeyValue *);
} Route;

typedef struct KeyValue {
    char *key;
    char *value;
    struct KeyValue *next;
} KeyValue;

KeyValue *parse_query_parameters(const char *query_string) {
    KeyValue *head = NULL, *tail = NULL;
    char *query_string_copy = strdup(query_string);
    char *token = strtok(query_string_copy, "&");

    while (token != NULL) {
        KeyValue *kv = (KeyValue *)malloc(sizeof(KeyValue));
        char *equal_sign = strchr(token, '=');

        if (equal_sign != NULL) {
            *equal_sign = '\0';
            kv->key = strdup(token);
            kv->value = strdup(equal_sign + 1);
        } else {
            kv->key = strdup(token);
            kv->value = strdup("");
        }

        kv->next = NULL;

        if (head == NULL) {
            head = kv;
            tail = kv;
        } else {
            tail->next = kv;
            tail = kv;
        }

        token = strtok(NULL, "&");
    }
    free(query_string_copy);
    return head;
}

void free_key_value_pairs(KeyValue *head) {
    KeyValue *current = head;
    while (current != NULL) {
        KeyValue *next = current->next;
        free(current->key);
        free(current->value);
        free(current);
        current = next;
    }
}


void send_html_file(int socket, const char *protocol, const char *file_path) {
    int file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        // Send a 404 Not Found response
        char not_found[] = "HTTP/1.1 404 Not Found\nContent-Type: text/plain\nContent-Length: 9\n\nNot Found";
        write(socket, not_found, strlen(not_found));
    } else {
        // Get the file size
        struct stat file_stat;
        fstat(file_fd, &file_stat);
        off_t file_size = file_stat.st_size;

        // Set the Content-Type header
        char content_type[] = "text/html";

        // Send the HTTP response headers
        char headers[256];
        snprintf(headers, sizeof(headers), "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %ld\n\n", content_type, file_size);
        write(socket, headers, strlen(headers));

        // Send the file content
        char file_buffer[1024];
        int bytes_read;
        while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
            write(socket, file_buffer, bytes_read);
        }
    }

    // Close the file
    close(file_fd);
}

void handle_root(int socket, const char *protocol, KeyValue *query_parameters) {
    // Print query parameters
    KeyValue *current = query_parameters;
    while (current != NULL) {
        printf("Key: %s, Value: %s\n", current->key, current->value);
        current = current->next;
    }
    const char *response = "HTTP/1.1 200 OK\nContent-Type: text/html\nContent-Length: 38\n\n<html><body>Welcome to my site!</body></html>";
    write(socket, response, strlen(response));
}


void about(int socket, const char *protocol, KeyValue *query_parameters) {
    const char *file_path = "about.html"; // Replace this with the path to your HTML file
    send_html_file(socket, protocol, file_path);
}

Route routes[] = {
    {"/", handle_root},
    {"/about", about},
    // Add more routes here
};

const size_t num_routes = sizeof(routes) / sizeof(routes[0]);


int main (int argc, char const *argv[]) {
    int server_fd, new_socket;
    long valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("In socket");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    memset(address.sin_zero, '\0', sizeof address.sin_zero);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("In bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("In listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("\n+++++++ Waiting for new connection ++++++++\n\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            perror("In accept");
            exit(EXIT_FAILURE);
        }

        char buffer[30000] = {0};
        valread = read(new_socket, buffer, sizeof(buffer));
        printf("%s\n",buffer );

         // Parse the request
        char method[8], path[1024], protocol[16];
        sscanf(buffer, "%s %s %s", method, path, protocol);

        // Extract query parameters
        char *query_start = strchr(path, '?');
        KeyValue *query_parameters = NULL;
        if (query_start != NULL) {
            *query_start = '\0';
            query_parameters = parse_query_parameters(query_start + 1);
        }

        // Find a matching route
        int route_matched = 0;
        for (size_t i = 0; i < num_routes; i++) {
            if (strcmp(path, routes[i].path) == 0) {
                routes[i].handler(new_socket, protocol, query_parameters);
                route_matched = 1;
                break;
            }
        }

        if (!route_matched) {
            // Remove the leading '/' from the path
            memmove(path, path + 1, strlen(path));

            // Set the default file if no file is requested
            if (strlen(path) == 0) {
                strcpy(path, "index.html");
            }
            // ...

            // Open the requested file
            int file_fd = open(path, O_RDONLY);
            if (file_fd < 0) {
                // Send a 404 Not Found response
                char not_found[] = "HTTP/1.1 404 Not Found\nContent-Type: text/plain\nContent-Length: 9\n\nNot Found";
                write(new_socket, not_found, strlen(not_found));
            } else {
                // Get the file size
                struct stat file_stat;
                fstat(file_fd, &file_stat);
                off_t file_size = file_stat.st_size;

                // Set the Content-Type header based on the file extension
                char content_type[64];
                set_content_type(get_file_extension(path), content_type);

                // Send the HTTP response headers
                char headers[256];
                snprintf(headers, sizeof(headers), "HTTP/1.1 200 OK\nContent-Type: %s\nContent-Length: %ld\n\n", content_type, file_size);
                write(new_socket, headers, strlen(headers));

                // Send the file content
                char file_buffer[1024];
                int bytes_read;
                while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
                    write(new_socket, file_buffer, bytes_read);
                }
            }
            close(file_fd);
        }


        if (query_parameters != NULL) {
            free_key_value_pairs(query_parameters);
        }

        // Close the file and the socket
        close(new_socket);
    }
    
    return 0;
}