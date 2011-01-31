#define LOG_NDEBUG 0
#define LOG_TAG "infodump"
#include <utils/Log.h>

#include <stdio.h>
#include <assert.h>
#include <sys/time.h>

static FILE* bufdump = NULL;
static FILE* buftrans = NULL;
static FILE* buftranslen = NULL;

void libinfodump_init(void) {
    bufdump = fopen("/data/app-private/bufferdump.txt", "wb+");
    assert(bufdump != NULL);

    buftrans = fopen("/data/app-private/buffertrans.txt", "wb+");
    assert(buftrans != NULL);

    buftranslen = fopen("/data/app-private/buffertranslen.txt", "w+");
    assert(buftranslen != NULL);
};

void libinfodump_destroy(void) {
    if (bufdump != NULL) {
	    fclose(bufdump);
	    bufdump = NULL;
    }

    if (buftrans != NULL) {
	    fclose(buftrans);
	    buftrans = NULL;
    }

    if (buftranslen != NULL) {
	    fclose(buftranslen);
	    buftranslen = NULL;
    }
};

void libinfodump_recordbuffer(int framecounter, unsigned char* buffer, int length) {
	int i,count;
	count = 0;
	char temp_buffer[512];

	assert(bufdump != NULL);
	assert(buftrans != NULL);
	assert(buftranslen != NULL);

	fwrite(buffer, sizeof(unsigned char), length, buftrans);
	//fflush(buftrans);
	fprintf(buftranslen, "%d\n", length);
	fflush(buftranslen);

	temp_buffer[0] = '\0';

	sprintf(temp_buffer, "frame counter: %d, %s(%d):", framecounter, __func__, length);
	fprintf(bufdump, "%s\n", temp_buffer);
	fflush(bufdump);
	temp_buffer[0] = '\0';

	for (i=0; i<length; i++) {
		count ++;
		sprintf(temp_buffer, "%s 0x%2x", temp_buffer, buffer[i]);
		if ((count%16) == 0) {
			fprintf(bufdump, "%s\n", temp_buffer);
			fflush(bufdump);
			temp_buffer[0] = '\0';
		};
	};
	if (strlen(temp_buffer) > 0) {
		fprintf(bufdump, "%s\n", temp_buffer);
		fflush(bufdump);
	};

        fprintf(bufdump, "\n");
	fflush(bufdump);
};

unsigned long libinfodump_gettimeofday_ms(void) {
	struct timeval tv;
	struct timezone tz;
	
	gettimeofday(&tv, &tz);

	return (tv.tv_sec * 1000000 + tv.tv_usec / 1000);
};
