#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void *receive_messages(void *sockfd_ptr) {
    int sockfd = *(int *)sockfd_ptr;
    char buffer[BUFFER_SIZE];
    int n;

    while ((n = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[n] = '\0';
        printf("%s\n", buffer);
    }

    if (n == 0) {
        printf("Server closed the connection.\n");
    } else {
        perror("ERROR receiving message");
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <username>\n", argv[0]);
        exit(1);
    }

    int sockfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];
    char *username = argv[3];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        error("ERROR invalid server IP address");
    }

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting to server");
    }

    // Envoyer le nom d'utilisateur au serveur
    if (send(sockfd, username, strlen(username), 0) < 0) {
        error("ERROR sending username");
    }

    printf("Connection to server... OK.\n");

    // Créer un thread pour recevoir les messages du serveur
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, (void *)&sockfd) != 0) {
        error("ERROR creating thread");
    }

    while (1) {
        printf("Send a new message: ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            error("ERROR sending message");
        }
    }

    close(sockfd);
    return 0;
}