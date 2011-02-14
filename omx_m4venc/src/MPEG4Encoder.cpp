
#include "MPEG4Encoder.h"

void   MPEG4Encoder::setEncodeProfile()
{
    ENTER_FUN;
    videoEncodeProfile = VAProfileMPEG4Simple;
    videoEncodeEntry = VAEntrypointEncSlice;
    
}

RtCode MPEG4Encoder::prepareSequenceParam()
{
   ENTER_FUN;

	VAStatus va_status;
	RtCode enc_status;
	VAEncSequenceParameterBufferMPEG4 sequence_param_buf;

   if(!bResetSequence)
        {
       return SUCCESS;
        }

   enc_status = manageParamBufId(idxSequenceParamBufId);

	LOG_EXEC_IF(enc_status!=SUCCESS,return enc_status);

	memset((void*)&sequence_param_buf, 0x0, sizeof(VAEncSequenceParameterBufferMPEG4));

        sequence_param_buf.profile_and_level_indication = 0x3;//to be determined.
        sequence_param_buf.video_object_layer_width = codecConfig.frameWidth;
        sequence_param_buf.video_object_layer_height = codecConfig.frameHeight;
        sequence_param_buf.vop_time_increment_resolution = codecConfig.frameRate;
        sequence_param_buf.fixed_vop_rate = 0;
        sequence_param_buf.fixed_vop_time_increment = 3;
        sequence_param_buf.bits_per_second = codecConfig.frameBitrate;
        sequence_param_buf.frame_rate = codecConfig.frameRate;
        sequence_param_buf.initial_qp = codecConfig.initialQp;
        sequence_param_buf.min_qp =  codecConfig.minimalQp;      


	if (codecConfig.intraInterval > 0) 
        {
            sequence_param_buf.intra_period = codecConfig.intraInterval;
	}
        else
        {
             sequence_param_buf.intra_period = 0;	//<=0 are treated as 0
	}
	

        va_status = vaCreateBuffer(vaDisplay, contextId,
			VAEncSequenceParameterBufferType,
                        sizeof(sequence_param_buf),1,
			&sequence_param_buf,&(TO_PARAM_BUF(idxSequenceParamBufId)));

	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS,return UNDEFINED);

   bResetSequence = false;
  
	return SUCCESS;
}

RtCode MPEG4Encoder::preparePictureParam(void)
{
      ENTER_FUN;

	VAStatus va_status;
	RtCode enc_status;
	VAEncPictureParameterBufferMPEG4 picture_param_buf;

	enc_status = manageParamBufId(idxPictureParamBufId);
	LOG_EXEC_IF(enc_status!=SUCCESS, return enc_status);

	memset((void*)&picture_param_buf, 0x0, sizeof(VAEncPictureParameterBufferMPEG4));

	 picture_param_buf.reference_picture = TO_SURFACE(idxRefSurfaceId);
        picture_param_buf.reconstructed_picture= TO_SURFACE(idxReconstructSurfaceId);
        picture_param_buf.coded_buf = TO_CODED_BUF(encodeFrameInfo.getCodedBufIdx());
        picture_param_buf.picture_width = codecConfig.frameWidth;
        picture_param_buf.picture_height = codecConfig.frameHeight;
        picture_param_buf.modulo_time_base = 0;
        picture_param_buf.vop_time_increment =  nFrameNum;

        /* determine the current frame type */
	if( frameType == I_FRAME)
        {
              picture_param_buf.picture_type = VAEncPictureTypeIntra;
        }
        else
        {
              picture_param_buf.picture_type = VAEncPictureTypePredictive;        
        }

       //hack code here for encode I frames
              	  
        va_status = vaCreateBuffer(vaDisplay, contextId,VAEncPictureParameterBufferType,
                                   sizeof(picture_param_buf),1,&picture_param_buf,&(TO_PARAM_BUF(idxPictureParamBufId)));

  	LOG_EXEC_IF(va_status!=VA_STATUS_SUCCESS, return UNDEFINED);

   nFrameNum++;

	return SUCCESS;

}

bool MPEG4Encoder::manageGOPCounter(void) 
{
    if(iGOPCounter == 0)
    {
       frameType = I_FRAME;
    }else if(iGOPCounter == codecConfig.intraInterval)
    {
       frameType = I_FRAME;            
       iGOPCounter = 0;
    }
    else
    {
        frameType = P_FRAME;   
    }

    iGOPCounter ++;

    return true;
}

