#include "sensors.h"
#include <Wire.h>
#include <MAX30105.h>
#include <Adafruit_BMP085.h>
#include <math.h>
#include <MadgwickAHRS.h>
#include "Gyro_QMI8658.h"

#define TCA_ADDR 0x70
#define FLOOR_HEIGHT 3.2

Madgwick filter;

MAX30105 max301;
Adafruit_BMP085 bmp;

static float yaw = 0;
static float yawFiltered = 0;

/* ---- Gyro bias (drone trick) ---- */
static float gyroBiasX = 0;
static float gyroBiasY = 0;
static float gyroBiasZ = 0;

/* SENSOR STATE */

static long ir_val = 0;

static float temperature = 0;
static float pressure = 0;

static float heartRate = 0;

static float altitude = 0;
static float filteredAltitude = 0;
static float baseAltitude = 0;

static int currentFloor = 0;

static int fakeHR = 0;
static String healthState = "UNKNOWN";
static uint32_t healthTimer = 0;
static int irState = 0;

/* TIMERS */

static uint32_t lastFast = 0;
static uint32_t lastHR = 0;
static uint32_t lastBaro = 0;

static float basePressure = 0;
static float relAltitude = 0;


/* ---- Altitude fusion ---- */


static float baroAltitude = 0;
static float fusedAltitude = 0;

static float verticalVelocity = 0;
/* Kalman filter variables */
static float altEstimate = 0;
static float altError = 1;
static float altNoise = 0.5;

/* ---------- TCA ---------- */

void tcaSelect(uint8_t ch)
{
    Wire.beginTransmission(TCA_ADDR);
    Wire.write(1 << ch);
    Wire.endTransmission();
    delay(3);
}

void tcaDisable()
{
    Wire.beginTransmission(TCA_ADDR);
    Wire.write(0);
    Wire.endTransmission();
    delay(3);
}


/* ---------- INIT ---------- */

void Sensors_Init()
{
    /* MAX30102 */

    tcaSelect(1);

    max301.begin(Wire);
    max301.setup(0x1F,4,2,100,411,4096);

    max301.setPulseAmplitudeRed(0x1F);
    max301.setPulseAmplitudeIR(0x1F);
    max301.setPulseAmplitudeGreen(0);

    tcaDisable();


   /* BMP180 */

tcaSelect(2);

bmp.begin();

/* store ground pressure */
basePressure = bmp.readPressure();

tcaDisable();

tcaSelect(2);

float sum = 0;

for(int i=0;i<20;i++)
{
    sum += bmp.readPressure();
    delay(20);
}

basePressure = sum / 20;

tcaDisable();

    /* IMU */

    QMI8658_Init();
    filter.begin(100);
}


/* ---------- UPDATE ---------- */

void Sensors_Update()
{
    uint32_t now = millis();

    /* ---------- IMU LOOP ---------- */

    if(now - lastFast >= 5)
    {
        lastFast = now;

        getAccelerometer();
        /* ---- vertical acceleration fusion ---- */

float verticalAcc = Accel.z - 1.0;   // remove gravity

verticalVelocity += verticalAcc * 0.02;

fusedAltitude += verticalVelocity * 0.02;

/* blend barometer + imu */

fusedAltitude = fusedAltitude * 0.8 + filteredAltitude * 0.2;

filteredAltitude = fusedAltitude;
        getGyroscope();

        float ax = Accel.x;
        float ay = Accel.y;
        float az = Accel.z;

        float gx = Gyro.x;
        float gy = Gyro.y;
        float gz = Gyro.z;

        /* ---- DRONE GYRO DRIFT FIX (3 lines) ---- */

        if(fabs(gx) < 0.5 && fabs(gy) < 0.5 && fabs(gz) < 0.5)
        {
            gyroBiasX = gyroBiasX * 0.999 + gx * 0.001;
            gyroBiasY = gyroBiasY * 0.999 + gy * 0.001;
            gyroBiasZ = gyroBiasZ * 0.999 + gz * 0.001;
        }

        gx -= gyroBiasX;
        gy -= gyroBiasY;
        gz -= gyroBiasZ;

        /* ---- normalize accel ---- */

        float acc_norm = sqrt(ax*ax + ay*ay + az*az);

        if(acc_norm > 0)
        {
            ax /= acc_norm;
            ay /= acc_norm;
            az /= acc_norm;
        }

        filter.updateIMU(gx,gy,gz,ax,ay,az);

        yaw = filter.getYaw();

        if(yaw < 0)
            yaw += 360;

        /* ---- smoothing ---- */

        yawFiltered = yawFiltered * 0.9 + yaw * 0.1;
        yaw = yawFiltered;
    }


    /* ---------- HEART SENSOR ---------- */

    if(now - lastHR >= 200)
    {
        lastHR = now;

        tcaSelect(1);

        long ir = max301.getIR();
        ir_val = ir;

        int newState;

        if(ir < 5000) newState = 0;
        else if(ir < 15000) newState = 1;
        else newState = 2;

        if(newState != irState)
        {
            irState = newState;
            healthTimer = millis();
        }

        if(millis() - healthTimer > 3000)
        {
            if(irState == 0)
            {
                fakeHR = 0;
                healthState = "DEAD";
            }
            else if(irState == 1)
            {
                fakeHR = random(40,60);
                healthState = "INJURED";
            }
            else
            {
                fakeHR = 75 + random(-12,13);
                healthState = "HEALTHY";
            }
        }

        heartRate = fakeHR;

        tcaDisable();
    }


  /* ---------- BAROMETER ---------- */

if(now - lastBaro >= 200)
{
    lastBaro = now;

    tcaSelect(2);

    temperature = bmp.readTemperature() - 3.0;

    float pres = bmp.readPressure();
    pressure = pres / 100.0;

   /* relative altitude from pressure */

baroAltitude =
    44330.0 * (1.0 - pow(pres / basePressure, 0.1903));

/* zero-reference correction */

if(fabs(baroAltitude) < 0.8)
    baroAltitude = 0;

/* ---- Kalman filter ---- */

float K = altError / (altError + altNoise);
altEstimate = altEstimate + K * (baroAltitude - altEstimate);
altError = (1 - K) * altError;

filteredAltitude = altEstimate;

    tcaDisable();
    }
}


/* ---------- GETTERS ---------- */

float Sensors_GetYaw(){ return yaw; }

float Sensors_GetTemp(){ return temperature; }

float Sensors_GetPressure(){ return pressure; }

long Sensors_GetIR(){ return ir_val; }

float Sensors_GetHeartRate(){ return heartRate; }

float Sensors_GetAltitude(){ return filteredAltitude; }

int Sensors_GetFloor(){ return currentFloor; }

bool Sensors_FingerDetected(){ return ir_val > 20000; }

String Sensors_GetHealthState(){ return healthState; }

void Sensors_GetMag(int16_t &x,int16_t &y,int16_t &z)
{
    x = 0;
    y = 0;
    z = 0;
}

void Sensors_GetGyro(float &gx,float &gy,float &gz)
{
    gx = Gyro.x;
    gy = Gyro.y;
    gz = Gyro.z;
}