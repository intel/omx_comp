/*
 * Copyright (C) 2009 Wind River Systems
 *      Author: Keun-O Park <keun-o.park@windriver.com>
 *              Ho-Eun Ryu <ho-eun.ryu@windriver.com>
 *              Min-Su Kim <min-su.kim@windriver.com>
 */

#ifndef __WRS_OMXIL_INTEL_MRST_SST
#define __WRS_OMXIL_INTEL_MRST_SST

#include <OMX_Core.h>
#include <OMX_Component.h>

#include <cmodule.h>
#include <portbase.h>
#include <componentbase.h>

class MrstSstComponent : public ComponentBase
{
public:
    /*
     * constructor & destructor
     */
    MrstSstComponent();
    ~MrstSstComponent();

private:
    /*
     * component methods & helpers
     */
    /* implement ComponentBase::ComponentAllocatePorts */
    virtual OMX_ERRORTYPE ComponentAllocatePorts(void);

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
    virtual void ProcessorProcess(OMX_BUFFERHEADERTYPE **buffers,
                                  bool *retain,
                                  OMX_U32 nr_buffers);

    OMX_ERRORTYPE __AllocateMp3Port(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocateAacPort(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocatePcmPort(OMX_U32 port_index, OMX_DIRTYPE dir);

    OMX_ERRORTYPE __AllocateMp3RolePorts(bool isencoder);
    OMX_ERRORTYPE __AllocateAacRolePorts(bool isencoder);

    /* end of component methods & helpers */

    /* mix audio */
    MixAudio *mix;
    MixAudioConfigParams *acp;
    MixIOVec *mixio;

    OMX_AUDIO_CODINGTYPE coding_type;
    MixCodecMode codec_mode;

    OMX_U32 ibuffercount;

    /* constant */
    /* ports */
    const static OMX_U32 NR_PORTS = 2;
    const static OMX_U32 INPORT_INDEX = 0;
    const static OMX_U32 OUTPORT_INDEX = 1;

    /* buffer */
    const static OMX_U32 INPORT_MP3_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 INPORT_MP3_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_MP3_BUFFER_SIZE = 4096;
    const static OMX_U32 OUTPORT_MP3_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_MP3_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_MP3_BUFFER_SIZE = 1024;
    const static OMX_U32 INPORT_AAC_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 INPORT_AAC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_AAC_BUFFER_SIZE = 1024;
    const static OMX_U32 OUTPORT_AAC_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_AAC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_AAC_BUFFER_SIZE = 1024;
    const static OMX_U32 INPORT_PCM_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 INPORT_PCM_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_PCM_BUFFER_SIZE = 1024;
    const static OMX_U32 OUTPORT_PCM_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 OUTPORT_PCM_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_PCM_BUFFER_SIZE = 16384;
};

/*
 * CModule Interface
 */
#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE omx_component_module_instantiate(OMX_PTR *);
OMX_ERRORTYPE omx_component_module_query_name(OMX_STRING, OMX_U32);
OMX_ERRORTYPE omx_component_module_query_roles(OMX_U32 *, OMX_U8 **);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __WRS_OMXIL_INTEL_MRST_SST */
