
#include "AVCEncoder.h"

void AVCEncoder::setEncodeProfile()
{
    videoEncodeProfile = VAProfileH264Baseline;
    videoEncodeEntry = VAEntrypointEncSlice;
}

RtCode AVCEncoder::prepareSequenceParam()
{
       ENTER_FUN;

	VAStatus va_status;
	RtCode enc_status;
	VAEncSequenceParameterBufferH264 sequence_param_buf;

        if(frameType != IDR_FRAME)
        {
        	 return SUCCESS;
        }		  

	enc_status = manageParamBufId(idxSequenceParamBufId);

	LOG_EXEC_IF(enc_status!=SUCCESS,return enc_status);

	memset((void*)&sequence_param_buf, 0x0, sizeof(VAEncSequenceParameterBufferH264));

	sequence_param_buf.level_idc = codecConfig.levelIDC;
        sequence_param_buf.picture_width_in_mbs = codecConfig.roundFrameWidth / 16;
        sequence_param_buf.picture_height_in_mbs = codecConfig.roundFrameHeight / 16;
        sequence_param_buf.bits_per_second = codecConfig.frameBitrate;
        sequence_param_buf.frame_rate = codecConfig.frameRate;
        sequence_param_buf.initial_qp = codecConfig.initialQp;
        sequence_param_buf.min_qp = codecConfig.minimalQp;
        sequence_param_buf.basic_unit_size = 0;

#if 1
	if (codecConfig.intraInterval > 0) 
        {
            sequence_param_buf.intra_period = codecConfig.intraInterval;
	}
        else
        {
            sequence_param_buf.intra_period = 0;	//<=0 are treated as 0
	}

	if (codecConfig.iDRInterval > 0) 
        {
	    sequence_param_buf.intra_idr_period = codecConfig.iDRInterval;
	} 
        else
        {
	    sequence_param_buf.intra_idr_period = 0;	//<=0 are treated as 0 (single SPS/PPS at the very beginning)
	}
#else
	sequence_param_buf.intra_period = codecConfig.intraInterval;
#endif

        va_status = vaCreateBuffer(vaDisplay, contextId,
			VAEncSequenceParameterBufferType,
                        sizeof(sequence_param_buf),1,
			&sequence_param_buf,&(TO_PARAM_BUF(idxSequenceParamBufId)));

	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);
  
	return SUCCESS;
}

RtCode AVCEncoder::preparePictureParam(void)
{
        ENTER_FUN;

	VAStatus va_status;
	RtCode enc_status;
	VAEncPictureParameterBufferH264 picture_param_buf;

	enc_status = manageParamBufId(idxPictureParamBufId);
	LOG_EXEC_IF(enc_status!=SUCCESS, return enc_status);

	memset((void*)&picture_param_buf, 0x0, sizeof(VAEncPictureParameterBufferH264));

	picture_param_buf.reference_picture = TO_SURFACE(idxRefSurfaceId);
        picture_param_buf.reconstructed_picture= TO_SURFACE(idxReconstructSurfaceId);
        picture_param_buf.coded_buf = TO_CODED_BUF(encodeFrameInfo.getCodedBufIdx());
        picture_param_buf.picture_width = codecConfig.frameWidth;
        picture_param_buf.picture_height = codecConfig.frameHeight;

	if (bSendEndOfSequence == true)
        {
            picture_param_buf.last_picture = 1;
	    bSendEndOfSequence = false;
	} 
        else
        {
	    picture_param_buf.last_picture = 0;
	}
        
        va_status = vaCreateBuffer(vaDisplay, contextId,VAEncPictureParameterBufferType,
                                   sizeof(picture_param_buf),1,&picture_param_buf,&(TO_PARAM_BUF(idxPictureParamBufId)));
	
        LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);
   
	return SUCCESS;
}

RtCode AVCEncoder::refreshIDR(void) 
{
	LOG_EXEC_IF(bInitialized==false,  return INVALID_STATE );
	iGOPCounter = 0;
	return SUCCESS;
}

 bool   AVCEncoder::manageGOPCounter(void) 
   {
	bool b_gop_start = false;

	if (iGOPCounter == 0) 
	{
		b_gop_start = true;
	}

	if (b_gop_start == true) 
	{
		frameType = IDR_FRAME;
	}
	else if ( (iGOPCounter % codecConfig.intraInterval) == 0) 
	{
		frameType = I_FRAME;
	}
	else 
	{
		frameType = P_FRAME;
	}

	iGOPCounter ++;
	
	if (iGOPCounter == codecConfig.iDRInterval) 
	{
		iGOPCounter = 0;
	};

	return b_gop_start;
}
   	