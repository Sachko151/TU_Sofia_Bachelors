/*================================================================
                        INCLUDES
=================================================================*/
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>

/*================================================================
                        DEFINES
=================================================================*/
#define DEST_PORT 151

typedef enum
{
  ECHO,
  RESET,
  SCAN,
  COUNT
} commands_t;

/*================================================================
                        GLOBAL VARS
=================================================================*/
static int sock;
static struct sockaddr_in dest;

/*================================================================
                    Prototype Declarations
=================================================================*/
static void send_udp_payload(uint8_t *buff, uint8_t buff_len, char* ip_dest);
static uint8_t setup_udp_socket(char **argv);
static void process_udp_payload(int argc, char **argv, uint8_t *buff, uint64_t bufflen);

/*================================================================
                        Main function
=================================================================*/
int main(int argc, char** argv) 
{
    uint8_t socket_status = 1;
    socket_status = setup_udp_socket(argv);
    if(0u == socket_status)
    {
        uint8_t buff[] = {0xBA, 0xBA, 0xCE, 0xCA};
        process_udp_payload(argc, argv, buff, sizeof(buff));
        close(sock);
    }
    else
    {
        assert(socket_status);
    }

    return 0;
}

/*================================================================
Function used to open an UDP socket.
=================================================================*/
static void process_udp_payload(int argc, char **argv, uint8_t *buff, uint64_t bufflen)
{
    if(argc > 1)
    {
        switch (argv[1][1])
        {
        case 'e':
            buff[0] = ECHO;
            send_udp_payload(buff, bufflen, argv[2]);
            break;
        case 'r':
            buff[0] = RESET;
            send_udp_payload(buff, bufflen, argv[2]);
            break;
        case 's':
            buff[0] = SCAN;
            send_udp_payload(buff, bufflen, argv[2]);
            break;
        
        default:
            fprintf(stderr, "Undefined TX option!!!\n");
            break;
        }
    }
}
/*================================================================
Function used to open an UDP socket.
=================================================================*/
static uint8_t setup_udp_socket(char **argv)
{
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
        return 0;
}

/*================================================================
Function used to send a paylod via UDP.
=================================================================*/
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
