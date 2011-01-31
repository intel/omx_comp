#ifndef __AVCENCODER_H__
#define __AVCENCODER_H__

#include "baseEncoder.h"

typedef enum _AvcNaluType 
{
    UNSPECIFIED = 0,
    CODED_SLICE_NON_IDR,
    CODED_SLICE_PARTITION_A,
    CODED_SLICE_PARTITION_B,
    CODED_SLICE_PARTITION_C,
    CODED_SLICE_IDR,
    SEI,
    SPS,
    PPS,
    AU_DELIMITER,
    END_OF_SEQ,
    END_OF_STREAM,
    FILLER_DATA,
    SPS_EXT,
    CODED_SLICE_AUX_PIC_NO_PARTITION = 19,
    INVALID_NAL_TYPE = 64
} AvcNaluType;

typedef enum _NalStartCodeType
{
    NAL_STARTCODE_3_BYTE,
    NAL_STARTCODE_4_BYTE,
    NAL_STARTCODE_INVALID
} NalStartCodeType;

class AVCEncoder:public BaseEncoder
{
public:
  virtual void setEncodeProfile(void);
  virtual RtCode prepareSequenceParam(void);
  virtual RtCode preparePictureParam(void);
  virtual  bool   manageGOPCounter(void);

   RtCode refreshIDR(void) ;
  
};



#endif




