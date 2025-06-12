/**
 * @file linux/flash/flash.c
 * @author Ciprian Hegbeli
 * @date 15 Nov 2019
 * @brief Function declarations used for Flash transfer.
 */

#ifdef LINUX_APP

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#include "utils.h"

//Linux I2c specific defines
#define I2C_DEVICES_DIRECTORY 	"/sys/bus/i2c/devices"
#define I2C_DEFAULT_DEVICE 		"/dev/i2c-0"
//maximum flash transfer length
#define FLASH_MAX_LENGTH 		256

/**
 * Struct containing data specific to this flash instance.
 */
typedef struct _flash_env {
	uint16_t slave_addr; ///< Address of the Iic slave
	int fd; ///< File descriptor used for writing to the Iic bus
} FlashEnv;

/**
 * Determines the file path of the selected Iic controller.
 * Selection of the Iic controller is done based on the base
 * address.
 *
 * @param i2c_addr is the Iic address of slave flash device
 * @param i2c_path is the file path to the selected Iic controller
 *
 * @return XST_SUCCESS if successful
 */
int fnGetI2cDevicePathforAddr(uintptr_t i2c_addr, char *i2c_path) {
	printf("i2c_addr: %d\n", i2c_addr);
	printf("Entering function: fnGetI2cDevicePathforAddr\n");
	printf("[DEBUG] Opening directory: %s\n", I2C_DEVICES_DIRECTORY);
	DIR *directory = opendir(I2C_DEVICES_DIRECTORY);
	printf("Creating path with size: %d\n", MAX_PATH_SIZE);
	char path[MAX_PATH_SIZE];

	if (!directory) {
		perror("[ERROR] Failed to open directory");
		return -1;
	} else {
		printf("[DEBUG] Successfully opened directory: %s\n", I2C_DEVICES_DIRECTORY);
	}

	/*
	 * Determine the name of the device node that corresponds to the I2C
	 * controller that's attached to the I2C bus of the Platform MCU.
	 */
	struct dirent *directory_entry;
	while ((directory_entry = readdir(directory)) != NULL) {
		// Inside the while loop, after reading a directory entry
		printf("[DEBUG] Processing directory entry: %s\n", directory_entry->d_name);

		// Check if the current directory is an I2C one
		bool is_i2c = fnStartsWith(directory_entry->d_name, "i2c");
		printf("[DEBUG] Is I2C device: %d\n", is_i2c);

		if (!is_i2c) {
			printf("[DEBUG] Skipping non-I2C directory entry: %s\n", directory_entry->d_name);
			continue; 
		}

		// Construct full path of the I2C directory
		snprintf(path, MAX_PATH_SIZE, "%s/%s", I2C_DEVICES_DIRECTORY, directory_entry->d_name);
		printf("[DEBUG] Constructed full path: %s\n", path);

		// Check if the I2C address is the one we're looking for
		uint32_t addr = fnReadUint32(path, "of_node/reg");
		printf("[DEBUG] Read address: 0x%x, Expected address: 0x%lx\n", addr, i2c_addr);

		if (i2c_addr == addr) {
			printf("[DEBUG] Found matching I2C address: 0x%lx\n", i2c_addr);
			break;
		} else {
			printf("[DEBUG] Address mismatch, continuing search\n");
		}
	}

	closedir(directory);

	//populate the variable which will be returned
	if (directory_entry) {
		snprintf(i2c_path, MAX_PATH_SIZE, "/dev/%s", directory_entry->d_name);
		printf("[DEBUG] Returning I2C path: %s\n", i2c_path);
	} else {
		strncpy(i2c_path, I2C_DEFAULT_DEVICE, MAX_PATH_SIZE);
	}

	printf("[DEBUG] Closing directory\n");

	return 0;
}

/**
 * Initialize the Iic Flash .
 *
 * @param addr the base address of the Iic device
 * @param slave_addr the Iic address of the Zmod
 *
 * @return XST_SUCCESS if successful
 */
uint32_t fnInitFlash(uintptr_t addr, uint16_t slave_addr) {
	printf("Entering function: fnInitFlash\n");
	printf("[DEBUG] Initializing Flash: Addr: 0x%lx, Slave Addr: 0x%x\n", addr, slave_addr);
	char filepath[MAX_PATH_SIZE];
	int rc;
	int fd;

	// Find the file path
	rc = fnGetI2cDevicePathforAddr(addr, filepath);
	if (rc < 0) {
		return 0;
	}

	// Before malloc
	printf("[DEBUG] Attempting to allocate FlashEnv structure\n");

	// Allocate the flash environment
	FlashEnv *flash_env = (FlashEnv *)malloc(sizeof(FlashEnv));
	if (flash_env == NULL) {
		printf("[ERROR] Failed to allocate memory for FlashEnv\n");
		return 0;
	} else {
		printf("[DEBUG] Successfully allocated FlashEnv: %p\n", (void *)flash_env);
	}

	fd = open(filepath, O_RDWR);
	if (fd < 0) {
		return 0;
	}

	// Populate the flash environment
	flash_env->fd = fd;
	flash_env->slave_addr = slave_addr;

	printf("[DEBUG] Found file path: %s\n", filepath);
	printf("[DEBUG] Opened file descriptor: %d\n", fd);
	printf("[DEBUG] Returning FlashEnv: %p\n", (void *)flash_env);

	return (uint32_t)flash_env;
}

/**
 * Destroy a Flash device.
 *
 * @param addr the address of the Flash device
 */
void fnDestroyFlash(uintptr_t addr) {
	printf("Entering function: fnDestroyFlash\n");
	FlashEnv *flash_env = (FlashEnv *)addr;
	if (!flash_env)
		return;

	close(flash_env->fd);
	free(flash_env);

}

/**
 * Prepares the first bites to be transfered for both read
 * and write
 *
 * @param data pointer which will be transfered
 * @param data_addr the address of the device register
 */
void fnFormatAddr(uint8_t *data, uint16_t data_addr) {
	printf("Entering function: fnFormatAddr\n");
	data[0] = (data_addr >> 8);
	data[1] = (data_addr & 0xff);
}

/**
 * Reads a register of the Zmod flash
 *
 * @param addr the address of the Flash device
 * @param data_addr the address of the device register
 * @param read_vals data pointer which will be read
 * @param length is the transfer length of the packet
 *
 * @return XST_SUCCESS if successful
 */
int fnReadFlash(uintptr_t addr, uint16_t data_addr, uint8_t *read_vals, size_t length) {
	printf("Entering function: fnReadFlash\n");
	FlashEnv *flash_env = (FlashEnv *)addr;
	if (!flash_env)
		return -1;

	uint8_t txData[2];
	int rc;

	// Add the register address to stream
	fnFormatAddr(txData, data_addr);

	// Send the Zmod slave address to the driver
	rc = ioctl(flash_env->fd, I2C_SLAVE, flash_env->slave_addr);
	if (rc < 0) {
		printf("%s: ioctl slave set fail\r\n", __func__);
		return rc;
	}

	// Send the register address to the flash
	rc = write(flash_env->fd, txData, 2);
	if (rc < 0) {
		printf("%s: failed to write i2c register address\r\n", __func__);
		return rc;
	}

	// Read back the values
	rc = read(flash_env->fd, read_vals, length);
	if (rc < 0) {
		printf("%s: failed to read i2c register address\r\n", __func__);
		return rc;
	}

	return 0;
}

/**
 * Write a register of the Zmod flash
 *
 * @param addr the address of the Flash device
 * @param data_addr the address of the device register
 * @param write_vals data pointer which will be written
 * @param length is the transfer length of the packet
 *
 * @return XST_SUCCESS if successful
 */
int fnWriteFlash(uintptr_t addr, uint16_t data_addr, uint8_t *write_vals, size_t length) {
	printf("Entering function: fnWriteFlash\n");
	FlashEnv *flash_env = (FlashEnv *)addr;
	if (!flash_env)
		return -1;

	int rc;
	uint8_t txData[FLASH_MAX_LENGTH];

	//add the register address to stream
	fnFormatAddr(txData, data_addr);

	// Copy the data in to the send data structure
	memcpy(txData + 2, write_vals, length);

	//send the Zmod slave address to the driver
	rc = ioctl(flash_env->fd, I2C_SLAVE, flash_env->slave_addr);
	if (rc < 0) {
		printf("%s: ioctl slave set fail\r\n", __func__);
		return rc;
	}

	//send the data stream
	rc = write(flash_env->fd, txData, length + 2);
	if (rc < 0) {
		printf("%s: failed to write i2c register address\r\n", __func__);
		return rc;
	}

	return 0;
}
#endif // LINUX_APP
