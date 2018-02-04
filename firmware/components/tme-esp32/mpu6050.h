
#include <stdint.h>

typedef struct {
	int16_t accelx;
	int16_t accely;
	int16_t accelz;
} mpu6050_accel_tp;


int mpu6050_read_fifo(mpu6050_accel_tp *meas, int maxct);


void mpu6050_poll(int *x, int *y, int *z);
int mpu6050_init();
