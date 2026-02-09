#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <Update.h>

const char* ssid = "TP-Link-151";
const char* password = "Sachko151!";

String fw_ver = "1.0.1";

WiFiUDP udp;

// URL where firmware is hosted
const char* firmwareUrl = "http://192.168.0.105:5000/ota";
void udp_comm();
void connectWiFi();
void checkForUpdate();
String getLatestVersion();


void setup() {
  Serial.begin(9600);
  connectWiFi();

  delay(3000);  // wait before checking
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

void loop() {
  Serial.print("sadsad\n");
  delay(1000);
}

void udp_comm()
{
  int packetSize = udp.parsePacket();

  if (packetSize) 
  {
    Serial.printf("Received %d bytes from %s:%d\n", packetSize, udp.remoteIP().toString().c_str(), udp.remotePort());

    char incoming[255];
    int len = udp.read(incoming, 254);
    if (len > 0) incoming[len] = 0;

    Serial.println("Packet:");
    Serial.println(incoming);
  }
}
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

String getLatestVersion() 
{
  HTTPClient http;
  http.begin("http://192.168.0.105:5000/version");  // your server IP
  int httpCode = http.GET();

  String version = "";
  if (httpCode == HTTP_CODE_OK) {
    version = http.getString();
    version.trim();  // removes newline
  }
  http.end();
  return version;
}