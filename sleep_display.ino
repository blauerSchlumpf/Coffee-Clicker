#include <WiFi.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "arduino_secrets.h"

// WLAN-Zugangsdaten
const char* ssid = SECRET_SSID;
const char* password = SECRET_PASSWORD;
const char* ntpServer = "pool.ntp.org";
const long winterOffset = 3600;
const int summerOffset = 7200;

// Supabase REST API Infos
const char* SUPABASE_URL = SECRET_URL;
const char* SUPABASE_API_KEY = SECRET_API_KEY;

// UUID des Benutzers (user_id aus Supabase)
const char* USER_ID = SECRET_USERID;

// LCD (z. B. 16x2, I²C-Adresse meist 0x27)
LiquidCrystal_I2C lcd(0x27, 16, 2);

void connectToWiFi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
}
WiFiUDP udpSocket;
NTPClient ntpClient(udpSocket, ntpServer, summerOffset);
void setup() {
  Wire.begin(8, 9);
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Verbinde WLAN");

  connectToWiFi();

  ntpClient.begin();
  ntpClient.update();
  int currentHour = ntpClient.getHours();
  Serial.printf("Es ist %02d:%02d Uhr\n", currentHour, ntpClient.getMinutes());

  if(currentHour >= 6 && currentHour <= 22){
    getCoffeeCount();
  }else{
    Serial.print("schlafenszeit");
  }
  goToSleep();
}

void goToSleep(){
  Serial.println("Deep Sleep für 1 Stunde...");
  esp_sleep_enable_timer_wakeup(3600ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void loop() {
}

void getCoffeeCount() {
  if ((WiFi.status() == WL_CONNECTED)) {
    HTTPClient http;

    String url = String(SUPABASE_URL) + "?user_id=eq." + USER_ID + "&select=coffee_count";
    Serial.println("Anfrage an: " + url);

    http.begin(url);
    http.addHeader("apikey", SUPABASE_API_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_API_KEY));
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println("Antwort: " + payload);

      // Erwartete Antwort: [{"coffee_count":123}]
      int start = payload.indexOf(":") + 1;
      int end = payload.indexOf("}");
      String countStr = payload.substring(start, end);
      int coffeeCount = countStr.toInt();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Coffees today:");
      lcd.setCursor(0, 1);
      lcd.print(coffeeCount);
      lcd.setCursor(0, 2);
      ntpClient.update();
      lcd.printf("Updated at %02d:%02d", ntpClient.getHours(), ntpClient.getMinutes());

    } else {
      Serial.println("Fehler: " + String(httpCode));
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("API Fehler");
    }

    http.end();
  } else {
    Serial.println("Keine WLAN-Verbindung");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WLAN verloren");
  }
}
