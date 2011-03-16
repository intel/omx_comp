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
#define LOG_TAG "intel-h263-encoder"
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

#ifdef COMPONENT_SUPPORT_OPENCORE
#include <pv_omxcore.h>
#include <pv_omxdefs.h>
#endif

#include <mixdisplayandroid.h>
#include <mixvideo.h>
#include <mixvideoconfigparamsenc_h263.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <va/va.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#include <va/va_android.h>

#include "h263.h"
#include "psb.h"

#define Display unsigned int
#define SHOW_FPS 0
#include "vabuffer.h"

/*
 * constructor & destructor
 */
MrstPsbComponent::MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit\n", __func__, __LINE__);
}

MrstPsbComponent::~MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

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

    ret = __AllocateH263Port(codec_port_index, codec_port_dir);

    if (ret != OMX_ErrorNone)
        goto free_ports;

    LOGV("---- prepare to call __AllocateRawPort() ----\n");
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

OMX_ERRORTYPE MrstPsbComponent::__AllocateH263Port(OMX_U32 port_index, OMX_DIRTYPE dir) {

    PortH263 *h263port;

    OMX_PARAM_PORTDEFINITIONTYPE h263portdefinition;
    OMX_VIDEO_PARAM_H263TYPE h263portparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortH263;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    h263port = static_cast<PortH263 *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&h263portdefinition, 0, sizeof(h263portdefinition));
    SetTypeHeader(&h263portdefinition, sizeof(h263portdefinition));
    h263portdefinition.nPortIndex = port_index;
    h263portdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        h263portdefinition.nBufferCountActual =
            INPORT_H263_ACTUAL_BUFFER_COUNT;
        h263portdefinition.nBufferCountMin = INPORT_H263_MIN_BUFFER_COUNT;
        h263portdefinition.nBufferSize = INPORT_H263_BUFFER_SIZE;
    } else {
        h263portdefinition.nBufferCountActual =
            OUTPORT_H263_ACTUAL_BUFFER_COUNT;
        h263portdefinition.nBufferCountMin = OUTPORT_H263_MIN_BUFFER_COUNT;
        h263portdefinition.nBufferSize = OUTPORT_H263_BUFFER_SIZE;
    }
    h263portdefinition.bEnabled = OMX_TRUE;
    h263portdefinition.bPopulated = OMX_FALSE;
    h263portdefinition.eDomain = OMX_PortDomainVideo;
    h263portdefinition.format.video.cMIMEType = (OMX_STRING)"video/h263";
    h263portdefinition.format.video.pNativeRender = NULL;
    h263portdefinition.format.video.nFrameWidth = 176;
    h263portdefinition.format.video.nFrameHeight = 144;
    h263portdefinition.format.video.nStride = 0;
    h263portdefinition.format.video.nSliceHeight = 0;
    h263portdefinition.format.video.nBitrate = 64000;
    h263portdefinition.format.video.xFramerate = 15 << 16;
    h263portdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    h263portdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
    h263portdefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    h263portdefinition.format.video.pNativeWindow = NULL;
    h263portdefinition.bBuffersContiguous = OMX_FALSE;
    h263portdefinition.nBufferAlignment = 0;
    h263port->SetPortDefinition(&h263portdefinition, true);
    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_H263TYPE */
    memset(&h263portparam, 0, sizeof(h263portparam));
    SetTypeHeader(&h263portparam, sizeof(h263portparam));
    h263portparam.nPortIndex = port_index;
    h263portparam.eProfile = OMX_VIDEO_H263ProfileBaseline;
    h263portparam.eLevel = OMX_VIDEO_H263Level70;
    h263port->SetPortH263Param(&h263portparam, true);
    /* end of OMX_VIDEO_PARAM_H263TYPE */

    /* encoder */
    if (dir == OMX_DirOutput) {
        /* OMX_VIDEO_PARAM_BITRATETYPE */
        OMX_VIDEO_PARAM_BITRATETYPE bitrateparam;

        memset(&bitrateparam, 0, sizeof(bitrateparam));
        SetTypeHeader(&bitrateparam, sizeof(bitrateparam));

        bitrateparam.nPortIndex = port_index;
        bitrateparam.eControlRate = OMX_Video_ControlRateConstant;
        bitrateparam.nTargetBitrate = 64000;

        h263port->SetPortBitrateParam(&bitrateparam, true);
        /* end of OMX_VIDEO_PARAM_BITRATETYPE */

        /* OMX_VIDEO_CONFIG_PRI_INFOTYPE */
        OMX_VIDEO_CONFIG_PRI_INFOTYPE privateinfoparam;
        memset(&privateinfoparam, 0, sizeof(privateinfoparam));
        SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));
        privateinfoparam.nPortIndex = port_index;
        privateinfoparam.nCapacity = 0;
        privateinfoparam.nHolder = NULL;
        h263port->SetPortPrivateInfoParam(&privateinfoparam, true);
        /* end of OMX_VIDEO_CONFIG_PRI_INFOTYPE */

        h263EncPFrames = 0;
        h263EncParamIntelBitrateType.nPortIndex = port_index;
        h263EncParamIntelBitrateType.eControlRate = OMX_Video_Intel_ControlRateMax;
        h263EncParamIntelBitrateType.nTargetBitrate = 0;
        SetTypeHeader(&h263EncParamIntelBitrateType, sizeof(h263EncParamIntelBitrateType));

        h263EncConfigIntelBitrateType.nPortIndex = port_index;
        h263EncConfigIntelBitrateType.nMaxEncodeBitrate = 4000 * 1024;    // Maximum bitrate
        h263EncConfigIntelBitrateType.nTargetPercentage = 95;             // Target bitrate as percentage of maximum bitrate; e.g. 95 is 95%
        h263EncConfigIntelBitrateType.nWindowSize = 1000;                 // Window size in milliseconds allowed for bitrate to reach target
        h263EncConfigIntelBitrateType.nInitialQP  = 36;                   // Initial QP for I frames
        h263EncConfigIntelBitrateType.nMinQP      = 18;
        SetTypeHeader(&h263EncConfigIntelBitrateType, sizeof(h263EncConfigIntelBitrateType));

    }

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
        LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
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
    rawportdefinition.format.video.nStride = 176;
    rawportdefinition.format.video.nSliceHeight = 144;
    rawportdefinition.format.video.nBitrate = 64000;
    rawportdefinition.format.video.xFramerate = 15 << 16;
    rawportdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    rawportdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawportdefinition.format.video.eColorFormat =
        OMX_COLOR_FormatYUV420SemiPlanar;
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

#define H263_ENCODE_ERROR_CHECKING(p) \
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
PortH263*port = static_cast<PortH263 *> (ports[index]); \
if (!port) { \
    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, \
         OMX_ErrorBadPortIndex); \
    return OMX_ErrorBadPortIndex; \
} \
LOGV("%s(), about to get native or supported nal format", __func__); \
if (!port->IsEnabled()) { \
    LOGV("%s() : port is not enabled", __func__); \
    return OMX_ErrorNotReady; \
} \
 
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
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortVideoParam(), sizeof(*p));

        LOGV("%s(), p->eColorFormat = %x\n", __func__, p->eColorFormat);
        break;
    }
    case OMX_IndexParamVideoH263: {
        OMX_VIDEO_PARAM_H263TYPE *p =
            (OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortH263 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortH263 *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortH263Param(), sizeof(*p));
        break;
    }
    case OMX_IndexParamVideoBitrate: {

        OMX_VIDEO_PARAM_BITRATETYPE *p =
            (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
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

        memcpy(p, port->GetPortBitrateParam(), sizeof(*p));
        break;
    }
    case OMX_IndexIntelPrivateInfo: {
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentParameterStructure;
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

        memcpy(p, port->GetPortPrivateInfoParam(), sizeof(*p));
        break;
    }
#ifdef COMPONENT_SUPPORT_OPENCORE
    /* PVOpenCore */
    case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX: {
        PV_OMXComponentCapabilityFlagsType *p =
            (PV_OMXComponentCapabilityFlagsType *)pComponentParameterStructure;

        p->iIsOMXComponentMultiThreaded = OMX_TRUE;
        p->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsMovableInputBuffers = OMX_FALSE;
        p->iOMXComponentSupportsPartialFrames = OMX_TRUE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;

        break;
    }
#endif
    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *p =
            (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        PortH263 *port = NULL;

        OMX_U32 index = p->nPortIndex;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone)
        {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortH263 *>(ports[index]);
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        const OMX_VIDEO_PARAM_H263TYPE *H263Param = port->GetPortH263Param();

        p->eProfile = H263Param->eProfile;
        p->eLevel  = H263Param->eLevel;

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
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                    state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }
        ret = port->SetPortVideoParam(p, false);
        break;
    }
    case OMX_IndexParamVideoH263: {
        OMX_VIDEO_PARAM_H263TYPE *p =
            (OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortH263 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortH263 *>(ports[index]);

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

        ret = port->SetPortH263Param(p, false);
        break;
    }
    case OMX_IndexParamVideoBitrate: {

        LOGV("%s(), OMX_IndexParamVideoBitrate", __func__);
        h263EncParamIntelBitrateType.eControlRate
        = OMX_Video_Intel_ControlRateMax;

        OMX_VIDEO_PARAM_BITRATETYPE *p =
            (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
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
    case OMX_IndexIntelPrivateInfo: {

        LOGV("%s(), OMX_IndexIntelPrivateInfo", __func__);
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentParameterStructure;
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
        ret = port->SetPortPrivateInfoParam(p, false);
        break;
    }

    case OMX_IndexParamVideoBytestream: {

        LOGV("%s(), OMX_IndexParamVideoBytestream", __func__);

        OMX_VIDEO_PARAM_BYTESTREAMTYPE *p =
            (OMX_VIDEO_PARAM_BYTESTREAMTYPE *) pComponentParameterStructure;

        H263_ENCODE_ERROR_CHECKING(p)

        OMX_STATETYPE state;
        CBaseGetState((void *) GetComponentHandle(), &state);
        if (state != OMX_StateLoaded && state != OMX_StateWaitForResources) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorIncorrectStateOperation);
            return OMX_ErrorIncorrectStateOperation;
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

        if (h263EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *) pComponentConfigStructure;

        H263_ENCODE_ERROR_CHECKING(pIntelBitrate)

        *pIntelBitrate = h263EncConfigIntelBitrateType;

        break;
    }

    case OMX_IndexConfigVideoFramerate: {

        LOGV("%s() : OMX_IndexConfigVideoFramerate", __func__);

        if (h263EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_CONFIG_FRAMERATETYPE *pxFramerate =
            (OMX_CONFIG_FRAMERATETYPE *) pComponentConfigStructure;

        H263_ENCODE_ERROR_CHECKING(pxFramerate)

        *pxFramerate = h263EncFramerate;
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

    LOGV("%s() : nIndex = %d\n", __func__, nParamIndex);

    switch (nParamIndex)
    {
    case OMX_IndexConfigVideoIntraVOPRefresh:
    {
        LOGV("%s(), OMX_IndexConfigVideoIntraVOPRefresh", __func__);

        pVideoIFrame = (OMX_CONFIG_INTRAREFRESHVOPTYPE*) pComponentConfigStructure;

        H263_ENCODE_ERROR_CHECKING(pVideoIFrame)

        LOGV("%s(), OMX_IndexConfigVideoIntraVOPRefresh", __func__);
        if(pVideoIFrame->IntraRefreshVOP == OMX_TRUE) {
            LOGV("%s(), pVideoIFrame->IntraRefreshVOP == OMX_TRUE", __func__);

            MixEncDynamicParams encdynareq;
            oscl_memset(&encdynareq, 0, sizeof(encdynareq));
            encdynareq.force_idr = TRUE;
            if(mix) {
                mret = mix_video_set_dynamic_enc_config (mix,
                        MIX_ENC_PARAMS_FORCE_KEY_FRAME, &encdynareq);
                if(mret != MIX_RESULT_SUCCESS) {
                    LOGW("%s(), failed to set IDR interval", __func__);
                }
            }
        }
    }
    break;

    case OMX_IndexConfigIntelBitrate: {

        LOGV("%s(), OMX_IndexConfigIntelBitrate", __func__);
        if (h263EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            LOGV("%s(), eControlRate == OMX_Video_Intel_ControlRateMax");
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *) pComponentConfigStructure;

        H263_ENCODE_ERROR_CHECKING(pIntelBitrate);

        h263EncConfigIntelBitrateType = *pIntelBitrate;

        if (mix && h263EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            LOGV("%s(), h263EncConfigIntelBitrateType.nInitialQP = %d", __func__,
                 h263EncConfigIntelBitrateType.nInitialQP);

            LOGV("%s(), h263EncConfigIntelBitrateType.nMinQP = %d", __func__,
                 h263EncConfigIntelBitrateType.nMinQP);

            LOGV("%s(), h263EncConfigIntelBitrateType.nMaxEncodeBitrate = %d", __func__,
                 h263EncConfigIntelBitrateType.nMaxEncodeBitrate);

            LOGV("%s(), h263EncConfigIntelBitrateType.nTargetPercentage = %d", __func__,
                 h263EncConfigIntelBitrateType.nTargetPercentage);

            LOGV("%s(), h263EncConfigIntelBitrateType.nWindowSize = %d", __func__,
                 h263EncConfigIntelBitrateType.nWindowSize);

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            oscl_memset(&dynamic_params, 0, sizeof(dynamic_params));

            params_type = MIX_ENC_PARAMS_INIT_QP;
            dynamic_params.init_QP = h263EncConfigIntelBitrateType.nInitialQP;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_MIN_QP;
            dynamic_params.min_QP = h263EncConfigIntelBitrateType.nMinQP;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_BITRATE;
            dynamic_params.bitrate
            = h263EncConfigIntelBitrateType.nMaxEncodeBitrate;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_TARGET_PERCENTAGE;
            dynamic_params.target_percentage
            = h263EncConfigIntelBitrateType.nTargetPercentage;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_WINDOW_SIZE;
            dynamic_params.window_size
            = h263EncConfigIntelBitrateType.nWindowSize;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }
        }
        break;
    }

    case OMX_IndexConfigVideoFramerate: {
        if (h263EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_CONFIG_FRAMERATETYPE *pxFramerate =
            (OMX_CONFIG_FRAMERATETYPE *) pComponentConfigStructure;

        H263_ENCODE_ERROR_CHECKING(pxFramerate);

        h263EncFramerate = *pxFramerate;

        if (mix && h263EncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            oscl_memset(&dynamic_params, 0, sizeof(dynamic_params));

            params_type = MIX_ENC_PARAMS_FRAME_RATE;
            dynamic_params.frame_rate_denom = 1;
            dynamic_params.frame_rate_num   = h263EncFramerate.xEncodeFramerate >> 16;  // Q16 format
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
    MixVideoConfigParamsEnc * pMixParam = NULL;
    uint major, minor;
    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

    mix = mix_video_new();
    LOGV("%s(): called to mix_video_new()", __func__);

    if (!mix) {
        LOGV("%s(),%d: exit, mix_video_new failed", __func__, __LINE__);
        goto error_out;
    }

    mix_video_get_version(mix, &major, &minor);
    LOGV("MixVideo version: %d.%d", major, minor);

    /* encoder */
    vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsenc_h263_new());
    mvp = MIX_PARAMS(mix_videoencodeparams_new());

    if (!vcp || !mvp ) {
        LOGV("%s(),%d: exit, failed to allocate vcp, mvp\n",
             __func__, __LINE__);
        goto error_out;
    }

    oret = ChangeVcpWithPortParam(vcp,
                                  static_cast<PortVideo *>(ports[INPORT_INDEX]),
                                  static_cast<PortVideo *>(ports[OUTPORT_INDEX]),
                                  NULL);
    if (oret != OMX_ErrorNone) {
        LOGV("%s(),%d: exit, ChangeVcpWithPortParam failed (ret == 0x%08x)\n",
             __func__, __LINE__, oret);
        goto error_out;
    }

    display = mix_displayandroid_new();
    if (!display) {
        LOGV("%s(),%d: exit, mix_displayandroid_new failed", __func__, __LINE__);
        goto error_out;
    }

    vip = mix_videoinitparams_new();
    if (!vip) {
        LOGV("%s(),%d: exit, mix_videoinitparams_new failed", __func__,
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
        LOGV("%s(),%d: exit, mix_videoinitparams_set_display failed "
             "(ret == 0x%08x)", __func__, __LINE__, mret);
        goto error_out;
    }

    mret = mix_video_initialize(mix, MIX_CODEC_MODE_ENCODE, vip, NULL);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGV("%s(),%d: exit, mix_video_initialize failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto error_out;
    }

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

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, oret);
    return oret;

error_out:
    mix_params_unref(mvp);
    mix_videoconfigparams_unref(vcp);
    mix_displayandroid_unref(display);
    mix_videoinitparams_unref(vip);
    mix_video_unref(mix);

    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE MrstPsbComponent::ProcessorDeinit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

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

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
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

    LOGV("%s(): enter encode\n", __func__);

    LOGV_IF(buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS,
            "%s(),%d: got OMX_BUFFERFLAG_EOS\n", __func__, __LINE__);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGV("%s(),%d: input buffer's nFilledLen is zero\n",
             __func__, __LINE__);
        goto out;
    }

    buffer_in.data =
        buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset;
    buffer_in.data_size = buffers[INPORT_INDEX]->nFilledLen;
    buffer_in.buffer_size = buffers[INPORT_INDEX]->nFilledLen;

    LOGV("buffer_in.data=%x, data_size=%d, buffer_size=%d",
         (unsigned)buffer_in.data, buffer_in.data_size, buffer_in.buffer_size);

    buffer_out.data =
        buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    buffer_out.data_size = 0;
    buffer_out.buffer_size = buffers[OUTPORT_INDEX]->nAllocLen - buffers[OUTPORT_INDEX]->nOffset;
    mixiovec_out[0] = &buffer_out;

normal_start:
    /* get MixBuffer */
    mret = mix_video_get_mixbuffer(mix, &mixbuffer_in[0]);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGV("%s(), %d: exit, mix_video_get_mixbuffer failed (ret:0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    /* fill MixBuffer */
    mret = mix_buffer_set_data(mixbuffer_in[0],
                               buffer_in.data, buffer_in.data_size,
                               (ulong)this, H263EncMixBufferCallback);

    if (mret != MIX_RESULT_SUCCESS) {
        LOGV("%s(), %d: exit, mix_buffer_set_data failed (ret:0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    /* encoder */
    mret = mix_video_encode(mix, mixbuffer_in, 1, mixiovec_out, 1,
                            MIX_VIDEOENCODEPARAMS(mvp));

    LOGV("%s(), mret = 0x%08x", __func__, mret);

    if (mret != MIX_RESULT_SUCCESS &&
            mret != MIX_RESULT_VIDEO_ENC_SLICESIZE_OVERFLOW) {
        LOGV("%s(), %d: exit, mix_video_encode failed (ret == 0x%08x)\n",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    outfilledlen = mixiovec_out[0]->data_size;
    outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;
    outflags |= OMX_BUFFERFLAG_ENDOFFRAME;

    int32 width;
    int32 height;
    int32 display_width;
    int32 display_height;
    bool b_intra;

    h263_parser.DecodeH263Header(mixiovec_out[0]->data, &width,
                                 &height, &display_width, &display_height, &b_intra);

    if (b_intra) {
        LOGV("%s(), syncframe", __func__);
        outflags |= OMX_BUFFERFLAG_SYNCFRAME;
    }

    LOGV("********** output buffer: len=%d, ts=%ld, flags=%x",
         outfilledlen,
         outtimestamp,
         outflags);

    // The buffer will be relaseed in H263EncMixBufferCallback()

    retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;

    if(mret == MIX_RESULT_VIDEO_ENC_SLICESIZE_OVERFLOW) {
        LOGV("%s(), mix_video_encode returns MIX_RESULT_VIDEO_ENC_SLICESIZE_OVERFLOW"
             , __func__);
        oret = (OMX_ERRORTYPE)OMX_ErrorIntelExtSliceSizeOverflow;
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

        LOGD("FPS = %2.1f\n", average_fps);
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

        LOGV("********** output buffer: len=%d, ts=%ld, flags=%x",
             outfilledlen,
             outtimestamp,
             outflags);
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

OMX_ERRORTYPE MrstPsbComponent::__H263ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortH263 *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    MixVideoConfigParamsEnc *config = MIX_VIDEOCONFIGPARAMSENC(vcp);

    mix_videoconfigparamsenc_set_encode_format(
        config, MIX_ENCODE_TARGET_FORMAT_H263);
    mix_videoconfigparamsenc_set_profile(config, MIX_PROFILE_H263BASELINE);
    mix_videoconfigparamsenc_set_mime_type(config, "video/x-h263");
    mix_videoconfigparamsenc_h263_set_dlk(
        MIX_VIDEOCONFIGPARAMSENC_H263(config), FALSE);

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp,
    PortVideo *port_in,
    PortVideo *port_out,
    bool *vcp_changed)
{
    const OMX_PARAM_PORTDEFINITIONTYPE *pd_out = port_out->GetPortDefinition();
    const OMX_PARAM_PORTDEFINITIONTYPE *pd_in = port_in->GetPortDefinition();
    OMX_ERRORTYPE ret;

    ret = __H263ChangeVcpWithPortParam(vcp,
                                       static_cast<PortH263 *>(port_out),
                                       vcp_changed);

    /* encoder */
    MixVideoConfigParamsEnc *config = MIX_VIDEOCONFIGPARAMSENC(vcp);
    const OMX_VIDEO_PARAM_BITRATETYPE *bitrate =
        port_out->GetPortBitrateParam();
    OMX_VIDEO_CONTROLRATETYPE controlrate;

    if ((config->picture_width != pd_out->format.video.nFrameWidth) ||
            (config->picture_height != pd_out->format.video.nFrameHeight)) {
        LOGV("%s(): width : %d != %ld", __func__,
             config->picture_width, pd_out->format.video.nFrameWidth);
        LOGV("%s(): height : %d != %ld", __func__,
             config->picture_height, pd_out->format.video.nFrameHeight);

        mix_videoconfigparamsenc_set_picture_res(config,
                pd_out->format.video.nFrameWidth,
                pd_out->format.video.nFrameHeight);
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

    if(h263EncPFrames == 0) {
        h263EncPFrames = config->frame_rate_num/2 ;
    }

    LOGV("%s() : h263EncPFrames = %d", __func__, h263EncPFrames);
    mix_videoconfigparamsenc_set_intra_period(config,h263EncPFrames);

    if (h263EncParamIntelBitrateType.eControlRate
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
        mix_videoconfigparamsenc_set_init_qp(config, 15);
        mix_videoconfigparamsenc_set_min_qp(config, 1);
        mix_videoconfigparamsenc_set_buffer_pool_size(config, 8);
        mix_videoconfigparamsenc_set_drawable(config, 0x0);
        mix_videoconfigparamsenc_set_need_display(config, FALSE);

    }

    LOGV("@@@@@ about to set buffer sharing @@@@@");
#if 0	//FiXME: We should get the correct buffer share mode
    const OMX_BOOL *isbuffersharing = port->GetPortBufferSharingInfo();
    const OMX_VIDEO_CONFIG_PRI_INFOTYPE *privateinfoparam = port->GetPortPrivateInfoParam();
    if(*isbuffersharing == OMX_TRUE) {
        LOGV("@@@@@ we are in buffer sharing mode @@@@@");
        if(privateinfoparam->nHolder != NULL) {
            guint size = (guint)privateinfoparam->nCapacity;
            gulong * p = (gulong *)privateinfoparam->nHolder;
            int i = 0;
            for(i = 0; i < size; i++)
                LOGV("@@@@@ nCapacity = %d, camera frame id array[%d] = 0x%08x @@@@@", size, i, p[i]);
            mix_videoconfigparamsenc_set_ci_frame_info(config, (gulong *)privateinfoparam->nHolder, (guint)privateinfoparam->nCapacity);
            mix_videoconfigparamsenc_set_share_buf_mode(config, TRUE);
            free(privateinfoparam->nHolder);
        }
    } else {
        LOGV("@@@@@ we are NOT in buffer sharing mode @@@@@");
        mix_videoconfigparamsenc_set_share_buf_mode(config, FALSE);
    }
#else
    LOGV("@@@@@ we are NOT in buffer sharing mode @@@@@");
    mix_videoconfigparamsenc_set_share_buf_mode(config, FALSE);
#endif
    return ret;
}
/* end of vcp setting helpers */

OMX_ERRORTYPE MrstPsbComponent::ProcessorFlush(OMX_U32 port_index) {

    LOGV("port_index = %d Flushed!\n", port_index);

    if ((port_index == INPORT_INDEX || port_index == OMX_ALL)) {
        ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
        mix_video_flush( mix);
    }
    return OMX_ErrorNone;
}

void MrstPsbComponent::H263EncMixBufferCallback(ulong token, uchar *data) {
    MrstPsbComponent *_this = (MrstPsbComponent *) token;

    LOGV("H263EncMixBufferCallback Begin\n");

    if(_this) {
        _this->ports[_this->INPORT_INDEX]->ReturnAllRetainedBuffers();
    }

    LOGV("H263EncMixBufferCallback End\n");
}

/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB.H263.Enc";

static const char *g_roles[] =
{
    (const char *)"video_encoder.h263",
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
