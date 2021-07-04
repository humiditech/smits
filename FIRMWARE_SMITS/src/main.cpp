#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "EEPROM.h"
#include "RTClib.h"
#include "DHT.h"

RTC_DS3231 rtc;


#define BLYNK_PRINT Serial

#define BUZZER_PIN  14
#define LED_PIN     16
#define POMPA_PIN   26
#define LAMPU_PIN   27
#define DHT11_PIN   19
#define LDR_PIN     34

DHT dht(DHT11_PIN, DHT11);

char auth[] = "zjoK1PoZkTDQaGWeiQHRngaVlzGg1onj";

const char * WIFI_SSID = "BISA";
const char * WIFI_PASS = "bayu1234";

void sendTemperatureValue(float temp);
void sendLightIntensityValue(int lightIntensity);
void rtcInit();
void turnOffPompa();
void turnOnPompa();
float readTemperature();
int readLightIntensity();

unsigned long prevMillis = 0;

#define SEND_INTERVAL 5000

int DELAY_POMPA_MATI = 0;
int DELAY_POMPA_MENYALA = 0;

#define DELAY_POMPA_MATI_EEPROM_ADDRESS     0
#define DELAY_POMPA_MENYALA_EEPROM_ADDRESS  1

DateTime nextTurnOnPompaTime;
DateTime nextTurnOffPompaTime;
DateTime now;

bool isPompaTurnedOn = false;
bool manualControl = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(POMPA_PIN, OUTPUT);
  pinMode(LAMPU_PIN, OUTPUT);

  turnOffPompa();

  EEPROM.begin(255);

  DELAY_POMPA_MATI = EEPROM.read(DELAY_POMPA_MATI_EEPROM_ADDRESS);
  DELAY_POMPA_MENYALA = EEPROM.read(DELAY_POMPA_MENYALA_EEPROM_ADDRESS);

  rtcInit();

  dht.begin();

  nextTurnOnPompaTime = rtc.now() + TimeSpan(0, 0, DELAY_POMPA_MATI , 0);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Connecting");
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.println("Connected to WiFi");

  Blynk.begin(auth, WIFI_SSID, WIFI_PASS);
}

void loop() {
  Blynk.run();
  
  if((millis() - prevMillis) >= SEND_INTERVAL){
    sendTemperatureValue(readTemperature());
    sendLightIntensityValue(readLightIntensity());

    prevMillis = millis();
  }

  now = rtc.now();

  if(!manualControl){
    if(now.hour() == nextTurnOnPompaTime.hour() && now.minute() == nextTurnOnPompaTime.minute() && !isPompaTurnedOn){
      turnOnPompa();

      isPompaTurnedOn = !isPompaTurnedOn;

      nextTurnOffPompaTime = rtc.now() + TimeSpan(0, 0, DELAY_POMPA_MENYALA, 0);
    }

    if(now.hour() == nextTurnOffPompaTime.hour() && now.minute() == nextTurnOffPompaTime.minute() && isPompaTurnedOn){
      turnOffPompa();

      isPompaTurnedOn = !isPompaTurnedOn;

      nextTurnOnPompaTime = rtc.now() + TimeSpan(0, 0, DELAY_POMPA_MATI, 0);
    }
  }
}

void sendTemperatureValue(float temp){
  char tempBuf[10];
  sprintf(tempBuf, "%s °C", String(temp,2));

  Blynk.virtualWrite(V4, tempBuf);
}

void sendLightIntensityValue(int lightIntensity){
  char intensityBuf[10];
  sprintf(intensityBuf, "%d %%", lightIntensity);

  Blynk.virtualWrite(V5, intensityBuf);
}

BLYNK_WRITE(V3){
  DELAY_POMPA_MATI = param.asInt() * 60 * 1000; //Convert minutes to milisecods

  EEPROM.write(DELAY_POMPA_MATI_EEPROM_ADDRESS, param.asInt());
  EEPROM.commit();
}

BLYNK_WRITE(V6){
  DELAY_POMPA_MENYALA = param.asInt() * 60 * 1000; //Convert minutes to miliseconds

  EEPROM.write(DELAY_POMPA_MENYALA_EEPROM_ADDRESS, param.asInt());
  EEPROM.commit();
}

BLYNK_WRITE(V0){
  manualControl = param.asInt();
}

BLYNK_WRITE(V1){
  if(manualControl){
    if(param.asInt()){
      turnOnPompa();
    }
    else{
      turnOffPompa();
    }
  }
}

void rtcInit(){
  if(!rtc.begin()){
    Serial.println("Could not find RTC!");
  }

  if(rtc.lostPower()){
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void turnOffPompa(){
  digitalWrite(POMPA_PIN, LOW);
}
void turnOnPompa(){
  digitalWrite(POMPA_PIN, HIGH);
}

float readTemperature(){
  return dht.readTemperature();
}

int readLightIntensity(){
  return map(analogRead(LDR_PIN), 0, 4096, 0, 100);
}
