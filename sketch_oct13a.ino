#include <WiFiS3.h>
#include <DHT.h>
#include <SPI.h>
#include <SD.h>
#include "RTC.h"

#define DHTPIN 9
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define LDR_PIN A5
#define LED_PIN 7          // преместен от 13, защото D13 е часовникът на SD (SCK)
#define SD_CS_PIN 10
const char* ssid = "Schoenmakers";
const char* password = "solutio365";
const char* host = "script.google.com";

const char* path = "/macros/s/AKfycbxP-p800kgiBqnOXZnZiWfP2Ywnl7qPOdk2hgJEK0JYXvj7TtcMolafInykjB9NDQJ_gQ/exec";

int lightThreshold = 26;
const unsigned long INTERVAL_MS = 60000;   // за истинското измерване: 300000 (5 мин)
const char* LOG_FILE = "LOG.CSV";

WiFiSSLClient client;
bool sdOk = false;

void connectWifi() {
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.print(".");
    delay(2000);
  }
  Serial.println("\nWi-Fi connected!");
}

void dumpLog() {
  File f = SD.open(LOG_FILE, FILE_READ);
  if (!f) {
    Serial.println("Could not read LOG.CSV");
    return;
  }
  Serial.println("---- LOG.CSV ----");
  while (f.available()) Serial.write(f.read());
  Serial.print("---- size: ");
  Serial.print(f.size());
  Serial.println(" bytes ----");
  f.close();
}


String nowString() {
  RTCTime now;
  RTC.getTime(now);
  char buf[25];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           now.getYear(), Month2int(now.getMonth()), now.getDayOfMonth(),
           now.getHour(), now.getMinutes(), now.getSeconds());
  return String(buf);
}

bool sendToGoogle(float t, float h, int light) {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
    delay(5000);
    if (WiFi.status() != WL_CONNECTED) return false;
  }

  String ledState = digitalRead(LED_PIN) == HIGH ? "ON" : "OFF";

  // Сглобяваме URL с параметрите
  String url = String(path) + "?temperature=" + String(t, 2) +
               "&humidity=" + String(h, 2) +
               "&light=" + String(light) +
               "&led=" + ledState;

  if (!client.connect(host, 443)) {
    Serial.println("Connection to Google failed!");
    return false;
  }

  // Изпращаме GET заявка
  client.println("GET " + url + " HTTP/1.1");
  client.println("Host: " + String(host));
  client.println("User-Agent: ArduinoWiFi/1.0");
  client.println("Connection: close");
  client.println();

  // Изчакваме отговор
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 5000) {
      Serial.println("Timeout waiting for response!");
      client.stop();
      return false;
    }
  }

  String status = client.readStringUntil('\r');
  Serial.print("Response: ");
  Serial.println(status);

  client.stop();
  return status.indexOf("302") >= 0 || status.indexOf("200") >= 0;
}

void logToSd(const String& ts, float t, float h, int light, bool sent) {
  File f = SD.open(LOG_FILE, FILE_WRITE);
  if (!f) {
    Serial.println("Could not open LOG.CSV");
    return;
  }
  f.print(ts);        f.print(",");
  f.print(t, 2);      f.print(",");
  f.print(h, 2);      f.print(",");
  f.print(light);     f.print(",");
  f.println(sent ? 1 : 0);
  f.close();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);

  sdOk = SD.begin(SD_CS_PIN);
  Serial.println(sdOk ? "SD OK" : "SD FAILED - only sending to Google");
  if (sdOk && !SD.exists(LOG_FILE)) {
    File f = SD.open(LOG_FILE, FILE_WRITE);
    if (f) {
      f.println("timestamp_utc,temperature,humidity,light,sent");
      f.close();
    }
  }

  connectWifi();
  RTC.begin();
  client.setTimeout(10000);
  // if(sdOk) dumpLog();
}

void loop() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int light = analogRead(LDR_PIN);

  digitalWrite(LED_PIN, light > lightThreshold ? HIGH : LOW);

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read DHT22!");
    delay(2000);
    return;
  }

  String ts = nowString();
  bool sent = sendToGoogle(t, h, light);
  if (sdOk) logToSd(ts, t, h, light, sent);

  Serial.print(ts);
  Serial.print(" | T: "); Serial.print(t);
  Serial.print(" C | H: "); Serial.print(h);
  Serial.print(" % | LDR: "); Serial.print(light);
  Serial.print(" | sent: "); Serial.println(sent ? "yes" : "NO");

  delay(INTERVAL_MS);
}
