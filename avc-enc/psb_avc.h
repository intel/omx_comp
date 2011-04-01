/*
 * psb.h, omx psb component header
 *
 * Copyright (c) 2009-2010 Wind River Systems, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __WRS_OMXIL_INTEL_MRST_PSB
#define __WRS_OMXIL_INTEL_MRST_PSB

#include <OMX_Core.h>
#include <OMX_Component.h>

#include <cmodule.h>
#include <portbase.h>
#include <componentbase.h>

#include <IntelBufferSharing.h>

using android::sp;
using android::BufferShareRegistry;

typedef enum
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

typedef enum
{
    NAL_STARTCODE_3_BYTE,
    NAL_STARTCODE_4_BYTE,
    NAL_STARTCODE_INVALID
} NalStartCodeType;

typedef enum
{
    BUFFER_SHARING_INVALID,
    BUFFER_SHARING_LOADED,
    BUFFER_SHARING_EXECUTING
} BufferSharingState;

class MrstPsbComponent : public ComponentBase
{
public:
    /*
     * constructor & destructor
     */
    MrstPsbComponent();
    ~MrstPsbComponent();

private:
    /*
     * component methods & helpers
     */
    /* implement ComponentBase::ComponentAllocatePorts */
    virtual OMX_ERRORTYPE ComponentAllocatePorts(void);

    OMX_ERRORTYPE __AllocateAvcPort(OMX_U32 port_index, OMX_DIRTYPE dir);
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

    /* encoders */
    OMX_ERRORTYPE __AvcChangeVcpWithPortParam(MixVideoConfigParams *vcp,
            PortAvc *port, bool *vcp_changed);
    OMX_ERRORTYPE ChangeVcpWithPortParam(MixVideoConfigParams *vcp,
                                         PortVideo *port_in,
                                         PortVideo *port_out,
                                         bool *vcp_changed);

    AvcNaluType GetNaluType(OMX_U8* nal,NalStartCodeType startcode_type);

    OMX_ERRORTYPE SplitNalByStartCode(OMX_U8* buf, OMX_U32 len,OMX_U8** nalbuf, OMX_U32* nallen,
                                      NalStartCodeType startcode_type);

    static void AvcEncMixBufferCallback(ulong token, uchar *data);
    OMX_ERRORTYPE ExtractConfigData(OMX_U8* coded_buf, OMX_U32 coded_len,OMX_U8** config_buf, OMX_U32* config_len,OMX_U8** video_buf, OMX_U32* video_len);
    bool DetectSyncFrame(OMX_U8* coded_buf, OMX_U32 coded_len);
    /* end of vcp setting helpers */

    /* share buffer setting */
    OMX_ERRORTYPE InitShareBufferingSettings();
    OMX_ERRORTYPE EnterShareBufferingMode();
    OMX_ERRORTYPE ExitShareBufferingMode();
    OMX_ERRORTYPE EnableBufferSharingMode();
    OMX_ERRORTYPE DisableBufferSharingMode();
    OMX_ERRORTYPE RequestShareBuffers(MixVideo* mix, int width, int height);
    OMX_ERRORTYPE RegisterShareBufferToPort();
    OMX_ERRORTYPE RegisterShareBufferToLib();
    /* end of share buffer setting */
    /* mix video */
    MixVideo *mix;
    MixVideoInitParams *vip;
    MixParams *mvp;
    MixVideoConfigParams *vcp;
    MixDisplayAndroid *display;
    MixBuffer *mixbuffer_in[1];
    MixIOVec *mixiovec_out[1];

    OMX_BOOL is_mixvideodec_configured;

    OMX_U32 inframe_counter;
    OMX_U32 outframe_counter;

    /* for fps */
    OMX_TICKS last_ts;
    float last_fps;

    /* for buffer sharing */
    sp<BufferShareRegistry> buffer_sharing_lib;
    int buffer_sharing_count;
    SharedBufferType* buffer_sharing_info;
    BufferSharingState buffer_sharing_state;

    /* for Nalu format encapsulation and config data setting*/
    bool b_config_sent;
    OMX_U8* video_data;
    OMX_U32 video_len;

    OMX_U8 *temp_coded_data_buffer;
    OMX_U32 temp_coded_data_buffer_size;

    OMX_NALUFORMATSTYPE avcEncNaluFormatType;
    OMX_U32 avcEncIDRPeriod;
    OMX_U32 avcEncPFrames;

    OMX_VIDEO_PARAM_INTEL_BITRATETYPE avcEncParamIntelBitrateType;
    OMX_VIDEO_CONFIG_INTEL_BITRATETYPE avcEncConfigIntelBitrateType;
    OMX_VIDEO_CONFIG_NALSIZE avcEncConfigNalSize;
    OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS avcEncConfigSliceNumbers;
    OMX_VIDEO_CONFIG_INTEL_AIR avcEncConfigAir;
    OMX_CONFIG_FRAMERATETYPE avcEncFramerate;

    /* constant */
    /* ports */
    const static OMX_U32 NR_PORTS = 2;
    const static OMX_U32 INPORT_INDEX = 0;
    const static OMX_U32 OUTPORT_INDEX = 1;

    /* default buffer */
    const static OMX_U32 INPORT_RAW_ACTUAL_BUFFER_COUNT = 2; //FIXME: must be set as 2
    const static OMX_U32 INPORT_RAW_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_RAW_BUFFER_SIZE = 614400;
    const static OMX_U32 OUTPORT_RAW_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_RAW_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_RAW_BUFFER_SIZE = 38016;

    const static OMX_U32 INPORT_AVC_ACTUAL_BUFFER_COUNT = 256;
    const static OMX_U32 INPORT_AVC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_AVC_BUFFER_SIZE = 614400;
    const static OMX_U32 OUTPORT_AVC_ACTUAL_BUFFER_COUNT = 10;
    const static OMX_U32 OUTPORT_AVC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_AVC_BUFFER_SIZE = 614400;


};

#endif /* __WRS_OMXIL_INTEL_MRST_PSB */
