#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <BlynkSimpleEsp8266.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SimpleTimer.h>

#define relaySolarPin D8
#define relayBoilerPin D7
#define relayBoostPin D6
#define relayFilterPin D5
#define ONE_WIRE_BUS D3
#define TUB_TEMP_SENSOR 1
#define PANEL_TEMP_SENSOR 0

int tempSolar = 0;
int tempTub = 0;
int tempTarget = 15;
int filterMinsDay = 10;
int filterMinsNight = 5;
int boostOn = LOW;
int boilerOn = LOW;
int solarOn = LOW;
int filterOn = LOW;

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
WidgetLED solarLed(V4);
WidgetLED boilerLed(V3);
WidgetLED boostLed(V5);
WidgetLED filterLed(V7);

char auth[] = "c7cc7d54c94d4f93a35da64982056568";
char ssid[] = "B-LINK-2.4G_F2C554";
char pass[] = "wmhmrucd";
//char auth[] = "23760e8fd45d4aca9fb6dd4cded92d24";
//char ssid[] = "granary";
//char pass[] = "sparkym00se";

static const char ntpServerName[] = "uk.pool.ntp.org";
unsigned int localPort = 8888;  // local port to listen for UDP packets
int timeZone = 0;
WiFiUDP Udp;

SimpleTimer timer;
bool isFirstConnect = true;

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.println("Transmit NTP Request");
  // get a random server from the pool
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

bool isBST(int year, int month, int day, int hour)
{
  // bst begins at 01:00 gmt on the last sunday of march
  // and ends at 01:00 gmt (02:00 bst) on the last sunday of october

  // january, february, and november are out
  if (month < 3 || month > 10) {
    return false;
  }

  // april to september are in
  if (month > 3 && month < 10) {
    return true;
  }

  // last sunday of march
  int lastMarSunday =  (31 - (5 * year / 4 + 4) % 7);

  // last sunday of october
  int lastOctSunday = (31 - (5 * year / 4 + 1) % 7);

  // in march we are bst if its past 1am gmt on the last sunday in the month
  if (month == 3)
  {
    if (day > lastMarSunday)
    {
      return true;
    }

    if (day < lastMarSunday)
    {
      return false;
    }

    if (hour < 1)
    {
      return false;
    }

    return true;
  }

  // in october we must be before 1am gmt (2am bst) on the last sunday to be bst
  if (month == 10)
  {
    if (day < lastOctSunday)
    {
      return true;
    }

    if (day > lastOctSunday)
    {
      return false;
    }

    if (hour >= 1)
    {
      return false;
    }

    return true;
  }
}


void checkTemps() {

  tempSolar = sensors.getTempCByIndex(PANEL_TEMP_SENSOR);
  tempTub = sensors.getTempCByIndex(TUB_TEMP_SENSOR);

  if (tempSolar < -50 || tempTub < -50) {
    ESP.restart();
  }
  else {

    if (tempSolar > tempTub + 5)
      solarOn = HIGH;
    else if (tempSolar < tempTub + 3)
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
    digitalWrite(relayFilterPin, filterOn);

  }
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
  Serial.print("Tub Temp (V0) Slider value is: ");
  Serial.println(pinValue);
  tempTarget = pinValue;
}

BLYNK_WRITE(V6)
{
  int pinValue = param.asInt();
  Serial.print("Boost Switch (V6) value is: ");
  Serial.println(pinValue);
  boostOn = pinValue;
}

BLYNK_WRITE(V8)
{
  int pinValue = param.asInt();
  Serial.print("Filter Mins Day Slider (V8) value is: ");
  Serial.println(pinValue);
  filterMinsDay = pinValue;
}

BLYNK_WRITE(V9)
{
  int pinValue = param.asInt();
  Serial.print("Filter Mins Night Slider (V9) value is: ");
  Serial.println(pinValue);
  filterMinsNight = pinValue;
}

void myTimerEvent()
{

  if (timeZone != (isBST(year(), month(), day(), hour()) ? 1 : 0)) {
    timeZone = 1 - timeZone;
    adjustTime(timeZone * 60 * 60);
    Serial.print("Time Zone is now ");
    Serial.println(timeZone);
  }

  if (hour() >= 9 && hour() < 22) {
    if (minute() < filterMinsDay) filterOn = HIGH; else filterOn = LOW;
  }
  else {
    if (minute() < filterMinsNight) filterOn = HIGH; else filterOn = LOW;
  }

  Blynk.virtualWrite(V1, tempTub);
  Blynk.virtualWrite(V2, tempSolar);
  Blynk.virtualWrite(V6, boostOn);
  if (boostOn == HIGH) boostLed.on(); else boostLed.off();
  if (boilerOn == HIGH) boilerLed.on(); else boilerLed.off();
  if (solarOn == HIGH) solarLed.on(); else solarLed.off();
  if (filterOn == HIGH) filterLed.on(); else filterLed.off();

  Serial.printf("%02d:%02d:%02d ", hour(), minute(), second());
  Serial.printf("Tub=%d panel=%d target=%d ", tempTub, tempSolar, tempTarget);
  Serial.printf("solar=%d boiler=%d boost=%d filter=%d\n", solarOn, boilerOn, boostOn, filterOn);

}

void setup()
{
  Serial.begin(115200);
  pinMode(relaySolarPin, OUTPUT);
  pinMode(relayBoilerPin, OUTPUT);
  pinMode(relayBoostPin, OUTPUT);
  pinMode(relayFilterPin, OUTPUT);

  sensors.begin();
  Blynk.begin(auth, ssid, pass);

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(24 * 60 * 60);

  timer.setInterval(1000L, myTimerEvent);
}

void loop()
{
  sensors.requestTemperatures();
  checkTemps();
  Blynk.run();
  timer.run();
}
