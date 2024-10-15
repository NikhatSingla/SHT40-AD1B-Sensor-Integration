#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <curl/curl.h>

//-----------------
// FOR I2C PROTOCOL
//-----------------

char *file = "/dev/i2c-1";
uint8_t commands[] = {0xFD};
#define ADDR 0x44
#define MAX_LOOPS 10000
#define MAX_ITER 1000

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
double Temp(uint8_t *buff) {
	uint16_t raw = ((*buff << 8) | *(buff + 1));
	double raw_t = (double) raw;

	raw_t *= 175;
	raw_t /= 65535;
	raw_t -= 45;
	printf("Temperature: %.2f\n", raw_t);
	return raw_t;
}

// Humidity conversion and verification
double Hum(uint8_t *buff) {
	uint16_t raw = ((*buff << 8) | *(buff + 1));
	double raw_h = (double) raw;

	raw_h *= 125;
	raw_h /= 65535;
	raw_h -= 6;
	printf("Humidity: %.2f\n", raw_h);
	return raw_h;
}

//-----------------
// FOR HTTP REQUEST
//-----------------
struct upload_data {
    char *data;
    size_t size;
    size_t offset;
};

size_t read_callback (char *buffer, size_t size, size_t nitems, void *ptr) {
    size_t max_size = size * nitems;

    struct upload_data *my_data = (struct upload_data *)ptr;

    if (my_data->offset >= my_data->size) {
        return 0;
    }

    size_t nBytesToWrite = ((my_data->size - my_data->offset) < max_size) ? (my_data->size - my_data->offset) : max_size;
    memcpy(buffer, my_data->data + my_data->offset, nBytesToWrite);

    my_data->offset += nBytesToWrite;

    return nBytesToWrite;
}

int main(void) {
    // Database configuration
    char *org = "IIT%20Jammu";
    char *bucket = "TAH";
    char *token = "Buuv-UL56nH5K6-C1hnHTB8aQscDgx5fEfEQADkmJuYkjn9LI511Zb8DRs6vCZdqK2m2AJ0_E26qWiIpewHxTg==";

    curl_global_init(CURL_GLOBAL_DEFAULT); // Initialise Libcurl modules and sub-modules

    CURL *handle = curl_easy_init(); // To store handle of easy session

    if (!handle) {
        perror("Couldn't create handle for Easy Session.");
    } else {
        printf("Successfully created an Easy Session.\n");

        // Generate the URL
        char url[1024];
        snprintf(url, sizeof(url), "http://10.42.0.34:8086/api/v2/write?org=%s&bucket=%s&precision=s", org, bucket);
	    printf("URL: %s\n", url);

        // Set URL address
        curl_easy_setopt(handle, CURLOPT_URL, url);

        // Generate token string header
        char Token_Header[256];
        snprintf(Token_Header, sizeof(Token_Header), "Authorization: Token %s", token);

        // Create an SLIST and store Headers
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, Token_Header);
        headers = curl_slist_append(headers, "Content-Type: text/plain; charset=utf-8");
        headers = curl_slist_append(headers, "Accept: application/json");
        
        // Set Headers, Readfunction and enable POST and UPLOAD
        curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(handle, CURLOPT_UPLOAD, 1L);
	    curl_easy_setopt(handle, CURLOPT_POST, 1L);
        curl_easy_setopt(handle, CURLOPT_READFUNCTION, read_callback);
	
        // Clear Headers list
        // curl_slist_free_all(headers);
    }

	int fd = OpenFile(file); // Try to open file, until MAX_ITER
	Init(fd, ADDR); // Try to establish connection with slave, until MAX_ITER

    int counter = 0;
    while (counter++ < MAX_LOOPS) {
        if (Write(commands, fd) == -1) { // Try to send a byte command, if failure, re-open the file
			failure(&fd, file, ADDR);
		} else {
			uint8_t buff[6];
			usleep(10000);

			if (Read(buff, 6, fd) == -1) {
				failure(&fd, file, ADDR); // Try to read from I2C Slave, if falure, re-open the file
			} else {
				double temp = Temp(buff);
				double hum = Hum(buff + 3);

                //-----------------
                // HTTP POST Request
                //-----------------

                char lineProtocolData[1024];
                snprintf(lineProtocolData, sizeof(lineProtocolData), "\nweather temperature=%.2f,humidity=%.2f\n", temp, hum);
                printf("%s\n", lineProtocolData);

                struct upload_data myData = {
                    .data = lineProtocolData,
                    .size = strlen(lineProtocolData),
                    .offset = 0
                };

                if (handle) {
                    curl_easy_setopt(handle, CURLOPT_READDATA, &myData);
                    CURLcode res = curl_easy_perform(handle);

                    if (res != CURLE_OK) {
                        perror("Failed to send data.");
                        printf("%s\n", curl_easy_strerror(res));
                    } else {
                        printf("Data uploaded successfully! T:%.2f H:%.2f\n", temp, hum);
                    }
                }
            }
        }

        // Wait a second
        usleep(1000000);
    }

    if (handle) {
        curl_easy_cleanup(handle);
    }
    curl_global_cleanup();

    close(fd);

    return 0;
}
