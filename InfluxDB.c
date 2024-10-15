#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <curl/curl.h>

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
        snprintf(url, sizeof(url), "http://10.42.0.34:8086/api/v2/write?org=%s&bucket=%s&precision=ns", org, bucket);
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

    int temp = 10, hum = 50;

    int counter = 0;
    while (counter++ < 10000) {
        char lineProtocolData[1024];
        snprintf(lineProtocolData, sizeof(lineProtocolData), "\nweather temperature=%d,humidity=%d\n", temp % 100 + 10, hum % 100 + 50);
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
                printf("Data uploaded successfully! T:%d H:%d\n", temp, hum);
            }
        }

        temp++; hum++;

        // Wait a Second!
	      usleep(1000000);
    }

    if (handle) {
        curl_easy_cleanup(handle);
    }
    curl_global_cleanup();

    return 0;
}
