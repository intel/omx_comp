/*
 * psb.cpp, omx psb component file
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
//#define LOG_NDEBUG 0
#undef LOG_TAG
#define LOG_TAG "intel-m4v-encoder"
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#include <OMX_Core.h>
#include <OMX_IndexExt.h>
#include <OMX_VideoExt.h>
#include <OMX_IntelErrorTypes.h>

#include <cmodule.h>
#include <portvideo.h>
#include <componentbase.h>

#include <mixdisplayandroid.h>
#include <mixvideo.h>
#include <mixvideoconfigparamsenc_mpeg4.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <va/va.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#include <va/va_android.h>
#include "psb_m4v.h"

#define Display unsigned int
#define SHOW_FPS 0
#include "vabuffer.h"

#include <IntelBufferSharing.h>

#define MPEG4_ENCODE_ERROR_CHECKING(p) \
if (!p) { \
    LOGV("%s(), NULL pointer", __func__); \
    return OMX_ErrorBadParameter; \
} \
ret = CheckTypeHeader(p, sizeof(*p)); \
if (ret != OMX_ErrorNone) { \
    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret); \
    return ret; \
} \
OMX_U32 index = p->nPortIndex; \
if (index != OUTPORT_INDEX) { \
    LOGV("%s(), wrong port index", __func__); \
    return OMX_ErrorBadPortIndex; \
} \
PortMpeg4*port = static_cast<PortMpeg4 *> (ports[index]); \
if (!port) { \
    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, \
         OMX_ErrorBadPortIndex); \
    return OMX_ErrorBadPortIndex; \
} \
LOGV("%s(), about to get native or supported nal format", __func__); \
if (!port->IsEnabled()) { \
    LOGV("%s() : port is not enabled", __func__); \
    return OMX_ErrorNotReady; \
}\
 
/*
 * constructor & destructor
 */
MrstPsbComponent::MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    buffer_sharing_state = BUFFER_SHARING_INVALID;

    OMX_ERRORTYPE oret = RequestToEnableBufferSharingMode();

    if (oret != OMX_ErrorNone) {
        LOGE("%s(),%d:  set buffer sharing mode failed", __func__, __LINE__);
        RequestToDisableBufferSharingMode();
    }

    LOGV("%s(),%d: exit\n", __func__, __LINE__);
}

MrstPsbComponent::~MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    OMX_ERRORTYPE oret = RequestToDisableBufferSharingMode();
    if (oret != OMX_ErrorNone) {
        LOGE("%s(),%d:  DisableBufferSharingMode failed", __func__, __LINE__);
    }

    LOGV("%s(),%d: exit\n", __func__, __LINE__);
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

    raw_port_index = INPORT_INDEX;
    codec_port_index = OUTPORT_INDEX;
    raw_port_dir = OMX_DirInput;
    codec_port_dir = OMX_DirOutput;

    working_role = strpbrk(working_role, ".");
    if (!working_role)
        return OMX_ErrorUndefined;
    working_role++;

    this->mix = NULL;
    this->vip = NULL;
    this->mvp = NULL;
    this->vcp = NULL;
    this->display = NULL;
    this->mixbuffer_in[0] = NULL;

    ret = __AllocateMpeg4Port(codec_port_index, codec_port_dir);

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

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

free_codecport:
    delete ports[codec_port_index];
    ports[codec_port_index] = NULL;

free_ports:

    delete []ports;
    ports = NULL;

    this->ports = NULL;
    this->nr_ports = 0;

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__AllocateMpeg4Port(OMX_U32 port_index,
        OMX_DIRTYPE dir)
{
    PortMpeg4 *mpeg4port;

    OMX_PARAM_PORTDEFINITIONTYPE mpeg4portdefinition;
    OMX_VIDEO_PARAM_MPEG4TYPE mpeg4portparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortMpeg4;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }
    mpeg4port = static_cast<PortMpeg4 *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&mpeg4portdefinition, 0, sizeof(mpeg4portdefinition));
    SetTypeHeader(&mpeg4portdefinition, sizeof(mpeg4portdefinition));
    mpeg4portdefinition.nPortIndex = port_index;
    mpeg4portdefinition.eDir = dir;

    mpeg4portdefinition.nBufferCountActual =OUTPORT_MPEG4_ACTUAL_BUFFER_COUNT;
    mpeg4portdefinition.nBufferCountMin = OUTPORT_MPEG4_MIN_BUFFER_COUNT;
    mpeg4portdefinition.nBufferSize = OUTPORT_MPEG4_BUFFER_SIZE;

    mpeg4portdefinition.bEnabled = OMX_TRUE;
    mpeg4portdefinition.bPopulated = OMX_FALSE;
    mpeg4portdefinition.eDomain = OMX_PortDomainVideo;
    mpeg4portdefinition.format.video.cMIMEType = (OMX_STRING)"video/mpeg4";
    mpeg4portdefinition.format.video.pNativeRender = NULL;
    mpeg4portdefinition.format.video.nFrameWidth = 176;
    mpeg4portdefinition.format.video.nFrameHeight = 144;
    mpeg4portdefinition.format.video.nStride = 0;
    mpeg4portdefinition.format.video.nSliceHeight = 0;
    mpeg4portdefinition.format.video.nBitrate = 64000;
    mpeg4portdefinition.format.video.xFramerate = 15 << 16;
    mpeg4portdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    mpeg4portdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
    mpeg4portdefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    mpeg4portdefinition.format.video.pNativeWindow = NULL;
    mpeg4portdefinition.bBuffersContiguous = OMX_FALSE;
    mpeg4portdefinition.nBufferAlignment = 0;
    mpeg4port->SetPortDefinition(&mpeg4portdefinition, true);
    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_MPEG4TYPE */
    memset(&mpeg4portparam, 0, sizeof(mpeg4portparam));
    SetTypeHeader(&mpeg4portparam, sizeof(mpeg4portparam));
    mpeg4portparam.nPortIndex = port_index;
    mpeg4portparam.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    mpeg4portparam.eLevel = OMX_VIDEO_MPEG4Level5;

    mpeg4port->SetPortMpeg4Param(&mpeg4portparam, true);
    /* end of OMX_VIDEO_PARAM_MPEG4TYPE */

    /* encoder */
    /* OMX_VIDEO_PARAM_BITRATETYPE */
    OMX_VIDEO_PARAM_BITRATETYPE bitrateparam;
    memset(&bitrateparam, 0, sizeof(bitrateparam));
    SetTypeHeader(&bitrateparam, sizeof(bitrateparam));
    bitrateparam.nPortIndex = port_index;
    bitrateparam.eControlRate = OMX_Video_ControlRateConstant;
    bitrateparam.nTargetBitrate = 64000;
    mpeg4port->SetPortBitrateParam(&bitrateparam, true);
    /* end of OMX_VIDEO_PARAM_BITRATETYPE */

    /* OMX_VIDEO_CONFIG_PRI_INFOTYPE */
    OMX_VIDEO_CONFIG_PRI_INFOTYPE privateinfoparam;

    memset(&privateinfoparam, 0, sizeof(privateinfoparam));
    SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));

    privateinfoparam.nPortIndex = port_index;
    privateinfoparam.nCapacity = 0;
    privateinfoparam.nHolder = NULL;
    mpeg4port->SetPortPrivateInfoParam(&privateinfoparam, true);
    /* end of OMX_VIDEO_CONFIG_PRI_INFOTYPE */

    mpeg4EncPFrames = 0;
    mpeg4EncParamIntelBitrateType.nPortIndex = port_index;
    mpeg4EncParamIntelBitrateType.eControlRate = OMX_Video_Intel_ControlRateMax;
    mpeg4EncParamIntelBitrateType.nTargetBitrate = 0;
    SetTypeHeader(&mpeg4EncParamIntelBitrateType, sizeof(mpeg4EncParamIntelBitrateType));

    mpeg4EncConfigIntelBitrateType.nPortIndex = port_index;
    mpeg4EncConfigIntelBitrateType.nMaxEncodeBitrate = 4000 * 1024;    // Maximum bitrate
    mpeg4EncConfigIntelBitrateType.nTargetPercentage = 95;             // Target bitrate as percentage of maximum bitrate; e.g. 95 is 95%
    mpeg4EncConfigIntelBitrateType.nWindowSize = 1000;                 // Window size in milliseconds allowed for bitrate to reach target
    mpeg4EncConfigIntelBitrateType.nInitialQP  = 36;                   // Initial QP for I frames
    mpeg4EncConfigIntelBitrateType.nMinQP      = 18;
    SetTypeHeader(&mpeg4EncConfigIntelBitrateType, sizeof(mpeg4EncConfigIntelBitrateType));

    mpeg4EncConfigAir.nPortIndex = port_index;
    mpeg4EncConfigAir.bAirEnable = OMX_FALSE;
    mpeg4EncConfigAir.bAirAuto = OMX_FALSE;
    mpeg4EncConfigAir.nAirMBs = 0;
    mpeg4EncConfigAir.nAirThreshold = 0;
    SetTypeHeader(&mpeg4EncConfigAir, sizeof(mpeg4EncConfigAir));

    mpeg4EncFramerate.nPortIndex = port_index;
    mpeg4EncFramerate.xEncodeFramerate =  0; // Q16 format
    SetTypeHeader(&mpeg4EncFramerate, sizeof(mpeg4EncFramerate));



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
        LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    rawport = static_cast<PortVideo *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&rawportdefinition, 0, sizeof(rawportdefinition));
    SetTypeHeader(&rawportdefinition, sizeof(rawportdefinition));
    rawportdefinition.nPortIndex = port_index;
    rawportdefinition.eDir = dir;

    rawportdefinition.nBufferCountActual = INPORT_RAW_ACTUAL_BUFFER_COUNT;
    rawportdefinition.nBufferCountMin = INPORT_RAW_MIN_BUFFER_COUNT;
    rawportdefinition.nBufferSize = INPORT_RAW_BUFFER_SIZE;

    rawportdefinition.bEnabled = OMX_TRUE;
    rawportdefinition.bPopulated = OMX_FALSE;
    rawportdefinition.eDomain = OMX_PortDomainVideo;
    rawportdefinition.format.video.cMIMEType = (char *)"video/raw";
    rawportdefinition.format.video.pNativeRender = NULL;
    rawportdefinition.format.video.nFrameWidth = 176;
    rawportdefinition.format.video.nFrameHeight = 144;
    rawportdefinition.format.video.nStride = 176;
    rawportdefinition.format.video.nSliceHeight = 144;
    rawportdefinition.format.video.nBitrate = 64000;
    rawportdefinition.format.video.xFramerate = 15 << 16;
    rawportdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    rawportdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawportdefinition.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    rawportdefinition.format.video.pNativeWindow = NULL;
    rawportdefinition.bBuffersContiguous = OMX_FALSE;
    rawportdefinition.nBufferAlignment = 0;
    rawport->SetPortDefinition(&rawportdefinition, true);
    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_PORTFORMATTYPE */
    rawvideoparam.nPortIndex = port_index;
    rawvideoparam.nIndex = 0;
    rawvideoparam.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawvideoparam.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    rawport->SetPortVideoParam(&rawvideoparam, true);
    /* end of OMX_VIDEO_PARAM_PORTFORMATTYPE */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
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
        LOGV("%s(), OMX_IndexParamVideoPortFormat", __func__);
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if(index == INPORT_INDEX ) {
            if(p->nIndex < 1) {
                p->eColorFormat = port->GetPortVideoParam()->eColorFormat;
                p->eCompressionFormat = OMX_VIDEO_CodingUnused;
            }
            else {
                LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,OMX_ErrorNoMore);
                return OMX_ErrorNoMore;
            }
        }
        else if(index == OUTPORT_INDEX) {
            if(p->nIndex < 1) {
                p->eCompressionFormat= port->GetPortVideoParam()->eCompressionFormat;
                p->eColorFormat = OMX_COLOR_FormatUnused;
            }
            else {
                LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,OMX_ErrorNoMore);
                return OMX_ErrorNoMore;
            }
        }
        else {
            LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }


        LOGV("p->eColorFormat = %x\n", p->eColorFormat);

        break;
    }
    case OMX_IndexParamVideoMpeg4: {
        LOGV("%s(), OMX_IndexParamVideoMpeg4", __func__);
        OMX_VIDEO_PARAM_MPEG4TYPE *p =
            (OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortMpeg4 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMpeg4 *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortMpeg4Param(), sizeof(*p));
        break;
    }
    case OMX_IndexParamVideoBitrate: {
        LOGV("%s(), OMX_IndexParamVideoBitrate", __func__);
        OMX_VIDEO_PARAM_BITRATETYPE *p =
            (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortBitrateParam(), sizeof(*p));
        break;
    }
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        LOGV("%s(), OMX_IndexParamVideoProfileLevelQuerySupported", __func__);
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *p =
            (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        PortMpeg4 *port = NULL;

        OMX_U32 index = p->nPortIndex;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone)
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortMpeg4 *>(ports[index]);
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        const OMX_VIDEO_PARAM_MPEG4TYPE *mpeg4Param = port->GetPortMpeg4Param();

        p->eProfile = mpeg4Param->eProfile;
        p->eLevel  = mpeg4Param->eLevel;

        break;
    }

    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
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
        LOGV("%s(), OMX_IndexParamVideoPortFormat", __func__);
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                    state != OMX_StateWaitForResources) {
                LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortVideoParam(p, false);
        break;
    }
    case OMX_IndexParamVideoMpeg4: {
        LOGV("%s(), OMX_IndexParamVideoMpeg4", __func__);
        OMX_VIDEO_PARAM_MPEG4TYPE *p =
            (OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortMpeg4 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMpeg4 *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                    state != OMX_StateWaitForResources) {
                LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }
        ret = port->SetPortMpeg4Param(p, false);
        break;
    }
    case OMX_IndexParamVideoBitrate: {
        LOGV("%s(), OMX_IndexParamVideoBitrate", __func__);
        OMX_VIDEO_PARAM_BITRATETYPE *p =
            (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
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
        ret = port->SetPortBitrateParam(p, false);

        break;
    }
    case OMX_IndexParamVideoBytestream: {
        LOGV("%s(), OMX_IndexParamVideoBytestream", __func__);
        OMX_VIDEO_PARAM_BYTESTREAMTYPE *p =
            (OMX_VIDEO_PARAM_BYTESTREAMTYPE *) pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *> (ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;
            CBaseGetState((void *) GetComponentHandle(), &state);
            if (state != OMX_StateLoaded && state != OMX_StateWaitForResources) {
                LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }

        }
        break;
    }

    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* Get/SetConfig */
OMX_ERRORTYPE MrstPsbComponent::ComponentGetConfig(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    OMX_CONFIG_INTRAREFRESHVOPTYPE* pVideoIFrame;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s() : nIndex = %d\n", __func__, nIndex);

    switch (nIndex)
    {

    case OMX_IndexConfigIntelBitrate: {

        LOGV("%s() : OMX_IndexParamIntelBitrate", __func__);

        if (mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *) pComponentConfigStructure;

        MPEG4_ENCODE_ERROR_CHECKING(pIntelBitrate)

        *pIntelBitrate = mpeg4EncConfigIntelBitrateType;

        break;
    }
    case OMX_IndexConfigIntelAIR: {

        LOGV("%s() : OMX_IndexConfigIntelAIR", __func__);

        if (mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_AIR *pIntelAir =
            (OMX_VIDEO_CONFIG_INTEL_AIR *) pComponentConfigStructure;

        MPEG4_ENCODE_ERROR_CHECKING(pIntelAir)

        *pIntelAir = mpeg4EncConfigAir;
        break;
    }
    case OMX_IndexConfigVideoFramerate: {

        LOGV("%s() : OMX_IndexConfigVideoFramerate", __func__);

        if (mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_CONFIG_FRAMERATETYPE *pxFramerate =
            (OMX_CONFIG_FRAMERATETYPE *) pComponentConfigStructure;

        MPEG4_ENCODE_ERROR_CHECKING(pxFramerate)

        *pxFramerate = mpeg4EncFramerate;
        break;
    }
    case OMX_IndexIntelPrivateInfo: {
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentConfigStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortPrivateInfoParam(), sizeof(*p));
        break;
    }
    default:
    {
        return OMX_ErrorUnsupportedIndex;
    }
    }
    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::ComponentSetConfig(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    OMX_CONFIG_INTRAREFRESHVOPTYPE* pVideoIFrame;
    MIX_RESULT mret;
    LOGV("%s(): enter\n", __func__);

    switch (nParamIndex)
    {
    case OMX_IndexConfigVideoIntraVOPRefresh:
    {
        LOGV("%s(), OMX_IndexConfigVideoIntraVOPRefresh", __func__);

        pVideoIFrame = (OMX_CONFIG_INTRAREFRESHVOPTYPE*) pComponentConfigStructure;

        MPEG4_ENCODE_ERROR_CHECKING(pVideoIFrame)

        LOGV("%s(), OMX_IndexConfigVideoIntraVOPRefresh", __func__);
        if(pVideoIFrame->IntraRefreshVOP == OMX_TRUE) {
            LOGV("%s(), pVideoIFrame->IntraRefreshVOP == OMX_TRUE", __func__);

            MixEncDynamicParams encdynareq;
            memset(&encdynareq, 0, sizeof(encdynareq));
            encdynareq.force_idr = TRUE;
            if(mix) {
                mret = mix_video_set_dynamic_enc_config (mix,
                        MIX_ENC_PARAMS_FORCE_KEY_FRAME, &encdynareq);
                if(mret != MIX_RESULT_SUCCESS) {
                    LOGW("%s(), failed to set IDR interval", __func__);
                }
            }
        }
        break;
    }

    case OMX_IndexConfigIntelBitrate: {

        LOGV("%s(), OMX_IndexConfigIntelBitrate", __func__);
        if (mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            LOGV("%s(), eControlRate == OMX_Video_Intel_ControlRateMax");
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *) pComponentConfigStructure;

        MPEG4_ENCODE_ERROR_CHECKING(pIntelBitrate);

        mpeg4EncConfigIntelBitrateType = *pIntelBitrate;

        if (mix && mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            LOGV("%s(), mpeg44EncConfigIntelBitrateType.nInitialQP = %d", __func__,
                 mpeg4EncConfigIntelBitrateType.nInitialQP);

            LOGV("%s(), mpeg4EncConfigIntelBitrateType.nMinQP = %d", __func__,
                 mpeg4EncConfigIntelBitrateType.nMinQP);

            LOGV("%s(), mpeg4EncConfigIntelBitrateType.nMaxEncodeBitrate = %d", __func__,
                 mpeg4EncConfigIntelBitrateType.nMaxEncodeBitrate);

            LOGV("%s(), mpeg4EncConfigIntelBitrateType.nTargetPercentage = %d", __func__,
                 mpeg4EncConfigIntelBitrateType.nTargetPercentage);

            LOGV("%s(), mpeg4EncConfigIntelBitrateType.nWindowSize = %d", __func__,
                 mpeg4EncConfigIntelBitrateType.nWindowSize);

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            memset(&dynamic_params, 0, sizeof(dynamic_params));

            params_type = MIX_ENC_PARAMS_INIT_QP;
            dynamic_params.init_QP = mpeg4EncConfigIntelBitrateType.nInitialQP;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_MIN_QP;
            dynamic_params.min_QP = mpeg4EncConfigIntelBitrateType.nMinQP;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_BITRATE;
            dynamic_params.bitrate
            = mpeg4EncConfigIntelBitrateType.nMaxEncodeBitrate;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_TARGET_PERCENTAGE;
            dynamic_params.target_percentage
            = mpeg4EncConfigIntelBitrateType.nTargetPercentage;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_WINDOW_SIZE;
            dynamic_params.window_size
            = mpeg4EncConfigIntelBitrateType.nWindowSize;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }
        }
        break;
    }
    case OMX_IndexConfigIntelAIR: {
        LOGV("%s(), OMX_IndexConfigIntelAIR", __func__);
        if (mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_AIR *pIntelAir =
            (OMX_VIDEO_CONFIG_INTEL_AIR *) pComponentConfigStructure;

        MPEG4_ENCODE_ERROR_CHECKING(pIntelAir);

        mpeg4EncConfigAir = *pIntelAir;

        if (mix && mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            memset(&dynamic_params, 0, sizeof(dynamic_params));

            if(pIntelAir->bAirEnable) {

                params_type = MIX_ENC_PARAMS_REFRESH_TYPE;
                dynamic_params.refresh_type = MIX_VIDEO_AIR;
                mret = mix_video_set_dynamic_enc_config (mix, params_type, &dynamic_params);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
                }

                params_type = MIX_ENC_PARAMS_AIR;
                dynamic_params.air_params.air_auto = pIntelAir->bAirAuto;
                dynamic_params.air_params.air_MBs  = pIntelAir->nAirMBs;
                dynamic_params.air_params.air_threshold = pIntelAir->nAirThreshold;
                mret = mix_video_set_dynamic_enc_config (mix, params_type, &dynamic_params);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
                }

            } else {

                params_type = MIX_ENC_PARAMS_REFRESH_TYPE;
                dynamic_params.refresh_type = MIX_VIDEO_NONIR;
                mret = mix_video_set_dynamic_enc_config (mix, params_type, &dynamic_params);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
                }
            }
        }
        break;
    }

    case OMX_IndexConfigVideoFramerate: {

        LOGV("%s(), OMX_IndexConfigVideoFramerate", __func__);
        if (mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_CONFIG_FRAMERATETYPE *pxFramerate =
            (OMX_CONFIG_FRAMERATETYPE *) pComponentConfigStructure;

        MPEG4_ENCODE_ERROR_CHECKING(pxFramerate);

        mpeg4EncFramerate = *pxFramerate;

        if (mix && mpeg4EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            memset(&dynamic_params, 0, sizeof(dynamic_params));

            params_type = MIX_ENC_PARAMS_FRAME_RATE;
            dynamic_params.frame_rate_denom = 1;
            dynamic_params.frame_rate_num   = mpeg4EncFramerate.xEncodeFramerate >> 16;  // Q16 format
            mret = mix_video_set_dynamic_enc_config (mix, params_type, &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }
        }
        break;
    }

    default:
    {
        return OMX_ErrorUnsupportedIndex;
    }
    }
    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return OMX_ErrorNone;
}
/* end of component methods & helpers */

/*
 * implement ComponentBase::Processor[*]
 */
OMX_ERRORTYPE MrstPsbComponent::ProcessorInit(void)
{
    MixVideo *mix = NULL;
    MixVideoInitParams *vip = NULL;
    MixParams *mvp = NULL;
    MixVideoConfigParams *vcp = NULL;
    MixDisplayAndroid *display = NULL;
    OMX_U32 port_index = (OMX_U32)-1;
    uint major, minor;
    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;
    BufferShareStatus bsret;
    temp_coded_data_buffer = NULL;

    LOGV("%s(): enter\n", __func__);
    mix = mix_video_new();
    LOGV("%s(): called to mix_video_new()", __func__);

    if (!mix) {
        LOGE("%s(),%d: exit, mix_video_new failed", __func__, __LINE__);
        goto error_out;
    }

    mix_video_get_version(mix, &major, &minor);
    LOGV("MixVideo version: %d.%d", major, minor);

    /* encoder */
    vcp =MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsenc_mpeg4_new());
    mvp = MIX_PARAMS(mix_videoencodeparams_new());
    port_index = OUTPORT_INDEX;

    if (!vcp || !mvp || (port_index == (OMX_U32)-1)) {
        LOGE("%s(),%d: exit, failed to allocate vcp, mvp, port_index\n",
             __func__, __LINE__);
        goto error_out;
    }

    oret = ChangeVcpWithPortParam(vcp,
                                  static_cast<PortVideo *>(ports[port_index]),
                                  NULL);
    if (oret != OMX_ErrorNone) {
        LOGE("%s(),%d: exit, ChangeVcpWithPortParam failed (ret == 0x%08x)\n",
             __func__, __LINE__, oret);
        goto error_out;
    }

    display = mix_displayandroid_new();
    if (!display) {
        LOGE("%s(),%d: exit, mix_displayandroid_new failed", __func__, __LINE__);
        goto error_out;
    }

    vip = mix_videoinitparams_new();
    if (!vip) {
        LOGE("%s(),%d: exit, mix_videoinitparams_new failed", __func__,
             __LINE__);
        goto error_out;
    }

    {
        Display *android_display = (Display*)malloc(sizeof(Display));
        *(android_display) = 0x18c34078;

        LOGV("*android_display = %d", *android_display);

        mret = mix_displayandroid_set_display(display, android_display);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGV("%s(),%d: exit, mix_displayandroid_set_display failed "
                 "(ret == 0x%08x)", __func__, __LINE__, mret);
            goto error_out;
        }
    }

    mret = mix_videoinitparams_set_display(vip, MIX_DISPLAY(display));
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videoinitparams_set_display failed "
             "(ret == 0x%08x)", __func__, __LINE__, mret);
        goto error_out;
    }

    mret = mix_video_initialize(mix, MIX_CODEC_MODE_ENCODE, vip, NULL);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_video_initialize failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto error_out;
    }

    oret = CheckAndEnableBufferSharingMode();
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d: CheckAndEnableBufferSharingMode() failed ", __func__, __LINE__);
        goto error_out;
    }

    oret = RequestShareBuffers(mix,
                               MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_width,
                               MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_height);
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d: RequestShareBuffers() failed ", __func__, __LINE__);
        goto error_out;
    }

    mix_videoconfigparamsenc_set_share_buf_mode(MIX_VIDEOCONFIGPARAMSENC(vcp), FALSE);

    LOGV("mix_video_configure");
    mret = mix_video_configure(mix, vcp, NULL);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGV("%s(), %d: exit, mix_video_configure failed "
             "(ret:0x%08x)", __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        if(mret == MIX_RESULT_NO_MEMORY) {
            oret = OMX_ErrorInsufficientResources;
        } else if(mret == MIX_RESULT_NOT_PERMITTED) {
            oret = (OMX_ERRORTYPE)OMX_ErrorIntelVideoNotPermitted;;
        }
        goto error_out;
    }

    LOGV("%s(): mix video configured", __func__);

    this->mix = mix;
    this->vip = vip;
    this->mvp = mvp;
    this->vcp = vcp;
    this->display = display;
    this->mixbuffer_in[0] = NULL;

    inframe_counter = 0;
    outframe_counter = 0;
    is_mixvideodec_configured = OMX_FALSE;

    last_ts = 0;
    last_fps = 0.0;

    video_data = NULL;
    video_len = 0;

    temp_coded_data_buffer_size = MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_width *
                                  MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_height *
                                  400 / 16 / 16;	//FIXME: same with h264?
    temp_coded_data_buffer = new OMX_U8 [temp_coded_data_buffer_size];

    b_config_sent = false;

    oret = RegisterShareBuffersToPort();
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d RegisterShareBufferToPort() failed", __func__, __LINE__);
        oret = OMX_ErrorUndefined;
        goto error_out;
    }

    oret = RegisterShareBuffersToLib();
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d register Share Buffering Mode  failed", __func__, __LINE__);
        goto error_out;
    }

    oret = EnterBufferSharingMode();
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d EnterShareBufferingMode() failed", __func__, __LINE__);
        goto error_out;
    }

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, oret);
    return oret;

error_out:
    if (buffer_sharing_state == BUFFER_SHARING_EXECUTING) {
        ExitBufferSharingMode();
    }
    mix_params_unref(mvp);
    mix_videoconfigparams_unref(vcp);
    mix_displayandroid_unref(display);
    mix_videoinitparams_unref(vip);
    mix_video_unref(mix);


    if (temp_coded_data_buffer != NULL) {
        delete [] temp_coded_data_buffer;
        temp_coded_data_buffer = NULL;
    }

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorDeinit(void)
{
    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);
    if (buffer_sharing_state == BUFFER_SHARING_EXECUTING) {
        oret = ExitBufferSharingMode();
        if (oret != OMX_ErrorNone) {
            LOGE("%s(),%d:    ExitShareBufferingMode failed", __func__, __LINE__);
        }
    }

    mix_video_eos(mix);
    mix_video_flush(mix);

    mix_params_unref(mvp);
    mix_videoconfigparams_unref(vcp);
    mix_displayandroid_unref(display);
    mix_videoinitparams_unref(vip);

    if (mixbuffer_in[0]) {
        mix_video_release_mixbuffer(mix, mixbuffer_in[0]);
        mixbuffer_in[0] = NULL;
    }

    mix_video_deinitialize(mix);
    mix_video_unref(mix);

    //delete temp coded buffer
    if (temp_coded_data_buffer != NULL) {
        delete [] temp_coded_data_buffer;
        temp_coded_data_buffer = NULL;
    }

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, oret);
    return oret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorStart(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorStop(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    ports[INPORT_INDEX]->ReturnAllRetainedBuffers();

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorPause(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}
bool MrstPsbComponent::DetectSyncFrame(OMX_U8* vop)
{
    M4vVopType voptype=  static_cast<M4vVopType>(vop[4] & M4V_VOP_TYPE_MASK);
    return ( voptype == I_FRAME );
}
OMX_ERRORTYPE MrstPsbComponent::ExtractConfigData(OMX_U8* coded_buf, OMX_U32 coded_len,
        OMX_U8** config_buf, OMX_U32* config_len,
        OMX_U8** video_buf, OMX_U32* video_len)
{

    if(coded_buf == NULL || coded_len < 4) {
        LOGE("%s,failed Wrong data ",__func__);
        return OMX_ErrorUndefined; // we won't proceed this not vop
    }
    OMX_U32 count = 0;
    OMX_U32 i = 0;
    OMX_U8* buf_in = coded_buf;
    *config_buf = NULL;
    *config_len = 0;
    *video_buf = NULL;
    *video_len = 0;

    while (i < coded_len)
    {
        if ((count > 1) && (buf_in[0] == 0x01) && (buf_in[1] == 0xB6)) //vop start code
        {
            i += 2;
            break;
        }

        if (*buf_in++)
            count = 0;
        else
            count++;
        i++;
    }
    if( i == coded_len) {
        LOGE("%s failed didn't find vop start code",__func__);
        return OMX_ErrorUndefined; //didn't find vop start code
    }
    //we need VOL header as codec config data not including 00 00 01 B6
    *config_buf = coded_buf;
    *config_len = i - 4;

    if(*config_len == 0) {
        *video_len = coded_len;
        *video_buf = coded_buf;
    }
    else {
        //retain the video buf
        *video_len = coded_len - *config_len;
        if(*video_len > temp_coded_data_buffer_size) {
            LOGE("temp_coded_data_buffer_size is too small %d",__LINE__);
            return OMX_ErrorUndefined;
        }
        memcpy(temp_coded_data_buffer,coded_buf+*config_len,*video_len);
        *video_buf =  temp_coded_data_buffer;
    }
    return OMX_ErrorNone;
}

/* implement ComponentBase::ProcessorProcess */
OMX_ERRORTYPE MrstPsbComponent::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retain,
    OMX_U32 nr_buffers)
{
    MixIOVec buffer_in, buffer_out;
    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;
    OMX_U32 outflags = 0;
    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    OMX_U8* config_data = NULL;
    OMX_U32 config_len = 0;

    LOGV("%s(): enter encode\n", __func__);

    LOGV_IF(buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS,
            "%s(),%d: got OMX_BUFFERFLAG_EOS\n", __func__, __LINE__);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGV("%s(),%d: input buffer's nFilledLen is zero\n",
             __func__, __LINE__);
        goto out;
    }

    if (buffer_sharing_state != BUFFER_SHARING_INVALID) {
        buffer_in.data_size = buffer_sharing_info[0].dataSize;
        buffer_in.buffer_size = buffer_sharing_info[0].allocatedSize;
        buffer_in.data =
            *(reinterpret_cast<uchar**>(buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset));
    } else {
        buffer_in.data =
            buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset;
        buffer_in.data_size = buffers[INPORT_INDEX]->nFilledLen;
        buffer_in.buffer_size = buffers[INPORT_INDEX]->nFilledLen;
    }

    LOGV("buffer_in.data=%x, data_size=%d, buffer_size=%d",
         (unsigned)buffer_in.data, buffer_in.data_size, buffer_in.buffer_size);

    buffer_out.data =
        buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    buffer_out.data_size = 0;
    buffer_out.buffer_size = buffers[OUTPORT_INDEX]->nAllocLen - buffers[OUTPORT_INDEX]->nOffset;
    mixiovec_out[0] = &buffer_out;

nomal_start:

    /* get MixBuffer */
    mret = mix_video_get_mixbuffer(mix, &mixbuffer_in[0]);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_video_get_mixbuffer failed (ret:0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    /* fill MixBuffer */
    mret = mix_buffer_set_data(mixbuffer_in[0],
                               buffer_in.data, buffer_in.data_size,
                               (ulong)this, M4vEncMixBufferCallback);

    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_buffer_set_data failed (ret:0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    if (video_len == 0) {

        LOGV("begin to call mix_video_encode()");
        mret = mix_video_encode(mix, mixbuffer_in, 1, mixiovec_out, 1,
                                MIX_VIDEOENCODEPARAMS(mvp));

        LOGV("%s(), mret = 0x%08x", __func__, mret);
        LOGV("output data size = %d", mixiovec_out[0]->data_size);

        outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d: exit, mix_video_encode failed (ret == 0x%08x)\n",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto out;
        }

        if (mixiovec_out[0]-> data_size== 0) {
            retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;
            goto out;
        }

        oret = ExtractConfigData(mixiovec_out[0]->data,mixiovec_out[0]->data_size,&config_data, &config_len,&video_data, &video_len);
        if(OMX_ErrorNone != oret)
        {
            LOGE("%s(), %d: exit, ExtractConfigData() failed (ret == 0x%08x)\n",__func__, __LINE__, oret);
            goto out;
        }
    }

    if(config_len != 0) {
        //config data needs to be sent completely in a ProcessorProcess() cycle
        if(!b_config_sent) {
            outfilledlen = config_len;
            outflags |= OMX_BUFFERFLAG_CODECCONFIG;
            b_config_sent = true;
            if(buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset != config_data) {
                memcpy(buffers[OUTPORT_INDEX]->pBuffer+buffers[OUTPORT_INDEX]->nOffset,config_data,config_len);
            }
        }
        else {
            //stagefright only accept the first set of config data, hence discard following ones
            outfilledlen = 0;
        }

        outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
    }
    else {
        outfilledlen = video_len;
        if(DetectSyncFrame(video_data)) {
            outflags |= OMX_BUFFERFLAG_SYNCFRAME;
        }
        if(buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset != video_data) {
            memcpy(buffers[OUTPORT_INDEX]->pBuffer+buffers[OUTPORT_INDEX]->nOffset,video_data,video_len);
        }
        video_data = NULL;
        video_len = 0;

        outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
    }

    if( outfilledlen > 0 ) {
        retain[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
    }
    else {
        retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
    }

    if (video_len == 0) {
        retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;  //release by callback
    } else {
        retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;  //get again
    }

#if SHOW_FPS
    {
        struct timeval t;
        OMX_TICKS current_ts, interval_ts;
        float current_fps, average_fps;

        t.tv_sec = t.tv_usec = 0;
        gettimeofday(&t, NULL);

        current_ts =
            (nsecs_t)t.tv_sec * 1000000000 + (nsecs_t)t.tv_usec * 1000;
        interval_ts = current_ts - last_ts;
        last_ts = current_ts;

        current_fps = (float)1000000000 / (float)interval_ts;
        average_fps = (current_fps + last_fps) / 2;
        last_fps = current_fps;

        LOGV("FPS = %2.1f\n", average_fps);
    }
#endif

out:
    if (mixbuffer_in[0]) {
        mix_video_release_mixbuffer(mix, mixbuffer_in[0]);
        mixbuffer_in[0] = NULL;
    }

    if(retain[OUTPORT_INDEX] != BUFFER_RETAIN_GETAGAIN) {
        buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
        buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;
        buffers[OUTPORT_INDEX]->nFlags = outflags;
    }

    if (retain[INPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN ||
            retain[INPORT_INDEX] == BUFFER_RETAIN_ACCUMULATE ) {
        inframe_counter++;
    }

    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
        outframe_counter++;

    LOGV_IF(oret == OMX_ErrorNone,
            "%s(),%d: exit, encode is done\n", __func__, __LINE__);

    return oret;
}

OMX_ERRORTYPE MrstPsbComponent::__Mpeg4ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortMpeg4 *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    MixVideoConfigParamsEnc *config = MIX_VIDEOCONFIGPARAMSENC(vcp);
    mix_videoconfigparamsenc_set_encode_format(config, MIX_ENCODE_TARGET_FORMAT_MPEG4);
    mix_videoconfigparamsenc_set_profile(config, MIX_PROFILE_MPEG4SIMPLE);
    mix_videoconfigparamsenc_set_mime_type(config, "video/mpeg");
    mix_videoconfigparamsenc_mpeg4_set_dlk(MIX_VIDEOCONFIGPARAMSENC_MPEG4(config), FALSE);
    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp,
    PortVideo *port,
    bool *vcp_changed)
{
    const OMX_PARAM_PORTDEFINITIONTYPE *pd = port->GetPortDefinition();
    OMX_ERRORTYPE ret;

    ret = __Mpeg4ChangeVcpWithPortParam(vcp,static_cast<PortMpeg4 *>(port), vcp_changed);

    /* encoder */
    MixVideoConfigParamsEnc *config = MIX_VIDEOCONFIGPARAMSENC(vcp);
    const OMX_VIDEO_PARAM_BITRATETYPE *bitrate =
        port->GetPortBitrateParam();
    OMX_VIDEO_CONTROLRATETYPE controlrate;

    if ((config->picture_width != pd->format.video.nFrameWidth) ||
            (config->picture_height != pd->format.video.nFrameHeight)) {
        LOGV("%s(): width : %d != %ld", __func__,
             config->picture_width, pd->format.video.nFrameWidth);
        LOGV("%s(): height : %d != %ld", __func__,
             config->picture_height, pd->format.video.nFrameHeight);

        mix_videoconfigparamsenc_set_picture_res(config,
                pd->format.video.nFrameWidth,
                pd->format.video.nFrameHeight);
        if (vcp_changed)
            *vcp_changed = true;
    }

    PortVideo *input_port = static_cast<PortVideo*>(ports[INPORT_INDEX]);
    const OMX_PARAM_PORTDEFINITIONTYPE *input_pd = input_port->GetPortDefinition();

    if (config->frame_rate_num != (input_pd->format.video.xFramerate >> 16)) {
        LOGV("%s(): framerate : %u != %ld", __func__,
             config->frame_rate_num, input_pd->format.video.xFramerate >> 16);

        mix_videoconfigparamsenc_set_frame_rate(config,
                                                input_pd->format.video.xFramerate >> 16,
                                                1);

        if (vcp_changed)
            *vcp_changed = true;
    }

    if(mpeg4EncPFrames == 0 && mpeg4EncParamIntelBitrateType.eControlRate
            == OMX_Video_Intel_ControlRateMax) {
        mpeg4EncPFrames = config->frame_rate_num / 2;
    }

    LOGV("%s() : mpeg4EncPFrames = %d", __func__, mpeg4EncPFrames);
    mix_videoconfigparamsenc_set_intra_period(config, mpeg4EncPFrames);

    if (mpeg4EncParamIntelBitrateType.eControlRate
            == OMX_Video_Intel_ControlRateMax) {

        LOGV("%s(), eControlRate == OMX_Video_Intel_ControlRateMax", __func__);

        if (config->bitrate != bitrate->nTargetBitrate) {
            LOGV("%s(): bitrate : %d != %ld", __func__,
                 config->bitrate, bitrate->nTargetBitrate);

            mix_videoconfigparamsenc_set_bit_rate(config,
                                                  bitrate->nTargetBitrate);

            if (vcp_changed)
                *vcp_changed = true;
        }

        if (config->rate_control == MIX_RATE_CONTROL_CBR)
            controlrate = OMX_Video_ControlRateConstant;
        else if (config->rate_control == MIX_RATE_CONTROL_VBR)
            controlrate = OMX_Video_ControlRateVariable;
        else
            controlrate = OMX_Video_ControlRateDisable;

        if (controlrate != bitrate->eControlRate) {
            LOGV("%s(): ratecontrol : %d != %d", __func__,
                 controlrate, bitrate->eControlRate);

            if ((bitrate->eControlRate == OMX_Video_ControlRateVariable) ||
                    (bitrate->eControlRate ==
                     OMX_Video_ControlRateVariableSkipFrames))
                config->rate_control = MIX_RATE_CONTROL_VBR;
            else if ((bitrate->eControlRate ==
                      OMX_Video_ControlRateConstant) ||
                     (bitrate->eControlRate ==
                      OMX_Video_ControlRateConstantSkipFrames))
                config->rate_control = MIX_RATE_CONTROL_CBR;
            else
                config->rate_control = MIX_RATE_CONTROL_NONE;

            if (vcp_changed)
                *vcp_changed = true;
        }

        /* hard coding */
        mix_videoconfigparamsenc_set_raw_format(config,
                                                MIX_RAW_TARGET_FORMAT_NV12);
        mix_videoconfigparamsenc_set_init_qp(config, 6);
        mix_videoconfigparamsenc_set_min_qp(config, 1);
        mix_videoconfigparamsenc_set_buffer_pool_size(config, 8);
        mix_videoconfigparamsenc_set_drawable(config, 0x0);
        mix_videoconfigparamsenc_set_need_display(config, FALSE);
    }

    return ret;
}
/* end of vcp setting helpers */

/* share buffer setting */
OMX_ERRORTYPE MrstPsbComponent::RequestToEnableBufferSharingMode()
{
    BufferShareStatus bsret;

    if (buffer_sharing_state != BUFFER_SHARING_INVALID) {
        LOGE("%s(),%d: invoke %s failed (incorrect state).", __func__, __LINE__, __func__);
        return OMX_ErrorUndefined;
    }

    buffer_sharing_count = 4;
    buffer_sharing_info = NULL;
    buffer_sharing_lib = BufferShareRegistry::getInstance();

    bsret = buffer_sharing_lib->encoderRequestToEnableSharingMode();
    if (bsret != BS_SUCCESS) {
        LOGE("%s(),%d: encoder request to enable buffer sharing mode failed:%d", __func__, __LINE__, bsret);
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::CheckAndEnableBufferSharingMode()
{
    BufferShareStatus bsret;

    if (buffer_sharing_state != BUFFER_SHARING_INVALID) {
        LOGE("%s(),%d: invoke %s failed (incorrect state).", __func__, __LINE__, __func__);
        return OMX_ErrorUndefined;
    }

    if (buffer_sharing_lib->isBufferSharingModeEnabled()) {
        LOGW("Buffer sharing is enabled (video source does support)");
        buffer_sharing_state = BUFFER_SHARING_LOADED;
    } else {
        LOGW("Buffer sharing is disabled (video source doesn't support)");
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::RequestToDisableBufferSharingMode()
{
    BufferShareStatus bsret;

    if ((buffer_sharing_state != BUFFER_SHARING_INVALID) &&
            (buffer_sharing_state != BUFFER_SHARING_LOADED)) {
        LOGE("%s(),%d: invoke %s failed (incorrect state).", __func__, __LINE__, __func__);
        return OMX_ErrorUndefined;
    }

    if (buffer_sharing_state == BUFFER_SHARING_INVALID) {
        LOGW("%s(),%d: buffer sharing already in invalid state.", __func__, __LINE__);
        return OMX_ErrorNone;
    }

    if (buffer_sharing_info) {
        delete [] buffer_sharing_info;
    }
    buffer_sharing_info = NULL;

    bsret = buffer_sharing_lib->encoderRequestToDisableSharingMode();
    if (bsret != BS_SUCCESS) {
        LOGE("%s(),%d: request to disable sharing mode failed.", __func__, __LINE__);
        return OMX_ErrorUndefined;
    }

    buffer_sharing_state = BUFFER_SHARING_INVALID;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::RequestShareBuffers(MixVideo* mix, int width, int height)
{
    int i;
    int buf_width, buf_height, buf_size;
    MIX_RESULT mret;
    bool is_request_ok = true;

    if (width <= 0 || height <= 0) {
        LOGE("%s(),%d:   width and height incorrect", __func__, __LINE__);
        return OMX_ErrorUndefined;
    }

    if ((buffer_sharing_state != BUFFER_SHARING_INVALID) &&
            (buffer_sharing_state != BUFFER_SHARING_LOADED)) {
        LOGE("%s(),%d: invoke %s failed (incorrect state:%d).", __func__, __LINE__, __func__, buffer_sharing_state);
        return OMX_ErrorUndefined;
    }

    if (buffer_sharing_state == BUFFER_SHARING_INVALID) {
        LOGW("%s(),%d: buffer sharing not enabled, do nothing.", __func__, __LINE__);
        return OMX_ErrorNone;
    }

    if (buffer_sharing_info != NULL) {
        delete [] buffer_sharing_info;
        buffer_sharing_info = NULL;
    }

    buffer_sharing_info = new SharedBufferType[buffer_sharing_count];
    //query mix for share buffer info.
    for (i = 0; i < buffer_sharing_count; i++) {
        buf_width = width;
        buf_height = height;
        buf_size = SHARE_PTR_ALIGN(buf_width) * buf_height * 3 / 2;
        mret = mix_video_get_new_userptr_for_surface_buffer(mix,
                (uint)buf_width,
                (uint)buf_height,
                MIX_STRING_TO_FOURCC("NV12"),
                (uint)buf_size,
                (uint*)&buffer_sharing_info[i].allocatedSize,
                (uint*)&buffer_sharing_info[i].stride,
                (uint8**)&buffer_sharing_info[i].pointer);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(),%d:  mix_video_get_new_userptr_for_surface_buffer failed", __func__, __LINE__);
            is_request_ok = false;
            break;
        }
        buffer_sharing_info[i].width = buf_width;
        buffer_sharing_info[i].height = buf_height;
        buffer_sharing_info[i].dataSize = buffer_sharing_info[i].stride * buf_height * 3 / 2;

        LOGD("width:%d, Height:%d, stride:%d, pointer:%p", buffer_sharing_info[i].width,
             buffer_sharing_info[i].height, buffer_sharing_info[i].stride,
             buffer_sharing_info[i].pointer);
    }

    if (!is_request_ok) {
        delete []buffer_sharing_info;
        buffer_sharing_info = NULL;
        LOGE("%s(),%d: %s  failed", __func__, __LINE__,"get usr ptr for surface buffer");
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::RegisterShareBuffersToLib()
{
    if ((buffer_sharing_state != BUFFER_SHARING_INVALID) &&
            (buffer_sharing_state != BUFFER_SHARING_LOADED)) {
        LOGE("%s(),%d: invoke %s failed (incorrect state).", __func__, __LINE__, __func__);
        return OMX_ErrorUndefined;
    }

    if (buffer_sharing_state == BUFFER_SHARING_INVALID) {
        LOGW("%s(),%d: buffer sharing not enabled, do nothing.", __func__, __LINE__);
        return OMX_ErrorNone;
    }

    BufferShareStatus bsret = buffer_sharing_lib->encoderSetSharedBuffer(buffer_sharing_info, buffer_sharing_count);
    if (bsret != BS_SUCCESS) {
        LOGE("%s(),%d:    encoder set shared buffer failed", __func__, __LINE__);
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::RegisterShareBuffersToPort()
{
    if ((buffer_sharing_state != BUFFER_SHARING_INVALID) &&
            (buffer_sharing_state != BUFFER_SHARING_LOADED)) {
        LOGE("%s(),%d: invoke %s failed (incorrect state).", __func__, __LINE__, __func__);
        return OMX_ErrorUndefined;
    }

    OMX_VIDEO_CONFIG_PRI_INFOTYPE privateinfoparam;
    memset(&privateinfoparam, 0, sizeof(privateinfoparam));
    SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));

    //caution: buffer-sharing info stored in INPORT (raw port)
    privateinfoparam.nPortIndex = INPORT_INDEX;
    if (buffer_sharing_state == BUFFER_SHARING_INVALID) {
        privateinfoparam.nCapacity = 0;
        privateinfoparam.nHolder = NULL;
    } else {
        privateinfoparam.nCapacity = buffer_sharing_count;
        privateinfoparam.nHolder = buffer_sharing_info;
    }
    OMX_ERRORTYPE ret = static_cast<PortVideo*>(ports[privateinfoparam.nPortIndex])->SetPortPrivateInfoParam(&privateinfoparam, false);

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::EnterBufferSharingMode()
{
    if ((buffer_sharing_state != BUFFER_SHARING_INVALID) &&
            (buffer_sharing_state != BUFFER_SHARING_LOADED)) {
        LOGE("%s(),%d: invoke %s failed (incorrect state).", __func__, __LINE__, __func__);
        return OMX_ErrorUndefined;
    }

    if (buffer_sharing_state == BUFFER_SHARING_INVALID) {
        LOGW("%s(),%d: buffer sharing not enabled, do nothing.", __func__, __LINE__);
        return OMX_ErrorNone;
    }

    BufferShareStatus bsret = buffer_sharing_lib->encoderEnterSharingMode();
    if (bsret != BS_SUCCESS) {
        LOGE("%s(),%d: encoderEnterSharingMode failed", __func__, __LINE__);
        if (bsret == BS_PEER_DOWN) {
            LOGE("%s(), %d: camera down during buffer sharing state transition.",__func__, __LINE__);
        }
        return OMX_ErrorUndefined;
    }

    buffer_sharing_state = BUFFER_SHARING_EXECUTING;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::ExitBufferSharingMode()
{
    if ((buffer_sharing_state != BUFFER_SHARING_INVALID) &&
            (buffer_sharing_state != BUFFER_SHARING_EXECUTING)) {
        LOGE("%s(),%d: invoke %s failed (incorrect state).", __func__, __LINE__, __func__);
        return OMX_ErrorUndefined;
    }

    if (buffer_sharing_state == BUFFER_SHARING_INVALID) {
        LOGW("%s(),%d: buffer sharing not enabled, do nothing.", __func__, __LINE__);
        return OMX_ErrorNone;
    }

    BufferShareStatus bsret = buffer_sharing_lib->encoderExitSharingMode();
    if (bsret != BS_SUCCESS) {
        LOGE("%s(),%d: encoderEnterSharingMode failed", __func__, __LINE__);
        if (bsret == BS_PEER_DOWN) {
            LOGE("%s(), %d: camera down during buffer sharing state transition.",__func__, __LINE__);
        }
        return OMX_ErrorUndefined;
    }

    buffer_sharing_state = BUFFER_SHARING_LOADED;
    return OMX_ErrorNone;
}
/* end of share buffer setting */

OMX_ERRORTYPE MrstPsbComponent::ProcessorFlush(OMX_U32 port_index) {

    LOGV("port_index = %d Flushed!\n", port_index);

    if (port_index == INPORT_INDEX || port_index == OMX_ALL) {
        ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
        mix_video_flush( mix);
    }
    return OMX_ErrorNone;
}

void MrstPsbComponent::M4vEncMixBufferCallback(ulong token, uchar *data) {
    MrstPsbComponent *_this = (MrstPsbComponent *) token;

    LOGV("M4vEncMixBufferCallback Begin\n");
    if(_this) {
        _this->ports[_this->INPORT_INDEX]->ReturnAllRetainedBuffers();
    }

    LOGV("M4vEncMixBufferCallback End\n");
}

/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB.M4V.Enc";

static const char *g_roles[] =
{
    (const char *)"video_encoder.mpeg4",
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
instantiate:
    wrs_omxil_cmodule_ops_instantiate,
};

struct wrs_omxil_cmodule_s WRS_OMXIL_CMODULE_SYMBOL = {
name:
    g_name,
roles:
    &g_roles[0],
nr_roles:
    ARRAY_SIZE(g_roles),
ops:
    &cmodule_ops,
};

