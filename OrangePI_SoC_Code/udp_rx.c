#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define UDP_PORT 151
#define BUFFER_SIZE 1024

// Function to create and bind a UDP socket
int setup_udp_socket(uint16_t port) {
    int sock;
    struct sockaddr_in addr;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket creation failed");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(sock);
        return -1;
    }

    return sock;
}

// Function to receive a UDP payload and print it in hex
void receive_udp_payload(int sock, int argc, char** argv) {
    uint8_t buffer[BUFFER_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    ssize_t received = recvfrom(sock, buffer, BUFFER_SIZE, 0,
                                (struct sockaddr*)&client_addr, &client_len);
    if (received < 0) {
        perror("recvfrom failed");
        return;
    }

    if(argc < 2 || (argv[1][1] != 's'))
    {
        printf("UDP server listening on port %d...\n", UDP_PORT);
    
        printf("Received %zd bytes from %s:%d\n",
            received,
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port));

        printf("Data: ");
        for (ssize_t i = 0; i < received; i++) {
            printf("%02X ", buffer[i]);
        }
        printf("\n\n");
    }
    else
    {
        printf("IoT Device detected: %s:%d\n",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port));
    }
}

int main(int argc, char** argv) {
    int sock = setup_udp_socket(UDP_PORT);
    if (sock < 0) {
        return 1;
    }

    if(argc < 2 || (argv[1][1] != 's'))
    {
        printf("UDP server listening on port %d...\n", UDP_PORT);
    }

    receive_udp_payload(sock, argc, argv);

    

    close(sock);
    return 0;
}
