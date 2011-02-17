#define LOG_NDEBUG 0
#define LOG_TAG "avcencoder-test"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "AVCEncoder.h"

class AVCCodecInfoObserver : public CodecInfoObserver {
	public:
		AVCCodecInfoObserver() {};
		~AVCCodecInfoObserver() {};
	protected:
		void handleEncoderCycleEnded(VASurfaceID surface,
			bool b_generated,
			bool b_share_buffer) {
			printf("surface id =%x is released by encoder\n", surface);
		};
		void handleEncoderStatisticsUpdated(CodecStatistics c) {
			printf("average encoding time = %f (ms)\n", c.averEncode);
			printf("recent encoding time = %f (ms)\n", c.recentEncode);
		};
};

static unsigned char static_buffer1[1280*720*3/2];
static unsigned char static_buffer2[1280*720*3/2];

int main(int argc, char** argv)
{
	int width, height, nframe, ntrial;
	char filename[128];
	
	strcpy(filename, "/sdcard/");
	strcpy(filename+8, argv[1]);
	width = atoi(argv[2]);
	height = atoi(argv[3]);
	nframe = atoi(argv[4]);
	ntrial = atoi(argv[5]);

	printf("AVCEncoder-test ----------------\n");
	printf("video file = %s\n", filename);
	printf("video width = %d\n", width);
	printf("video height = %d\n", height);
	printf("video frames = %d\n", nframe);
	printf("encode trials = %d\n", ntrial);

	FILE* filein;
	FILE* fileout;

	filein = fopen(filename, "rb");
	assert(filein!=NULL);

	fileout = fopen("/sdcard/dump.h264", "wb");
	assert(fileout!=NULL);

	printf("input/output files are ready\n");

	AVCEncoder* encoder;
	CodecConfig config;
	RtCode status;
	AVCCodecInfoObserver obs;

	encoder = new AVCEncoder();
	status = encoder->getConfig(&config);
	assert(status==SUCCESS);

	config.frameWidth = width;
	config.frameHeight = height;
	config.frameRate = 30;
	config.frameBitrate = 8000000;
	config.nSlice = 1;

	status = encoder->setConfig(config);
	assert(status==SUCCESS);

	status = encoder->getConfig(&config);
	assert(status==SUCCESS);

	encoder->setObserver(&obs);
	encoder->enableTimingStatistics();

#define DUMP_VALUE(v) printf("%s = %d (0x%x)\n", #v, (unsigned int)(v),(unsigned int)(v))

	printf("dump configs\n");
	DUMP_VALUE(config.frameWidth);
	DUMP_VALUE(config.frameHeight);
	DUMP_VALUE(config.frameBitrate);
	DUMP_VALUE(config.intraInterval);
	DUMP_VALUE(config.iDRInterval);
	DUMP_VALUE(config.initialQp);
	DUMP_VALUE(config.minimalQp);
	DUMP_VALUE(config.rateControl);
	DUMP_VALUE(config.naluFormat);
	DUMP_VALUE(config.levelIDC);
	DUMP_VALUE(config.bShareBufferMode);
	DUMP_VALUE(config.nSlice);

	printf("AvcEncoder is created and configed\n");

	int trial;

	for (trial = 0;trial<ntrial;trial++) {

		printf("test (%d) START=================\n", trial);

		rewind(filein);

		status = encoder->init();

		assert(status==SUCCESS);

		int chunk = width * height * 3 / 2;
		int framecount = 0;

//		unsigned char* bufferin = (unsigned char*) malloc(chunk);
//		unsigned char* bufferout = (unsigned char*) malloc(chunk);

		unsigned char* bufferin = static_buffer1;
		unsigned char* bufferout = static_buffer2;

		MediaBuffer in;
		MediaBuffer out;
		
		CodecStatistics statistics;
		encoder->resetStatistics(NULL);

               
                printf("chunk is %d \n", chunk);

//		while( (fread(bufferin, 1, chunk, filein) != 0) && (framecount < nframe)) {
		while( framecount < nframe ) {
                        if (fread(bufferin, 1, chunk, filein) == 0) {
				rewind(filein);
				assert(read(bufferin, 1, chunk, filein)>0);
			};

			in.len = chunk;
			out.len = chunk;
			in.timestamp = 0;
			out.timestamp = 0;
			in.buf = bufferin;
			out.buf = bufferout;
			in.bCompressed = false;
			out.bCompressed = true;

			status = encoder->encode(&in, &out);
			printf("encoding frame %d , type %d, - len = %d\n", framecount++, out.frameType, out.len);

			if (out.len != 0) {
				fwrite(out.buf, 1, out.len, fileout);
			}
		};

		encoder->resetStatistics(&statistics);
		//----------------------------------------------
		printf("Codec Statistics ********************************************\n");
		printf("time eclapsed = %f (ms)\n", statistics.timeEclapsed);
		printf("#frames [encoded = %d|skipped = %d|total = %d]\n", statistics.frameEncoded,
				statistics.frameSkipped,
				statistics.frameTotal);
		printf("#I frames (%d IDR) = %d | #P frames = %d | #B frames %d\n", statistics.frameI + statistics.frameIDR,
				statistics.frameIDR,
				statistics.frameP,
				statistics.frameB);
		printf("average encoding time = %f (ms) - fps = %f\n", statistics.averEncode, 1000/statistics.averEncode);
		printf("average frame size = %f (Bytes) - bitrate = %f(Kbps)\n", statistics.averFrameSize, 
				statistics.averFrameSize * config.frameRate * 8 / 1024);
		printf("libva related details :\n");
		printf("average surface load time = %f (ms)\n", statistics.averSurfaceLoad);
		printf("average surface copy output time = %f (ms)\n", statistics.averCopyOutput);
		printf("average surface sync time = %f (ms)\n", statistics.averSurfaceSync);
		printf("average pure encoding time = %f (ms) - fps = %f\n", statistics.averEncode - 
				statistics.averSurfaceLoad,
				1000/(statistics.averEncode - statistics.averSurfaceLoad));
		//----------------------------------------------

//		free(bufferin);
//		free(bufferout);

		status = encoder->deinit();
		assert(status==SUCCESS);

		printf("test (%d) STOP=================\n", trial);
	}

	delete encoder;

	fclose(filein);
	fclose(fileout);
}
