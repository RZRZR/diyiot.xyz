// CONNECT THE RS485 MODULE RX->RX, TX->TX.
// Disconnect when uploading code.
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp8266.h>
#include <SimpleTimer.h>
#include <ModbusMaster.h>
#include <ESP8266WiFi.h>
#include "settings.h"

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

const char* WIFI_SSID     = ""; //your wifi SSID/name
const char* WIFI_PASS = ""; //your wifi password

int timerTask1, timerTask2, timerTask3;
float battBhargeCurrent, bvoltage, ctemp, btemp, lpower, lcurrent, pvvoltage, pvcurrent, pvpower, pvcurrent12, batt_left_charge;
int bremaining;
float stats_today_pv_volt_min, stats_today_pv_volt_max;
uint8_t result;

// this is to check if we can write since rs485 is half duplex

ModbusMaster node;
SimpleTimer timer;

// tracer requires no handshaking
void preTransmission() {}
void postTransmission() {}

// a list of the regisities to query in order
typedef void (*RegistryList[])();
RegistryList Registries = {
  AddressRegistry_3100,
  AddressRegistry_311A,
  AddressRegistry_3300,
};
// keep log of where we are
uint8_t currentRegistryNumber = 0;
// function to switch to next registry
void nextRegistryNumber() {
  currentRegistryNumber = (currentRegistryNumber + 1) % ARRAY_SIZE( Registries);
}

void setup()
{

  // Modbus communication runs at 115200 baud --> you can make this slower if you want, but works fine either way. I would suggest unplugging the RS485 from the ESP when you are uploading the code. Plug it back in as soon as the code successfully uploads
  Serial.begin(115200);
  Serial.print("testing?");
  // Modbus slave ID 1
  node.begin(1, Serial);
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);
  Serial.println("tested");
  WiFi.mode(WIFI_STA);
  Serial.println("connected?");
  delay(10000);
  if (debug == 1){
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    delay(5000);
  }

  Blynk.begin(AUTH, WIFI_SSID, WIFI_PASS);
  while (Blynk.connect() == false) {}
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.begin();

  timerTask1 = timer.setInterval(1000, updateBlynk);
  timerTask2 = timer.setInterval(1000, doRegistryNumber);
  timerTask3 = timer.setInterval(1000, nextRegistryNumber);
  Serial.println("Set Up End");
}

bool rs485DataReceived = true;
int numLoops = 10000;

void updateBlynk() {
  Blynk.virtualWrite(vPIN_PV_POWER,         pvpower);
  Blynk.virtualWrite(vPIN_PV_CURRENT,       pvcurrent);
  Blynk.virtualWrite(vPIN_PV_VOLTAGE,       pvvoltage);
  Blynk.virtualWrite(vPIN_PV_CURRENT_12V,   pvcurrent12);
  Blynk.virtualWrite(vPIN_LOAD_CURRENT,     lcurrent);
  Blynk.virtualWrite(vPIN_LOAD_POWER,       lpower);
  Blynk.virtualWrite(vPIN_BATT_TEMP,        btemp);
  Blynk.virtualWrite(vPIN_BATT_VOLTAGE,     bvoltage);
  Blynk.virtualWrite(vPIN_BATT_REMAIN,      bremaining);
  Blynk.virtualWrite(vPIN_BATT_CHARGE,      battBhargeCurrent);
  Blynk.virtualWrite(vPIN_CONTROLLER_TEMP,  ctemp);
  Blynk.virtualWrite(vBATT_LEFT_CHARGE,     batt_left_charge);
}

void doRegistryNumber() {
  Registries[currentRegistryNumber]();
}

void AddressRegistry_3100() {
  result = node.readInputRegisters(0x3100, 7);
  if (result == node.ku8MBSuccess)
  {
    ctemp = node.getResponseBuffer(0x11) / 100.0f;
    if (debug == 1) {
      Serial.println(ctemp);
      Serial.print("Battery Voltage: ");
    }
    bvoltage = node.getResponseBuffer(0x04) / 100.0f;
    if (debug == 1) {
      Serial.println(bvoltage);

    }
    lpower = ((long)node.getResponseBuffer(0x0F) << 16 | node.getResponseBuffer(0x0E)) / 100.0f;
    if (debug == 1) {
      Serial.print("Load Power: ");
      Serial.println(lpower);

    }
    lcurrent = (long)node.getResponseBuffer(0x0D) / 100.0f;
    if (debug == 1) {
      Serial.print("Load Current: ");
      Serial.println(lcurrent);

    }
    pvvoltage = (long)node.getResponseBuffer(0x00) / 100.0f;
    if (debug == 1) {
      Serial.print("PV Voltage: ");
      Serial.println(pvvoltage);

    }
    pvcurrent = (long)node.getResponseBuffer(0x01) / 100.0f;
    if (debug == 1) {
      Serial.print("PV Current: ");
      Serial.println(pvcurrent);

    }
    pvpower = ((long)node.getResponseBuffer(0x03) << 16 | node.getResponseBuffer(0x02)) / 100.0f;
    if (debug == 1) {
      Serial.print("PV Power: ");
      Serial.println(pvpower);
      Serial.print("PV amps at 12V: ");
      pvcurrent12 = pvpower / 12;
      Serial.println(pvcurrent12);
    }
    battBhargeCurrent = (long)node.getResponseBuffer(0x05) / 100.0f;
    if (debug == 1) {
      Serial.print("Battery Charge Current: ");
      Serial.println(battBhargeCurrent);
      batt_left_charge = battBhargeCurrent - lcurrent;
      Serial.print("Left over amps for battery: ");
      Serial.println(batt_left_charge);


      Serial.println();
    }
  } else {
    rs485DataReceived = false;
  }
}

void AddressRegistry_311A() {
  result = node.readInputRegisters(0x311A, 2);
  if (result == node.ku8MBSuccess)
  {
    bremaining = node.getResponseBuffer(0x00) / 1.0f;
    if (debug == 1) {
      Serial.print("Battery Remaining %: ");
      Serial.println(bremaining);

    }
    btemp = node.getResponseBuffer(0x01) / 100.0f;
    if (debug == 1) {
      Serial.print("Battery Temperature: ");
      Serial.println(btemp);
      Serial.println();
    }
  } else {
    rs485DataReceived = false;
  }
}

void AddressRegistry_3300() {
  result = node.readInputRegisters(0x3300, 2);
  if (result == node.ku8MBSuccess)
  {
    stats_today_pv_volt_max = node.getResponseBuffer(0x00) / 100.0f;
    if (debug == 1) {
      Serial.print("Stats Today PV Voltage MAX: ");
      Serial.println(stats_today_pv_volt_max);
    }
    stats_today_pv_volt_min = node.getResponseBuffer(0x01) / 100.0f;
    if (debug == 1) {
      Serial.print("Stats Today PV Voltage MIN: ");
      Serial.println(stats_today_pv_volt_min);
    }
  } else {
    rs485DataReceived = false;
  }
}

void loop()
{
  Blynk.run();
  Serial.println("blynk");
  uint8_t result,time1, time2, time3, date1, date2, date3, dateDay, dateMonth, dateYear, timeHour, timeMinute, timeSecond;
  // uint16_t data[6];
  char buf[10];
  String dtString;
  float bvoltage, ctemp, btemp, bremaining, lpower, lcurrent, pvvoltage, pvcurrent, pvpower;

  if (debug == 1){
    Serial.print("Beginning Loop ");
    Serial.println(numLoops);
  }


  delay(500);




  // Read 20 registers starting at 0x3100)
  result = node.readInputRegisters(0x3100, 20);
  if (result == node.ku8MBSuccess)
  {
    if (debug == 1){
      Serial.println("------------------------------------------------------------------");
      Serial.print("Controller Temperature: ");
    }
    ctemp = node.getResponseBuffer(0x11)/100.0f;
    if (debug == 1){
      Serial.println(ctemp);
    }
    Serial.print("Battery Voltage: ");

    bvoltage = node.getResponseBuffer(0x04)/100.0f;
    if (debug == 1){
      Serial.println(bvoltage);
    }
  Serial.print("Load Power: ");
    lpower = ((long)node.getResponseBuffer(0x0F)<<16|node.getResponseBuffer(0x0E))/100.0f;
    if (debug == 1){
       Serial.println(lpower);
       Serial.print("Load Current: ");
    }
    lcurrent = (long)node.getResponseBuffer(0x0D)/100.0f;
    if (debug == 1){
      Serial.println(lcurrent);
      Serial.print("PV Voltage: ");
    }
    pvvoltage = (long)node.getResponseBuffer(0x00)/100.0f;
    if (debug == 1){
      Serial.println(pvvoltage);
      Serial.print("PV Current: ");
    }
    pvcurrent = (long)node.getResponseBuffer(0x01)/100.0f;
    if (debug == 1){
      Serial.println(pvcurrent);
      Serial.print("PV Power: ");
    }
    pvpower = ((long)node.getResponseBuffer(0x03)<<16|node.getResponseBuffer(0x02))/100.0f;
    if (debug == 1){
      Serial.println(pvpower);
      Serial.println("------------------------------------------------------------------");
      delay(500);
    }
  }else{
    rs485DataReceived = false;
  }

  timer.run();
}
