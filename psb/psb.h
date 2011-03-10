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
#define oscl_memset	memset
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
    OMX_ERRORTYPE __AllocateMpeg4Port(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocateH263Port(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocateRawPort(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocateRawVAPort(OMX_U32 port_index, OMX_DIRTYPE dir);

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

    /*
     * vcp setting helpers
     */
    OMX_ERRORTYPE __RawChangePortParamWithVcp(MixVideoConfigParams *vcp,
                                              PortVideo *port);
    OMX_ERRORTYPE __AvcChangePortParamWithVcp(MixVideoConfigParams *vcp,
                                              PortAvc *port);
    OMX_ERRORTYPE __Mpeg4ChangePortParamWithVcp(MixVideoConfigParams *vcp,
                                              PortMpeg4 *port);
    OMX_ERRORTYPE ChangePortParamWithVcp(void);

    OMX_ERRORTYPE __AvcChangeVcpWithPortParam(MixVideoConfigParams *vcp,
                                             PortAvc *port, bool *vcp_changed);
    OMX_ERRORTYPE __Mpeg4ChangeVcpWithPortParam(MixVideoConfigParams *vcp,
                                             PortMpeg4 *port, bool *vcp_changed);
    OMX_ERRORTYPE __H263ChangeVcpWithPortParam(MixVideoConfigParams *vcp,
                                             PortH263 *port, bool *vcp_changed);
    OMX_ERRORTYPE ChangeVcpWithPortParam(MixVideoConfigParams *vcp,
                                         PortVideo *port, bool *vcp_changed);

    OMX_ERRORTYPE __AvcChangePortParamWithCodecData(const OMX_U8 *codec_data,
                                                    OMX_U32 size,
                                                    PortBase **ports);
    OMX_ERRORTYPE __Mpeg4ChangePortParamWithCodecData(const OMX_U8 *codec_data,
                                                    OMX_U32 size,
                                                    PortBase **ports);
    OMX_ERRORTYPE __H263ChangePortParamWithCodecData(const OMX_U8 *codec_data,
                                                    OMX_U32 size,
                                                    PortBase **ports);
    OMX_ERRORTYPE ChangePortParamWithCodecData(const OMX_U8 *codec_data,
                                               OMX_U32 size,
                                               PortBase **ports);

    static void AvcEncMixBufferCallback(gulong token, guchar *data);

    /* end of vcp setting helpers */

    /* mix video */
    MixVideo *mix;
    MixVideoInitParams *vip;
    MixParams *mvp;
    MixVideoConfigParams *vcp;
    MixDisplayAndroid *display;
    MixBuffer *mixbuffer_in[1];
    MixIOVec *mixiovec_out[1];

    OMX_VIDEO_CODINGTYPE coding_type;
    MixCodecMode codec_mode;
    OMX_BOOL is_mixvideodec_configured;

    OMX_U32 inframe_counter;
    OMX_U32 outframe_counter;

    /* for fps */
    OMX_TICKS last_ts;
    float last_fps;

    OMX_U8 *avc_enc_buffer;
    OMX_U32 avc_enc_buffer_offset;
    OMX_U32 avc_enc_buffer_length;
    OMX_U32 avc_enc_frame_size_left;

    OMX_NALUFORMATSTYPE avcEncNaluFormatType;
    OMX_U32 avcEncIDRPeriod;
    OMX_U32 avcEncPFrames;

    OMX_VIDEO_PARAM_INTEL_BITRATETYPE avcEncParamIntelBitrateType;
    OMX_VIDEO_CONFIG_INTEL_BITRATETYPE avcEncConfigIntelBitrateType;
    OMX_VIDEO_CONFIG_NALSIZE avcEncConfigNalSize;
    OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS avcEncConfigSliceNumbers;
    OMX_VIDEO_CONFIG_INTEL_AIR avcEncConfigAir;
    OMX_CONFIG_FRAMERATETYPE avcEncFramerate;

    OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS avcDecodeSettings;
    OMX_U32 avcDecFrameWidth;
    OMX_U32 avcDecFrameHeight;
    OMX_BOOL avcDecGotRes;

    /* constant */
    /* ports */
    const static OMX_U32 NR_PORTS = 2;
    const static OMX_U32 INPORT_INDEX = 0;
    const static OMX_U32 OUTPORT_INDEX = 1;

    /* default buffer */
    const static OMX_U32 INPORT_AVC_ACTUAL_BUFFER_COUNT = 256; //10; //32; //10;
    const static OMX_U32 INPORT_AVC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_AVC_BUFFER_SIZE = 1382400; //40960;
    const static OMX_U32 OUTPORT_AVC_ACTUAL_BUFFER_COUNT = 10;
    const static OMX_U32 OUTPORT_AVC_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_AVC_BUFFER_SIZE = 1382400; //480000;

    const static OMX_U32 INPORT_RAW_ACTUAL_BUFFER_COUNT = 5; //2;
    const static OMX_U32 INPORT_RAW_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_RAW_BUFFER_SIZE = 614400;
    const static OMX_U32 OUTPORT_RAW_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_RAW_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_RAW_BUFFER_SIZE = 38016;

    const static OMX_U32 INPORT_MPEG4_ACTUAL_BUFFER_COUNT = 10;
    const static OMX_U32 INPORT_MPEG4_MIN_BUFFER_COUNT = 1;
//    const static OMX_U32 INPORT_MPEG4_BUFFER_SIZE = 16000;
    const static OMX_U32 INPORT_MPEG4_BUFFER_SIZE = 1382400;
    const static OMX_U32 OUTPORT_MPEG4_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_MPEG4_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_MPEG4_BUFFER_SIZE = 38016;

    const static OMX_U32 INPORT_H263_ACTUAL_BUFFER_COUNT = 10;
    const static OMX_U32 INPORT_H263_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_H263_BUFFER_SIZE = 1382400;
    const static OMX_U32 OUTPORT_H263_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_H263_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_H263_BUFFER_SIZE = 1382400;
};

#endif /* __WRS_OMXIL_INTEL_MRST_PSB */
