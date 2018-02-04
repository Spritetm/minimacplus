#include <stdio.h>
#include "driver/i2c.h"
#include "mpu6050.h"

#define I2C_MASTER_SCL_IO    26    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    4   /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0   /*!< I2C port number for master dev */

#define MPU_ADDR 0x68

static esp_err_t setreg(int i2c_num, int reg, int val) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( (MPU_ADDR<<1) | I2C_MASTER_WRITE), true);
	i2c_master_write_byte(cmd, reg, true);
	i2c_master_write_byte(cmd, val, true);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 30 / portTICK_RATE_MS);
	if (ret!=ESP_OK) printf("MPU6050: NACK setting reg 0x%X to val 0x%X\n", reg, val);
	i2c_cmd_link_delete(cmd);
	return ret;
}

static int getreg(int i2c_num, int reg) {
	unsigned char byte;
	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( (MPU_ADDR<<1) | I2C_MASTER_WRITE), true);
	i2c_master_write_byte(cmd, reg, true);
	ret=i2c_master_cmd_begin(i2c_num, cmd, 100 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( (MPU_ADDR<<1) | I2C_MASTER_READ), true);
	i2c_master_read_byte(cmd, &byte, true);
	i2c_master_stop(cmd);
	ret=i2c_master_cmd_begin(i2c_num, cmd, 100 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if (ret!=ESP_OK) printf("MPU6050: NACK reading reg 0x%X\n", reg);
	if (ret!=ESP_OK) {
		return -1;
	}
	return byte;
}


esp_err_t mpu6050_start(int i2c_num, int samp_div) {
	printf("Initializing MPU6050...\n");
	setreg(i2c_num, 107, 0x80);		//Reset
	vTaskDelay(10);
	int i=getreg(i2c_num, 0x75);
	if (i!=0x68) {
		printf("No MPU6050 detected at i2c addr %x! (%d) No mouse available.\n", MPU_ADDR, i);
		return ESP_ERR_NOT_FOUND;
	}
	setreg(i2c_num, 107, 1);		//Take out of sleep, gyro as clk
	setreg(i2c_num, 108, 0);		//Un-standby everything
	setreg(i2c_num, 106, 0x0C);		//reset more stuff
	vTaskDelay(10);
	setreg(i2c_num, 106, 0x0);		//reset more stuff
	setreg(i2c_num, 25, samp_div);	//Sample divider
	setreg(i2c_num, 26, (7<<3)|0);	//fsync to accel_z[0], 260Hz bw (making Fsamp 8KHz)
	setreg(i2c_num, 27, 0);			//gyro def
	setreg(i2c_num, 28, 0);			//accel 2G
	setreg(i2c_num, 35, 0x08);		//fifo: emit accel data only
	setreg(i2c_num, 36, 0x00);		//no slave
	setreg(i2c_num, 106, 0x40);		//fifo: enable
	printf("MPU6050 found and initialized.\n");
	
	return ESP_OK;
}


int mpu6050_read_fifo(mpu6050_accel_tp *meas, int maxct) {
	int i2c_num=I2C_MASTER_NUM;
	int i;
	int no;
	uint8_t buf[6];
	i2c_cmd_handle_t cmd;
	esp_err_t ret;
	no=(getreg(i2c_num, 114)<<8);
	no|=getreg(i2c_num, 115);
	if (no>0x8000) {
		printf("Huh? Fifo has %x bytes.\n", no);
		return 0; //huh?
	}
//	printf("FIFO: %d\n", no);
	no=no/6; //bytes -> samples
	if (no>maxct) no=maxct;


	for (i=0; i<no; i++) {
		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, ( (MPU_ADDR<<1) | I2C_MASTER_WRITE), true);
		i2c_master_write_byte(cmd, 116, true);
		i2c_master_stop(cmd);
		ret=i2c_master_cmd_begin(i2c_num, cmd, 100 / portTICK_RATE_MS);
		i2c_cmd_link_delete(cmd);

		cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, ( (MPU_ADDR<<1) | I2C_MASTER_READ), true);
		i2c_master_read(cmd, buf, 5, 0);
		i2c_master_read(cmd, buf+5, 1, 1);
		i2c_master_stop(cmd);
		ret=i2c_master_cmd_begin(i2c_num, cmd, 100 / portTICK_RATE_MS);
		if (ret!=ESP_OK) {
			printf("Error reading packet %d/%d\n", i, no);
			return 0;
		}
		meas[i].accelx=(buf[0]<<8)|buf[1];
		meas[i].accely=(buf[2]<<8)|buf[3];
		meas[i].accelz=(buf[4]<<8)|buf[5];
		i2c_cmd_link_delete(cmd);
	}
	return no;
}

//Actually initializes the i2c port, but atm there's nothing else connected to it... should put this
//into some io init routine or something tho'.

int mpu6050_init() {
	i2c_config_t conf;
	conf.mode = I2C_MODE_MASTER;
	conf.scl_io_num = I2C_MASTER_SCL_IO;
	conf.sda_io_num = I2C_MASTER_SDA_IO;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
	conf.master.clk_speed = 400000;
	ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
	ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 1024, 1024, 0));

	//MPU has sample rate of 8KHz. We have an 1K fifo where we put 6byte samples --> about 128 samples.
	//We want to grab the data at about 10Hz -> sample rate of about 1KHz.
    esp_err_t r=mpu6050_start(I2C_MASTER_NUM, 8);
	if (r!=ESP_OK) {
		i2c_driver_delete(I2C_MASTER_NUM);
		return 0;
	}
	return 1;
}

