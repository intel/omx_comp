#ifndef __OMXIL_INTEL_AVC__
#define __OMXIL_INTEL_AVC__

class AvcCodecDataConstructor {
private:
	unsigned char *m_buffer;
	unsigned int m_length;
	bool is_done;

public:

	AvcCodecDataConstructor();
	~AvcCodecDataConstructor();

	bool add_buffer(unsigned char *buffer, unsigned int length);
	unsigned char *get_avc_codec_data(unsigned int &length);
};

struct NalBuffer {
	unsigned char *buffer;
	unsigned int offset;
	unsigned int length;
	void *appdata;
};

class AvcFrameNals {
private:

	NalBuffer nals[256];
	unsigned int nal_count;

public:
	AvcFrameNals();
	~AvcFrameNals();
	bool add_nal(unsigned char *buffer, unsigned int offset,
			unsigned int length, void *appdata);
	NalBuffer *get_nals(unsigned int &nal_count);

	void reset();
};

#endif /* __OMXIL_INTEL_AVC__ */
