#ifndef __MPEG4ENCODER_H__
#define __MPEG4ENCODER_H__

#include "baseEncoder.h"

class MPEG4Encoder:public BaseEncoder
{
public:
  virtual void setEncodeProfile(void);
  virtual RtCode prepareSequenceParam(void);
  virtual RtCode preparePictureParam(void);

  virtual bool manageGOPCounter(void) ;
private:

   bool bNewSequence;
};



#endif




