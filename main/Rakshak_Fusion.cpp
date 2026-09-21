#include <MadgwickAHRS.h>
#include "Gyro_QMI8658.h"

#define DEG_TO_RAD 0.01745329251f
extern Madgwick filter;

float fusedYaw = 0;

void Fusion_Update(float mx, float my, float mz)
{
    getAccelerometer();
    getGyroscope();

    float ax = Accel.x;
    float ay = Accel.y;
    float az = Accel.z;

    float gx = Gyro.x * DEG_TO_RAD;
    float gy = Gyro.y * DEG_TO_RAD;
    float gz = Gyro.z * DEG_TO_RAD;

    filter.update(
        gx, gy, gz,
        ax, ay, az,
        mx, my, mz
    );

    fusedYaw = filter.getYaw();

    if (fusedYaw < 0)
        fusedYaw += 360;
}