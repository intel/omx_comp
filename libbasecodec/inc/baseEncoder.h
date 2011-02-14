#ifndef __BaseEncoder_H__
#define __BaseEncoder_H__

#include "stdio.h"
#include "codecHelper.h"

#define MAX_SLICE_PER_FRAME 8
#define MIN_SLICE_PER_FRAME 1
#define MAX_PARAM_BUF 10
#define MAX_CODED_BUF 8

class BaseEncoder
{
 public:
  BaseEncoder();
  virtual ~BaseEncoder();

  RtCode setConfig(CodecConfig config);
  RtCode setDynamicConfig(CodecConfig config);
  RtCode reconfig(CodecConfig config);
  RtCode getConfig(CodecConfig *config);

  virtual void setEncodeProfile(void);
  virtual RtCode prepareSequenceParam(void);
  virtual RtCode preparePictureParam(void);
  virtual RtCode prepareSlice(void);

  virtual void initGOPCounter(void);
  virtual bool manageGOPCounter(void);

  RtCode init(void);
  RtCode encode(MediaBuffer* in, MediaBuffer* out);
  RtCode deinit(void);

  void setObserver(CodecInfoObserver* obs);
  void removeObserver(void);

  void resetStatistics(CodecStatistics* old_statics);
  void enableTimingStatistics(void);
  void disableTimingStatistics(void);

  void setKeyFrame(void);			
protected:
  RtCode createEncodeResource(void);
  RtCode destroyEncodeResource(void);

  void initCodecInternalSurfaceId(void);
  void manageCodecInternalSurfaceId(void);

  void initParamBufId(void);
  RtCode manageParamBufId(int& id_to_alloc);

  RtCode checkGeneratedFrame(void);
  
  RtCode beginPicture(void);
  RtCode endPicture(void);

  /* prepare parameter buffers */
  RtCode launchSequenceParam(void);
  RtCode launchPictureParam(void);
  RtCode launchSlice(void);
  RtCode launchParamArray(void);

  RtCode loadSourceSurface(bool force_not_share);

  RtCode generateOutput(void);

  void initEncodeFrameInfo(void);
  void prepareEncodeFrameInfo(void);

  void prepareMediaBufferIn(MediaBuffer* in);
  void prepareMediaBufferOut(MediaBuffer* out);

  void _generateInvalidOutput(void);

  protected:  
  //va general handling
  VAProfile videoEncodeProfile;
  VAEntrypoint videoEncodeEntry;

  VADisplay vaDisplay;
  VAContextID contextId;
  VAConfigID configId;
  VASurfaceID *surfaceId;
  VABufferID codedBufId[MAX_CODED_BUF];
  VABufferID paramBufId[MAX_PARAM_BUF];
  int nTotalSurface;
  int nCiSurface;

  int CODEC_SURFACE_ID_BASE ;
  int NORMAL_SRC_SURFACE_ID_BASE;
  int CI_SRC_SURFACE_ID_BASE;

  int CODEC_BUFFER_ID_BASE;

  //va codec handling
  VASurfaceStatus surfaceStatus;
	
  //internal resource	
  int idxRefSurfaceId;
  int idxReconstructSurfaceId;
		
  int idxParamBufId;
  int idxSequenceParamBufId;
  int idxPictureParamBufId;
  int idxSliceArrayParamBufId;

  CodecFrameInfo encodeFrameInfo;

  int codecBufSize;

  //config
  CodecConfig codecConfig;

  //statistics
  CodecStatistics codecStatistics;

  //slices info
  VAEncSliceParameterBuffer sliceInfo[MAX_SLICE_PER_FRAME];

  //internal state
  bool	bInitialized;
  int	iGOPCounter;
  FrameType frameType;
  bool	bFrameGenerated;
  bool	bSendEndOfSequence;

  //buffer hook
  MediaBuffer* inBuf;
  MediaBuffer* outBuf;

  //info observer
  CodecInfoObserver *observer;

  int  nFrameNum;
  bool bResetSequence;
};


#endif
