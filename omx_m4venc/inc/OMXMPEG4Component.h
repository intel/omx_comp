#ifndef __WRS_OMXIL_INTEL_MRST_PSB
#define __WRS_OMXIL_INTEL_MRST_PSB

#include <OMX_Core.h>
#include <OMX_Component.h>

#include <cmodule.h>
#include <portbase.h>
#include <componentbase.h>

#include <pthread.h>
#include "MPEG4Encoder.h"
#include "codecHelper.h"

class OMXMPEG4Component: 
	public ComponentBase,
	public CodecInfoObserver

{

public:
   OMXMPEG4Component(); 
   ~OMXMPEG4Component();

private:
    /*
     * component methods & helpers
     */
    /* implement ComponentBase::ComponentAllocatePorts */
    virtual OMX_ERRORTYPE ComponentAllocatePorts(void);

    OMX_ERRORTYPE __AllocateMPEG4Port(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocateRawPort(OMX_U32 port_index, OMX_DIRTYPE dir);

    /* implement ComponentBase::ComponentGet/SetPatameter */
    virtual OMX_ERRORTYPE
        ComponentGetParameter(OMX_INDEXTYPE nParamIndex,
                              OMX_PTR pComponentParameterStructure);
    virtual OMX_ERRORTYPE
        ComponentSetParameter(OMX_INDEXTYPE nIndex,
                              OMX_PTR pComponentParameterStructure);

    /* implement ComponentBase::ComponentGet/SetConfig */
    virtual OMX_ERRORTYPE
        ComponentGetConfig(OMX_INDEXTYPE nIndex,
                           OMX_PTR pComponentConfigStructure);
    virtual OMX_ERRORTYPE
        ComponentSetConfig(OMX_INDEXTYPE nIndex,
                           OMX_PTR pComponentConfigStructure);

    /* implement ComponentBase::Processor[*] */
    virtual OMX_ERRORTYPE ProcessorInit(void);  /* Loaded to Idle */
    virtual OMX_ERRORTYPE ProcessorDeinit(void);/* Idle to Loaded */
    virtual OMX_ERRORTYPE ProcessorStart(void); /* Idle to Executing/Pause */
    virtual OMX_ERRORTYPE ProcessorStop(void);  /* Executing/Pause to Idle */
    virtual OMX_ERRORTYPE ProcessorPause(void); /* Executing to Pause */
    virtual OMX_ERRORTYPE ProcessorResume(void);/* Pause to Executing */
    virtual OMX_ERRORTYPE ProcessorFlush(OMX_U32 port_index);
    virtual OMX_ERRORTYPE ProcessorProcess(OMX_BUFFERHEADERTYPE **buffers,
                                           buffer_retain_t *retain,
                                           OMX_U32 nr_buffers);

    /* end of component methods & helpers */

    /* config setting helpers */
    OMX_ERRORTYPE ChangeMPEG4EncoderConfigWithPortParam(CodecConfig* pconfig,
                                         PortVideo *port);

    /* observers funcs */
    OMX_VIDEO_CONFIG_PRI_INFOTYPE *temp_port_private_info;

    void handleEncoderCycleEnded(VASurfaceID surface,bool b_generated,	bool b_share_buffer);
    void handleConfigChange(bool b_dynamic);

    /* MPEG4 encoder */
    MPEG4Encoder* encoder;
    MediaBuffer in;
    MediaBuffer out;
    CodecConfig config;
    CodecStatistics statistics;

    pthread_mutex_t	*config_lock;
#define LOCK_CONFIG	{\
	int pret;\
	pret = pthread_mutex_lock(config_lock);\
	assert(pret == 0);\

#define UNLOCK_CONFIG \
	pret = pthread_mutex_unlock(config_lock);\
	assert(pret = 0);\
}

    OMX_U32 inframe_counter;
    OMX_U32 outframe_counter;

    /* for fps */
    OMX_TICKS last_ts;
    float last_fps;

    /* for  format translation */
    OMX_U8 *temp_coded_data_buffer;
    OMX_U32 temp_coded_data_buffer_size;

    OMX_U8 *avc_enc_buffer;   
    OMX_U32 avc_enc_buffer_offset;
    OMX_U32 avc_enc_buffer_length;
    OMX_U32 avc_enc_frame_size_left;

    OMX_S64 last_out_timestamp;
    FrameType last_out_frametype;

    /* constant */
    /* ports */
    const static OMX_U32 NR_PORTS = 2;
    const static OMX_U32 INPORT_INDEX = 0;
    const static OMX_U32 OUTPORT_INDEX = 1;

    /* default buffer */
    const static OMX_U32 INPORT_RAW_ACTUAL_BUFFER_COUNT = 5; //2;
    const static OMX_U32 INPORT_RAW_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_RAW_BUFFER_SIZE = 614400;
    const static OMX_U32 OUTPORT_RAW_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_RAW_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_RAW_BUFFER_SIZE = 38016;
    
    const static OMX_U32 INPORT_MPEG4_ACTUAL_BUFFER_COUNT = 10;
    const static OMX_U32 INPORT_MPEG4_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_MPEG4_BUFFER_SIZE = 16000;
    const static OMX_U32 OUTPORT_MPEG4_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_MPEG4_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_MPEG4_BUFFER_SIZE = 38016;
  
};

#endif
