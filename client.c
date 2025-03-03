#include <stdio.h>      // Inclusion de la bibliothèque standard d'entrée/sortie
#include <stdlib.h>     // Inclusion de la bibliothèque standard pour les fonctions utilitaires
#include <string.h>     // Inclusion de la bibliothèque pour les fonctions de manipulation de chaînes
#include <unistd.h>     // Inclusion de la bibliothèque pour les fonctions POSIX
#include <arpa/inet.h>  // Inclusion de la bibliothèque pour les fonctions de manipulation des adresses IP
#include <pthread.h>    // Inclusion de la bibliothèque pour les threads

#define BUFFER_SIZE 1024  // Définition de la taille du buffer

// Fonction pour afficher les messages d'erreur et quitter le programme
void error(const char *msg) {
    perror(msg);  // Affiche le message d'erreur
    exit(1);      // Quitte le programme avec un code d'erreur
}

// Fonction exécutée par le thread pour recevoir les messages du serveur
void *receive_messages(void *sockfd_ptr) {
    int sockfd = *(int *)sockfd_ptr;  // Récupère le descripteur de socket
    char buffer[BUFFER_SIZE];         // Buffer pour stocker les messages reçus
    int n;                            // Variable pour stocker le nombre d'octets reçus

    // Boucle pour recevoir les messages du serveur
    while ((n = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[n] = '\0';  // Ajoute un caractère nul à la fin du message reçu
        printf("%s\n", buffer);  // Affiche le message reçu
    }

    // Vérifie si la connexion a été fermée par le serveur
    if (n == 0) {
        printf("Server closed the connection.\n");  // Affiche un message indiquant que le serveur a fermé la connexion
    } else {
        perror("ERROR receiving message");  // Affiche un message d'erreur si la réception a échoué
    }

    pthread_exit(NULL);  // Termine le thread
}

int main(int argc, char *argv[]) {
    // Vérifie le nombre d'arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port> <username>\n", argv[0]);  // Affiche un message d'utilisation
        exit(1);  // Quitte le programme avec un code d'erreur
    }

    int sockfd;  // Descripteur de socket
    struct sockaddr_in serv_addr;  // Structure pour stocker l'adresse du serveur
    char buffer[BUFFER_SIZE];  // Buffer pour stocker les messages à envoyer
    char *username = argv[3];  // Récupère le nom d'utilisateur à partir des arguments

    // Crée une socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");  // Affiche un message d'erreur si la création de la socket échoue
    }

    // Configure l'adresse du serveur
    serv_addr.sin_family = AF_INET;  // Définit la famille d'adresses à AF_INET (IPv4)
    serv_addr.sin_port = htons(atoi(argv[2]));  // Convertit le numéro de port en format réseau
    if (inet_pton(AF_INET, argv[1], &serv_addr.sin_addr) <= 0) {
        error("ERROR invalid server IP address");  // Affiche un message d'erreur si l'adresse IP est invalide
    }

    // Se connecte au serveur
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR connecting to server");  // Affiche un message d'erreur si la connexion échoue
    }

    // Envoie le nom d'utilisateur au serveur
    if (send(sockfd, username, strlen(username), 0) < 0) {
        error("ERROR sending username");  // Affiche un message d'erreur si l'envoi échoue
    }

    // Recevoir la réponse du serveur
    int n = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (n > 0) {
        buffer[n] = '\0';
        if (strcmp(buffer, "Username already taken. Connection refused.\n") == 0) {
            printf("Username already taken. Connection refused.\n");
            close(sockfd);
            return 1;
        }
    }

    printf("Connection to server... OK.\n");  // Affiche un message indiquant que la connexion est réussie

    // Crée un thread pour recevoir les messages du serveur
    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, (void *)&sockfd) != 0) {
        error("ERROR creating thread");  // Affiche un message d'erreur si la création du thread échoue
    }

    // Boucle pour envoyer les messages au serveur
    while (1) {
        printf("Send a new message: ");  // Affiche une invite pour envoyer un nouveau message
        fgets(buffer, BUFFER_SIZE, stdin);  // Lit le message de l'utilisateur
        buffer[strcspn(buffer, "\n")] = 0;  // Supprime le caractère de nouvelle ligne

        // Envoie le message au serveur
        if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
            error("ERROR sending message");  // Affiche un message d'erreur si l'envoi échoue
        }
    }

    close(sockfd);  // Ferme la socket
    return 0;  // Retourne 0 pour indiquer que le programme s'est terminé avec succès
}