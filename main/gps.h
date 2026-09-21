#pragma once

void GPS_Init();
void GPS_Update();

double GPS_Lat();
double GPS_Lon();

int GPS_Sats();
bool GPS_Fix();
float GPS_HDOP();
float Sensors_GetHeading();