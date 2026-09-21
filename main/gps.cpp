#include "gps.h"
#include <TinyGPS++.h>

#define GPS_RX_PIN 12
#define GPS_TX_PIN 13

TinyGPSPlus gps;
HardwareSerial GPSSerial(1);

void GPS_Init()
{
  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
}

void GPS_Update()
{
  while (GPSSerial.available())
  {
    gps.encode(GPSSerial.read());
  }
}

double GPS_Lat()
{
  return gps.location.lat();
}

double GPS_Lon()
{
  return gps.location.lng();
}

int GPS_Sats()
{
  return gps.satellites.value();
}

bool GPS_Fix()
{
  return gps.location.isValid();
}

float GPS_HDOP()
{
  return gps.hdop.hdop();
}