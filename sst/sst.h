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
     * parser wrappers
     */
    class MixIO;

    OMX_S32 Mp3FillMixIOAndChangeAcp(
        OMX_BUFFERHEADERTYPE *bufferheader,
        MixIO *mixio,
        MixAudioConfigParams *acp, bool *acp_changed,
        bool *acp_change_pending);

    OMX_ERRORTYPE ChangeAcpWithCodecConfigData(
        const OMX_U8 *buffer,
        MixAudioConfigParams *acp, bool *acp_changed);

    OMX_S32 AacFillMixIO(OMX_BUFFERHEADERTYPE *bufferheader, MixIO *mixio);

    /* end of parser wrappers */

    /* mix audio */
    MixAudio *mix;
    MixAudioConfigParams *acp;
    bool acp_changed;
    bool acp_change_pending;

    /*
     *                     MixIO
     * start timestamp --> +-------+ <-- mixio vector 0 -+-
     *                     |       |                     | frame length
     *                     |-------| <-- mixio vector 1 -+-
     *                     |       |
     *                     |-------| <-- mixio vector 2
     *                     |  ...  |         ...
     * last timestamp -->  +-------+
     */
    class MixIO
    {
    public:
        MixIO();
        ~MixIO();

        OMX_ERRORTYPE AllocateVector(OMX_U32 allocate_count);
        void FreeVector(void);

        OMX_ERRORTYPE SetOneVector(OMX_U8 *data, OMX_U32 size);

        OMX_U32 GetFilledCount(void);
        OMX_U32 GetFilledLength(void);
        OMX_U32 GetAllocCount(void);

        MixIOVec *GetVector(void);

        OMX_TICKS GetTimeStamp(void);
        void SetTimeStamp(OMX_TICKS timestamp);
        OMX_TICKS GetLastTimeStamp(void);
        void IncLastTimeStamp(OMX_TICKS duration);

        void ResetVector(void);

    private:
        MixIO(const MixIO &other);
        MixIO &operator=(const MixIO &other);

        MixIOVec *iovector;

        OMX_U32 filled_count;
        OMX_U32 filled_len;
        OMX_U32 alloc_count;

        OMX_TICKS start;
        OMX_TICKS last;
    };

    MixIO *mixio_in;
    MixIO *mixio_out;

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
    const static OMX_U32 OUTPORT_MP3_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 OUTPORT_MP3_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_MP3_BUFFER_SIZE = 4096;
    const static OMX_U32 INPORT_AAC_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 INPORT_AAC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_AAC_BUFFER_SIZE = 4096;
    const static OMX_U32 OUTPORT_AAC_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 OUTPORT_AAC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_AAC_BUFFER_SIZE = 4096;
    const static OMX_U32 INPORT_PCM_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 INPORT_PCM_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_PCM_BUFFER_SIZE = 4096;
    const static OMX_U32 OUTPORT_PCM_ACTUAL_BUFFER_COUNT = 5;
    const static OMX_U32 OUTPORT_PCM_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_PCM_BUFFER_SIZE = 16384;

    /* mixio */
    const static OMX_U32 INPORT_MP3_MIXIO_VECTOR_COUNT = 5;
    const static OMX_U32 OUTPORT_MP3_MIXIO_VECTOR_COUNT = 1;
    const static OMX_U32 INPORT_AAC_MIXIO_VECTOR_COUNT = 5;
    const static OMX_U32 OUTPORT_AAC_MIXIO_VECTOR_COUNT = 1;
    const static OMX_U32 INPORT_PCM_MIXIO_VECTOR_COUNT = 1;
    const static OMX_U32 OUTPORT_PCM_MIXIO_VECTOR_COUNT = 1;

    /* mixio max length to be accumulated */
    const static OMX_U32 MP3DEC_MAX_ACCUMULATE_LENGTH = 4096;
    const static OMX_U32 AACDEC_MAX_ACCUMULATE_LENGTH = 4096;
};

#endif /* __WRS_OMXIL_INTEL_MRST_SST */
