#define LOG_NDEBUG 0
#define LOG_TAG "infodump"
#include <utils/Log.h>

#include <stdio.h>
#include <assert.h>
#include <sys/time.h>

static FILE* bufdump = NULL;
static FILE* buftrans = NULL;
static FILE* buftranslen = NULL;

static char bufferdump_name_base[] = "/data/app-private/bufferdump";
static char buffertrans_name_base[] = "/data/app-private/buffertrans";
static char buffertranslen_name_base[] = "/data/app-private/buffertranslen";

static char bufferdump_name[1024];
static char buffertrans_name[1024];
static char buffertranslen_name[1024];


static void format_file_name(void) {
	struct timeval tv;
	struct timezone tz;

	gettimeofday(&tv, &tz);
	sprintf(bufferdump_name, "%s-%ld.dat", bufferdump_name_base, tv.tv_sec);
	sprintf(buffertrans_name, "%s-%ld.dat", buffertrans_name_base, tv.tv_sec);
	sprintf(buffertranslen_name, "%s-%ld.dat", buffertranslen_name_base, tv.tv_sec);
};

void libinfodump_init(void) {
    format_file_name();

    bufdump = fopen(bufferdump_name, "w");
    assert(bufdump != NULL);

    buftrans = fopen(buffertrans_name, "wb");
    assert(buftrans != NULL);

    buftranslen = fopen(buffertranslen_name, "w");
    assert(buftranslen != NULL);

    LOGV("Create record file 1) - %s", bufferdump_name);
    LOGV("Create record file 2) - %s", buffertrans_name);
    LOGV("Create record file 3) - %s", buffertranslen_name);
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
	int ret;

	assert(bufdump != NULL);
	assert(buftrans != NULL);
	assert(buftranslen != NULL);

        LOGV("%s: %d, %p, %d", __func__, framecounter, buffer, length);

	ret = fwrite(buffer, sizeof(unsigned char), length, buftrans);
	LOGV("%s: write in %d bytes", __func__, ret);
	fflush(buftrans);

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
