#include <stdio.h>
#include "driver/i2c.h"

#define I2C_MASTER_SCL_IO    4    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    26   /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0   /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000     /*!< I2C master clock frequency */

#define WRITEW_BIT 1
#define MPU_ADDR (0x68<<1)

/*
esp_err_t i2c_master_read_slave(i2c_port_t i2c_num, uint8_t* data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, ( ESP_SLAVE_ADDR << 1 ) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}
*/

static esp_err_t setreg(int i2c_num, int reg, int val) {
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( MPU_ADDR | I2C_MASTER_WRITE), true);
	i2c_master_write_byte(cmd, reg, true);
	i2c_master_write_byte(cmd, val, true);
	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 100 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	return ret;
}

static int getreg(int i2c_num, int reg) {
	unsigned char byte;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( MPU_ADDR | I2C_MASTER_WRITE), true);
	i2c_master_write_byte(cmd, reg, true);
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( MPU_ADDR | I2C_MASTER_READ), true);
	i2c_master_read_byte(cmd, &byte, false);
	i2c_master_stop(cmd);
	esp_err_t ret=i2c_master_cmd_begin(i2c_num, cmd, 100 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);
	if (ret!=ESP_OK) return -1;
	return byte;
}


esp_err_t mpu6050_start(int i2c_num, int samp_div) {
	int i=getreg(i2c_num, 0x75);
	printf("Mpu: %x\n", i);
	return ESP_OK;
}


void mpu6050_poll(int *x, int *y, int *z) {

}


//Actually initializes the i2c port, but atm there's nothing else connected to it... should put this
//into some io init routine or something tho'.

void mpu6050_init() {
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, 1024, 1024, 0);

    mpu6050_start(I2C_MASTER_NUM, 10);
}

