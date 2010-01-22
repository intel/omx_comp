/*
 * Copyright (c) 2009 Wind River Systems, Inc.
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable Wind River license agreement.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <OMX_Core.h>

#include <cmodule.h>
#include <portvideo.h>
#include <componentbase.h>

#include <pv_omxcore.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <mixdisplayx11.h>
#include <mixvideo.h>
#include <mixvideoformat_h264.h>
#include <mixvideoconfigparamsdec_h264.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "psb.h"

#define LOG_NDEBUG 0

#define LOG_TAG "mrst_psb"
#include <log.h>

/*
 * constructor & destructor
 */
MrstPsbComponent::MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = void)\n", __func__, __LINE__);
}

MrstPsbComponent::~MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = void)\n", __func__, __LINE__);
}

/* end of constructor & destructor */

/* core methods & helpers */
OMX_ERRORTYPE MrstPsbComponent::ComponentAllocatePorts(void)
{
    PortBase **ports;

    OMX_U32 codec_port_index, raw_port_index;
    OMX_DIRTYPE codec_port_dir, raw_port_dir;

    OMX_PORT_PARAM_TYPE portparam;

    const char *working_role;

    OMX_ERRORTYPE ret = OMX_ErrorUndefined;

    LOGV("%s(): enter\n", __func__);

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;

    this->nr_ports = NR_PORTS;
    this->ports = ports;

    /* video_[encoder/decoder].[avc/whatever] */
    working_role = GetWorkingRole();
    working_role = strpbrk(working_role, "_");

    if (!strncmp(working_role, "_encoder", strlen("_encoder")))
        isencoder = true;
    else
        isencoder = false;

    working_role = strpbrk(working_role, ".");
    if (!strcmp(working_role, ".avc"))
        coding_type = OMX_VIDEO_CodingAVC;

    if (isencoder) {
        raw_port_index = INPORT_INDEX;
        codec_port_index = OUTPORT_INDEX;
        raw_port_dir = OMX_DirInput;
        codec_port_dir = OMX_DirOutput;
    }
    else {
        codec_port_index = INPORT_INDEX;
        raw_port_index = OUTPORT_INDEX;
        codec_port_dir = OMX_DirInput;
        raw_port_dir = OMX_DirOutput;
    }

    if (coding_type == OMX_VIDEO_CodingAVC)
        ret = __AllocateAvcPort(codec_port_index, codec_port_dir);

    if (ret != OMX_ErrorNone)
        goto free_ports;

    ret = __AllocateRawPort(raw_port_index, raw_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    /* OMX_PORT_PARAM_TYPE */
    memset(&portparam, 0, sizeof(portparam));
    SetTypeHeader(&portparam, sizeof(portparam));
    portparam.nPorts = NR_PORTS;
    portparam.nStartPortNumber = INPORT_INDEX;

    memcpy(&this->portparam, &portparam, sizeof(portparam));
    /* end of OMX_PORT_PARAM_TYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

free_codecport:
    delete ports[codec_port_index];
    ports[codec_port_index] = NULL;

free_ports:
    coding_type = OMX_VIDEO_CodingUnused;

    delete []ports;
    ports = NULL;

    this->ports = NULL;
    this->nr_ports = 0;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__AllocateAvcPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortAvc *avcport;

    OMX_PARAM_PORTDEFINITIONTYPE avcportdefinition;
    OMX_VIDEO_PARAM_AVCTYPE avcportparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortAvc;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    avcport = static_cast<PortAvc *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&avcportdefinition, 0, sizeof(avcportdefinition));
    SetTypeHeader(&avcportdefinition, sizeof(avcportdefinition));
    avcportdefinition.nPortIndex = port_index;
    avcportdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        avcportdefinition.nBufferCountActual = INPORT_AVC_ACTUAL_BUFFER_COUNT;
        avcportdefinition.nBufferCountMin = INPORT_AVC_MIN_BUFFER_COUNT;
        avcportdefinition.nBufferSize = INPORT_AVC_BUFFER_SIZE;
    }
    else {
        avcportdefinition.nBufferCountActual = OUTPORT_AVC_ACTUAL_BUFFER_COUNT;
        avcportdefinition.nBufferCountMin = OUTPORT_AVC_MIN_BUFFER_COUNT;
        avcportdefinition.nBufferSize = OUTPORT_AVC_BUFFER_SIZE;
    }
    avcportdefinition.bEnabled = OMX_TRUE;
    avcportdefinition.bPopulated = OMX_FALSE;
    avcportdefinition.eDomain = OMX_PortDomainVideo;
    avcportdefinition.format.video.cMIMEType = (char *)"video/h264";
    avcportdefinition.format.video.pNativeRender = NULL;
    avcportdefinition.format.video.nFrameWidth = 176;
    avcportdefinition.format.video.nFrameHeight = 144;
    avcportdefinition.format.video.nStride = 0;
    avcportdefinition.format.video.nSliceHeight = 0;
    avcportdefinition.format.video.nBitrate = 64000;
    avcportdefinition.format.video.xFramerate = 15 << 16;
    avcportdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    avcportdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
    avcportdefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    avcportdefinition.format.video.pNativeWindow = NULL;
    avcportdefinition.bBuffersContiguous = OMX_FALSE;
    avcportdefinition.nBufferAlignment = 0;

    avcport->SetPortDefinition(&avcportdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_AVCTYPE */
    memset(&avcportparam, 0, sizeof(avcportparam));
    SetTypeHeader(&avcportparam, sizeof(avcportparam));
    avcportparam.nPortIndex = port_index;
    avcportparam.eProfile = OMX_VIDEO_AVCProfileBaseline;
    avcportparam.eLevel = OMX_VIDEO_AVCLevel1;

    avcport->SetPortAvcParam(&avcportparam, true);
    /* end of OMX_VIDEO_PARAM_AVCTYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::__AllocateRawPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortVideo *rawport;

    OMX_PARAM_PORTDEFINITIONTYPE rawportdefinition;
    OMX_VIDEO_PARAM_PORTFORMATTYPE rawvideoparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortVideo;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    rawport = static_cast<PortVideo *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&rawportdefinition, 0, sizeof(rawportdefinition));
    SetTypeHeader(&rawportdefinition, sizeof(rawportdefinition));
    rawportdefinition.nPortIndex = port_index;
    rawportdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        rawportdefinition.nBufferCountActual = INPORT_RAW_ACTUAL_BUFFER_COUNT;
        rawportdefinition.nBufferCountMin = INPORT_RAW_MIN_BUFFER_COUNT;
        rawportdefinition.nBufferSize = INPORT_RAW_BUFFER_SIZE;
    }
    else {
        rawportdefinition.nBufferCountActual = OUTPORT_RAW_ACTUAL_BUFFER_COUNT;
        rawportdefinition.nBufferCountMin = OUTPORT_RAW_MIN_BUFFER_COUNT;
        rawportdefinition.nBufferSize = OUTPORT_RAW_BUFFER_SIZE;
    }
    rawportdefinition.bEnabled = OMX_TRUE;
    rawportdefinition.bPopulated = OMX_FALSE;
    rawportdefinition.eDomain = OMX_PortDomainVideo;
    rawportdefinition.format.video.cMIMEType = (char *)"video/raw";
    rawportdefinition.format.video.pNativeRender = NULL;
    rawportdefinition.format.video.nFrameWidth = 176;
    rawportdefinition.format.video.nFrameHeight = 144;
    rawportdefinition.format.video.nStride = 0;
    rawportdefinition.format.video.nSliceHeight = 0;
    rawportdefinition.format.video.nBitrate = 64000;
    rawportdefinition.format.video.xFramerate = 15 << 16;
    rawportdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    rawportdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawportdefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    rawportdefinition.format.video.pNativeWindow = NULL;
    rawportdefinition.bBuffersContiguous = OMX_FALSE;
    rawportdefinition.nBufferAlignment = 0;

    rawport->SetPortDefinition(&rawportdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_PORTFORMATTYPE */
    rawvideoparam.nPortIndex = port_index;
    rawvideoparam.nIndex = 0;
    rawvideoparam.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawvideoparam.eColorFormat = OMX_COLOR_FormatYUV420Planar;

    rawport->SetPortVideoParam(&rawvideoparam, true);

    /* end of OMX_VIDEO_PARAM_PORTFORMATTYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

/* end of core methods & helpers */

/*
 * component methods & helpers
 */
/* Get/SetParameter */
OMX_ERRORTYPE MrstPsbComponent::ComponentGetParameter(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nParamIndex);

    switch (nParamIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortVideoParam(), sizeof(*p));
        break;
    }
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        if (strcmp(GetWorkingRole(), "video_decoder.avc")) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorUnsupportedIndex);
            return OMX_ErrorUnsupportedIndex;
        }

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAvc *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortAvcParam(), sizeof(*p));
        break;
    }
    case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX: {
        PV_OMXComponentCapabilityFlagsType *p =
            (PV_OMXComponentCapabilityFlagsType *)pComponentParameterStructure;

        p->iIsOMXComponentMultiThreaded = OMX_TRUE;
        p->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
#ifdef PV_FULL_AVC_FRAME_MODE
        p->iOMXComponentSupportsMovableInputBuffers = OMX_FALSE;
        p->iOMXComponentUsesNALStartCodes = OMX_FALSE;
        p->iOMXComponentSupportsPartialFrames = OMX_FALSE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
        p->iOMXComponentUsesFullAVCFrames = OMX_TRUE;
#else
        p->iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
        p->iOMXComponentUsesNALStartCodes = OMX_FALSE;
        p->iOMXComponentSupportsPartialFrames = OMX_TRUE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
        p->iOMXComponentUsesFullAVCFrames = OMX_FALSE;
#endif
        break;
    }
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ComponentSetParameter(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nIndex);

    switch (nIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortVideoParam(p, false);
        break;
    }
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAvc *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortAvcParam(p, false);
        break;
    }
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* Get/SetConfig */
OMX_ERRORTYPE MrstPsbComponent::ComponentGetConfig(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ComponentSetConfig(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::Processor[*] */
OMX_ERRORTYPE MrstPsbComponent::ProcessorInit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    MixIOVec *mixio = NULL;
    MixVideo *mix_video = NULL;
    MixDrmParams *mix_drm = NULL;
    MixVideoInitParams *init_params = NULL;
    MixVideoDecodeParams *mix_decode_params = NULL;
    MixVideoConfigParamsDec *config_params = NULL;
    MixDisplayX11 *mix_display_x11 = NULL;
    MixBuffer *mix_buffer = NULL;
    MixVideoRenderParams *mix_video_render_params = NULL;
    MixRect src_rect, dst_rect;

    guint major, minor;

    FrameCount = 0;

    MIX_RESULT mret;

    g_type_init();

    /*
     * common codes
     */
    mix_video = mix_video_new();
    if (!mix_video) {
        LOGE("%s(),%d: exit, mix_video_new failed", __func__, __LINE__);
        goto exit_pinit;
    }

    mix_video_get_version(mix_video, &major, &minor);
    LOGV("MixVideo version: %d.%d", major, minor);

    mix_display_x11 = mix_displayx11_new();
    if (!mix_display_x11) {
        LOGE("%s(),%d: exit, mix_displayx11_new failed", __func__, __LINE__);
        goto exit_pinit;
    }

    mret = mix_displayx11_set_drawable(mix_display_x11, 176);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_displayx11_set_drawable failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto exit_pinit;
    }

    init_params = mix_videoinitparams_new();
    if (!init_params) {
        LOGE("%s(),%d: exit, mix_videoinitparams_new failed", __func__, __LINE__);
        goto exit_pinit;
    }

    mret = mix_videoinitparams_set_display(init_params, MIX_DISPLAY(mix_display_x11));
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videoinitparams_set_display failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        goto exit_pinit;
    }

    mix_drm = mix_drmparams_new();
    if (!mix_drm) {
        LOGE("%s(),%d: exit, mix_videoinitparams_new failed", __func__, __LINE__);
        goto exit_pinit;
    }

    /*
     * file format specific code
     */
    if (coding_type == OMX_VIDEO_CodingAVC) {
        MixVideoConfigParamsDecH264 *configH264 = NULL;

        LOGV("%s(%d)\n", __func__, __LINE__);
        configH264 = mix_videoconfigparamsdec_h264_new();
        config_params = MIX_VIDEOCONFIGPARAMSDEC(configH264);
        /* mime type */
        mret = mix_videoconfigparamsdec_set_mime_type(config_params, "video/x-h264");
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(),%d: exit, mix_videoconfigparamsdec_set_mime_type failed (ret == 0x%08x)",
                    __func__, __LINE__, mret);
            goto exit_pinit;
        }
    } 
    else {
        LOGE("%s(),%d: exit, unkown role (ret == 0x%08x)\n",
             __func__, __LINE__, OMX_ErrorInvalidState);
        goto exit_pinit;
    }

    /* decoder */
    if (!isencoder) {
        /*
         * decoder specific code
         */
        mret = mix_video_initialize(mix_video, MIX_CODEC_MODE_DECODE, init_params, mix_drm);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(),%d: exit, mix_video_initialize failed (ret == 0x%08x)",
                    __func__, __LINE__, mret);
            goto exit_pinit;
        }

        /* set frame order mode */
        mret = mix_videoconfigparamsdec_set_frame_order_mode(config_params, MIX_FRAMEORDER_MODE_DECODEORDER);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(),%d: exit, mix_videoconfigparamsdec_set_frame_order_mode failed (ret == 0x%08x)",
                    __func__, __LINE__, mret);
            goto exit_pinit;
        }

        mix_decode_params = mix_videodecodeparams_new();
        if (!mix_decode_params) {
            LOGE("%s(),%d: exit, mix_decode_params failed (ret == 0x%08x)",
                    __func__, __LINE__, mret);
            goto exit_pinit;
        }

        /* fill discontinuity flag */
        mret = mix_videodecodeparams_set_discontinuity(mix_decode_params, FALSE);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(),%d: exit, mix_videodecodeparams_set_discontinuity (ret == 0x%08x)",
                    __func__, __LINE__, mret);
            goto exit_pinit;
        }
    }
    else {
        LOGE("%s(),%d: exit, unkown mode (ret == 0x%08x)\n",
             __func__, __LINE__, OMX_ErrorInvalidState);
        goto exit_pinit;
    }

    LOGV("%s(%d)\n", __func__, __LINE__);
    /* configure params */

    /* frame rate - TODO? numerator:30 denominator:1 */
    mret = mix_videoconfigparamsdec_set_frame_rate(config_params, 15, 1);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videoconfigparamsdec_set_frame_rate failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        goto exit_pinit;
    }

    /* Picture resolution */
    mret = mix_videoconfigparamsdec_set_picture_res(config_params, 176, 144);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videoconfigparamsdec_set_picture_res failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        goto exit_pinit;
    }

    /* buffer pool size */
    mret = mix_videoconfigparamsdec_set_buffer_pool_size(config_params, 8);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videoconfigparamsdec_set_buffer_pool_size failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        goto exit_pinit;
    }

    mret = mix_videoconfigparamsdec_set_extra_surface_allocation(config_params, 4);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videoconfigparamsdec_set_extra_surface_allocation (ret == 0x%08x)",
                __func__, __LINE__, mret);
        goto exit_pinit;
    }

    mix_video_render_params = mix_videorenderparams_new();
    if (!mix_video_render_params) {
        LOGE("%s(),%d: exit, mix_videorenderparams_new failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        goto exit_pinit;
    }

    mret = mix_videorenderparams_set_display(mix_video_render_params, MIX_DISPLAY(mix_display_x11));
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videorenderparams_set_display (ret == 0x%08x)",
                __func__, __LINE__, mret);
        goto exit_pinit;
    }

    /* fill src_rect, the video size */
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = 176;
    src_rect.height = 144;
    /* fill dst_rect, the display size
     * TODO: we shall calculate the dst video position
     */
    dst_rect.x = 0;
    dst_rect.y = 0;
    dst_rect.width = 176;
    dst_rect.height = 144;

    mret = mix_videorenderparams_set_src_rect(mix_video_render_params, src_rect);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("Failed to set src_rect\n");
        goto exit_pinit;
    }
    mret = mix_videorenderparams_set_dest_rect(mix_video_render_params, dst_rect);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("Failed to set dst_rect\n");
        goto exit_pinit;
    }
    mret = mix_videorenderparams_set_clipping_rects(mix_video_render_params, NULL, 0);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("Failed to set clipping rects\n");
        goto exit_pinit;
    }

    mixio = (MixIOVec *)malloc(sizeof(MixIOVec));
    if (!mixio) {
        LOGE("%s(),%d: exit, failed to allocate mbuffer (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto exit_pinit;
    }

    this->mix_video = mix_video;
    this->mix_drm = mix_drm;
    this->init_params = init_params;
    this->mix_decode_params = mix_decode_params;
    this->config_params = config_params;
    this->configH264 = configH264;
    this->mix_display_x11 = mix_display_x11;
    this->mixio = mixio;
    this->mix_buffer = mix_buffer;
    this->mix_video_render_params = mix_video_render_params;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;

 exit_pinit:
    mix_videoconfigparamsdec_unref(config_params);
    g_object_unref(G_OBJECT(mix_video));
    mix_displayx11_unref(mix_display_x11);
    mix_videoinitparams_unref(init_params);
    mix_params_unref(MIX_PARAMS(mix_drm));

    return OMX_ErrorInvalidState;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorDeinit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    mix_videoconfigparamsdec_unref(config_params);
    g_object_unref(G_OBJECT(mix_video));
    mix_displayx11_unref(mix_display_x11);
    mix_videoinitparams_unref(init_params);
    mix_params_unref(MIX_PARAMS(mix_drm));

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorStart(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorStop(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorPause(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

static void mix_buffer_callback(gulong token, guchar *data)
{
    if (token) {

    }
}

/* implement ComponentBase::ProcessorProcess */
void MrstPsbComponent::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retain,
    OMX_U32 nr_buffers)
{
    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;
    OMX_S64 delta_timestamp = 1;
    int i;

    MIX_RESULT mret;

    MixBuffer *mix_buffer_array[1];

    LOGV("%s(): enter\n", __func__);

//    DumpBuffer(buffers[INPORT_INDEX], false);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGE("%s(),%d: exit, input buffer's nFilledLen is zero (ret = void)\n",
             __func__, __LINE__);
        return;
    }

    mixio->data = buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset;
    LOGV("pBuffer=%p, nOffset=%d \n", buffers[INPORT_INDEX]->pBuffer, buffers[INPORT_INDEX]->nOffset);
    mixio->size = buffers[INPORT_INDEX]->nFilledLen;
    LOGV("FilledLen:%d \n", mixio->size);

#if 0
    LOGV("nFlags=0x%08x", buffers[INPORT_INDEX]->nFlags);
    LOGV(">");
    unsigned char xBuff[40];
    memcpy(xBuff, mixio->data, 40);
    for(i = 0; i < 40; i+=8) {
        LOGV("%02X %02X %02X %02X %02X %02X %02X %02X",
        xBuff[i], xBuff[i+1], xBuff[i+2], xBuff[i+3],
        xBuff[i+4], xBuff[i+5], xBuff[i+6], xBuff[i+7]);
    }
    LOGV("<");
#endif


    if ((buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) &&
        (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) {

        if(FrameCount == 0) {
            tBuff[0] = 0x00;
            tBuff[1] = 0x00;
            tBuff[2] = 0x00;
            tBuff[3] = 0x00;
            tBuff[4] = 0x03;
            tBuff[5] = 0x01;
            tBuff[6] = 0x00;
            tBuff[7] = 0x17;
            memcpy(tBuff+8, mixio->data, mixio->size);
        } 
        else if(FrameCount == 1) {
            tBuff[31] = 0x01;
            tBuff[32] = 0x00;
            tBuff[33] = 0x04;
            memcpy(tBuff+8+23+3, mixio->data, mixio->size);

            for(i = 0; i < 40; i+=8) {
                LOGV("%02X %02X %02X %02X %02X %02X %02X %02X",
                tBuff[i], tBuff[i+1], tBuff[i+2], tBuff[i+3],
                tBuff[i+4], tBuff[i+5], tBuff[i+6], tBuff[i+7]);
            }

            mixio->data = tBuff;
            mixio->size = 38;

            mret = mix_videoconfigparamsdec_set_header(config_params, mixio);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGE("%s(), %d: exit, mix_videoconfigparamsdec_set_header failed (ret == 0x%08x)",
                        __func__, __LINE__, mret);
                return;
            }
            mret = mix_video_configure(mix_video, (MixVideoConfigParams *)config_params, mix_drm);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGE("%s(), %d: exit, mix_video_configure failed (ret == 0x%08x)",
                        __func__, __LINE__, mret);
                return;
            }
        }
        FrameCount++;

	retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
        return;
    }

    tBuff[3] = buffers[INPORT_INDEX]->nFilledLen & 0xFF;
    tBuff[2] = (buffers[INPORT_INDEX]->nFilledLen >> 8) & 0xFF;
    tBuff[1] = (buffers[INPORT_INDEX]->nFilledLen >> 16) & 0xFF;
    tBuff[0] = (buffers[INPORT_INDEX]->nFilledLen >> 24) & 0xFF;
    memcpy(tBuff+4, mixio->data, mixio->size);
    mixio->data = tBuff;
    mixio->size = mixio->size+4;

    /* get MixBuffer */
    mret = mix_video_get_mixbuffer(mix_video, &mix_buffer);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_video_get_mixbuffer failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        return;
    }

    /* fill MixBuffer */
    mix_buffer->token = 0;
    mret = mix_buffer_set_data(mix_buffer, mixio->data, mixio->size, (gulong)0, mix_buffer_callback);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_buffer_set_data failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        return;
    }
    mix_buffer_array[0] = mix_buffer;

    /* set timestamp */
    mix_videodecodeparams_set_timestamp(mix_decode_params, outtimestamp);

    /* decode */
 retry_decode:
    mret = mix_video_decode(mix_video, mix_buffer_array, 1, NULL, 0, mix_decode_params);
    LOGV("%s(%d)\n", __func__, __LINE__);
    if (mret != MIX_RESULT_SUCCESS) {
        if (mret == MIX_RESULT_OUTOFSURFACES) {
            LOGV("%s() mix_video_decode() MIX_RESULT_OUTOFSURFACES", __func__);
            goto retry_decode;
        }
        else if (mret == MIX_RESULT_DROPFRAME) {
            LOGV("%s() mix_video_decode() Frame is dropped", __func__);
            return;
        }
        LOGE("%s(), %d: exit, mix_video_decode failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        return;
    }

 retry_getframe:
    /* ToDo - ready to send decoded frame to downstream */
    MixVideoFrame *mixvideoframe;
    mret = mix_video_get_frame(mix_video, &mixvideoframe);
    if (mret != MIX_RESULT_SUCCESS) {
        if (mret == MIX_RESULT_FRAME_NOTAVAIL) {
            LOGE("mix_video_get_frame() MIX_RESULT_FRAME_NOTAVAIL");
	} else if (mret == MIX_RESULT_EOS) {
            LOGE("mix_video_get_frame() MIX_RESULT_EOS");
	} else {
            LOGE("%s(), %d mix_video_get_frame() failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
	}
	goto end_getframe;
    }
    /* shall never happen */
    if (!mixvideoframe) {
        LOGE("mixvideoframe == NULL");
	return;
    }

#if 0
    mret = mix_video_eos(mix_video);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d mix_video_eos() failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        return;
    }
#endif


    MixIOVec *out_mixio;
    out_mixio = (MixIOVec *)malloc(sizeof(MixIOVec));
    if (!out_mixio) {
        LOGE("%s(),%d: exit, failed to allocate out_mixio", __func__, __LINE__);
        return;
    }
    //out_mixio->data = buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    out_mixio->data = buffers[OUTPORT_INDEX]->pBuffer + outfilledlen;

    mret = mix_video_get_decoded_data(mix_video, out_mixio, mix_video_render_params, mixvideoframe);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_video_get_decoded_data failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        return;
    }

    mret = mix_video_release_frame(mix_video, mixvideoframe);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d mix_video_release_frame() failed (ret == 0x%08x)",
                __func__, __LINE__, mret);
        return;
    }

//    /* set timestamp */
//    mix_videodecodeparams_set_timestamp(mix_decode_params, outtimestamp);

    outfilledlen += out_mixio->size;
    goto retry_getframe;

 end_getframe:
 

    /* release MixBuffer */
    if (mix_buffer) {
        mix_video_release_mixbuffer(mix_video, mix_buffer);
    }


    outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

    buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
    buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;

    FrameCount++;
    retain[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;

    LOGV("%s(),%d: exit (ret = void)\n", __func__, __LINE__);
}

/* end of implement ComponentBase::Processor[*] */

/* end of component methods & helpers */

/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB";

static const char *g_roles[] =
{
    (const char *)"video_decoder.avc",
};

OMX_ERRORTYPE wrs_omxil_cmodule_ops_instantiate(OMX_PTR *instance)
{
    ComponentBase *cbase;

    cbase = new MrstPsbComponent;
    if (!cbase) {
        *instance = NULL;
        return OMX_ErrorInsufficientResources;
    }

    *instance = cbase;
    return OMX_ErrorNone;
}

struct wrs_omxil_cmodule_ops_s cmodule_ops = {
    instantiate: wrs_omxil_cmodule_ops_instantiate,
};

struct wrs_omxil_cmodule_s WRS_OMXIL_CMODULE_SYMBOL = {
    name: g_name,
    roles: &g_roles[0],
    nr_roles: ARRAY_SIZE(g_roles),
    ops: &cmodule_ops,
};
