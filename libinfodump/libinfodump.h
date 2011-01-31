#ifndef __LIBINFODUMP_H__
#define __LIBINFODUMP_H__

#include <sys/time.h>

#ifdef __cplusplus
extern "C"
{
#endif

void libinfodump_init(void);

void libinfodump_destroy(void);

void libinfodump_recordbuffer(int framecounter, unsigned char* buffer, int length);

unsigned long libinfodump_gettimeofday_ms(void);

#define libinfodump_timingstart(timing_name) \
{\
	unsigned long start_##timing_name##_inms;\
	unsigned long stop_##timing_name##_inms;\
	unsigned long delta_##timing_name##_inms;\
	start_##timing_name##_inms = libinfodump_gettimeofday_ms();

#define libinfodump_timingstop(timing_name)\
	stop_##timing_name##_inms = libinfodump_gettimeofday_ms();\
	delta_##timing_name##_inms = stop_##timing_name##_inms - start_##timing_name##_inms;\
	LOGD("timing for %s = %d (ms)", #timing_name, delta_##timing_name##_inms);\
}

#ifdef __cplusplus
}
#endif

#endif
