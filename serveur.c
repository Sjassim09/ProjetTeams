#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define BUFFER_SIZE 2048
#define MAX_CLIENTS 100

typedef struct {
    int sockfd;
    struct sockaddr_in addr;
    char username[50];
} client_info;

client_info *clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void broadcast_message(const char *message, int sender_sockfd) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i]->sockfd != sender_sockfd) {
            if (send(clients[i]->sockfd, message, strlen(message), 0) < 0) {
                perror("ERROR sending message");
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    client_info *client = (client_info *)arg;
    char buffer[BUFFER_SIZE];
    int n;

    while ((n = recv(client->sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[n] = '\0';
        printf("Message reçu du %s: %s\n", client->username, buffer);

        char message[BUFFER_SIZE + 100]; // Augmenter la taille du tableau message
        int message_len = snprintf(message, sizeof(message), "# %s > %s\n", client->username, buffer);
        if (message_len >= sizeof(message)) {
            fprintf(stderr, "Warning: message truncated\n");
        }
        broadcast_message(message, client->sockfd);
    }

    close(client->sockfd);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i] == client) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    free(client);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int sockfd, portno;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    memset((char *)&serv_addr, 0, sizeof(serv_addr));
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }

    if (listen(sockfd, MAX_CLIENTS) < 0) {
        error("ERROR on listen");
    }
    printf("My_teams starting...\nBinding... OK.\nServer reachable on IP 0.0.0.0 port %d\nWaiting for new clients...\n", portno);

    while (1) {
        client_info *client = malloc(sizeof(client_info));
        if (!client) {
            perror("ERROR allocating memory");
            continue;
        }
        clilen = sizeof(client->addr);
        client->sockfd = accept(sockfd, (struct sockaddr *)&client->addr, &clilen);
        if (client->sockfd < 0) {
            perror("ERROR on accept");
            free(client);
            continue;
        }

        // Recevoir le nom d'utilisateur du client
        int n = recv(client->sockfd, client->username, 50, 0);
        if (n < 0) {
            perror("ERROR receiving username");
            close(client->sockfd);
            free(client);
            continue;
        }
        client->username[n] = '\0';

        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            clients[client_count++] = client;
            pthread_mutex_unlock(&clients_mutex);

            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_client, (void *)client) != 0) {
                perror("ERROR creating thread");
                close(client->sockfd);
                free(client);
                pthread_mutex_lock(&clients_mutex);
                client_count--;
                pthread_mutex_unlock(&clients_mutex);
            } else {
                pthread_detach(thread);
            }
        } else {
            pthread_mutex_unlock(&clients_mutex);
            printf("Max clients reached. Rejecting: %s\n", client->username);
            close(client->sockfd);
            free(client);
        }
    }

    close(sockfd);
    return 0;
}