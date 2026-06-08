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
#define MAIN_SOC_IP_4th_OCTET 107
#define UDP_COM_PORT 151
#define OTA_ENABLED 0u

#if LIGHTS_ECU
#define PIN_LIGHTS_RELAY      16
#define PIN_LIGHTS_PWM        17
#endif

#if HEATING_ECU
#define PIN_HEATER_RELAY      18
#define PIN_HEATER_PWM        19
#endif

#if DOOR_ECU
#define PIN_DOOR_LOCK         21
#define PIN_DOOR_BUZZER       22
#endif


typedef enum
{
  ECHO,
  RESET,
  SCAN,
  HEALTH_CHECK,
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

typedef enum
{
  echo_state,
  reset_state,
  heartbeat_state,
  idle_state,
  healthcheck_state,
  execute_action_state,
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
uint8_t current_exec_state = COUNT;
uint8_t heartbeat_counter = 0x0;


typedef void (*command_func_t)(void);
/* Function declarations */
#ifdef AC_ECU
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Mitsubishi.h>

#define IR_LED_PIN 4

IRMitsubishiAC ac(IR_LED_PIN);

extern uint8_t g_cmd_data[32];
extern uint8_t g_cmd_len;

typedef struct
{
    bool power;
    uint8_t temperature;
    uint8_t fan;
    uint8_t mode;
    bool swing;
} ac_state_t;

ac_state_t g_ac_state =
{
    false,
    24,
    0,
    4,
    false
};

static void send_ac_state(void);
void ac_on(void);
void ac_off(void);
void set_fan(void);
void set_temp(void);
void set_mode(void);
void set_swing(void);
void turbo_mode(void);
void eco_mode(void);
void sleep_mode(void);
#endif //AC_ECU
#if LIGHTS_ECU
ledcSetup(0, 5000, 8); // channel 0, 5kHz, 8-bit
ledcAttachPin(PIN_LIGHTS_PWM, 0);
void lights_on(void);
void lights_off(void);
void set_brightness(void);
void set_lights_mode(void);
#endif //LIGHTS_ECU
#if HEATING_ECU
ledcSetup(1, 2000, 8);
ledcAttachPin(PIN_HEATER_PWM, 1);
void heating_on(void);
void heating_off(void);
void set_heating_temp(void);
void set_heating_mode(void);
#endif //HEATING_ECU
#if DOOR_ECU
void door_lock(void);
void door_unlock(void);
void ring_doorbell(void);
#endif

command_func_t const command_table[] = {
  NULL, /*Echo*/
  NULL, /*Reset*/
  NULL, /*heartbeat*/
  NULL, /*health_check*/
  #ifdef AC_ECU
  ac_on,
  ac_off,
  set_fan,
  set_temp,
  set_mode,
  set_swing,
  turbo_mode,
  eco_mode,
  sleep_mode,
  #endif //AC_ECU
  #ifdef LIGHTS_ECU
  lights_on,
  lights_off,
  set_brightness,
  set_lights_mode,
  #endif //LIGHTS_ECU
  #ifdef HEATING_ECU
  heating_on,
  heating_off,
  set_heating_temp,
  set_heating_mode,
  #endif //HEATING_ECU
  #ifdef DOOR_ECU
  door_lock,
  door_unlock,
  ring_doorbell
  #endif //DOOR_ECU
};

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
static void healthcheck();
static void execute_action();
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
  #if AC_ECU
    ac.begin();
  #endif //AC_ECU
  #if LIGHTS_ECU
pinMode(PIN_LIGHTS_RELAY, OUTPUT);
pinMode(PIN_LIGHTS_PWM, OUTPUT);
digitalWrite(PIN_LIGHTS_RELAY, LOW);
digitalWrite(PIN_LIGHTS_PWM, LOW);
#endif

#if HEATING_ECU
pinMode(PIN_HEATER_RELAY, OUTPUT);
pinMode(PIN_HEATER_PWM, OUTPUT);
digitalWrite(PIN_HEATER_RELAY, LOW);
digitalWrite(PIN_HEATER_PWM, LOW);
#endif

#if DOOR_ECU
pinMode(PIN_DOOR_LOCK, OUTPUT);
pinMode(PIN_DOOR_BUZZER, OUTPUT);
digitalWrite(PIN_DOOR_LOCK, LOW);
digitalWrite(PIN_DOOR_BUZZER, LOW);
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
    case healthcheck_state:
      healthcheck();
      break;
    case execute_action_state:
      execute_action();
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
      case AC_ON:
      case AC_OFF:
      case SET_FAN:
      case SET_TEMP:
      case SET_MODE:
      case SET_SWING:
      case TURBO_MODE:
      case ECO_MODE:
      case SLEEP_MODE:
      case LIGTHS_ON:
      case LIGHTS_OFF:
      case SET_BRIGHTNESS:
      case SET_LIGHTS_MODE:
      case HEATING_ON:
      case HEATING_OFF:
      case SET_HEATING_TEMP:
      case SET_HEATING_MODE:
      case DOOR_LOCK:
      case DOOR_UNLOCK:
      case RING_DOORBELL:
        Serial.println("Execution state!");
        current_exec_state = incoming[0];
        current_state = execute_action_state;
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
  for (uint8_t i = 0; i < 50; i++)
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

/*================================================================
  Periodic Heart Beat, used to monitor whether any resets have happened.
=================================================================*/
static void healthcheck()
{
  Serial.println("HealthCheck State!");
  uint8_t echo_data[1] = {0x0};
  #ifdef AC_ECU
    echo_data[0] = {0x1};
  #endif
  #ifdef LIGHTS_ECU
    echo_data[0] = {0x2};
  #endif
  #ifdef HEATING_ECU
    echo_data[0] = {0x3};
  #endif
  #ifdef DOOR_ECU
    echo_data[0] = {0x4};
  #endif
  Serial.println("Sending UDP packet to main SoC");
  udp_tx(echo_data, sizeof(echo_data));
}

static void execute_action()
{
  command_table[current_exec_state]();
}

#ifdef AC_ECU

static void send_ac_state(void)
{
    ac.setPower(g_ac_state.power);

    ac.setTemp(g_ac_state.temperature);

    switch(g_ac_state.mode)
    {
        case 0:
            ac.setMode(kMitsubishiAcCool);
            break;

        case 1:
            ac.setMode(kMitsubishiAcHeat);
            break;

        case 2:
            ac.setMode(kMitsubishiAcDry);
            break;

        case 3:
            ac.setMode(kMitsubishiAcFan);
            break;

        default:
            ac.setMode(kMitsubishiAcAuto);
            break;
    }

    switch(g_ac_state.fan)
    {
        case 0:
            ac.setFan(kMitsubishiAcFanAuto);
            break;

        case 1:
            ac.setFan(kMitsubishiAcFanLow);
            break;

        case 2:
            ac.setFan(kMitsubishiAcFanMed);
            break;

        case 3:
            ac.setFan(kMitsubishiAcFanHigh);
            break;

        default:
            ac.setFan(kMitsubishiAcFanAuto);
            break;
    }

    if(g_ac_state.swing)
    {
        ac.setVane(kMitsubishiAcVaneAutoMove);
    }
    else
    {
        ac.setVane(kMitsubishiAcVaneAuto);
    }

    ac.send();
}

void ac_on(void)
{
    Serial.println("AC ON");

    g_ac_state.power = true;

    send_ac_state();

    current_state = idle_state;
}

void ac_off(void)
{
    Serial.println("AC OFF");

    g_ac_state.power = false;

    send_ac_state();

    current_state = idle_state;
}

// Byte 0 = SET_FAN
// Byte 1 = Fan speed

// 0 = Auto
// 1 = Low
// 2 = Medium
// 3 = High
void set_fan(void)
{
    Serial.println("SET FAN");

    if(g_cmd_len >= 2)
    {
        g_ac_state.fan = g_cmd_data[1];

        if(g_ac_state.fan > 3)
        {
            g_ac_state.fan = 0;
        }

        send_ac_state();
    }

    current_state = idle_state;
}

// Byte 0 = SET_TEMP
// Byte 1 = Temperature (16-31)
void set_temp(void)
{
    Serial.println("SET TEMP");

    if(g_cmd_len >= 2)
    {
        uint8_t temp = g_cmd_data[1];

        if(temp < 16)
        {
            temp = 16;
        }

        if(temp > 31)
        {
            temp = 31;
        }

        g_ac_state.temperature = temp;

        send_ac_state();
    }

    current_state = idle_state;
}

// Byte 0 = SET_MODE
// Byte 1 = Mode

// 0 = Cool
// 1 = Heat
// 2 = Dry
// 3 = Fan
// 4 = Auto
void set_mode(void)
{
  Serial.println("SET MODE");

    if(g_cmd_len >= 2)
    {
        g_ac_state.mode = g_cmd_data[1];

        if(g_ac_state.mode > 4)
        {
            g_ac_state.mode = 4;
        }

        send_ac_state();
    }

    current_state = idle_state;
}

// Byte 0 = SET_SWING
// Byte 1 = 0 or 1
void set_swing(void)
{
    Serial.println("SET SWING");

    if(g_cmd_len >= 2)
    {
        g_ac_state.swing = (g_cmd_data[1] != 0);

        send_ac_state();
    }

    current_state = idle_state;
}

void turbo_mode(void)
{
    Serial.println("TURBO MODE");

    g_ac_state.power = true;
    g_ac_state.mode = 0;
    g_ac_state.temperature = 18;
    g_ac_state.fan = 3;

    send_ac_state();

    current_state = idle_state;
}

void eco_mode(void)
{
    Serial.println("ECO MODE");

    g_ac_state.power = true;
    g_ac_state.mode = 0;
    g_ac_state.temperature = 26;
    g_ac_state.fan = 1;

    send_ac_state();

    current_state = idle_state;
}

void sleep_mode(void)
{
    Serial.println("SLEEP MODE");

    g_ac_state.power = true;
    g_ac_state.temperature = 27;
    g_ac_state.fan = 0;

    send_ac_state();

    current_state = idle_state;
}
#endif //AC_ECU
#if LIGHTS_ECU
void lights_on(void)
{
    Serial.println("LIGHTS ON");

    digitalWrite(PIN_LIGHTS_RELAY, HIGH);

    current_state = idle_state;
}
void lights_off(void)
{
    Serial.println("LIGHTS OFF");

    digitalWrite(PIN_LIGHTS_RELAY, LOW);

    current_state = idle_state;
}

void set_brightness(void)
{
    Serial.println("SET BRIGHTNESS");

    if(g_cmd_len >= 2)
    {
        uint8_t brightness = g_cmd_data[1]; // 0-255

        ledcWrite(0, brightness);
    }

    current_state = idle_state;
}

void set_lights_mode(void)
{
    Serial.println("SET LIGHTS MODE");

    if(g_cmd_len >= 2)
    {
        uint8_t mode = g_cmd_data[1];

        switch(mode)
        {
            case 0: // normal
                digitalWrite(PIN_LIGHTS_RELAY, HIGH);
                ledcWrite(0, 255);
                break;

            case 1: // dim
                digitalWrite(PIN_LIGHTS_RELAY, HIGH);
                ledcWrite(0, 80);
                break;

            case 2: // off
                digitalWrite(PIN_LIGHTS_RELAY, LOW);
                ledcWrite(0, 0);
                break;
        }
    }

    current_state = idle_state;
}
#endif
#if HEATING_ECU
void heating_on(void)
{
    Serial.println("HEATING ON");

    digitalWrite(PIN_HEATER_RELAY, HIGH);

    current_state = idle_state;
}

void heating_off(void)
{
    Serial.println("HEATING OFF");

    digitalWrite(PIN_HEATER_RELAY, LOW);

    current_state = idle_state;
}

void set_heating_temp(void)
{
    Serial.println("SET HEATING TEMP");

    if(g_cmd_len >= 2)
    {
        uint8_t temp = g_cmd_data[1]; // e.g. 0–100%

        ledcWrite(1, temp);
    }

    current_state = idle_state;
}

void set_heating_mode(void)
{
    Serial.println("SET HEATING MODE");

    if(g_cmd_len >= 2)
    {
        uint8_t mode = g_cmd_data[1];

        switch(mode)
        {
            case 0: // off
                digitalWrite(PIN_HEATER_RELAY, LOW);
                break;

            case 1: // low
                digitalWrite(PIN_HEATER_RELAY, HIGH);
                ledcWrite(1, 80);
                break;

            case 2: // high
                digitalWrite(PIN_HEATER_RELAY, HIGH);
                ledcWrite(1, 255);
                break;
        }
    }

    current_state = idle_state;
}
#endif
#if DOOR_ECU
void door_lock(void)
{
    Serial.println("DOOR LOCK");

    digitalWrite(PIN_DOOR_LOCK, HIGH);

    current_state = idle_state;
}

void door_unlock(void)
{
    Serial.println("DOOR UNLOCK");

    digitalWrite(PIN_DOOR_LOCK, LOW);

    uint8_t msg[2] = {0x02, 0x00};
    udp_tx(msg, sizeof(msg));

    current_state = idle_state;
}

void ring_doorbell(void)
{
    Serial.println("DOORBELL");

    for(int i = 0; i < 3; i++)
    {
        digitalWrite(PIN_DOOR_BUZZER, HIGH);
        delay(150);
        digitalWrite(PIN_DOOR_BUZZER, LOW);
        delay(150);
    }

    current_state = idle_state;
}
#endif //DOOR_ECU 