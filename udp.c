#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    int sock;
    struct sockaddr_in dest;
    char *message = "hello device";

    // 1. Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket failed");
        return 1;
    }

    // 2. Set destination info
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(10);  // Destination port
    inet_pton(AF_INET, "192.168.1.12", &dest.sin_addr);  // Destination IP

    // 3. Send data
    ssize_t sent = sendto(sock, message, strlen(message), 0,
                          (struct sockaddr*)&dest, sizeof(dest));
    if (sent < 0) {
        perror("sendto failed");
        close(sock);
        return 1;
    }

    printf("Sent %ld bytes\n", sent);

    // 4. Close socket
    close(sock);
    return 0;
}