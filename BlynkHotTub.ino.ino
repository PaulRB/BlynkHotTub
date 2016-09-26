#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SimpleTimer.h>

#define relaySolarPin D8
#define relayBoilerPin D7
#define relayBoostPin D6
#define ONE_WIRE_BUS D3
#define TUB_TEMP_SENSOR 1
#define PANEL_TEMP_SENSOR 0

int tempSolar = 0;
int tempTub = 0;
int tempTarget = 15;
int boostOn = LOW;
int boilerOn = LOW;
int solarOn = LOW;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WidgetLED solarLed(V4);
WidgetLED boilerLed(V3);
WidgetLED boostLed(V5);

char auth[] = "zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";
char ssid[] = "zzzzzzz";
char pass[] = "zzzzzzzzzz";

SimpleTimer timer;
bool isFirstConnect = true;

void checkTemps() {

  tempSolar = sensors.getTempCByIndex(PANEL_TEMP_SENSOR);
  tempTub = sensors.getTempCByIndex(TUB_TEMP_SENSOR);
  Serial.print("Tub=");
  Serial.print(tempTub);
  Serial.print(" panel=");
  Serial.print(tempSolar);
  Serial.print(" target=");
  Serial.print(tempTarget);

  if (tempSolar > tempTub + 1)
    solarOn = HIGH;
  else if (tempSolar < tempTub - 1)
    solarOn = LOW;

  if (tempTub < tempTarget - 1)
    boilerOn = HIGH;
  else if (tempTub > tempTarget + 1) {
    boilerOn = LOW;
    boostOn = LOW;
  }

  digitalWrite(relaySolarPin, solarOn);
  digitalWrite(relayBoilerPin, boilerOn);
  digitalWrite(relayBoostPin, boostOn);
  Serial.print(" solar=");
  Serial.print(solarOn);
  Serial.print(" boiler=");
  Serial.print(boilerOn);
  Serial.print(" boost=");
  Serial.print(boostOn);
  Serial.println();

}

BLYNK_CONNECTED() {
  if (isFirstConnect) {
    Blynk.syncAll();
    isFirstConnect = false;
  }
}

BLYNK_WRITE(V0)
{
  int pinValue = param.asInt();
  Serial.print("V0 Slider value is: ");
  Serial.println(pinValue);
  tempTarget = pinValue;
}

BLYNK_WRITE(V6)
{
  int pinValue = param.asInt();
  Serial.print("V6 Switch value is: ");
  Serial.println(pinValue);
  boostOn = pinValue;
}

void myTimerEvent()
{

  Blynk.virtualWrite(V1, tempTub);
  Blynk.virtualWrite(V2, tempSolar);
  Blynk.virtualWrite(V6, boostOn);
  if (boostOn == HIGH) boostLed.on(); else boostLed.off();
  if (boilerOn == HIGH) boilerLed.on(); else boilerLed.off();
  if (solarOn == HIGH) solarLed.on(); else solarLed.off();
  
}

void setup()
{
  pinMode(relaySolarPin, OUTPUT);
  pinMode(relayBoilerPin, OUTPUT);
  pinMode(relayBoostPin, OUTPUT);

  sensors.begin();
  Serial.begin(115200);
  Blynk.begin(auth, ssid, pass);
  timer.setInterval(1000L, myTimerEvent);
}

void loop()
{
  sensors.requestTemperatures();
  checkTemps();
  Blynk.run();
  timer.run();
}
