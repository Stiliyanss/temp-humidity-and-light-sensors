#include <WiFiS3.h>
#include <DHT.h>

#define DHTPIN 9
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define LDR_PIN A5
#define LED_PIN 13

const char* ssid = "TP-LINK Alexandersson Tove";
const char* password = "14306778";

// Разбий URL-а: https://script.google.com/macros/s/ID/exec
const char* host = "script.google.com";
const char* path = "/macros/s/AKfycbz54v-mUF-sI4SuTTp8tWvNJG8X6TXJnO3EAenAi5AUQUVbIDJQFGDIHxXMRNmGDwlH3w/exec";

int lightThreshold = 26;   // на R4 analogRead е 0-1023, калибрирай го

WiFiSSLClient client;

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LED_PIN, OUTPUT);

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.begin(ssid, password) != WL_CONNECTED) {
    Serial.print(".");
    delay(2000);
  }
  Serial.println("\nWi-Fi connected!");
  Serial.println(WiFi.localIP());
  client.setTimeout(10000);
}

void loop() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int lightValue = analogRead(LDR_PIN);

  digitalWrite(LED_PIN, lightValue > lightThreshold ? HIGH : LOW);
  String ledState = digitalRead(LED_PIN) == HIGH ? "ON" : "OFF";

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read DHT22!");
    delay(2000);
    return;
  }

  Serial.print("T: "); Serial.print(temperature);
  Serial.print(" C | H: "); Serial.print(humidity);
  Serial.print(" % | LDR: "); Serial.print(lightValue);
  Serial.print(" | LED: "); Serial.println(ledState);

  String body = "{\"temperature\":" + String(temperature, 2) +
                ",\"humidity\":" + String(humidity, 2) +
                ",\"light\":" + String(lightValue) +
                ",\"led\":\"" + ledState + "\"}";

  if (client.connect(host, 443)) {
    client.println(String("POST ") + path + " HTTP/1.1");
    client.println(String("Host: ") + host);
    client.println("Content-Type: application/json");
    client.print("Content-Length: ");
    client.println(body.length());
    client.println("Connection: close");
    client.println();
    client.print(body);

    String status = client.readStringUntil('\n');
    Serial.print("Response: ");
    Serial.println(status);
    client.stop();
  } else {
    Serial.println("Connection to Google failed");
  }

  delay(10000);
}