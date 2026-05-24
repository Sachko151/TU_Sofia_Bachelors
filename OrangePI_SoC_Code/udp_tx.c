/*================================================================
                        INCLUDES
=================================================================*/
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/*================================================================
                        DEFINES
=================================================================*/
#define DEST_PORT 151

typedef enum
{
  ECHO,
  RESET,
  SCAN,
  AC_ON,
  AC_OFF,
  SET_FAN,
  SET_TEMP,
  SET_MODE,
  SET_SWING,
  TURBO_MODE,
  ECO_MODE,
  SLEEP_MODE,
  LIGTHS_ON,
  LIGHTS_OFF,
  SET_BRIGHTNESS,
  SET_LIGHTS_MODE,
  HEATING_ON,
  HEATING_OFF,
  SET_HEATING_TEMP,
  SET_HEATING_MODE,
  DOOR_LOCK,
  DOOR_UNLOCK,
  RING_DOORBELL,
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
        //cases where e.g. brightess params should be passed too!
        if(argc <= 3)
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
                case 'q':
                    buff[0] = AC_ON;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'w':
                    buff[0] = AC_OFF;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 't':
                    buff[0] = TURBO_MODE;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'y':
                    buff[0] = ECO_MODE;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'o':
                    buff[0] = SLEEP_MODE;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'u':
                    buff[0] = LIGTHS_ON;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'i':
                    buff[0] = LIGHTS_OFF;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'h':
                    buff[0] = HEATING_ON;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'a':
                    buff[0] = HEATING_OFF;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'd':
                    buff[0] = DOOR_LOCK;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'f':
                    buff[0] = DOOR_UNLOCK;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                case 'g':
                    buff[0] = RING_DOORBELL;
                    send_udp_payload(buff, bufflen, argv[2]);
                    break;
                default:
                    assert("Undefined TX option!!!\n");
                    break;
            }
        }
        else
        {
            size_t second_argv_len = 0u;
            second_argv_len = strlen(argv[2]);
            

            if(0u == strncmp("-sfan", argv[1], second_argv_len))
            {
                int arg = atoi(argv[2]);
                buff[0] = SET_FAN;
                buff[1] = argv[2];
                send_udp_payload(buff, bufflen, argv[3]);
            }
            else if(0u == strncmp("-stemp", argv[1], second_argv_len))
            {
                int arg = atoi(argv[2]);
                buff[0] = SET_TEMP;
                buff[1] = argv[2];
                send_udp_payload(buff, bufflen, argv[3]);
            }
            else if(0u == strncmp("-lbright", argv[1], second_argv_len))
            {
                int arg = atoi(argv[2]);
                buff[0] = SET_BRIGHTNESS;
                buff[1] = argv[2];
                send_udp_payload(buff, bufflen, argv[3]);
            }
            else if(0u == strncmp("-slmode", argv[1], second_argv_len))
            {
                int arg = atoi(argv[2]);
                buff[0] = SET_LIGHTS_MODE;
                buff[1] = argv[2];
                send_udp_payload(buff, bufflen, argv[3]);
            }
            else if(0u == strncmp("-shtemp", argv[1], second_argv_len))
            {
                int arg = atoi(argv[2]);
                buff[0] = SET_HEATING_TEMP;
                buff[1] = argv[2];
                send_udp_payload(buff, bufflen, argv[3]);
            }
            else if(0u == strncmp("-shmode", argv[1], second_argv_len))
            {
                int arg = atoi(argv[2]);
                buff[0] = SET_HEATING_MODE;
                buff[1] = argv[2];
                send_udp_payload(buff, bufflen, argv[3]);
            }
            else
            {
                assert("UNKNOWN PARAMS!\n");
            }
        }
    }
    else
    {
        assert("Wrong TX usage!\n");

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
            assert("Socket Error aborting");
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
