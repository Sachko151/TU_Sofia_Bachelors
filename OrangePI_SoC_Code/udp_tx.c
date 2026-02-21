#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define DEST_PORT 151

static int sock;
static struct sockaddr_in dest;

static void send_udp_payload(uint8_t *buff, uint8_t buff_len, char* ip_dest);

int main(int argc, char** argv) {

    printf("%d\n", argc);

    printf("%s\n", *(argv+2));
    // 1. Create UDP socket
    sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) {
        fprintf(stderr, "Socket Error aborting");
        return 1;
    }

    // 2. Set destination info
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DEST_PORT);  // Destination port
    inet_pton(AF_INET, argv[2], &dest.sin_addr);  // Destination IP is received from second argv
    uint8_t buff[] = {0xBA, 0xBA, 0xCE, 0xCA};
    
    if(argc > 1)
    {
        switch (argv[1][1])
        {
        case 'e':
            buff[0] = 0x00;
            send_udp_payload(buff, sizeof(buff), argv[2]);
            break;
        case 's':
            buff[0] = 0x01;
            send_udp_payload(buff, sizeof(buff), argv[2]);
            break;
        default:
            break;
        }
    }
    // 4. Close socket
    close(sock);

    return 0;
}

static void send_udp_payload(uint8_t *buff, uint8_t buff_len, char* ip_dest)
{
    // 3. Send data
    ssize_t sent = sendto(sock, buff, buff_len, 0, (struct sockaddr*)&dest, sizeof(dest));
    if (sent < 0) 
    {
        perror("sendto failed");
        close(sock);
        return;
    }

    printf("Sent %li bytes to %s\n", sent, ip_dest);

}
