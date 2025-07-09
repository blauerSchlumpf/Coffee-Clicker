#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_PN532.h>
#include "arduino_secrets.h"

#define SDA_PIN 21
#define SCL_PIN 22

const char* ssid = SECRET_SSID;
const char* password = SECRET_PASSWORD;

const char* SUPABASE_URL = SECRET_URL;
const char* SUPABASE_API_KEY = SECRET_API_KEY;

Adafruit_PN532 nfc(SDA_PIN, SCL_PIN);

// NDEF: coffeeclickermaui://open
const uint8_t ndef_msg[] = {
  0x03, 0x13,
  0xD1, 0x01, 0x0F,
  0x55, 0x00,
  'c', 'o', 'f', 'f', 'e', 'e', 'c', 'l', 'i', 'c', 'k', 'e', 'r', ':', '/', '/', 'o', 'p', 'e', 'n',
  0xFE
};

String place = "";
int coffee_id = -1;

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  // WLAN verbinden
  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWLAN verbunden!");

  // NFC initialisieren
  nfc.begin();
  if (!nfc.getFirmwareVersion()) {
    Serial.println("PN532 nicht gefunden!");
    while (1)
      ;
  }
  nfc.SAMConfig();
  Serial.println("PN532 bereit");
}

void loop() {
  String place = "";
  int coffee_id = -1;

  // Daten für den Tag eingeben
  Serial.println("\nBitte 'place' eingeben (z.B. Küche):");
  while (place.length() == 0) {
    if (Serial.available()) {
      place = Serial.readStringUntil('\n');
      place.trim();
    }
  }

  Serial.println("Bitte coffee_id eingeben (z.B. 2):");
  while (coffee_id < 0) {
    if (Serial.available()) {
      coffee_id = Serial.readStringUntil('\n').toInt();
    }
  }

  Serial.printf("Ort: %s | coffee_id: %d\n", place.c_str(), coffee_id);
  Serial.println("Tag auflegen zum Beschreiben...");

  // Warten auf Tag
  while (true) {
    uint8_t uid[7];
    uint8_t uidLength;
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
      Serial.print("UID erkannt: ");
      for (uint8_t i = 0; i < uidLength; i++) {
        Serial.printf("%02X ", uid[i]);
      }
      Serial.println();

      bool writeSuccess = writeTag(uid, uidLength);
      if (writeSuccess) {
        sendToSupabase(uid, uidLength, place, coffee_id);
      } else {
        Serial.println("Fehler beim Schreiben des Tags.");
      }

      delay(2000);  // Kurze Pause
      break;        // Danach wieder neue Eingaben abfragen
    }
  }
}

// Tag beschreiben
bool writeTag(uint8_t* uid, uint8_t uidLength) {
  uint8_t keyA[6] = { 0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7 };

  int block = 4;
  int ndef_len = sizeof(ndef_msg);
  for (int i = 0; i < ndef_len && block < 7; i += 16, block++) {
    uint8_t buffer[16] = { 0 };
    memcpy(buffer, ndef_msg + i, min(16, ndef_len - i));

    if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, block, 0, keyA)) {
      if (!nfc.mifareclassic_WriteDataBlock(block, buffer)) {
        Serial.printf("Fehler bei Block %d\n", block);
        return false;
      }
    } else {
      Serial.printf("Authentifizierung fehlgeschlagen (Block %d)\n", block);
      return false;
    }
  }
  Serial.println("Tag erfolgreich beschrieben.");
  return true;
}

// Supabase senden
void sendToSupabase(uint8_t* uid, uint8_t uidLength, String place, int coffee_id) {
  HTTPClient http;
  http.begin(SUPABASE_URL);
  http.addHeader("apikey", SUPABASE_API_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_API_KEY);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=representation");

  String uidStr = "";
  for (uint8_t i = 0; i < uidLength; i++) {
    if (i > 0) uidStr += ":";
    if (uid[i] < 0x10) uidStr += "0";
    uidStr += String(uid[i], HEX);
  }

  String payload = "{";
  payload += "\"uid\": \"" + uidStr + "\",";
  payload += "\"place\": \"" + place + "\",";
  payload += "\"coffee_type\": " + String(coffee_id);
  payload += "}";

  int httpCode = http.POST(payload);
  if (httpCode > 0) {
    Serial.println("Supabase POST erfolgreich");
    Serial.println("Antwort: " + http.getString());
  } else {
    Serial.println("Fehler bei POST: " + String(httpCode));
  }
  http.end();
}