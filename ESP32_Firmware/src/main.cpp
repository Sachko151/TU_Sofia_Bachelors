/*================================================================
                        INCLUDES
=================================================================*/
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <Update.h>


/*================================================================
                        DEFINES
=================================================================*/
#define OTA_CHECK_URL "http://192.168.0.102:5000/ota"
#define OTA_FW_LOCATION "http://192.168.0.102:5000/version"
#define MAIN_SOC_IP_3rd_OCTET 0
#define MAIN_SOC_IP_4th_OCTET 102
#define UDP_COM_PORT 151
#define OTA_ENABLED 0u

typedef enum
{
  ECHO,
  RESET,
  SCAN,
  COUNT
} commands_t;

typedef enum
{
  echo_state,
  reset_state,
  heartbeat_state,
  idle_state,
  count_state
} state_t;

/*================================================================
                      GLOBALS/CONST
=================================================================*/
const char* firmwareUrl = OTA_CHECK_URL;
const char* ssid = "TP-Link-151";
const char* password = "Sachko151!";
// const char* ssid = "2__1__3";
// const char* password = "Mehana213";
String fw_ver = "1.0.1";
WiFiUDP udp;
uint8_t current_state = idle_state;
uint8_t heartbeat_counter = 0x0;


/*================================================================
                      PROTOTYPES
=================================================================*/
void udp_comm();
void connectWiFi();
void checkForUpdate();
String getLatestVersion();
void handleOTAUpdate();
static void state_machine();
static void echo();
static void reset();
static void idle();
static void heartbeat();
void udp_tx(uint8_t *data, uint8_t len);
void udp_rx();
void mainLogic();

/*================================================================
                      Setup of ESP
=================================================================*/
void setup() {
  Serial.begin(9600);
  connectWiFi();
  delay(3000);  // wait before checking
  #if  OTA_ENABLED == 1
    handleOTAUpdate();
  #endif
  udp.begin(UDP_COM_PORT);
  
}

/*================================================================
                      Loop of ESP
=================================================================*/
void loop() {
  udp_rx(); 
  state_machine();
}

/*================================================================
                     State Machine Function
=================================================================*/
void state_machine()
{
  switch (current_state)
  {
    case echo_state:
      echo();
      break;
    case reset_state:
      reset();
      break;
    case heartbeat_state:
      heartbeat();
      break;
  default:
    break;
  }
}

/*================================================================
        Handle OTA Update - check whether update is needed
=================================================================*/
void handleOTAUpdate()
{
  String latest = getLatestVersion();
  if(latest.isEmpty() || latest.charAt(4) == fw_ver.charAt(4))
  {
    Serial.println("No update needed.");
  }
  else
  {
    Serial.println("Update needed.");
    checkForUpdate();
  }
}

/*================================================================
                      UDP RX of packets
=================================================================*/
void udp_rx()
{
  int packetSize = udp.parsePacket();

  if (packetSize) 
  {
    Serial.printf("Received %d bytes from %s:%d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());
    char incoming[32];
    int len = udp.read(incoming, 32);
    if (len > 0)
    {
      incoming[len] = 0;
    } 
    Serial.println("Packet:");
    for (size_t i = 0; i < packetSize; i++)
    {
      Serial.printf("%x ", incoming[i]);
    }
    Serial.println();
    

    switch (incoming[0])
    {
      case ECHO:
      case SCAN:
        Serial.println("Going into Echo state!");
        current_state = echo_state;
        break;
      case RESET:
      Serial.println("Going into Reset state!");
        current_state = reset_state;
        break;
      default:
      Serial.println("Going into Idle state!");
      current_state = idle_state;
        break;
    }
  }
}

/*================================================================
                    UDP TX to main SoC
=================================================================*/
void udp_tx(uint8_t *data, uint8_t len)
{
  IPAddress ip(192,168,MAIN_SOC_IP_3rd_OCTET,MAIN_SOC_IP_4th_OCTET);
  if (!udp.beginPacket(ip, UDP_COM_PORT))
  {
    Serial.println("beginPacket failed");
    return;
  }

  int written = udp.write(data, len);

  Serial.printf("Bytes written: %d\n", written);

  if (!udp.endPacket())
  {
    Serial.println("endPacket failed");
  }
  else
  {
    Serial.println("Packet sent OK");
  }
  
}

/*================================================================
                    Connect to WiFi
=================================================================*/
void connectWiFi() 
{
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

/*================================================================
                    Check for OTA Update
=================================================================*/
void checkForUpdate() 
{
  HTTPClient http;
  http.begin(firmwareUrl);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    bool canBegin = Update.begin(contentLength);

    if (canBegin) {
      Serial.println("Starting OTA update...");

      WiFiClient * stream = http.getStreamPtr();
      size_t written = Update.writeStream(*stream);

      if (written == contentLength) {
        Serial.println("Written successfully");
      } else {
        Serial.println("Write failed");
      }

      if (Update.end()) {
        if (Update.isFinished()) {
          Serial.println("Update complete. Rebooting...");
          ESP.restart();
        } else {
          Serial.println("Update not finished.");
        }
      } else {
        Serial.println("Error Occurred.");
      }

    } else {
      Serial.println("Not enough space for OTA");
    }
  } else {
    Serial.printf("HTTP Error: %d\n", httpCode);
  }

  http.end();
}

/*================================================================
   Check Latest Version of ESP Firmware.bin provided by main SoC
=================================================================*/
String getLatestVersion() 
{
  HTTPClient http;
  http.begin(OTA_FW_LOCATION);
  int httpCode = http.GET();

  String version = "";
  if (httpCode == HTTP_CODE_OK) {
    version = http.getString();
    version.trim();
  }
  http.end();
  return version;
}

/*================================================================
                ECHO response back to main SoC
=================================================================*/
static void echo()
{
  Serial.println("Echo State!");
  uint8_t echo_data[2] = {0xFF, 0xFF};
  for (uint8_t i = 0; i < 25; i++)
  {
    //??? Perhaps handshake
    delay(50);
    Serial.println("Sending UDP packet to main SoC");
    
    udp_tx(echo_data, sizeof(echo_data));
  }
  current_state = idle_state;
  
}

/*================================================================
            Perform Reset requested by Main SoC
=================================================================*/
static void reset()
{
  Serial.println("Reset State!");
  uint8_t echo_data[2] = {0xFF, 0xFE};
  Serial.println("Sending UDP packet to main SoC");
  udp_tx(echo_data, sizeof(echo_data));
  esp_restart();
  
  
}

/*================================================================
  Periodic Heart Beat, used to monitor whether any resets have happened.
=================================================================*/
static void heartbeat()
{
  Serial.println("HeartBeat State!");
  uint8_t echo_data[1] = {heartbeat_counter++};
  Serial.println("Sending UDP packet to main SoC");
  udp_tx(echo_data, sizeof(echo_data));
  esp_restart();
}