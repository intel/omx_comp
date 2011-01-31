#ifndef __CODECHELPER_H__
#define __CODECHELPER_H__
//#define LOG_NDEBUG 0

#include <va/va.h>
#include <va/va_tpi.h>
#include <va/va_android.h>

/********* Android Log ************/
#include <utils/Log.h>

#define ENTER_EXIT_KEY 1

#if ENTER_EXIT_KEY
#define ENTER_FUN    LOGE("%s(): enter ", __func__);
#define EXIT_FUN     LOGV("  exit\n");
#define TRA          LOGV("%s:%s:%d", __FILE__, __func__, __LINE__)
#else
#define ENTER_FUN   
#define EXIT_FUN  
#endif

#define LOG_EXEC_IF(cond, statement) {\
	if ((cond)) {\
		LOGE("<[%s:%d] %s> - OCCURS", __func__, __LINE__,  #cond);\
		statement;\
	}\
}

/********* Math Macro  *******/
static inline int MAX(int a, int b) { return (a>b)?a:b; };
static inline int MIN(int a, int b) { return (a<b)?a:b; };

#define POINTER_RESET(p) (p)=NULL
#define ARRAY_RESET(p) memset((p), sizeof((p)) * sizeof(p[0]), 0)
#define VALUE_RESET(p) p=0x0
#define VALUE_RESET_NEG(p) p=-1
#define VALUE_RESET_BOOL(p) p=false

#define CLASS_COPY(var, cls) var = cls.var
#define COPY_AVERAGE_VARS(val, c) {\
	CLASS_COPY(num##val,c);\
	CLASS_COPY(aver##val, c);\
	CLASS_COPY(total##val, c);\
	CLASS_COPY(recent##val, c);\
}

#define RESET_AVERAGE_VARS(val) {\
	VALUE_RESET(num##val);\
	VALUE_RESET(aver##val);\
	VALUE_RESET(total##val);\
	VALUE_RESET(recent##val);\
}

/*******************************/
#define INVALID_SURFACE_IDX -1
#define INVALID_BUFFER_IDX -1
#define INVALID_TS  0xFFFFFFFFFFFFFFFF
#define CODEC_SURFACE_ID_COUNT CI_SRC_SURFACE_ID_BASE
#define TO_SURFACE(x) surfaceId[x]
#define TO_CODED_BUF(x) codedBufId[x]
#define TO_PARAM_BUF(x) paramBufId[x]

/*******************************/
#define LOG_TIMER_START(timer) {\
	signed long long start_time_##timer;\
	signed long long end_time_##timer;\
	signed long long eclapsed_time_##timer;\
	start_time_##timer = codecStatistics.getCurrentTimeInMS();

#define LOG_TIMER_STOP(timer) \
	end_time_##timer = codecStatistics.getCurrentTimeInMS();\
	if (codecStatistics.collectTimingInfo == true) {\
		eclapsed_time_##timer = end_time_##timer - start_time_##timer;\
		codecStatistics.total##timer += eclapsed_time_##timer;\
		codecStatistics.num##timer ++;\
		codecStatistics.recent##timer = static_cast<float>(eclapsed_time_##timer);\
	}\
}

#define GET_AVERAGE(timer) {\
	if (num##timer > 0) {\
		aver##timer = static_cast<float>(total##timer)/num##timer;\
	} else {\
		aver##timer = -1;\
	}\
}


typedef signed long long TS64;


typedef enum _RtCode {
	SUCCESS,
	INVALID_PARAMETER,
	INVALID_STATE,
	NOT_FOUND,
	DEVICE_ERROR,
	UNDEFINED
} RtCode;

typedef enum _RcMode {
	RC_NONE,
	RC_CBR,
	RC_VBR,
	RC_CBR_FSKIP,
	RC_VBR_FSKIP
} RcMode;

typedef enum _NaluFormat {
	START_CODE,
	FOUR_BYTE_LENGTH_PREFIX,
	ZERO_BYTE_LENGTH_PREFIX
} NaluFormat;

typedef enum _LevelIDC {
	SH_LEVEL_1 = 10,
	SH_LEVEL_1B = 111,
	SH_LEVEL_11 = 11,
	SH_LEVEL_12 = 12,
	SH_LEVEL_2 = 20,
	SH_LEVEL_3 = 30,
	SH_LEVEL_31 = 31,
	SH_LEVEL_32 = 32,
	SH_LEVEL_4 = 40,
	SH_LEVEL_5 = 50
} LevelIDC;

typedef enum _FrameType {
	I_FRAME,
	P_FRAME,
	IDR_FRAME,
	B_FRAME,	//not supported
	INVALID_FRAME
} FrameType;

class MediaBuffer
{
 public:
  MediaBuffer(unsigned char *a_buf = NULL,
		int a_len = 0,
		TS64 a_timestamp = INVALID_TS,
		bool a_compressed = false);
  ~MediaBuffer();
  MediaBuffer (MediaBuffer& b);
  MediaBuffer& operator=(const MediaBuffer& b);
			
 public:
  unsigned char* buf;
  int len;
  TS64 timestamp;
  FrameType frameType;
  bool bCompressed;
};

class CodecFrameInfo
{
public:
  CodecFrameInfo(void): bEncoded(false),
		idxCodedBuf(INVALID_BUFFER_IDX),
		idxPrivateSurface(INVALID_SURFACE_IDX),
		idxSurface(INVALID_SURFACE_IDX)
		{
		};

  CodecFrameInfo(CodecFrameInfo& s):
		bEncoded(s.bEncoded),
		idxSurface(s.idxSurface),
		idxCodedBuf(s.idxCodedBuf),
		idxPrivateSurface(s.idxPrivateSurface)
		{
		};
 CodecFrameInfo& operator=(const CodecFrameInfo& s);
 ~CodecFrameInfo() {};

public:
   void init(int idx_private_surface,int idx_coded_buf);
   int getCodedBufIdx(void) {return idxCodedBuf;};
   int getSurfaceIdx(void) {return idxSurface;};
   void useShareSurface(int idx_share_surface);
   void usePrivateSurface(void);
  // RtCode forceSyncBackToPrivateSurface(const AvcEncoder& enc);
		
public:
   bool bEncoded;
   MediaBuffer mediaInfo;
		
private:
   int  idxPrivateSurface;
   int  idxCodedBuf;
   int  idxSurface;
};

class CodecConfig
{
public:
  CodecConfig():
	frameWidth(352),
	frameHeight(288),
	frameRate(20),
	frameBitrate(8000000),
	intraInterval(30),
	iDRInterval(30),
	initialQp(15),
	minimalQp(0),
	rateControl(RC_VBR),
	naluFormat(START_CODE),
	levelIDC(SH_LEVEL_3),
	bShareBufferMode(false),
	nSlice(1),
	nShareBufferCount(0),
	pShareBufferArray(NULL)
      {
	   roundFrameWidth = frameWidth;
	   roundFrameHeight = ((frameHeight + 15) / 16) * 16;
      };

	CodecConfig (CodecConfig& c);
	~CodecConfig();
				
	CodecConfig& operator=(const CodecConfig& c);

	//friend class AvcEncoder;

public:
	int frameWidth;
	int frameHeight;
	int frameRate;
	int intraInterval;
	int iDRInterval;
	unsigned int frameBitrate;
	int initialQp;
	int minimalQp;
	int nSlice;
	RcMode rateControl;
	NaluFormat naluFormat;
	LevelIDC levelIDC;
	bool bShareBufferMode;
	int nShareBufferCount;
	unsigned long *pShareBufferArray;

	int roundFrameWidth;
	int roundFrameHeight;
	unsigned long _pShareBufferArray[64];	//should be enough?
};

class CodecStatistics
{
public:
 CodecStatistics();
 CodecStatistics(CodecStatistics& c);
 ~CodecStatistics() {};

  CodecStatistics& operator=(const CodecStatistics& c);

   void enableTimingInfo(void);
   void disableTimingInfo(void);
   void summarize(void);
   void reset(void);

   void resetTimingInfo(void);
   void resetCounterInfo(void);
   TS64 getCurrentTimeInMS(void);
   static TS64 _getCurrentTimeInMS(void);

public:
   int frameSkipped;
   int frameEncoded;
   int frameTotal;
			
   int frameI;
   int frameIDR;
   int frameP;
   int frameB;

   float averSurfaceLoad;
   float averSurfaceSync;
   float averCopyOutput;
   float averEncode;
   float averFrameSize;

   float recentSurfaceLoad;
   float recentSurfaceSync;
   float recentCopyOutput;
   float recentEncode;

   float timeEclapsed;



    bool collectTimingInfo;
			
    signed long long totalSurfaceLoad;
    signed long long numSurfaceLoad;

    signed long long totalSurfaceSync;
    signed long long numSurfaceSync;

    signed long long totalCopyOutput;
    signed long long numCopyOutput;

    signed long long totalEncode;
    signed long long numEncode;

    int totalFrameSize;
    int recentFrameSize;
    int numFrameSize;

    signed long long lastResetTime;
    //friend class AvcEncoder;
};

class CodecInfoObserver
{
public:
  CodecInfoObserver():context(NULL){ };

  virtual ~CodecInfoObserver() {};
			
  void setContext(void *a_context) {context = a_context; };

public:
   virtual void handleEncoderCycleEnded(VASurfaceID surface,bool b_generated,bool b_share_buffer) {};
   virtual void handleEncoderError(void) {};
   virtual void handleEncoderStatisticsUpdated(CodecStatistics statistics) {};
   virtual void handleConfigChange(bool b_dynamic) {};
   virtual void handleEncoderStateChanged(bool b_initialized) {};
   //friend class AvcEncoder;

private:
   void* context;
};

#endif
