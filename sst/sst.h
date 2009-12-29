/*
 * Copyright (c) 2009 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
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
                                  buffer_retain_t *retain,
                                  OMX_U32 nr_buffers);

    OMX_ERRORTYPE __AllocateMp3Port(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocateAacPort(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocatePcmPort(OMX_U32 port_index, OMX_DIRTYPE dir);

    /* end of component methods & helpers */

    /*
     * acp setting helpers
     */
    OMX_ERRORTYPE __Mp3ChangeAcpWithConfigHeader(const unsigned char *buffer,
                                                 bool *acp_changed);
    OMX_ERRORTYPE __AacChangeAcpWithConfigHeader(const unsigned char *buffer,
                                                 bool *acp_changed);
    OMX_ERRORTYPE ChangeAcpWithConfigHeader(const unsigned char *buffer,
                                            bool *acp_changed);

    OMX_ERRORTYPE __Mp3ChangeAcpWithPortParam(MixAudioConfigParams *acp,
                                              PortMp3 *port,
                                              bool *acp_changed);
    OMX_ERRORTYPE __AacChangeAcpWithPortParam(MixAudioConfigParams *acp,
                                              PortAac *port,
                                              bool *acp_changed);
    OMX_ERRORTYPE ChangeAcpWithPortParam(MixAudioConfigParams *acp,
                                         PortBase *port,
                                         bool *acp_changed);

    OMX_ERRORTYPE __PcmChangePortParamWithAcp(MixAudioConfigParams *acp,
                                              PortPcm *port);
    OMX_ERRORTYPE __Mp3ChangePortParamWithAcp(MixAudioConfigParams *acp,
                                              PortMp3 *port);
    OMX_ERRORTYPE __AacChangePortParamWithAcp(MixAudioConfigParams *acp,
                                              PortAac *port);
    OMX_ERRORTYPE ChangePortParamWithAcp(void);

    /* end of acp setting helpers */

    /* mix audio */
    MixAudio *mix;
    MixAudioConfigParams *acp;
    MixIOVec *mixio_in, *mixio_out;

    OMX_AUDIO_CODINGTYPE coding_type;
    MixCodecMode codec_mode;

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
    const static OMX_U32 INPORT_AAC_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 INPORT_AAC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_AAC_BUFFER_SIZE = 4096;
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

#endif /* __WRS_OMXIL_INTEL_MRST_SST */
