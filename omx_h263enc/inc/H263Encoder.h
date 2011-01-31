#ifndef __H263ENCODER_H__
#define __H263ENCODER_H__

#include "baseEncoder.h"

class H263Encoder:public BaseEncoder
{
public:
  virtual void setEncodeProfile(void);
  virtual RtCode prepareSequenceParam(void);
  virtual RtCode preparePictureParam(void);

  virtual bool manageGOPCounter(void) ;
   
private:
   int a;
  
};



#endif




