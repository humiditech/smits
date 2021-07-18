#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "EEPROM.h"
#include "RTClib.h"
#include "DHT.h"
#include <RBDdimmer.h>

RTC_DS3231 rtc;


#define BLYNK_PRINT Serial

#define BUZZER_PIN      14
#define LED_PIN         16
#define POMPA_PIN       26
#define LAMPU_PIN       27
#define DHT11_PIN       19
#define LDR_PIN         34
#define DIMMER_OUT_PIN  17
#define DIMMER_ZERO_PIN 18

dimmerLamp dimmer(DIMMER_OUT_PIN, DIMMER_ZERO_PIN);

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
void turnUVOn(int intensity);

unsigned long prevMillis = 0;

#define SEND_INTERVAL 5000

int DELAY_POMPA_MATI = 0;
int DELAY_POMPA_MENYALA = 0;

#define DELAY_POMPA_MATI_EEPROM_ADDRESS     0
#define DELAY_POMPA_MENYALA_EEPROM_ADDRESS  1

DateTime nextTurnOnPompaTime;
DateTime nextTurnOffPompaTime;
DateTime nextTurnOnUV;
DateTime nextTurnOffUV;
DateTime now;

bool isPompaTurnedOn = false;
bool manualControl = false;

#define LIGHT_INTENSITY_THRESHOLD 50 // Batas intensitas cahaya untuk menyalakan lampu UV (dalam persen)

bool isDayLightTurnedOn = false;
bool isNightLightTurnedOn = false;
int turnOnUvStep = 1;
bool isTimeToChangeUVIntensity = false;

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  // pinMode(LED_PIN, OUTPUT);
  pinMode(POMPA_PIN, OUTPUT);
  pinMode(LAMPU_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);

  dimmer.begin(NORMAL_MODE, ON);
  
  turnUVOn(0);

  turnOffPompa();

  EEPROM.begin(255);

  DELAY_POMPA_MATI = EEPROM.read(DELAY_POMPA_MATI_EEPROM_ADDRESS);
  DELAY_POMPA_MENYALA = EEPROM.read(DELAY_POMPA_MENYALA_EEPROM_ADDRESS);

  Serial.printf("DELAY_POMPA_MATI : %d \n", DELAY_POMPA_MATI);
  Serial.printf("DELAY_POMPA_MENYALA : %d \n", DELAY_POMPA_MENYALA);

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

  now = rtc.now();
  
  if((millis() - prevMillis) >= SEND_INTERVAL){
    // sendTemperatureValue(readTemperature());
    // sendLightIntensityValue(readLightIntensity());

    sendTemperatureValue(readTemperature());
    sendLightIntensityValue(readLightIntensity());

    prevMillis = millis();
  }

  if(!manualControl){
    if(now.hour() == nextTurnOnPompaTime.hour() && now.minute() == nextTurnOnPompaTime.minute() && !isPompaTurnedOn){
      Serial.println("Menyalakan Pompa");
      Serial.printf("Current Time : %02d:%02d", now.hour(), now.minute());
      turnOnPompa();

      isPompaTurnedOn = !isPompaTurnedOn;

      nextTurnOffPompaTime = rtc.now() + TimeSpan(0, 0, DELAY_POMPA_MENYALA, 0);
    }

    if(now.hour() == nextTurnOffPompaTime.hour() && now.minute() == nextTurnOffPompaTime.minute() && isPompaTurnedOn){
      Serial.println("Mematikan Pompa");
      Serial.printf("Current Time : %02d:%02d", now.hour(), now.minute());
      turnOffPompa();

      isPompaTurnedOn = !isPompaTurnedOn;

      nextTurnOnPompaTime = rtc.now() + TimeSpan(0, 0, DELAY_POMPA_MATI, 0);
    }

    if((now.hour() >= 5) && (now.hour() < 17)){ // Cek apakah waktu terbit sampai terbenam
      if(readLightIntensity() <= LIGHT_INTENSITY_THRESHOLD){
        if(!isDayLightTurnedOn){
          isDayLightTurnedOn = true;
          isTimeToChangeUVIntensity = false;
          turnOnUvStep = 1;

          nextTurnOnUV = now;
        }

        else{
          if((nextTurnOnUV.hour() == now.hour()) && (nextTurnOnUV.minute() == now.minute())){
            isTimeToChangeUVIntensity = true;

            nextTurnOnUV = now + TimeSpan(0, 0, 12 , 0);
          }

          if(isTimeToChangeUVIntensity){
            switch (turnOnUvStep)
            {
              case 1 :
                turnUVOn(20);

                turnOnUvStep = 2;
                break;
              case 2 :
                turnUVOn(40);

                turnOnUvStep = 3;
                break;
              case 3 :
                turnUVOn(60);

                turnOnUvStep = 4;
                break;
              case 4 :
                turnUVOn(80);

                turnOnUvStep = 5;
                break;
              case 5 :
                turnUVOn(100);

                turnOnUvStep = 1;
                isDayLightTurnedOn = false;
                break;
              
              default:
                break;
            }
            isTimeToChangeUVIntensity = false;
          }
        }
      }
      else{
        isDayLightTurnedOn = false;
        turnUVOn(1);
      }
    }

    else{
      if(!isNightLightTurnedOn){
        nextTurnOnUV = now;

        isNightLightTurnedOn = true;
        isTimeToChangeUVIntensity = false;
      }
      else{
        if((nextTurnOnUV.hour() == now.hour()) && (nextTurnOnUV.minute() == now.minute())){
          isTimeToChangeUVIntensity = true;

          nextTurnOnUV = now + TimeSpan(0, 0, 12 , 0);
        }
        if(isTimeToChangeUVIntensity){
            switch (turnOnUvStep)
            {
              case 1 :
                Serial.println("Case 1"); 
                Serial.printf("%02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
                turnUVOn(100);

                turnOnUvStep = 2;
                break;
              case 2 :
                Serial.println("Case 1"); 
                Serial.printf("%02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
                turnUVOn(82);

                turnOnUvStep = 3;
                break;
              case 3 :
                Serial.println("Case 1"); 
                Serial.printf("%02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
                turnUVOn(90);

                turnOnUvStep = 4;
                break;
              case 4 :
                Serial.println("Case 1"); 
                Serial.printf("%02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
                turnUVOn(95);

                turnOnUvStep = 5;
                break;
              case 5 :
                Serial.println("Case 1"); 
                Serial.printf("%02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
                turnUVOn(80);

                turnOnUvStep = 1;
                isNightLightTurnedOn = false;
                break;
              
              default:
                break;
            }
            isTimeToChangeUVIntensity = false;
        }
      }
      
    }
  }
}

void turnUVOn(int intensity){
  intensity = map(intensity, 0, 100, 0, 90);

  if(intensity == 0){
    digitalWrite(LAMPU_PIN, HIGH);
  }
  else{
    digitalWrite(LAMPU_PIN, LOW);
    dimmer.setPower(intensity);
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
  Serial.println(param.asInt());
  if(manualControl){
    if(param.asInt()){
      turnOnPompa();
    }
    else{
      turnOffPompa();
    }
  }
}

BLYNK_WRITE(V2){
  Serial.println(param.asInt());
  if(manualControl){
    turnUVOn(param.asInt());
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
  digitalWrite(POMPA_PIN, HIGH);
}
void turnOnPompa(){
  digitalWrite(POMPA_PIN, LOW);
}

float readTemperature(){
  return dht.readTemperature();
}

int readLightIntensity(){
  return map(analogRead(LDR_PIN), 0, 4096, 100, 0);
}
