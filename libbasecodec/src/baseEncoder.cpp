#define LOG_TAG "baseEncoder"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <assert.h>

#include "baseEncoder.h"

#define FORCE_NOT_SHARE 0

BaseEncoder::BaseEncoder()
{
    POINTER_RESET(surfaceId);
    POINTER_RESET(observer);
    VALUE_RESET(vaDisplay);
    VALUE_RESET(contextId);
    VALUE_RESET(configId);
    VALUE_RESET_BOOL(bInitialized);
    VALUE_RESET_BOOL(bSendEndOfSequence);
    VALUE_RESET(iGOPCounter);
    VALUE_RESET(nTotalSurface);
    VALUE_RESET(nCiSurface);
    ARRAY_RESET(codedBufId);

   CODEC_SURFACE_ID_BASE = 0;
   NORMAL_SRC_SURFACE_ID_BASE = 2;
   CI_SRC_SURFACE_ID_BASE = 3 ;
};

BaseEncoder::~BaseEncoder() {
};


RtCode BaseEncoder::setConfig(CodecConfig config)
{
   if (bInitialized == true) 
   {
	LOGV("has already configed yet, the new setting will not take effect. please invoke reconfig if necessary");
	return INVALID_STATE;
   };

  LOG_EXEC_IF(config.frameWidth <= 0 ||
        	config.frameHeight <= 0 ||
		config.frameRate <= 0 ||
		config.frameBitrate <= 0 ||
		config.intraInterval < 0 ||
		config.initialQp < 0 ||
		config.initialQp > 52 ||
		config.minimalQp < 0 ||
		config.minimalQp > 52 ||
		config.rateControl < RC_NONE ||
		config.rateControl > RC_VBR_FSKIP ||
		config.naluFormat < START_CODE ||
		config.naluFormat > ZERO_BYTE_LENGTH_PREFIX ||
		config.nSlice > MAX_SLICE_PER_FRAME ||
		config.nSlice < MIN_SLICE_PER_FRAME,
		return INVALID_PARAMETER
		);

  LOG_EXEC_IF(config.bShareBufferMode == true &&
		(config.nShareBufferCount <= 0 ||
		 config.pShareBufferArray == NULL),
		return INVALID_PARAMETER
		);

  LOG_EXEC_IF(config.naluFormat != START_CODE,
		LOGV("naluFormat: only START_CODE is supported");return INVALID_PARAMETER
        	   );

  /*
  LOG_EXEC_IF(config.levelIDC != SH_LEVEL_30,	
		return INVALID_PARAMETER
	   );	//FIXME: only allow SH_LEVEL_31 (max at 720p)
  */
 
  codecConfig = config;
	
  //fire observers
  if(observer != NULL)
  {
      observer->handleConfigChange(false);
  }

  return SUCCESS;
}


RtCode BaseEncoder::setDynamicConfig(CodecConfig config)
{
     if (bInitialized == false)
     {
	LOGV("not initialized yet, cannot set dynamic config");
	return INVALID_STATE;
     };

     LOG_EXEC_IF(config.frameWidth != codecConfig.frameWidth,   return INVALID_PARAMETER);
     LOG_EXEC_IF(config.frameHeight != codecConfig.frameHeight,	return INVALID_PARAMETER);
     LOG_EXEC_IF(config.frameRate != codecConfig.frameRate,     return INVALID_PARAMETER);
     LOG_EXEC_IF(config.rateControl != codecConfig.rateControl, return INVALID_PARAMETER);
     LOG_EXEC_IF(config.naluFormat != codecConfig.naluFormat,	return INVALID_PARAMETER);
     LOG_EXEC_IF(config.levelIDC != codecConfig.levelIDC,	return INVALID_PARAMETER);
     LOG_EXEC_IF(config.bShareBufferMode != codecConfig.bShareBufferMode,  return INVALID_PARAMETER);
     LOG_EXEC_IF(config.nShareBufferCount != codecConfig.nShareBufferCount,return INVALID_PARAMETER);
     LOG_EXEC_IF(config.pShareBufferArray != codecConfig.pShareBufferArray,return INVALID_PARAMETER);
	
     if (config.pShareBufferArray != NULL && config.nShareBufferCount > 0)
     {
	int idx;
	for(idx = 0; idx < config.nShareBufferCount ; idx ++) 
        {
	    if (config.pShareBufferArray[idx] != codecConfig.pShareBufferArray[idx])
            {
		return INVALID_PARAMETER;
	    }
	}
     }

     LOGV("dynamic parameter selection passed");

     LOG_EXEC_IF(config.frameBitrate <= 0 ||
			config.intraInterval < 0 ||
		       	config.initialQp < 0 ||
			config.initialQp > 52 ||
			config.minimalQp < 0 ||
			config.minimalQp > 52 ||
			config.nSlice > MAX_SLICE_PER_FRAME ||
			config.nSlice < MIN_SLICE_PER_FRAME,
			return INVALID_PARAMETER
			);

     codecConfig = config;
	
     iGOPCounter = 0;

      //fire observers
     if (observer != NULL)
     {
	observer->handleConfigChange(true);
     }

     return SUCCESS;
}


RtCode BaseEncoder::getConfig(CodecConfig *config)
{
   *config = codecConfig;
    return SUCCESS;
};

RtCode BaseEncoder::reconfig(CodecConfig config)
{
    RtCode status;

    if (bInitialized == false)
    {
	LOGV("not initialized yet, please run setconfig and them init");
	return INVALID_STATE;
    }

    deinit();

    status = setConfig(config);
    LOG_EXEC_IF(status!=SUCCESS,
		init();LOGV("reconfig failed, restore original config");return INVALID_PARAMETER
		   );
    init();

    return SUCCESS;
}

void BaseEncoder::setKeyFrame()
{
   iGOPCounter = 0;
}

void BaseEncoder::setEncodeProfile()
{
    //set the encode profile for avc or mpeg4
}

RtCode BaseEncoder::init()
{
   VAStatus va_status;
   VAConfigAttrib attribute[2];
   VAEntrypoint *entry_points;
		
   int n_entry_points;
   int target_entry_point;		
   int va_major_ver;	
   int va_minor_ver;

   int display_port = 0;


   if (bInitialized == true)
   {
	LOGV("has already initialized. if an reconfig is expected, please invoke reconfig");
	return INVALID_STATE;
   };

   setEncodeProfile();  

   videoEncodeEntry = VAEntrypointEncSlice;

   vaDisplay = vaGetDisplay(&display_port);

   LOG_EXEC_IF(vaDisplay==NULL,	return NOT_FOUND);
	
   va_status = vaInitialize(vaDisplay, &va_major_ver, &va_minor_ver);

   LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return DEVICE_ERROR );
   LOGV("libva initialized: ver(%d.%d)", va_major_ver, va_minor_ver);

   n_entry_points = vaMaxNumEntrypoints(vaDisplay);

   LOG_EXEC_IF(n_entry_points==0,return NOT_FOUND);

   entry_points = new VAEntrypoint [n_entry_points];	//resource assigned

   vaQueryConfigEntrypoints(vaDisplay,videoEncodeProfile, entry_points, &n_entry_points);
   {
	int idx;
        for (idx = 0; idx < n_entry_points; idx ++) 
        {
	    if ( entry_points[idx] == videoEncodeEntry ) 
            {
		break;
	    }
	}

	LOG_EXEC_IF(idx == n_entry_points,delete []entry_points;return NOT_FOUND);	//resource unassigned	
	
    }

    //LOGV("find the avc baseline slice from libva");

   attribute[0].type = VAConfigAttribRTFormat;
   attribute[1].type = VAConfigAttribRateControl;

   vaGetConfigAttributes(vaDisplay, videoEncodeProfile, videoEncodeEntry, &attribute[0], 2);	
   LOG_EXEC_IF(attribute[0].value & VA_RT_FORMAT_YUV420 == 0,delete [] entry_points;return NOT_FOUND);	
   LOG_EXEC_IF(attribute[1].value & VA_RC_VBR == 0,   delete [] entry_points;return NOT_FOUND);	
   LOG_EXEC_IF(attribute[1].value & VA_RC_CBR == 0,   delete [] entry_points;return NOT_FOUND);
   LOG_EXEC_IF(attribute[1].value & VA_RC_NONE == 0,   delete [] entry_points;return NOT_FOUND);

   attribute[0].value = VA_RT_FORMAT_YUV420; 	// by default, only YUV420 is supported
	
   switch (codecConfig.rateControl) 
   {
	case RC_NONE:
		attribute[1].value = VA_RC_NONE;
		break;
	case RC_VBR:
	case RC_VBR_FSKIP:
		attribute[1].value = VA_RC_VBR;
		break;
	case RC_CBR:
	case RC_CBR_FSKIP:
		attribute[1].value = VA_RC_CBR;
		break;
	default:
		assert(0);
		break;
    }

    va_status = vaCreateConfig(vaDisplay, videoEncodeProfile, videoEncodeEntry, &attribute[0], 2,&configId);
    
    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,delete [] entry_points;return DEVICE_ERROR);
    LOGV("create the libva configuration");

    createEncodeResource();
    LOGV("create the libva encode resources");

    va_status = vaCreateContext(vaDisplay, configId,
                                codecConfig.frameWidth, codecConfig.roundFrameHeight,
                                VA_PROGRESSIVE, &surfaceId[0],
				nTotalSurface, &contextId);	//FIXME:width is not rounded

    LOGV("rounded frame size = %dx%d", codecConfig.roundFrameWidth, codecConfig.roundFrameHeight);
    LOGV("original frame size = %dx%d", codecConfig.frameWidth, codecConfig.frameHeight);


   LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
			LOGV("%s", vaErrorStr(va_status));delete [] entry_points;delete [] surfaceId;return DEVICE_ERROR
	   );

    LOGV("create the libva context");


  codecBufSize = codecConfig.roundFrameWidth * codecConfig.roundFrameHeight * 2;  //enough?

 	
  va_status = vaCreateBuffer(vaDisplay, contextId, 
			VAEncCodedBufferType,
			codecBufSize, MAX_CODED_BUF,
			NULL, &codedBufId[CODEC_BUFFER_ID_BASE]);

   LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,delete [] entry_points; return DEVICE_ERROR);
   LOGV("create the coded buffer");

    bInitialized = true;
    bSendEndOfSequence = false;

    initGOPCounter();
	
    initCodecInternalSurfaceId();

    initEncodeFrameInfo();

    inBuf = NULL;
    outBuf = NULL;

    delete [] entry_points;

    return SUCCESS;
}

RtCode BaseEncoder::createEncodeResource()
{
   ENTER_FUN;
   VAStatus va_status;
   nCiSurface = 0;
   nTotalSurface = CODEC_SURFACE_ID_COUNT;

   if (codecConfig.bShareBufferMode == true)
   {
	LOGV("ci buffer sharing is enabled, total share buffer count = %d", nTotalSurface);
	nCiSurface = codecConfig.nShareBufferCount;
	nTotalSurface += nCiSurface;
   }
   else
   {
	nCiSurface = 0;
   }

    surfaceId = new VASurfaceID [nTotalSurface];

    LOGV("create the internal codec surface pool: %d", CODEC_SURFACE_ID_COUNT);	
    va_status = vaCreateSurfaces(vaDisplay,codecConfig.frameWidth, codecConfig.frameHeight,
                                 VA_RT_FORMAT_YUV420, CODEC_SURFACE_ID_COUNT , &surfaceId[CODEC_SURFACE_ID_BASE]);

    if (codecConfig.bShareBufferMode == true) 
    {
	LOGV("append ci sharing buffer, nShareBufferCount =%d\n",codecConfig.nShareBufferCount);
	for (int idx = 0;idx<codecConfig.nShareBufferCount; idx ++)  
        {
		va_status = vaCreateSurfaceFromCIFrame(vaDisplay,
				codecConfig.pShareBufferArray[idx],
				&surfaceId[CI_SRC_SURFACE_ID_BASE + idx]
				);

		LOGV("ci sharing surface = %x | %x", surfaceId[CI_SRC_SURFACE_ID_BASE + idx],
				codecConfig.pShareBufferArray[idx]);
        }
     }

   for(int z = 0; z < nTotalSurface; z++)
   {
	LOGV("SURFACE %x", surfaceId[z]);
   }

   return SUCCESS;
}

RtCode BaseEncoder::destroyEncodeResource()
{
   VAStatus va_status;

   va_status = vaDestroySurfaces(vaDisplay,&surfaceId[CODEC_SURFACE_ID_BASE],CODEC_SURFACE_ID_COUNT);
   LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);
   LOGV("delete va surfaces (internal surfaces)");

   if (nCiSurface > 0)
   {
	va_status = vaDestroySurfaces(vaDisplay,&surfaceId[CI_SRC_SURFACE_ID_BASE],nCiSurface);

	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
				return UNDEFINED
	   	);
	LOGV("delete va surfaces (ci surfaces)");
   }

   va_status = vaDestroyBuffer(vaDisplay, codedBufId[CODEC_BUFFER_ID_BASE]);

    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
			LOGV("%s",vaErrorStr(va_status));return UNDEFINED
		   );
    LOGV("delete va coded buffer");

   return SUCCESS;

}

void BaseEncoder::initEncodeFrameInfo(void) 
{
   encodeFrameInfo.init(NORMAL_SRC_SURFACE_ID_BASE,0);
   encodeFrameInfo.bEncoded = false;
};

RtCode BaseEncoder::deinit(void) 
{
   VAStatus va_status;
   RtCode enc_status;
   int idx_src_surface_id = encodeFrameInfo.getSurfaceIdx();

   if (encodeFrameInfo.bEncoded == true) 
   {
	printf("flush pending picture\n");

	enc_status = checkGeneratedFrame();
	LOG_EXEC_IF(enc_status!=SUCCESS, return UNDEFINED);

		//fire observers
	if (observer != NULL ) 
        {
	     observer->handleEncoderCycleEnded(TO_SURFACE(idx_src_surface_id),
				bFrameGenerated,
				codecConfig.bShareBufferMode);
	}
   }

   enc_status = destroyEncodeResource();
   
   if(enc_status != SUCCESS)
   {
      return enc_status;
   } 
   
   va_status = vaDestroyContext(vaDisplay,contextId);
   LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
			LOGV("%s",vaErrorStr(va_status));return UNDEFINED
		   );
   LOGV("delete va context");

   va_status = vaDestroyConfig(vaDisplay,configId);
   LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
			return UNDEFINED
		   );
   LOGV("delete va config");

   va_status = vaTerminate(vaDisplay);
   LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
			return UNDEFINED
		   );
   LOGV("terminate va");

   delete [] surfaceId;

   bInitialized = false;

   LOGV("reclaim heap memory");

   return SUCCESS;
}


RtCode BaseEncoder::loadSourceSurface(bool force_not_share)
{
   ENTER_FUN;
   VAStatus va_status;
   VAImage source_image;
   RtCode enc_status;

   void* surface_buf;
	
  void *y_start, *y_src_start;
  void *u_start, *u_src_start;
  void *v_start, *v_src_start;
  
  int y_stride, y_width, y_height;
  int u_stride, u_width, u_height;
  int v_stride, v_width, v_height;

  LOG_EXEC_IF(inBuf->buf==NULL ||
		inBuf->len==0 ||
		inBuf->timestamp == INVALID_TS,
		return INVALID_PARAMETER
	   );

  if (codecConfig.bShareBufferMode == false)
  {
	//FIXME:
	//loadSourceSurface will always generate effective idxSrcSurface
	//for data copy mode, the idxSrcSurface will always be derived from currentSourceSurfaceInfo.idxSurface
	//for share buffer mode, the idxSrcSurface will be derived from input data and the currentSourceSurfaceInfo.idxSurface need to be updated accordingly

	
	encodeFrameInfo.usePrivateSurface();

	va_status = vaDeriveImage(vaDisplay, TO_SURFACE(encodeFrameInfo.getSurfaceIdx()), &source_image);
	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
				return DEVICE_ERROR
			);

	va_status = vaMapBuffer(vaDisplay, source_image.buf, &surface_buf);
	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
			vaDestroyImage(vaDisplay, source_image.image_id);return DEVICE_ERROR
		   );

	y_stride = source_image.pitches[0];	//only YUV420 is considered
	u_stride = source_image.pitches[1];
	v_stride = source_image.pitches[2];
	y_width = codecConfig.frameWidth;
	u_width = y_width / 2;
	v_width = y_width / 2;
	y_height = codecConfig.frameHeight;
	u_height = y_height / 2;
	v_height = y_height / 2;
	y_start = surface_buf;
	u_start = surface_buf + source_image.offsets[1];
	v_start = surface_buf + source_image.offsets[2];
		
	y_src_start = inBuf->buf;			//only consider NV12
	u_src_start = inBuf->buf + y_height * y_width;
	v_src_start = inBuf->buf + y_height * y_width + 1;
		
	{
             int idx;			//copy y for NV12
	     for(idx = 0;idx<y_height;idx++)
             {
		 memcpy(y_start+idx*y_stride, y_src_start+idx*y_width, y_width);
	     }
	}


	{
	   int idx;			//copy uv for NV12
	   for(idx = 0;idx<u_height;idx++) 
           {
		memcpy(u_start+idx*y_stride, u_src_start+idx*y_width, y_width);
	   }
	}

	va_status = vaUnmapBuffer(vaDisplay, source_image.buf);
	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
	vaDestroyImage(vaDisplay, source_image.image_id);return UNDEFINED
			   );
		
	va_status = vaDestroyImage(vaDisplay, source_image.image_id);
	
        LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);
   }
   else
   {
	LOG_EXEC_IF(inBuf->len<sizeof(unsigned long),return INVALID_PARAMETER);
	{
	    int idx;

            for(idx = 0; idx<nCiSurface; idx ++)
            {

                LOGV("CI frame id[%d] = %d\n", idx,  codecConfig.pShareBufferArray[idx]);

            }
	            
			 
	    for(idx = 0; idx<nCiSurface; idx ++)
            {
		if ((*((unsigned long*)(inBuf->buf)) == codecConfig.pShareBufferArray[idx]) )
                {
		    break;
		 }
	    }
		
            LOG_EXEC_IF(idx>=codecConfig.nShareBufferCount, return NOT_FOUND);

	    encodeFrameInfo.useShareSurface (CI_SRC_SURFACE_ID_BASE + idx);

	     LOGV("Incoming frame id = %x, surface id = %x (idx:%x)",
					codecConfig.pShareBufferArray[idx],
					surfaceId[encodeFrameInfo.getSurfaceIdx()],
					encodeFrameInfo.getSurfaceIdx());
	}

	if (force_not_share == true)
        {   
	
            /* not support currently */
            LOG_EXEC_IF(force_not_share == true,return UNDEFINED);

            //copy surface image forcefully
            /*
	    enc_status = encodeFrameInfo.forceSyncBackToPrivateSurface(*this);
	    LOG_EXEC_IF(enc_status != SUCCESS, return UNDEFINED);

            encodeFrameInfo.usePrivateSurface();
            */

	}
    }

    EXIT_FUN;
    return SUCCESS;
}

RtCode BaseEncoder::checkGeneratedFrame(void)
{
     VAStatus va_status;
	
     int idx_src_surface_id = encodeFrameInfo.getSurfaceIdx();

     bFrameGenerated = false;

     va_status = vaSyncSurface(vaDisplay, TO_SURFACE(idx_src_surface_id));
     LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);

     surfaceStatus = (VASurfaceStatus)0x0;
     va_status = vaQuerySurfaceStatus(vaDisplay, TO_SURFACE(idx_src_surface_id), &surfaceStatus);
     LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);

     bFrameGenerated = (surfaceStatus & VASurfaceSkipped) == 0;

     return SUCCESS;
}

void BaseEncoder::prepareEncodeFrameInfo(void)
{
    encodeFrameInfo.mediaInfo = *inBuf;
    encodeFrameInfo.bEncoded = false;

    encodeFrameInfo.mediaInfo.frameType = frameType;
}

void BaseEncoder::initCodecInternalSurfaceId(void)
{
	idxRefSurfaceId = CODEC_SURFACE_ID_BASE + 0;
	idxReconstructSurfaceId = CODEC_SURFACE_ID_BASE + 1;
}

void BaseEncoder::manageCodecInternalSurfaceId(void)
 {
	int temp_id;
	if (bFrameGenerated == true) {
		temp_id = idxRefSurfaceId;
		idxRefSurfaceId = idxReconstructSurfaceId;
		idxReconstructSurfaceId = temp_id;
	}
};

void BaseEncoder::initParamBufId(void)
{
	idxParamBufId = 0;

	idxSequenceParamBufId = INVALID_BUFFER_IDX;
	idxPictureParamBufId = INVALID_BUFFER_IDX;
	idxSliceArrayParamBufId = INVALID_BUFFER_IDX;
}

RtCode BaseEncoder::manageParamBufId(int& id_to_alloc) 
{
    id_to_alloc = idxParamBufId;
    idxParamBufId ++;

    LOGV("Current idxParamBufId =%x \n", idxParamBufId);
	
    if (idxParamBufId >= MAX_PARAM_BUF) 
    {
	return UNDEFINED;
    }

    return SUCCESS;
}

void BaseEncoder::initGOPCounter(void)
{
	iGOPCounter = 0;
}
bool BaseEncoder::manageGOPCounter(void)
{
        return true;//implement by derived class
}
		
RtCode BaseEncoder::encode(MediaBuffer* in,MediaBuffer* out)
{
   ENTER_FUN;
    RtCode enc_status;
    VAStatus va_status;

    LOG_EXEC_IF(bInitialized==false,return INVALID_STATE);

    LOG_TIMER_START(Encode);

    prepareMediaBufferOut(out);

    if (encodeFrameInfo.bEncoded == true)
    {
	printf("output pending picture\n");
	LOG_TIMER_START(SurfaceSync);

	enc_status = checkGeneratedFrame();

	LOG_TIMER_STOP(SurfaceSync);

	LOG_EXEC_IF(enc_status!=SUCCESS,return UNDEFINED);

	LOG_TIMER_START(CopyOutput);

	enc_status = generateOutput();

	LOG_TIMER_STOP(CopyOutput);

	LOG_EXEC_IF(enc_status!=SUCCESS,return UNDEFINED);

	manageCodecInternalSurfaceId();

	encodeFrameInfo.bEncoded = false;
    }
    else
    {
	_generateInvalidOutput();
    }
	
    prepareMediaBufferIn(in);

    LOG_TIMER_START(SurfaceLoad);

#if FORCE_NOT_SHARE==0
    enc_status = loadSourceSurface(false);	//prepare input surface
#else
    enc_status = loadSourceSurface(true);	//prepare input surface
#endif
	
    LOG_TIMER_STOP(SurfaceLoad);
	
    manageGOPCounter();
    initParamBufId();

    LOG_EXEC_IF(enc_status!=SUCCESS,LOGV("surface load failure");return UNDEFINED);
    prepareEncodeFrameInfo();

    enc_status = beginPicture();		//begin picture scope

    LOG_EXEC_IF(enc_status!=SUCCESS,LOGV("begin picture failure");return UNDEFINED);

  /*
   if (frameType == IDR_FRAME)
   {	
	//generate SPS & PPS
	enc_status = prepareSequenceParam();
	LOG_EXEC_IF(enc_status != SUCCESS,return UNDEFINED);
   }
 */

   enc_status = prepareSequenceParam();
   LOG_EXEC_IF(enc_status != SUCCESS,return UNDEFINED);

   enc_status = preparePictureParam();
   LOG_EXEC_IF(enc_status != SUCCESS,return UNDEFINED);

   enc_status = prepareSlice();
   LOG_EXEC_IF(enc_status != SUCCESS,return UNDEFINED);
	
   enc_status = launchParamArray();
   LOG_EXEC_IF(enc_status != SUCCESS,return UNDEFINED);

   enc_status = endPicture();
   LOG_EXEC_IF(va_status!=SUCCESS,LOGE("end picture failure");return UNDEFINED);

   LOG_TIMER_STOP(Encode);

   encodeFrameInfo.bEncoded = true;
   EXIT_FUN;

   return SUCCESS;
}

RtCode BaseEncoder::prepareSlice(void)
{
   ENTER_FUN;
   int slice_width = codecConfig.frameWidth;
   int slice_height = codecConfig.frameHeight / codecConfig.nSlice;
   int cur_slice_height;

   VAStatus va_status;
   VAEncSliceParameterBuffer *slice_array_buffer;
   RtCode enc_status;
	
   //calculate slice parameter
   slice_height = (slice_height & (~15));
   {
	int idx;

	for (idx = 0; idx < codecConfig.nSlice; idx ++) 
        {
	   if (idx < codecConfig.nSlice - 1)
           {	//standard sized slice
		cur_slice_height = slice_height;
	   }
           else
           {	//last slice (larger)
		cur_slice_height = MAX(slice_height, codecConfig.frameHeight - idx * slice_height);
	   }

	  cur_slice_height = ((cur_slice_height + 15) & (~15));

          sliceInfo[idx].slice_height = cur_slice_height / 16;
	  sliceInfo[idx].start_row_number = (idx * slice_height) / 16;
	  sliceInfo[idx].slice_flags.bits.is_intra = ((frameType == I_FRAME) || (frameType == IDR_FRAME));
	  sliceInfo[idx].slice_flags.bits.disable_deblocking_filter_idc = 2;	//hard code (enable deblock)					
	}
    }

    enc_status = manageParamBufId(idxSliceArrayParamBufId);
    LOG_EXEC_IF(enc_status!=SUCCESS,return enc_status);
	
    //prepare slice parameter buffer
    va_status = vaCreateBuffer(vaDisplay, contextId, 
			VAEncSliceParameterBufferType,
			sizeof(VAEncSliceParameterBuffer),
			codecConfig.nSlice,
			NULL,
			&(TO_PARAM_BUF(idxSliceArrayParamBufId))
			);
    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);

    va_status = vaMapBuffer(vaDisplay, TO_PARAM_BUF(idxSliceArrayParamBufId),
			(void**)&slice_array_buffer
			);
     LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);
     memcpy(slice_array_buffer, &(sliceInfo[0]), 
		sizeof(VAEncSliceParameterBuffer)*codecConfig.nSlice);

     va_status = vaUnmapBuffer(vaDisplay, TO_PARAM_BUF(idxSliceArrayParamBufId));
     LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);

     return SUCCESS;
}


RtCode BaseEncoder::launchSlice(void)
{
    VAStatus va_status;

    LOG_EXEC_IF(idxSliceArrayParamBufId==INVALID_BUFFER_IDX,return INVALID_STATE);

    va_status = vaRenderPicture(vaDisplay, contextId,&(TO_PARAM_BUF(idxSliceArrayParamBufId)), 1);

    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);
	
    return SUCCESS;
}


RtCode BaseEncoder::prepareSequenceParam(void) 
{

    return SUCCESS;

}

RtCode BaseEncoder::launchSequenceParam(void)
{
    VAStatus va_status;
	
    LOG_EXEC_IF(idxSequenceParamBufId==INVALID_BUFFER_IDX,
			return INVALID_STATE
		   );

    va_status = vaRenderPicture(vaDisplay,contextId, &(TO_PARAM_BUF(idxSequenceParamBufId)), 1);
		
    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);
    
    return SUCCESS;
}

RtCode BaseEncoder::preparePictureParam(void)
{

	return SUCCESS;  //implemented in derived class
}

RtCode BaseEncoder::launchPictureParam(void)
{
    VAStatus va_status;
	
    LOG_EXEC_IF(idxPictureParamBufId==INVALID_BUFFER_IDX,
			return INVALID_STATE
		   );

    va_status = vaRenderPicture(vaDisplay, contextId, &(TO_PARAM_BUF(idxPictureParamBufId)), 1);	

    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,
				LOGV("%s", vaErrorStr(va_status));return UNDEFINED
				);
    return SUCCESS;
}

RtCode BaseEncoder::launchParamArray(void)
{
    VAStatus va_status;
    int n_param_array_len = idxParamBufId;

    LOG_EXEC_IF(n_param_array_len>MAX_PARAM_BUF, return INVALID_STATE);

    va_status = vaRenderPicture(vaDisplay, contextId,
			&(paramBufId[0]), n_param_array_len);
    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);
	
    return SUCCESS;
}


RtCode BaseEncoder::beginPicture(void) 
{
    VAStatus va_status;
    int idx_src_surface_id = encodeFrameInfo.getSurfaceIdx();

    va_status = vaBeginPicture(vaDisplay, contextId, TO_SURFACE(idx_src_surface_id));

    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);

    return SUCCESS;
}

RtCode BaseEncoder::endPicture(void)
{
    VAStatus va_status;

    va_status = vaEndPicture(vaDisplay,contextId);
    LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);

    return SUCCESS;
}

RtCode BaseEncoder::generateOutput(void)
{
    ENTER_FUN;
	
    VAStatus va_status;
    int idx_coded_buf_id = encodeFrameInfo.getCodedBufIdx();
    int idx_src_surface_id = encodeFrameInfo.getSurfaceIdx();

    LOG_EXEC_IF(outBuf->buf==NULL, return INVALID_PARAMETER);

    if (bFrameGenerated == true)
    {

	
	VACodedBufferSegment *output_list = NULL;
	VACodedBufferSegment *_list = NULL;
	int ofst;

	va_status = vaMapBuffer(vaDisplay, TO_CODED_BUF(idx_coded_buf_id),
                              (void**)&output_list);

	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);

	ofst = 0;	//just check the output buffer length
	_list = output_list;

	while (_list != NULL) 
        {
	    ofst += _list->size;
	    _list = (VACodedBufferSegment*)_list->next;
	 }

	LOG_EXEC_IF(ofst > outBuf->len,	return UNDEFINED);

	ofst = 0;
	_list = output_list;

	while (_list != NULL) 
        {
	    memcpy(&(outBuf->buf[ofst]), output_list->buf, output_list->size);
	    ofst += _list->size;
	    _list = (VACodedBufferSegment*)_list->next;
	}
		
	va_status = vaUnmapBuffer(vaDisplay, TO_CODED_BUF(idx_coded_buf_id));
	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);

	outBuf->len = ofst; 

	//FIXME: frame information is derived from pending source surface info
	outBuf->frameType = encodeFrameInfo.mediaInfo.frameType;
	outBuf->timestamp = encodeFrameInfo.mediaInfo.timestamp;
	outBuf->bCompressed = true;

	//statistics
	codecStatistics.frameEncoded ++;
	codecStatistics.frameTotal ++;
		
	codecStatistics.recentFrameSize = outBuf->len;
	codecStatistics.totalFrameSize += outBuf->len;
	codecStatistics.numFrameSize ++;

	switch (outBuf->frameType) 
        {
	     case IDR_FRAME:
		codecStatistics.frameIDR ++;
		break;
	     case I_FRAME:
		codecStatistics.frameI ++;
		break;
	     case P_FRAME:
		codecStatistics.frameP ++;
		break;
	     case B_FRAME:
		codecStatistics.frameB ++;
		break;
	     default:
		break;
	 }
    } 
    else
    {
	_generateInvalidOutput();

	//statistics
	codecStatistics.frameSkipped ++;
	codecStatistics.frameTotal ++;
	codecStatistics.recentFrameSize = -1;
    }

    //fire observers
    if (observer != NULL) 
    {
	observer->handleEncoderCycleEnded(TO_SURFACE(idx_src_surface_id),
					bFrameGenerated,
					codecConfig.bShareBufferMode);
	codecStatistics.summarize();
	observer->handleEncoderStatisticsUpdated(codecStatistics);
     }

     return SUCCESS;
}

void BaseEncoder::prepareMediaBufferIn(MediaBuffer* in)
{
   inBuf = in;
}

void BaseEncoder::prepareMediaBufferOut(MediaBuffer* out)
{
   outBuf = out;
}

void BaseEncoder::setObserver(CodecInfoObserver *obs) 
{
    observer = obs;
}

void BaseEncoder::removeObserver(void)
{
    observer = NULL;
}

void BaseEncoder::resetStatistics(CodecStatistics* old_statistics)
{
    if (old_statistics != NULL) 
    {
	signed long long current_time = CodecStatistics::_getCurrentTimeInMS();
	codecStatistics.timeEclapsed = static_cast<float>(current_time - 
				codecStatistics.lastResetTime);
	if (codecStatistics.collectTimingInfo == true)
        {
	    codecStatistics.summarize();
	} 
        else if (codecStatistics.frameTotal != 0) 
        {
		codecStatistics.averEncode = codecStatistics.timeEclapsed / 
				codecStatistics.frameTotal;
	}

	*old_statistics = codecStatistics;
     }

     codecStatistics.reset();
     codecStatistics.lastResetTime = CodecStatistics::_getCurrentTimeInMS();
}

void BaseEncoder::enableTimingStatistics(void) 
{
    codecStatistics.enableTimingInfo();
}

void BaseEncoder::disableTimingStatistics(void) 
{
    codecStatistics.disableTimingInfo();
}

void BaseEncoder::_generateInvalidOutput(void)
{
    ENTER_FUN;
    outBuf->len = 0;	//frame is skipped
    outBuf->bCompressed = false;
    outBuf->frameType = INVALID_FRAME;
    outBuf->timestamp = INVALID_TS;
    EXIT_FUN;
}

