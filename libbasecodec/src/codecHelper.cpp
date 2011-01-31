#include "codecHelper.h"

/************** codec configure class implementation ************/
CodecConfig::CodecConfig(CodecConfig& c)
{
   frameWidth = c.frameWidth;
   frameHeight = c.frameHeight;
   frameRate = c.frameRate;
   frameBitrate = c.frameBitrate;
   intraInterval = c.intraInterval;
   iDRInterval = c.iDRInterval;
   initialQp = c.initialQp;
   minimalQp = c.minimalQp;
   rateControl = c.rateControl;
   naluFormat = c.naluFormat;
   levelIDC = c.levelIDC;
   nSlice = c.nSlice;
	
   if (c.nShareBufferCount != 0 && c.bShareBufferMode == true && c.pShareBufferArray != NULL)
   {
	bShareBufferMode = c.bShareBufferMode;
	nShareBufferCount = c.nShareBufferCount;
	memcpy(_pShareBufferArray, c.pShareBufferArray, c.nShareBufferCount * sizeof (VASurfaceID));
	pShareBufferArray = _pShareBufferArray;
   }
   else
   {
	nShareBufferCount = 0;
	bShareBufferMode = false;
	pShareBufferArray = NULL;
   }

   roundFrameWidth = ((frameWidth + 15) / 16) * 16;
   roundFrameHeight = ((frameHeight + 15) / 16) * 16;
}

CodecConfig& CodecConfig::operator=(const CodecConfig& c)
{
   if (this == &c)
   {	
      return *this;
   }	
   frameWidth = c.frameWidth;
   frameHeight = c.frameHeight;
   frameRate = c.frameRate;
   frameBitrate = c.frameBitrate;
   intraInterval = c.intraInterval;
   iDRInterval = c.iDRInterval;
   initialQp = c.initialQp;
   minimalQp = c.minimalQp;
   rateControl = c.rateControl;
   naluFormat = c.naluFormat;
   levelIDC = c.levelIDC;
   nSlice = c.nSlice;
	
   if (c.nShareBufferCount != 0 &&c.bShareBufferMode == true && c.pShareBufferArray != NULL)
   {
	bShareBufferMode = c.bShareBufferMode;
	nShareBufferCount = c.nShareBufferCount;
	memcpy(_pShareBufferArray, c.pShareBufferArray, c.nShareBufferCount * sizeof (VASurfaceID));
	pShareBufferArray = _pShareBufferArray;
   } 
   else
   {
	nShareBufferCount = 0;
	bShareBufferMode = false;
	pShareBufferArray = NULL;
   }
   
   roundFrameWidth = ((frameWidth + 15) / 16) * 16;
   roundFrameHeight = ((frameHeight + 15) / 16) * 16;

   return *this;
}

CodecConfig::~CodecConfig()
{
}

/************** the implementation of class CodecStatistics *********/
CodecStatistics::CodecStatistics(void)
{
   VALUE_RESET(collectTimingInfo);
	
   resetTimingInfo();
   resetCounterInfo();
};


CodecStatistics::CodecStatistics(CodecStatistics& c)
{
   frameSkipped = c.frameSkipped;
   frameEncoded = c.frameEncoded;
   frameTotal = frameTotal;
   collectTimingInfo = collectTimingInfo;

   frameI   = c.frameI;
   frameIDR = c.frameIDR;
   frameP   = c.frameP;
   frameB   = c.frameB;

   lastResetTime = c.lastResetTime;
   timeEclapsed  = c.timeEclapsed;
	
   COPY_AVERAGE_VARS(SurfaceLoad, c);
   COPY_AVERAGE_VARS(SurfaceSync, c);
   COPY_AVERAGE_VARS(CopyOutput, c);
   COPY_AVERAGE_VARS(Encode, c);
   COPY_AVERAGE_VARS(FrameSize, c);
};

CodecStatistics& CodecStatistics::operator=(const CodecStatistics& c) 
{
   if (this == &c)
   {
   	return *this;
   }

   CLASS_COPY(frameSkipped,c);
   CLASS_COPY(frameEncoded,c);
   CLASS_COPY(frameTotal,c);
   CLASS_COPY(collectTimingInfo,c);

   CLASS_COPY(frameI,c);
   CLASS_COPY(frameIDR,c);
   CLASS_COPY(frameP,c);
   CLASS_COPY(frameB,c);
		
   CLASS_COPY(lastResetTime,c);
   CLASS_COPY(timeEclapsed,c);

   COPY_AVERAGE_VARS(SurfaceLoad, c);
   COPY_AVERAGE_VARS(SurfaceSync, c);
   COPY_AVERAGE_VARS(CopyOutput, c);
   COPY_AVERAGE_VARS(Encode, c);
   COPY_AVERAGE_VARS(FrameSize, c);

   return *this;
};

void CodecStatistics::resetTimingInfo(void)
{
   RESET_AVERAGE_VARS(SurfaceLoad);
   RESET_AVERAGE_VARS(SurfaceSync);
   RESET_AVERAGE_VARS(CopyOutput);
   RESET_AVERAGE_VARS(Encode);
	
   VALUE_RESET(lastResetTime);
};

void CodecStatistics::resetCounterInfo(void)
{
   VALUE_RESET(frameSkipped);
   VALUE_RESET(frameEncoded);
   VALUE_RESET(frameTotal);

   VALUE_RESET(frameI);
   VALUE_RESET(frameIDR);
   VALUE_RESET(frameP);
   VALUE_RESET(frameB);

   RESET_AVERAGE_VARS(FrameSize);
};

void CodecStatistics::reset(void)
{
   resetTimingInfo();
   resetCounterInfo();
}

void CodecStatistics::summarize(void)
{
   GET_AVERAGE(FrameSize);
   GET_AVERAGE(SurfaceLoad);
   GET_AVERAGE(SurfaceSync);
   GET_AVERAGE(CopyOutput);
   GET_AVERAGE(Encode);
}


void CodecStatistics::enableTimingInfo(void)
{
   collectTimingInfo = true;
   resetTimingInfo();

}

void CodecStatistics::disableTimingInfo(void)
{
   collectTimingInfo = false;
}

TS64 CodecStatistics::_getCurrentTimeInMS(void)
{
   struct timeval tv;
   struct timezone tz;
	
   gettimeofday(&tv, &tz);
   return tv.tv_sec * 1000 + tv.tv_usec / 1000;
};


TS64 CodecStatistics::getCurrentTimeInMS(void)
{
   if (collectTimingInfo == true)
   {
	return _getCurrentTimeInMS();   
   }
   else
   {
	return 0;
   }
}

/****************** implementation of class MediaBuffer *****************/
MediaBuffer::MediaBuffer(unsigned char *a_buf,int a_len, TS64 a_timestamp, bool a_compressed)
{
    buf = a_buf;
    len = a_len;
    timestamp = a_timestamp;
    bCompressed = a_compressed;
    frameType = INVALID_FRAME;
}

MediaBuffer::~MediaBuffer()
{
}

MediaBuffer::MediaBuffer (MediaBuffer& b)
{
    len = b.len;
    timestamp = b.timestamp;
    frameType = b.frameType;
    bCompressed = b.bCompressed;
    buf = b.buf;
}

MediaBuffer& MediaBuffer::operator=(const MediaBuffer& b)
{
    if (this == &b)
    {
	return *this;
    };
	
    len = b.len;
    buf = b.buf;
    timestamp = b.timestamp;
    frameType = b.frameType;
    bCompressed = b.bCompressed;

    return *this;
}


/********* the implementation of class CodecFrameInfo **************/
CodecFrameInfo& CodecFrameInfo::operator=(const CodecFrameInfo& s)
{
   if (this == &s)
   {
	return *this;
   }
	
   bEncoded = s.bEncoded;
   idxSurface = s.idxSurface;
   idxCodedBuf = s.idxCodedBuf;
   idxPrivateSurface = s.idxPrivateSurface;
   mediaInfo = s.mediaInfo;

   return *this;
}

void CodecFrameInfo::init(int idx_private_surface,int idx_coded_buf) 
{
     idxCodedBuf = idx_coded_buf;
     idxPrivateSurface = idx_private_surface;
     bEncoded = false;
     usePrivateSurface();
};

void CodecFrameInfo::useShareSurface(int idx_share_surface)
{
	idxSurface = idx_share_surface;
}

void CodecFrameInfo::usePrivateSurface(void)
{
	idxSurface = idxPrivateSurface;
}
/*
RtCode CodecFrameInfo::forceSyncBackToPrivateSurface(const AvcEncoder& enc)
{
}*/



