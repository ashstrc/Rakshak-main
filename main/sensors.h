#pragma once
#include <Arduino.h>
#include <stdint.h>

// init + update
void Sensors_Init();
void Sensors_Update();

// magnetometer
void Sensors_GetMag(int16_t &x,int16_t &y,int16_t &z);
float Sensors_GetHeading();

// heart rate
float Sensors_GetHeartRate();
long Sensors_GetIR();
bool Sensors_FingerDetected();
String Sensors_GetHealthState();

// BMP180
float Sensors_GetTemp();
float Sensors_GetPressure();
float Sensors_GetAltitude();
int Sensors_GetFloor();
float Sensors_GetYaw();

//getAccelerometer();
//getGyroscope();

void Sensors_GetGyro(float &gx, float &gy, float &gz);