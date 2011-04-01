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
#define LOG_TAG "intel-avc-encoder"
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
#include <mixvideoconfigparamsenc_h264.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <va/va.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#include <va/va_android.h>

#include "psb_avc.h"

#define Display unsigned int

#define SHOW_FPS 0

#include "vabuffer.h"

#include <IntelBufferSharing.h>

/*
 * constructor & destructor
 */
MrstPsbComponent::MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    buffer_sharing_state = BUFFER_SHARING_INVALID;

#ifdef ENABLE_BUFFER_SHARE_MODE
    OMX_ERRORTYPE oret = EnableBufferSharingMode();
#else
    OMX_ERRORTYPE oret = DisableBufferSharingMode();
#endif

    if (oret != OMX_ErrorNone) {
        LOGE("%s(),%d:  set buffer sharing mode failed", __func__, __LINE__);
        DisableBufferSharingMode();
    }

    LOGV("%s(),%d: exit\n", __func__, __LINE__);
}

MrstPsbComponent::~MrstPsbComponent()
{
    LOGV("%s(): enter\n", __func__);

    OMX_ERRORTYPE oret = DisableBufferSharingMode();
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

    OMX_ERRORTYPE ret = OMX_ErrorUndefined;

    LOGV("%s(): enter\n", __func__);

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;

    this->nr_ports = NR_PORTS;
    this->ports = ports;

    raw_port_index = INPORT_INDEX;
    codec_port_index = OUTPORT_INDEX;
    raw_port_dir = OMX_DirInput;
    codec_port_dir = OMX_DirOutput;

    ret = __AllocateAvcPort(codec_port_index, codec_port_dir);

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

OMX_ERRORTYPE MrstPsbComponent::__AllocateAvcPort(OMX_U32 port_index,
        OMX_DIRTYPE dir)
{
    PortAvc *avcport;

    OMX_PARAM_PORTDEFINITIONTYPE avcportdefinition;
    OMX_VIDEO_PARAM_AVCTYPE avcportparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortAvc;
    if (!ports[port_index]) {
        LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }
    avcport = static_cast<PortAvc *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&avcportdefinition, 0, sizeof(avcportdefinition));
    SetTypeHeader(&avcportdefinition, sizeof(avcportdefinition));
    avcportdefinition.nPortIndex = port_index;
    avcportdefinition.eDir = dir;

    avcportdefinition.nBufferCountActual = OUTPORT_AVC_ACTUAL_BUFFER_COUNT;
    avcportdefinition.nBufferCountMin = OUTPORT_AVC_MIN_BUFFER_COUNT;
    avcportdefinition.nBufferSize = OUTPORT_AVC_BUFFER_SIZE;

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

    /* encoder */
    /* OMX_VIDEO_PARAM_BITRATETYPE */
    OMX_VIDEO_PARAM_BITRATETYPE bitrateparam;

    memset(&bitrateparam, 0, sizeof(bitrateparam));
    SetTypeHeader(&bitrateparam, sizeof(bitrateparam));

    bitrateparam.nPortIndex = port_index;
    bitrateparam.eControlRate = OMX_Video_ControlRateConstant;
    bitrateparam.nTargetBitrate = 192000;

    avcport->SetPortBitrateParam(&bitrateparam, true);
    /* end of OMX_VIDEO_PARAM_BITRATETYPE */

    /* OMX_VIDEO_CONFIG_PRI_INFOTYPE */
    OMX_VIDEO_CONFIG_PRI_INFOTYPE privateinfoparam;

    memset(&privateinfoparam, 0, sizeof(privateinfoparam));
    SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));

    privateinfoparam.nPortIndex = port_index;
    privateinfoparam.nCapacity = 0;
    privateinfoparam.nHolder = NULL;

    avcport->SetPortPrivateInfoParam(&privateinfoparam, true);
    /* end of OMX_VIDEO_CONFIG_PRI_INFOTYPE */

    avcEncIDRPeriod = 0;
    avcEncPFrames = 0;
    avcEncNaluFormatType = OMX_NaluFormatStartCodes;

    avcEncParamIntelBitrateType.nPortIndex = port_index;
    avcEncParamIntelBitrateType.eControlRate = OMX_Video_Intel_ControlRateMax;
    avcEncParamIntelBitrateType.nTargetBitrate = 0;
    SetTypeHeader(&avcEncParamIntelBitrateType, sizeof(avcEncParamIntelBitrateType));

    avcEncConfigNalSize.nPortIndex = port_index;
    avcEncConfigNalSize.nNaluBytes = 0;
    SetTypeHeader(&avcEncConfigNalSize, sizeof(avcEncConfigNalSize));

    avcEncConfigSliceNumbers.nPortIndex = port_index;
    avcEncConfigSliceNumbers.nISliceNumber = 1;
    avcEncConfigSliceNumbers.nPSliceNumber = 1;
    SetTypeHeader(&avcEncConfigSliceNumbers, sizeof(avcEncConfigSliceNumbers));

    avcEncConfigAir.nPortIndex = port_index;
    avcEncConfigAir.bAirEnable = OMX_FALSE;
    avcEncConfigAir.bAirAuto = OMX_FALSE;
    avcEncConfigAir.nAirMBs = 0;
    avcEncConfigAir.nAirThreshold = 0;
    SetTypeHeader(&avcEncConfigAir, sizeof(avcEncConfigAir));

    avcEncFramerate.nPortIndex = port_index;
    avcEncFramerate.xEncodeFramerate =  0; // Q16 format
    SetTypeHeader(&avcEncFramerate, sizeof(avcEncFramerate));

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
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

/*
   Add OMX_COLOR_FormatVendorStartUnused + 0xA00E00 -> PVMF_MIME_RAWVA in file
   external/opencore/nodes/pvomxvideodecnode/src/pvmf_omx_videodec_node.cpp

*/

#define AVC_ENCODE_ERROR_CHECKING(p) \
if (!p) { \
    LOGE("%s(), NULL pointer", __func__); \
    return OMX_ErrorBadParameter; \
} \
ret = CheckTypeHeader(p, sizeof(*p)); \
if (ret != OMX_ErrorNone) { \
    LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret); \
    return ret; \
} \
OMX_U32 index = p->nPortIndex; \
if (index != OUTPORT_INDEX) { \
    LOGE("%s(), wrong port index", __func__); \
    return OMX_ErrorBadPortIndex; \
} \
PortAvc *port = static_cast<PortAvc *> (ports[index]); \
if (!port) { \
    LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, \
         OMX_ErrorBadPortIndex); \
    return OMX_ErrorBadPortIndex; \
} \
LOGV("%s(), about to get native or supported nal format", __func__); \
if (!port->IsEnabled()) { \
    LOGE("%s() : port is not enabled", __func__); \
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

        memcpy(p, port->GetPortVideoParam(), sizeof(*p));

        LOGV("%s(), p->eColorFormat = %x\n", __func__, p->eColorFormat);
        break;
    }
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAvc *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortAvcParam(), sizeof(*p));
        break;
    }
    case OMX_IndexParamVideoBitrate: {

        if (avcEncParamIntelBitrateType.eControlRate
                != OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

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
    case OMX_IndexParamNalStreamFormat:
    case OMX_IndexParamNalStreamFormatSupported: {
        OMX_NALSTREAMFORMATTYPE *p =
            (OMX_NALSTREAMFORMATTYPE *)pComponentParameterStructure;

        LOGV("%s(), OMX_IndexParamNalStreamFormat or OMX_IndexParamNalStreamFormatSupported", __func__);

        AVC_ENCODE_ERROR_CHECKING(p)

        if(nParamIndex == OMX_IndexParamNalStreamFormat) {
            p->eNaluFormat = OMX_NaluFormatStartCodes;
            LOGV("%s(), OMX_IndexParamNalStreamFormat 0x%x", __func__,
                 p->eNaluFormat);
        } else {
            p->eNaluFormat = (OMX_NALUFORMATSTYPE)(OMX_NaluFormatStartCodes |
                                                   OMX_NaluFormatFourByteInterleaveLength |
                                                   OMX_NaluFormatZeroByteInterleaveLength);
            LOGV("%s(), OMX_IndexParamNalStreamFormatSupported 0x%x",
                 __func__, p->eNaluFormat);
        }
        break;
    }

    case OMX_IndexConfigVideoAVCIntraPeriod: {
        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval =
            (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentParameterStructure;

        LOGV("%s(), OMX_IndexConfigVideoAVCIntraPeriod", __func__);

        if(!vcp) {
            LOGE("%s(), vcp is NULL", __func__);
            return OMX_ErrorNotReady;
        }

        AVC_ENCODE_ERROR_CHECKING(pVideoIDRInterval)
        ret = ComponentGetConfig(OMX_IndexConfigVideoAVCIntraPeriod,
                                 pComponentParameterStructure);

        break;
    }
    case OMX_IndexParamIntelBitrate: {

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }
        LOGV("%s(), OMX_IndexParamIntelBitrate", __func__);

        OMX_VIDEO_PARAM_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_PARAM_INTEL_BITRATETYPE *) pComponentParameterStructure;

        AVC_ENCODE_ERROR_CHECKING(pIntelBitrate)

        *pIntelBitrate = avcEncParamIntelBitrateType;

        break;
    }

    case OMX_IndexConfigIntelBitrate: {

        LOGV("%s(), OMX_IndexConfigIntelBitrate", __func__);
        ret = ComponentGetConfig((OMX_INDEXTYPE)OMX_IndexConfigIntelBitrate,
                                 (OMX_PTR)pComponentParameterStructure);
        break;

    }

    case OMX_IndexConfigVideoNalSize: {

        LOGV("%s(), OMX_IndexConfigVideoNalSize", __func__);
        ret = ComponentGetConfig(OMX_IndexConfigVideoNalSize,
                                 pComponentParameterStructure);

        break;
    }

    case OMX_IndexConfigIntelSliceNumbers: {

        LOGV("%s(), OMX_IndexConfigIntelSliceNumbers", __func__);
        ret = ComponentGetConfig((OMX_INDEXTYPE)OMX_IndexConfigIntelSliceNumbers,
                                 pComponentParameterStructure);

        break;
    }

    case OMX_IndexConfigIntelAIR: {

        LOGV("%s(), OMX_IndexConfigIntelAIR", __func__);
        ret = ComponentGetConfig((OMX_INDEXTYPE)OMX_IndexConfigIntelAIR,
                                 pComponentParameterStructure);

        break;
    }

    case OMX_IndexConfigVideoFramerate: {

        LOGV("%s(), OMX_IndexConfigVideoFramerate", __func__);
        ret = ComponentGetConfig((OMX_INDEXTYPE)OMX_IndexConfigVideoFramerate,
                                 pComponentParameterStructure);

        break;
    }

    case OMX_IndexParamVideoProfileLevelQuerySupported:
    {
        OMX_VIDEO_PARAM_PROFILELEVELTYPE *p =
            (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        PortAvc *port = NULL;

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
            port = static_cast<PortAvc *>(ports[index]);
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        const OMX_VIDEO_PARAM_AVCTYPE *avcParam = port->GetPortAvcParam();

        p->eProfile = avcParam->eProfile;
        p->eLevel  = avcParam->eLevel;

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
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAvc *>(ports[index]);

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

        ret = port->SetPortAvcParam(p, false);
        break;
    }
    case OMX_IndexParamVideoBitrate: {

        LOGV("%s(), OMX_IndexParamVideoBitrate", __func__);
        avcEncParamIntelBitrateType.eControlRate = OMX_Video_Intel_ControlRateMax;

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
                LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
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

        AVC_ENCODE_ERROR_CHECKING(p)

        OMX_STATETYPE state;
        CBaseGetState((void *) GetComponentHandle(), &state);
        if (state != OMX_StateLoaded && state != OMX_StateWaitForResources) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorIncorrectStateOperation);
            return OMX_ErrorIncorrectStateOperation;
        }

        if (p->bBytestream == OMX_TRUE) {
            avcEncNaluFormatType = OMX_NaluFormatStartCodes;
        } else {
            avcEncNaluFormatType = OMX_NaluFormatZeroByteInterleaveLength;
        }

        break;
    }
    case OMX_IndexParamNalStreamFormatSelect: {

        LOGV("%s() : OMX_IndexParamNalStreamFormatSelect", __func__);
        OMX_NALSTREAMFORMATTYPE *p =
            (OMX_NALSTREAMFORMATTYPE *) pComponentParameterStructure;

        AVC_ENCODE_ERROR_CHECKING(p)

        if (p->eNaluFormat == OMX_NaluFormatStartCodes || p->eNaluFormat
                == OMX_NaluFormatFourByteInterleaveLength || p->eNaluFormat
                == OMX_NaluFormatZeroByteInterleaveLength) {
            OMX_STATETYPE state;
            CBaseGetState((void *) GetComponentHandle(), &state);
            if (state != OMX_StateLoaded && state != OMX_StateWaitForResources) {
                LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }

            avcEncNaluFormatType = p->eNaluFormat;
            LOGE("%s(), OMX_IndexParamNalStreamFormatSelect : 0x%x",
                 __func__, avcEncNaluFormatType);
        }
        break;
    }

    case OMX_IndexConfigVideoAVCIntraPeriod: {

        LOGV("%s() : OMX_IndexConfigVideoAVCIntraPeriod", __func__);

        OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval =
            (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentParameterStructure;

        AVC_ENCODE_ERROR_CHECKING(pVideoIDRInterval)

        avcEncIDRPeriod = pVideoIDRInterval->nIDRPeriod;
        avcEncPFrames   = pVideoIDRInterval->nPFrames;
        LOGV("%s(), OMX_IndexConfigVideoAVCIntraPeriod : avcEncIDRPeriod = %d avcEncPFrames = %d",
             __func__, avcEncIDRPeriod, avcEncPFrames);
        break;
    }
    case OMX_IndexParamIntelBitrate: {
        OMX_VIDEO_PARAM_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_PARAM_INTEL_BITRATETYPE *) pComponentParameterStructure;

        LOGV("%s(), OMX_IndexParamIntelBitrate", __func__);

        AVC_ENCODE_ERROR_CHECKING(pIntelBitrate)
        avcEncParamIntelBitrateType = *pIntelBitrate;

        break;
    }

    case OMX_IndexConfigIntelBitrate: {

        LOGV("%s(), OMX_IndexParamIntelBitrate", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *) pComponentParameterStructure;

        AVC_ENCODE_ERROR_CHECKING(pIntelBitrate)
        avcEncConfigIntelBitrateType = *pIntelBitrate;

        break;
    }
    case OMX_IndexConfigVideoNalSize: {

        LOGV("%s() : OMX_IndexConfigVideoNalSize", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_NALSIZE *pNalSize =
            (OMX_VIDEO_CONFIG_NALSIZE *) pComponentParameterStructure;

        AVC_ENCODE_ERROR_CHECKING(pNalSize)

        avcEncConfigNalSize = *pNalSize;

        break;
    }

    case OMX_IndexConfigIntelSliceNumbers: {

        LOGV("%s() : OMX_IndexConfigIntelSliceNumbers", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *pSliceNumbers =
            (OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *) pComponentParameterStructure;

        AVC_ENCODE_ERROR_CHECKING(pSliceNumbers)

        avcEncConfigSliceNumbers = *pSliceNumbers;

        LOGV("%s(), nISliceNumber = %d nPSliceNumber = %d", __func__,
             pSliceNumbers->nISliceNumber, pSliceNumbers->nPSliceNumber);

        break;
    }

    case OMX_IndexConfigIntelAIR: {

        LOGV("%s() : OMX_IndexConfigIntelAIR", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_AIR *pIntelAir =
            (OMX_VIDEO_CONFIG_INTEL_AIR *) pComponentParameterStructure;

        AVC_ENCODE_ERROR_CHECKING(pIntelAir)

        avcEncConfigAir = *pIntelAir;

        break;
    }

    case OMX_IndexConfigVideoFramerate: {

        LOGV("%s() : OMX_IndexConfigVideoFramerate", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_CONFIG_FRAMERATETYPE *pxFramerate =
            (OMX_CONFIG_FRAMERATETYPE *) pComponentParameterStructure;

        AVC_ENCODE_ERROR_CHECKING(pxFramerate)

        avcEncFramerate = *pxFramerate;

        break;
    }

    case OMX_IndexParamIntelAVCDecodeSettings: {
        OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS *pAvcDecodeSettings =
            (OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS *) pComponentParameterStructure;

        LOGV("%s(), OMX_IndexParamIntelAVCDecodeSettings", __func__);

        ret = CheckTypeHeader(pAvcDecodeSettings, sizeof(*pAvcDecodeSettings));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        LOGE("%s(), wrong codec mode", __func__);
        return OMX_ErrorUnsupportedIndex;

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
    case OMX_IndexConfigVideoAVCIntraPeriod:
    {
        if(!vcp) {
            return OMX_ErrorNotReady;
        }
        pVideoIDRInterval = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentConfigStructure;
        if(!pVideoIDRInterval) {
            LOGE("%s(), NULL pointer", __func__);
            return OMX_ErrorBadParameter;
        }

        AVC_ENCODE_ERROR_CHECKING(pVideoIDRInterval)

        if(!mix) {
            LOGE("%s(), MixVideo is not created", __func__);
            return OMX_ErrorUndefined;
        }

        MixVideoConfigParams *mixbaseconfig = NULL;
        MIX_RESULT mret = mix_video_get_config(mix, &mixbaseconfig);
        if(mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), failed to get config", __func__);
            return OMX_ErrorUndefined;
        }

        uint intra_period = 0;
        mret = mix_videoconfigparamsenc_get_intra_period(MIX_VIDEOCONFIGPARAMSENC(mixbaseconfig), &intra_period);
        if(mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), failed to get intra period", __func__);
            return OMX_ErrorUndefined;
        }

        uint idr_interval = 0;
        mret = mix_videoconfigparamsenc_h264_get_IDR_interval(MIX_VIDEOCONFIGPARAMSENC_H264(mixbaseconfig), &idr_interval);
        if(mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), failed to get IDR interval", __func__);
            return OMX_ErrorUndefined;
        }

        mix_videoconfigparams_unref(mixbaseconfig);

        pVideoIDRInterval->nIDRPeriod = idr_interval;
        pVideoIDRInterval->nPFrames = intra_period;

        LOGV("%s(), OMX_IndexConfigVideoAVCIntraPeriod : nIDRPeriod = %d, nPFrames = %d",
             __func__, pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);

        SetTypeHeader(pVideoIDRInterval, sizeof(OMX_VIDEO_CONFIG_AVCINTRAPERIOD));
    }
    break;

    case OMX_IndexConfigIntelBitrate: {

        LOGV("%s() : OMX_IndexParamIntelBitrate", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pIntelBitrate)

        *pIntelBitrate = avcEncConfigIntelBitrateType;

        break;
    }
    case OMX_IndexConfigVideoNalSize: {

        LOGV("%s() : OMX_IndexConfigVideoNalSize", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_NALSIZE *pNalSize =
            (OMX_VIDEO_CONFIG_NALSIZE *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pNalSize)

        *pNalSize = avcEncConfigNalSize;
        break;
    }

    case OMX_IndexConfigIntelSliceNumbers: {

        LOGV("%s() : OMX_IndexConfigIntelSliceNumbers", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *pSliceNumbers =
            (OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pSliceNumbers)

        *pSliceNumbers = avcEncConfigSliceNumbers;
        break;
    }

    case OMX_IndexConfigIntelAIR: {

        LOGV("%s() : OMX_IndexConfigIntelAIR", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_AIR *pIntelAir =
            (OMX_VIDEO_CONFIG_INTEL_AIR *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pIntelAir)

        *pIntelAir = avcEncConfigAir;
        break;
    }
    case OMX_IndexConfigVideoFramerate: {

        LOGV("%s() : OMX_IndexConfigVideoFramerate", __func__);

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_CONFIG_FRAMERATETYPE *pxFramerate =
            (OMX_CONFIG_FRAMERATETYPE *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pxFramerate)

        *pxFramerate = avcEncFramerate;
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
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);
    LOGV("%s() : nIndex = %d\n", __func__, nParamIndex);

    switch (nParamIndex)
    {
    case OMX_IndexConfigVideoIntraVOPRefresh:
    {
        LOGV("%s(), OMX_IndexConfigVideoIntraVOPRefresh", __func__);

        pVideoIFrame = (OMX_CONFIG_INTRAREFRESHVOPTYPE*) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pVideoIFrame)

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
    }
    break;

    case OMX_IndexConfigVideoAVCIntraPeriod:
    {
        LOGV("%s(), OMX_IndexConfigVideoAVCIntraPeriod", __func__);

        pVideoIDRInterval = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pVideoIDRInterval)

        LOGV("%s(), OMX_IndexConfigVideoAVCIntraPeriod : nIDRPeriod = %d, nPFrames = %d",
             __func__, pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);

        MixEncDynamicParams encdynareq;
        memset(&encdynareq, 0, sizeof(encdynareq));
        encdynareq.idr_interval = pVideoIDRInterval->nIDRPeriod;
        encdynareq.intra_period = pVideoIDRInterval->nPFrames;
        if(mix) {

            // Ignore the return code
            mret = mix_video_set_dynamic_enc_config (mix, MIX_ENC_PARAMS_IDR_INTERVAL, &encdynareq);
            if(mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), failed to set IDR interval", __func__);
            }

            mret = mix_video_set_dynamic_enc_config (mix, MIX_ENC_PARAMS_GOP_SIZE, &encdynareq);
            if(mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), failed to set GOP size", __func__);
            }
        }
    }
    break;
    case OMX_IndexConfigIntelBitrate: {

        LOGV("%s(), OMX_IndexConfigIntelBitrate", __func__);
        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            LOGV("%s(), eControlRate == OMX_Video_Intel_ControlRateMax");
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *pIntelBitrate =
            (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pIntelBitrate)

        avcEncConfigIntelBitrateType = *pIntelBitrate;

        if (mix && avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            LOGV("%s(), avcEncConfigIntelBitrateType.nInitialQP = %d", __func__,
                 avcEncConfigIntelBitrateType.nInitialQP);

            LOGV("%s(), avcEncConfigIntelBitrateType.nMinQP = %d", __func__,
                 avcEncConfigIntelBitrateType.nMinQP);

            LOGV("%s(), avcEncConfigIntelBitrateType.nMaxEncodeBitrate = %d", __func__,
                 avcEncConfigIntelBitrateType.nMaxEncodeBitrate);

            LOGV("%s(), avcEncConfigIntelBitrateType.nTargetPercentage = %d", __func__,
                 avcEncConfigIntelBitrateType.nTargetPercentage);

            LOGV("%s(), avcEncConfigIntelBitrateType.nWindowSize = %d", __func__,
                 avcEncConfigIntelBitrateType.nWindowSize);

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            memset(&dynamic_params, 0, sizeof(dynamic_params));


            params_type = MIX_ENC_PARAMS_INIT_QP;
            dynamic_params.init_QP = avcEncConfigIntelBitrateType.nInitialQP;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_MIN_QP;
            dynamic_params.min_QP = avcEncConfigIntelBitrateType.nMinQP;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_BITRATE;
            dynamic_params.bitrate
            = avcEncConfigIntelBitrateType.nMaxEncodeBitrate;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_TARGET_PERCENTAGE;
            dynamic_params.target_percentage
            = avcEncConfigIntelBitrateType.nTargetPercentage;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }

            params_type = MIX_ENC_PARAMS_WINDOW_SIZE;
            dynamic_params.window_size
            = avcEncConfigIntelBitrateType.nWindowSize;
            mret = mix_video_set_dynamic_enc_config(mix, params_type,
                                                    &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }
        }
        break;
    }

    case OMX_IndexConfigVideoNalSize: {
        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_NALSIZE *pNalSize =
            (OMX_VIDEO_CONFIG_NALSIZE *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pNalSize)

        avcEncConfigNalSize = *pNalSize;

        if (mix && avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            memset(&dynamic_params, 0, sizeof(dynamic_params));

            params_type = MIX_ENC_PARAMS_MTU_SLICE_SIZE;
            dynamic_params.max_slice_size = avcEncConfigNalSize.nNaluBytes * 8; // bits
            mret = mix_video_set_dynamic_enc_config (mix,
                    params_type, &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }
        }
        break;
    }

    case OMX_IndexConfigIntelSliceNumbers: {
        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *pSliceNumbers =
            (OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pSliceNumbers)

        avcEncConfigSliceNumbers = *pSliceNumbers;

        LOGV("%s(), OMX_IndexConfigIntelSliceNumbers", __func__);
        LOGV("%s(), nISliceNumber = %d nPSliceNumber = %d", __func__,
             pSliceNumbers->nISliceNumber, pSliceNumbers->nPSliceNumber);

        if (mix && avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            memset(&dynamic_params, 0, sizeof(dynamic_params));

            params_type = MIX_ENC_PARAMS_I_SLICE_NUM;
            dynamic_params.I_slice_num = pSliceNumbers->nISliceNumber;
            mret = mix_video_set_dynamic_enc_config (mix,
                    params_type, &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }
            params_type = MIX_ENC_PARAMS_P_SLICE_NUM;
            dynamic_params.P_slice_num = pSliceNumbers->nPSliceNumber;
            mret = mix_video_set_dynamic_enc_config (mix,
                    params_type, &dynamic_params);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGW("%s(), mixvideo return error : 0x%x", __func__, mret);
            }
        }
        break;
    }
    case OMX_IndexConfigIntelAIR: {

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }
        OMX_VIDEO_CONFIG_INTEL_AIR *pIntelAir =
            (OMX_VIDEO_CONFIG_INTEL_AIR *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pIntelAir)

        avcEncConfigAir = *pIntelAir;

        if (mix && avcEncParamIntelBitrateType.eControlRate
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
        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateMax) {
            ret = OMX_ErrorUnsupportedIndex;
            break;
        }

        OMX_CONFIG_FRAMERATETYPE *pxFramerate =
            (OMX_CONFIG_FRAMERATETYPE *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pxFramerate)

        avcEncFramerate = *pxFramerate;

        if (mix && avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            MixEncParamsType params_type;
            MixEncDynamicParams dynamic_params;
            memset(&dynamic_params, 0, sizeof(dynamic_params));

            params_type = MIX_ENC_PARAMS_FRAME_RATE;
            dynamic_params.frame_rate_denom = 1;
            dynamic_params.frame_rate_num   = avcEncFramerate.xEncodeFramerate >> 16;  // Q16 format
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

    temp_coded_data_buffer = NULL;

    uint major, minor;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

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
    vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsenc_h264_new());

    mvp = MIX_PARAMS(mix_videoencodeparams_new());

    if (!vcp || !mvp ) {
        LOGE("%s(),%d: exit, failed to allocate vcp, mvp\n",
             __func__, __LINE__);
        goto error_out;
    }

    oret = ChangeVcpWithPortParam(vcp,
                                  static_cast<PortVideo *>(ports[INPORT_INDEX]),
                                  static_cast<PortVideo *>(ports[OUTPORT_INDEX]),
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
            LOGE("%s(),%d: exit, mix_displayandroid_set_display failed "
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

    oret = RequestShareBuffers(mix,
                               MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_width,
                               MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_height);
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d:  InitShareBufferSettings() failed ", __func__, __LINE__);
        goto error_out;
    }

    mix_videoconfigparamsenc_set_share_buf_mode(MIX_VIDEOCONFIGPARAMSENC(vcp), FALSE);

    LOGV("mix_video_configure");
    mret = mix_video_configure(mix, vcp, NULL);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_video_configure failed "
             "(ret:0x%08x)", __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        if(mret == MIX_RESULT_NO_MEMORY) {
            oret = OMX_ErrorInsufficientResources;
        } else if(mret == MIX_RESULT_NOT_PERMITTED) {
            oret = (OMX_ERRORTYPE)OMX_ErrorIntelVideoNotPermitted;
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

    b_config_sent = false;

    video_len = 0;
    video_data = NULL;

    LOGV("temp coded buffer %dx%d", MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_width,
         MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_height);
    temp_coded_data_buffer_size = MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_width *
                                  MIX_VIDEOCONFIGPARAMSENC(vcp)->picture_height * 400 / 16 / 16;
    temp_coded_data_buffer = new OMX_U8 [temp_coded_data_buffer_size];

    oret = RegisterShareBufferToPort();
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d RegisterShareBufferToPort() failed", __func__, __LINE__);
        oret = OMX_ErrorUndefined;
        goto error_out;
    }

    oret = RegisterShareBufferToLib();
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d register Share Buffering Mode  failed", __func__, __LINE__);
        goto error_out;
    }

    oret = EnterShareBufferingMode();
    if (oret != OMX_ErrorNone) {
        LOGE("%s(), %d EnterShareBufferingMode() failed", __func__, __LINE__);
        goto error_out;
    }

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, oret);
    return oret;

error_out:
    if (buffer_sharing_state == BUFFER_SHARING_EXECUTING) {
        ExitShareBufferingMode();
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
        oret = ExitShareBufferingMode();
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

bool MrstPsbComponent::DetectSyncFrame(OMX_U8* coded_buf, OMX_U32 coded_len)
{
    AvcNaluType nalu_type;
    OMX_U8 *nal;
    OMX_U32 nal_len;

    SplitNalByStartCode(coded_buf, coded_len , &nal, &nal_len, NAL_STARTCODE_4_BYTE);
    if(nal == NULL)
    {
        LOGE("%s:%d: No nal unit extracted", __func__, __LINE__);
        return false;
    }
    nalu_type = GetNaluType(nal, NAL_STARTCODE_4_BYTE);

    if ( nalu_type == CODED_SLICE_IDR ) {
        return true;
    };

    return false;
}

OMX_ERRORTYPE MrstPsbComponent::ExtractConfigData(OMX_U8* coded_buf, OMX_U32 coded_len,
        OMX_U8** config_buf, OMX_U32* config_len,
        OMX_U8** video_buf, OMX_U32* video_len)
{
    AvcNaluType nalu_type;
    OMX_U8 *nal;
    OMX_U32 nal_len;

    *config_buf = NULL;
    *config_len = 0;
    *video_buf = NULL;
    *video_len = 0;

    SplitNalByStartCode(coded_buf, coded_len , &nal, &nal_len, NAL_STARTCODE_4_BYTE);
    if(nal == NULL)
    {
        LOGE("%s:%d: No nal unit extracted", __func__, __LINE__);
        return OMX_ErrorUndefined;
    }
    nalu_type = GetNaluType(nal, NAL_STARTCODE_4_BYTE);

    //Stagefright need SPS+PPS as codec data
    if(nalu_type == SPS)
    {
        *config_buf = coded_buf;
        *config_len += nal_len;

        //followed must be PPS
        SplitNalByStartCode(coded_buf+*config_len, coded_len-*config_len,
                            &nal,&nal_len,NAL_STARTCODE_4_BYTE);
        nalu_type = GetNaluType(nal, NAL_STARTCODE_4_BYTE);
        if(nalu_type != PPS)
        {
            LOGE("%s:%d: No trailing PPS after SPS", __func__, __LINE__);
            return OMX_ErrorUndefined;
        }
        *config_len += nal_len;

        //copy remaining video data to temp buffer
        *video_len = coded_len - *config_len;
        if(*video_len > temp_coded_data_buffer_size)
        {
            LOGE("%s:%d: temp_coded_data_buffer_size is too small", __func__, __LINE__);
            return OMX_ErrorUndefined;
        }
        memcpy(temp_coded_data_buffer,coded_buf + *config_len,*video_len);
        *video_buf = temp_coded_data_buffer;
    }
    else if(nalu_type == PPS)
    {
        //FIXME: PPS is considered as codec config info,
        *config_buf = coded_buf;
        *config_len += nal_len;
        //copy remaining video data to temp buffer
        *video_len = coded_len - *config_len;
        if(*video_len > temp_coded_data_buffer_size)
        {
            LOGE("%s:%d: temp_coded_data_buffer_size is too small", __func__, __LINE__);
            return OMX_ErrorUndefined;
        }
        memcpy(temp_coded_data_buffer,coded_buf + *config_len, *video_len);
        *video_buf = temp_coded_data_buffer;
    }
    else
    {
        //video data nalu
        *video_buf = coded_buf;
        *video_len = coded_len;
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

    OMX_U8* config_data = NULL;
    OMX_U32 config_len = 0;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s():  <******avc******> enter encode\n", __func__);

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

    buffer_out.data = buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    buffer_out.data_size = 0;
    buffer_out.buffer_size = buffers[OUTPORT_INDEX]->nAllocLen - buffers[OUTPORT_INDEX]->nOffset;
    mixiovec_out[0] = &buffer_out;

normal_start:
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
                               (ulong)this, AvcEncMixBufferCallback);

    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_buffer_set_data failed (ret:0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    /* encoder */
    if (avcEncNaluFormatType == OMX_NaluFormatZeroByteInterleaveLength) {

        LOGE("Not support yet");
        oret = OMX_ErrorUndefined;
        goto out;

    } else if (avcEncNaluFormatType == OMX_NaluFormatStartCodes) {
        /*
        FIXME: Stagefright requires:
        1. SPS and PPS are packed together in single output buffer;
        2. Only 1 SPS & PPS buffer is sent in one session;
        */
        LOGV("in buffer = 0x%x ts = %lld",
             buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset,
             buffers[INPORT_INDEX]->nTimeStamp);

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
            if(OMX_ErrorNone != oret )
            {
                LOGE("%s(), %d: exit, ExtractConfigData() failed (ret == 0x%08x)\n",__func__, __LINE__, oret);
                goto out;
            }

        }
        if( config_len != 0) {
            //config data needs to be sent completely in a ProcessorProcess() cycle
            if(!b_config_sent) {
                outfilledlen = config_len;
                outflags |= OMX_BUFFERFLAG_CODECCONFIG;
                b_config_sent = true;
                if (buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset != config_data) {
                    memcpy(buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset, config_data, config_len);
                }
            }
            else {
#if 1           //stagefright only accept the first set of config data, hence discard following ones
                outfilledlen = 0;
#else           //not discard following config data, might cause some composing issue
                outfilledlen = config_len;
                if (buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset != config_data) {
                    memcpy(buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset, config_data, config_len);
                }
#endif
            }
            outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
        }
        else {
            //video data might be sent one or multiple times. current implementation sent data all at once
            outfilledlen = video_len;
            if (DetectSyncFrame(video_data, video_len)) {
                outflags |= OMX_BUFFERFLAG_SYNCFRAME;
            }

            if (buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset != video_data) {
                memcpy(buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset, video_data, video_len);
            }

            video_data = NULL;
            video_len = 0;

            outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
        }

        if (outfilledlen > 0) {
            retain[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
        } else {
            retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
        }

        if (video_len == 0) {
            retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;  //release by callback
        } else {
            retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;  //get again
        }
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

    LOGV("output buffers = %p:%d, flag = %x",
         buffers[OUTPORT_INDEX]->pBuffer,
         outfilledlen,
         outflags);

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

    if (avcEncParamIntelBitrateType.eControlRate != OMX_Video_Intel_ControlRateVideoConferencingMode) {
        if (oret == (OMX_ERRORTYPE) OMX_ErrorIntelExtSliceSizeOverflow) {
            oret = OMX_ErrorNone;
        }
    }

    LOGV_IF(oret == OMX_ErrorNone,
            "%s(),%d: exit, encode is done\n", __func__, __LINE__);

    return oret;
}

OMX_ERRORTYPE MrstPsbComponent::SplitNalByStartCode(OMX_U8* buf, OMX_U32 len,
        OMX_U8** nalbuf, OMX_U32* nallen,
        NalStartCodeType startcode_type)
{
    if (buf == NULL || len == 0 ||
            nalbuf == NULL || nallen == NULL) {
        return OMX_ErrorBadParameter;
    };

    *nalbuf = NULL;
    *nallen = 0;

    OMX_U32 ofst = 0;
    OMX_U8 *data = buf;
    OMX_U8 *next_nalbuf;

    if (startcode_type == NAL_STARTCODE_3_BYTE) {
        while(ofst < len - 2) {
            if ( data[0] ==0x00 &&
                    data[1] == 0x00 &&
                    data[2] == 0x01 ) {
                *nalbuf = data;
                break;
            }
            data ++;
            ofst ++;
        };

        if (*nalbuf == NULL) {
            return OMX_ErrorNone;
        };

        data += 3;
        ofst += 3;

        next_nalbuf = NULL;

        while(ofst < len - 2) {
            if (data[0] == 0x00 &&
                    data[1] == 0x00 &&
                    data[2] == 0x01 ) {
                next_nalbuf = data;
                break;
            }
            data ++;
            ofst ++;
        }
    } else if (startcode_type == NAL_STARTCODE_4_BYTE) {
        while(ofst < len - 3) {
            if ( data[0] ==0x00 &&
                    data[1] == 0x00 &&
                    data[2] == 0x00 &&
                    data[3] == 0x01) {
                *nalbuf = data;
                break;
            }
            data ++;
            ofst ++;
        };

        if (*nalbuf == NULL) {
            return OMX_ErrorNone;
        };

        data += 4;
        ofst += 4;

        next_nalbuf = NULL;

        while(ofst < len - 3) {
            if (data[0] == 0x00 &&
                    data[1] == 0x00 &&
                    data[2] == 0x00 &&
                    data[3] == 0x01) {
                next_nalbuf = data;
                break;
            }
            data ++;
            ofst ++;
        }

    } else {
        return OMX_ErrorBadParameter;
    };

    if (next_nalbuf != NULL) {
        *nallen = next_nalbuf - *nalbuf;
    } else {
        *nallen = &buf[len - 1] - *nalbuf + 1;
    };

    return OMX_ErrorNone;
};

AvcNaluType MrstPsbComponent::GetNaluType(OMX_U8* nal,
        NalStartCodeType startcode_type)
{
    if (startcode_type == NAL_STARTCODE_3_BYTE) {
        return static_cast<AvcNaluType>(nal[3] & 0x1F);
    } else if (startcode_type == NAL_STARTCODE_4_BYTE) {
        return static_cast<AvcNaluType>(nal[4] & 0x1F);
    } else {
        return INVALID_NAL_TYPE;
    };
}

/* end of implement ComponentBase::Processor[*] */

OMX_ERRORTYPE MrstPsbComponent::__AvcChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortAvc *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    /* encoder */
    MixVideoConfigParamsEnc *config = MIX_VIDEOCONFIGPARAMSENC(vcp);
    mix_videoconfigparamsenc_set_encode_format(
        config, MIX_ENCODE_TARGET_FORMAT_H264);
    mix_videoconfigparamsenc_set_profile(config, MIX_PROFILE_H264BASELINE);
    mix_videoconfigparamsenc_set_mime_type(config, "video/x-h264");
    mix_videoconfigparamsenc_h264_set_bus(
        MIX_VIDEOCONFIGPARAMSENC_H264(config), 0);
    mix_videoconfigparamsenc_h264_set_dlk(
        MIX_VIDEOCONFIGPARAMSENC_H264(config), FALSE);

    if (avcEncParamIntelBitrateType.eControlRate
            == OMX_Video_Intel_ControlRateVideoConferencingMode) {

        LOGV("%s(), avcEncConfigSliceNumbers.nISliceNumber = %d", __func__, avcEncConfigSliceNumbers.nISliceNumber);
        LOGV("%s(), calls to mix_videoconfigparamsenc_h264_set_I_slice_num()", __func__);

        mix_videoconfigparamsenc_h264_set_I_slice_num(
            MIX_VIDEOCONFIGPARAMSENC_H264(config),
            avcEncConfigSliceNumbers.nISliceNumber);

        LOGV("%s(), avcEncConfigSliceNumbers.nPSliceNumber = %d", __func__, avcEncConfigSliceNumbers.nPSliceNumber);
        LOGV("%s(), calls to mix_videoconfigparamsenc_h264_set_P_slice_num()", __func__);

        mix_videoconfigparamsenc_h264_set_P_slice_num(
            MIX_VIDEOCONFIGPARAMSENC_H264(config),
            avcEncConfigSliceNumbers.nPSliceNumber);

        if(avcEncConfigAir.bAirEnable == OMX_TRUE) {
            mix_videoconfigparamsenc_set_refresh_type(config, MIX_VIDEO_AIR);

            MixAIRParams mixairparam;
            mixairparam.air_auto = avcEncConfigAir.bAirAuto;
            mixairparam.air_MBs  = avcEncConfigAir.nAirMBs;
            mixairparam.air_threshold = avcEncConfigAir.nAirThreshold;
            mix_videoconfigparamsenc_set_AIR_params(config, mixairparam);

        } else {
            mix_videoconfigparamsenc_set_refresh_type(config, MIX_VIDEO_NONIR);
        }

    } else {
        mix_videoconfigparamsenc_h264_set_slice_num(
            MIX_VIDEOCONFIGPARAMSENC_H264(config), 1);
    }


    if (avcEncNaluFormatType == OMX_NaluFormatStartCodes) {
        mix_videoconfigparamsenc_h264_set_delimiter_type(
            MIX_VIDEOCONFIGPARAMSENC_H264(config), MIX_DELIMITER_ANNEXB);
        LOGV("%s(), use MIX_DELIMITER_ANNEXB", __func__);
    } else {
        mix_videoconfigparamsenc_h264_set_delimiter_type(
            MIX_VIDEOCONFIGPARAMSENC_H264(config),
            MIX_DELIMITER_LENGTHPREFIX);
        LOGV("%s(), use MIX_DELIMITER_LENGTHPREFIX", __func__);
    }

    if(avcEncIDRPeriod == 0)   //FIXME: avcEncIDRPeriod should be set dynamic
        avcEncIDRPeriod = 1;

    LOGV("%s() : avcEncIDRPeriod = %d", __func__, avcEncIDRPeriod);
    mix_videoconfigparamsenc_h264_set_IDR_interval(
        MIX_VIDEOCONFIGPARAMSENC_H264(config), avcEncIDRPeriod);

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

    ret = __AvcChangeVcpWithPortParam(vcp,
                                      static_cast<PortAvc *>(port_out),
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

    if (config->frame_rate_num != (pd_in->format.video.xFramerate >> 16)) {
        LOGV("%s(): framerate : %u != %ld", __func__,
             config->frame_rate_num, pd_in->format.video.xFramerate >> 16);

        mix_videoconfigparamsenc_set_frame_rate(config,
                                                pd_in->format.video.xFramerate >> 16,
                                                1);


        if(avcEncFramerate.xEncodeFramerate != 0) {
            mix_videoconfigparamsenc_set_frame_rate(config,
                                                    avcEncFramerate.xEncodeFramerate >> 16, 1);
        } else {
            avcEncFramerate.xEncodeFramerate = pd_in->format.video.xFramerate;
        }

        if (vcp_changed)
            *vcp_changed = true;
    }

    if(avcEncPFrames == 0) {
        avcEncPFrames = config->frame_rate_num / 2;
    }

    LOGV("%s() : avcEncPFrames = %d", __func__, avcEncPFrames);
    mix_videoconfigparamsenc_set_intra_period(config, avcEncPFrames);

    if (avcEncParamIntelBitrateType.eControlRate
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

        mix_videoconfigparamsenc_set_init_qp(config, 24);
        mix_videoconfigparamsenc_set_min_qp(config, 1);
        mix_videoconfigparamsenc_set_buffer_pool_size(config, 8);
        mix_videoconfigparamsenc_set_drawable(config, 0x0);
        mix_videoconfigparamsenc_set_need_display(config, FALSE);

    } else {

        LOGV("%s(), eControlRate != OMX_Video_Intel_ControlRateMax, AVC", __func__);
        config->rate_control = MIX_RATE_CONTROL_VCM;

        if (avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateConstant
                || avcEncParamIntelBitrateType.eControlRate
                == OMX_Video_Intel_ControlRateVariable) {

            if (config->bitrate
                    != avcEncParamIntelBitrateType.nTargetBitrate) {
                LOGV("%s(): bitrate : %d != %ld", __func__,
                     config->bitrate,
                     avcEncParamIntelBitrateType.nTargetBitrate);

                mix_videoconfigparamsenc_set_bit_rate(config,
                                                      avcEncParamIntelBitrateType.nTargetBitrate);

                if (vcp_changed)
                    *vcp_changed = true;
            }
        } else if (avcEncParamIntelBitrateType.eControlRate
                   == OMX_Video_Intel_ControlRateVideoConferencingMode) {

            LOGV("%s(), VCM mode", __func__);

            // call mix api to set the following

            mix_videoconfigparamsenc_set_bit_rate(config,
                                                  avcEncConfigIntelBitrateType.nMaxEncodeBitrate);

            LOGV("%s(), nMaxEncodeBitrate = %d", __func__,
                 avcEncConfigIntelBitrateType.nMaxEncodeBitrate);

            mix_videoconfigparamsenc_set_target_percentage(config,
                    avcEncConfigIntelBitrateType.nTargetPercentage);

            LOGV("%s(), nTargetPercentage = %d", __func__,
                 avcEncConfigIntelBitrateType.nTargetPercentage);

            mix_videoconfigparamsenc_set_window_size(config,
                    avcEncConfigIntelBitrateType.nWindowSize);

            LOGV("%s(), nWindowSize = %d", __func__,
                 avcEncConfigIntelBitrateType.nWindowSize);

            mix_videoconfigparamsenc_set_init_qp(config,
                                                 avcEncConfigIntelBitrateType.nInitialQP);

            LOGV("%s(), nInitialQP = %d", __func__,
                 avcEncConfigIntelBitrateType.nInitialQP);

            mix_videoconfigparamsenc_set_min_qp(config,
                                                avcEncConfigIntelBitrateType.nMinQP);

            LOGV("%s(), nMinQP = %d", __func__,
                 avcEncConfigIntelBitrateType.nMinQP);

            mix_videoconfigparamsenc_set_max_slice_size(config,
                    avcEncConfigNalSize.nNaluBytes * 8); // bits

            LOGV("%s(), nNaluBytes = %d", __func__,
                 avcEncConfigNalSize.nNaluBytes);

        }

        /* hard coding */
        mix_videoconfigparamsenc_set_raw_format(config,
                                                MIX_RAW_TARGET_FORMAT_YUV420);
        mix_videoconfigparamsenc_set_buffer_pool_size(config, 8);
        mix_videoconfigparamsenc_set_drawable(config, 0x0);
        mix_videoconfigparamsenc_set_need_display(config, FALSE);
    }

    return ret;
}

/* end of vcp setting helpers */

/* share buffer setting */
OMX_ERRORTYPE MrstPsbComponent::EnableBufferSharingMode()
{
    BufferShareStatus bsret;

    if (buffer_sharing_state != BUFFER_SHARING_INVALID) {
        LOGE("%s(),%d: invoke %s failed (incorrect state).", __func__, __LINE__, __func__);
        return OMX_ErrorUndefined;
    }

    buffer_sharing_count = 4;
    buffer_sharing_info = NULL;
    buffer_sharing_lib = BufferShareRegistry::getInstance();
    bsret = buffer_sharing_lib->encoderEnableSharingMode();
    if (bsret != BS_SUCCESS) {
        LOGE("%s(),%d:   encoder enable buffer sharing mode failed:%d", __func__, __LINE__, bsret);
        return OMX_ErrorUndefined;
    }

    buffer_sharing_state = BUFFER_SHARING_LOADED;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::DisableBufferSharingMode()
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

    bsret = buffer_sharing_lib->encoderDisableSharingMode();
    if (bsret != BS_SUCCESS) {
        LOGE("%s(),%d:   disable sharing mode failed.", __func__, __LINE__);
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

OMX_ERRORTYPE MrstPsbComponent::RegisterShareBufferToLib()
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

OMX_ERRORTYPE MrstPsbComponent::RegisterShareBufferToPort()
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

OMX_ERRORTYPE MrstPsbComponent::EnterShareBufferingMode()
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

OMX_ERRORTYPE MrstPsbComponent::ExitShareBufferingMode()
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

    if ((port_index == INPORT_INDEX || port_index == OMX_ALL)) {
        ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
        mix_video_flush( mix);
    }
    return OMX_ErrorNone;
}

void MrstPsbComponent::AvcEncMixBufferCallback(ulong token, uchar *data) {
    MrstPsbComponent *_this = (MrstPsbComponent *) token;

    LOGV("AvcEncMixBufferCallback Begin\n");

    if(_this) {
        _this->ports[_this->INPORT_INDEX]->ReturnAllRetainedBuffers();
    }

    LOGV("AvcEncMixBufferCallback End\n");
}

/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB.AVC.Enc";

static const char *g_roles[] =
{
    (const char *)"video_encoder.avc",
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
