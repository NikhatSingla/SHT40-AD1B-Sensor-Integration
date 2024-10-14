#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>

char *file = "/dev/i2c-1";
uint8_t commands[] = {0xFD};
#define ADDR 0x44
#define MAX_LOOPS 1000
#define MAX_ITER 50

// Open file
int OpenFile(char* file) {
	int fd, i = 0;
	while (((fd = open(file, O_RDWR)) < 0) && i++ < MAX_ITER) {
		perror("File cannot be opened.");
		usleep(1000000);
	}
	return fd;
}

// Set I2C Slave Address
void Init (int fd, uint8_t ADDRESS) {
	int i = 0;
	while ((ioctl(fd, I2C_SLAVE, ADDRESS) < 0) && i++ < MAX_ITER) {
		perror("Unable to establish communication to Slave device.");
		usleep(1000000);
	}
}

// Write a byte
int Write(uint8_t *byte, int fd) {
	if (write(fd, byte, 1) != 1) {
		perror("Error writing the byte.");
		return -1;
	}
	
	return 1;
}

// Read from file
int Read(uint8_t* buffer, int length, int fd) {
	if (read(fd, buffer, length) != length) {
		perror("Error reading the information.");
		return -1;
	}
	
	return 1;
}

// Failure function
void failure(int *fd, char *file, uint8_t ADDRESS) {
	close(*fd);
	*fd = OpenFile(file);
	Init(*fd, ADDRESS);
}

// Temperature conversion and verification
void Temp(uint8_t *buff) {
	uint16_t raw = ((*buff << 8) | *(buff + 1));
	double raw_t = (double) raw;
	
	raw_t *= 175;
	raw_t /= 65535;
	raw_t -= 45;
	printf("Temperature: %.2f\n", raw_t);
}

// Humidity conversion and verification
void Hum(uint8_t *buff) {
	uint16_t raw = ((*buff << 8) | *(buff + 1));
	double raw_h = (double) raw;
	
	raw_h *= 125;
	raw_h /= 65535;
	raw_h -= 6;
	printf("Humidity: %.2f\n", raw_h);
}

int main() {
	int i = 0;
	int fd = OpenFile(file);
	Init(fd, ADDR);
	
	while (i++ < MAX_LOOPS) {
		if (Write(commands, fd) == -1) {
			failure(&fd, file, ADDR);
		} else {
			uint8_t buff[6];
			usleep(10000);
			
			if (Read(buff, 6, fd) == -1) {
				failure(&fd, file, ADDR);
			} else {
				Temp(buff);
				Hum(buff + 3);
				printf("\n");
			}
		}
		usleep(1000000);
	}
	
	return 0;
}
