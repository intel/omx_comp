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

// #define LOG_NDEBUG 0

#define LOG_TAG "mrst_psb"
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
#include <mixvideoconfigparamsdec_h264.h>
#include <mixvideoconfigparamsenc_h264.h>
#include <mixvideoconfigparamsdec_mp42.h>
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

    bool isencoder;

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

    working_role = strpbrk(working_role, ".");
    if (!working_role)
        return OMX_ErrorUndefined;
    working_role++;

    if (!strcmp(working_role, "avc")) {
        ret = __AllocateAvcPort(codec_port_index, codec_port_dir);
        coding_type = OMX_VIDEO_CodingAVC;
    }
    else if (!strcmp(working_role, "mpeg4")) {
        ret = __AllocateMpeg4Port(codec_port_index, codec_port_dir);
        coding_type = OMX_VIDEO_CodingMPEG4;
    }
    else if (!strcmp(working_role, "h263")) {
        ret = __AllocateH263Port(codec_port_index, codec_port_dir);
        coding_type = OMX_VIDEO_CodingH263;
    }
    else
        ret = OMX_ErrorUndefined;

    if (ret != OMX_ErrorNone)
        goto free_ports;


    if(isencoder) {

        LOGV("---- prepare to call __AllocateRawPort() ----\n");

        ret = __AllocateRawPort(raw_port_index, raw_port_dir);
    } else {

        LOGV("---- prepare to call __AllocateRawVAPort() ----\n");

        ret = __AllocateRawVAPort(raw_port_index, raw_port_dir);
    }
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    codec_mode = isencoder ? MIX_CODEC_MODE_ENCODE : MIX_CODEC_MODE_DECODE;

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
    coding_type = OMX_VIDEO_CodingUnused;

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
        LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
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

    /* encoder */
    if (dir == OMX_DirOutput) {
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
        avcEncNaluFormatType = OMX_NaluFormatZeroByteInterleaveLength;

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

    } else {

        avcDecFrameWidth  = 0;
        avcDecFrameHeight = 0;
        memset(&avcDecodeSettings, 0, sizeof(avcDecodeSettings));
        avcDecodeSettings.nMaxNumberOfReferenceFrame = 4;
        avcDecGotRes = OMX_FALSE;

    }

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
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
    if (dir == OMX_DirInput) {
        mpeg4portdefinition.nBufferCountActual =
            INPORT_MPEG4_ACTUAL_BUFFER_COUNT;
        mpeg4portdefinition.nBufferCountMin = INPORT_MPEG4_MIN_BUFFER_COUNT;
        mpeg4portdefinition.nBufferSize = INPORT_MPEG4_BUFFER_SIZE;
    } else {
        mpeg4portdefinition.nBufferCountActual =
            OUTPORT_MPEG4_ACTUAL_BUFFER_COUNT;
        mpeg4portdefinition.nBufferCountMin = OUTPORT_MPEG4_MIN_BUFFER_COUNT;
        mpeg4portdefinition.nBufferSize = OUTPORT_MPEG4_BUFFER_SIZE;
    }
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
    mpeg4portdefinition.format.video.eCompressionFormat =
        OMX_VIDEO_CodingMPEG4;
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
    mpeg4portparam.eLevel = OMX_VIDEO_MPEG4Level3;

    mpeg4port->SetPortMpeg4Param(&mpeg4portparam, true);
    /* end of OMX_VIDEO_PARAM_MPEG4TYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
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
    h263portparam.eLevel = OMX_VIDEO_H263Level10;

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

/*
   Add OMX_COLOR_FormatVendorStartUnused + 0xA00E00 -> PVMF_MIME_RAWVA in file
   external/opencore/nodes/pvomxvideodecnode/src/pvmf_omx_videodec_node.cpp

*/
OMX_ERRORTYPE MrstPsbComponent::__AllocateRawVAPort(OMX_U32 port_index,
        OMX_DIRTYPE dir)
{
    PortVideo *rawport;

    OMX_PARAM_PORTDEFINITIONTYPE rawportdefinition;
    OMX_VIDEO_PARAM_PORTFORMATTYPE rawvideoparam;

    LOGV("%s(): enter\n", __func__);

    if(dir != OMX_DirOutput) {
        return OMX_ErrorBadParameter;
    }

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

    rawportdefinition.nBufferCountActual = OUTPORT_RAW_ACTUAL_BUFFER_COUNT;
    rawportdefinition.nBufferCountMin = OUTPORT_RAW_MIN_BUFFER_COUNT;
    rawportdefinition.nBufferSize = sizeof(VABuffer) + 16; /* OUTPORT_RAW_BUFFER_SIZE; */

    rawportdefinition.bEnabled = OMX_TRUE;
    rawportdefinition.bPopulated = OMX_FALSE;
    rawportdefinition.eDomain = OMX_PortDomainVideo;
    rawportdefinition.format.video.cMIMEType = (char *)"video/x-raw-va"; // (char *)"video/raw";
    rawportdefinition.format.video.pNativeRender = NULL;
    rawportdefinition.format.video.nFrameWidth = 176;
    rawportdefinition.format.video.nFrameHeight = 144;
    rawportdefinition.format.video.nStride = 176;
    rawportdefinition.format.video.nSliceHeight = 144;
    rawportdefinition.format.video.nBitrate = 64000;
    rawportdefinition.format.video.xFramerate = 15 << 16;
    rawportdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    rawportdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawportdefinition.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)0x7FA00E00;
    rawportdefinition.format.video.pNativeWindow = NULL;
    rawportdefinition.bBuffersContiguous = OMX_FALSE;
    rawportdefinition.nBufferAlignment = 0;

    rawport->SetPortDefinition(&rawportdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_PORTFORMATTYPE */
    rawvideoparam.nPortIndex = port_index;
    rawvideoparam.nIndex = 0;
    rawvideoparam.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawvideoparam.eColorFormat = (OMX_COLOR_FORMATTYPE)0x7FA00E00;

    rawport->SetPortVideoParam(&rawvideoparam, true);

    /* end of OMX_VIDEO_PARAM_PORTFORMATTYPE */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}
/* end of core methods & helpers */


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
if (codec_mode != MIX_CODEC_MODE_ENCODE) { \
    LOGE("%s(), wrong codec mode", __func__); \
    return OMX_ErrorUnsupportedIndex; \
} \
if (coding_type != OMX_VIDEO_CodingAVC) { \
    LOGE("%s(), wrong coding type", __func__); \
    return OMX_ErrorUnsupportedIndex; \
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
    case OMX_IndexParamVideoMpeg4: {
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
    case OMX_IndexParamVideoH263: {
        OMX_VIDEO_PARAM_H263TYPE *p =
            (OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortH263 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortH263 *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortH263Param(), sizeof(*p));
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
    case OMX_IndexIntelPrivateInfo: {
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentParameterStructure;
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
    case OMX_IndexParamNalStreamFormat:
    case OMX_IndexParamNalStreamFormatSupported: {
        OMX_NALSTREAMFORMATTYPE *p =
            (OMX_NALSTREAMFORMATTYPE *)pComponentParameterStructure;

        LOGV("%s(), OMX_IndexParamNalStreamFormat or OMX_IndexParamNalStreamFormatSupported", __func__);

        AVC_ENCODE_ERROR_CHECKING(p)

#if 0
        OMX_STATETYPE state;
        CBaseGetState((void *)GetComponentHandle(), &state);
        if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorIncorrectStateOperation);
            return OMX_ErrorIncorrectStateOperation;
        }
#endif
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
    case OMX_IndexParamVideoMpeg4: {
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
    case OMX_IndexParamVideoH263: {
        OMX_VIDEO_PARAM_H263TYPE *p =
            (OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortH263 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortH263 *>(ports[index]);

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

        ret = port->SetPortH263Param(p, false);
        break;
    }
    case OMX_IndexParamVideoBitrate: {

        LOGV("%s(), OMX_IndexParamVideoBitrate", __func__);
        avcEncParamIntelBitrateType.eControlRate
        = OMX_Video_Intel_ControlRateMax;

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
    case OMX_IndexIntelPrivateInfo: {

        LOGV("%s(), OMX_IndexIntelPrivateInfo", __func__);
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentParameterStructure;
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

        ret = port->SetPortPrivateInfoParam(p, false);
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

        if (coding_type != OMX_VIDEO_CodingAVC) {
            LOGE("%s(), wrong coding type", __func__);
            return OMX_ErrorUnsupportedIndex;
        }

        OMX_U32 index = pAvcDecodeSettings->nPortIndex;
        /*        if (index != OUTPORT_INDEX) {
                    LOGE("%s(), wrong port index", __func__);
                    return OMX_ErrorBadPortIndex;
                }
        */
        PortAvc *port = static_cast<PortAvc *> (ports[index]);
        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            if(pAvcDecodeSettings->nMaxNumberOfReferenceFrame != 0)
                avcDecodeSettings = *pAvcDecodeSettings;
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

    case OMX_IndexConfigVideoAVCIntraPeriod:
    {
        LOGV("%s(), OMX_IndexConfigVideoAVCIntraPeriod", __func__);

        pVideoIDRInterval = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentConfigStructure;

        AVC_ENCODE_ERROR_CHECKING(pVideoIDRInterval)

        LOGV("%s(), OMX_IndexConfigVideoAVCIntraPeriod : nIDRPeriod = %d, nPFrames = %d",
             __func__, pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);

        MixEncDynamicParams encdynareq;
        oscl_memset(&encdynareq, 0, sizeof(encdynareq));
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
            LOGE("%s(), eControlRate == OMX_Video_Intel_ControlRateMax");
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
            oscl_memset(&dynamic_params, 0, sizeof(dynamic_params));


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
            oscl_memset(&dynamic_params, 0, sizeof(dynamic_params));

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
            oscl_memset(&dynamic_params, 0, sizeof(dynamic_params));

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
            oscl_memset(&dynamic_params, 0, sizeof(dynamic_params));

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
            oscl_memset(&dynamic_params, 0, sizeof(dynamic_params));

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

    OMX_U32 port_index = (OMX_U32)-1;

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

    /* decoder */
    if (coding_type == OMX_VIDEO_CodingAVC) {
        vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsdec_h264_new());
    }
    else if (coding_type == OMX_VIDEO_CodingMPEG4 || coding_type == OMX_VIDEO_CodingH263) {
        vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsdec_mp42_new());
    }

    mvp = MIX_PARAMS(mix_videodecodeparams_new());
    port_index = INPORT_INDEX;

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

    mret = mix_videodecodeparams_set_discontinuity(
               MIX_VIDEODECODEPARAMS(mvp), FALSE);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_videodecodeparams_set_discontinuity "
             "(ret == 0x%08x)", __func__, __LINE__, mret);
        goto error_out;
    }

    mret = mix_video_initialize(mix, codec_mode, vip, NULL);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_video_initialize failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto error_out;
    }

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

    avc_enc_frame_size_left = 0;

    avc_enc_buffer = NULL;
    avc_enc_buffer_length = 0;
    avc_enc_buffer_offset = 0;

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

    //to release glib thread resource;
    //g_thread_deinit();

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

#if 0
    if (codec_mode == MIX_CODEC_MODE_ENCODE && coding_type
            == OMX_VIDEO_CodingAVC) {

        ports[OUTPORT_INDEX]->ReturnAllRetainedBuffers();
        inframe_counter = 0;
        outframe_counter = 0;
    }
#endif

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

    VABuffer *vaBuf;
    int retry_decode_count;

    LOGE("%s(): <**************> enter decode\n", __func__);

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

    if(is_mixvideodec_configured) {
        MixVideoFrame *frame = NULL;
        vaBuf = (VABuffer *)(mixiovec_out[0]->data);

        mret = mix_video_get_frame(mix, &frame);
        if (mret != MIX_RESULT_SUCCESS) {
            if (mret == MIX_RESULT_FRAME_NOTAVAIL) {
                LOGV("%s(), no more frames, continue to decode "
                     "(ret == 0x%08x)", __func__, mret);
                goto normal_start;
            }
            else {
                LOGE("%s(), %d mix_video_get_frame() failed (ret == 0x%08x)",
                     __func__, __LINE__, mret);
                oret = OMX_ErrorUndefined;
                goto local_cleanup;
            }
        }

        mret = mix_videoframe_get_frame_id(frame, (ulong *)&vaBuf->surface);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d mix_videoframe_get_frame_id() failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto local_cleanup;
        }
        mret = mix_videoframe_get_vadisplay(frame, (void **)&vaBuf->display);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d mix_videoframe_get_vadisplay() failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto local_cleanup;
        }

        mret = mix_videoframe_get_timestamp(frame, (uint64 *)&outtimestamp);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d mix_videoframe_get_timestamp() failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto local_cleanup;
        }

        LOGV("--- Output timestamp = %"G_GINT64_FORMAT"\n ---", outtimestamp);

        mret = mix_videoframe_get_frame_structure(frame, (uint32 *)&vaBuf->frame_structure);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d mix_videoframe_get_frame_structure() failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto local_cleanup;
        }

        LOGV(" frame_structure = %d \n", vaBuf->frame_structure);

        outfilledlen = sizeof(VABuffer);
        LOGV("In OMX DEC vadisplay = %x surfaceid = %x outfilledlen = %d\n",
             vaBuf->display,  vaBuf->surface, outfilledlen);

        outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
        retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;

        outframe_counter++;

local_cleanup:

        if(frame) {
            mix_video_release_frame(mix, frame);
        }

        LOGV("%s(),%d: exit, done\n", __func__, __LINE__);
        return oret;
    }

normal_start:

    if (coding_type == OMX_VIDEO_CodingH263) {

        LOGV("data = 0x%x size = %d inframe_counter = %d",  buffer_in.data, buffer_in.data_size, inframe_counter);

        int min_size = buffer_in.data_size;
        if(min_size > 8 ) {
            min_size = 8;
        }
        for(int i = 0; i < min_size; i++) {
            LOGV("0x%x ", buffer_in.data[i]);
        }

        if (inframe_counter == 0) {
            oret = ChangePortParamWithCodecData(buffer_in.data,
                                                buffer_in.data_size,
                                                ports);
            if (oret != OMX_ErrorNone) {
                LOGE("%s(),%d: exit ChangePortParamWithCodecData failed "
                     "(ret:0x%08x)\n",
                     __func__, __LINE__, oret);
                goto out;
            }

            oret = ChangeVcpWithPortParam(vcp,
                                          static_cast<PortVideo *>(ports[INPORT_INDEX]),
                                          NULL);
            if (oret != OMX_ErrorNone) {
                LOGE("%s(),%d: exit ChangeVcpWithPortParam failed "
                     "(ret:0x%08x)\n", __func__, __LINE__, oret);
                goto out;
            }

            mret = mix_videoconfigparamsdec_set_header(
                       MIX_VIDEOCONFIGPARAMSDEC(vcp), &buffer_in);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGE("%s(), %d: exit, "
                     "mix_videoconfigparamsdec_set_header failed "
                     "(ret:0x%08x)", __func__, __LINE__, mret);
                oret = OMX_ErrorUndefined;
                goto out;
            }
            mret = mix_video_configure(mix, vcp, NULL);
            if (mret != MIX_RESULT_SUCCESS) {
                LOGE("%s(), %d: exit, mix_video_configure failed "
                     "(ret:0x%08x)", __func__, __LINE__, mret);
                oret = OMX_ErrorUndefined;
                if(mret == MIX_RESULT_NO_MEMORY) {
                    oret = OMX_ErrorInsufficientResources;
                } else if(mret == MIX_RESULT_NOT_PERMITTED) {
                    oret = (OMX_ERRORTYPE)OMX_ErrorIntelVideoNotPermitted;;
                }
                goto out;
            }
            is_mixvideodec_configured = OMX_TRUE;
            LOGV("%s(): mix video configured", __func__);

            /*
             * port reconfigure
             */
            ports[OUTPORT_INDEX]->ReportPortSettingsChanged();
        }
    }

    /* only in case of decode mode */
    if ((buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) &&
            (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG)) {

        if (coding_type == OMX_VIDEO_CodingAVC) {
            if (inframe_counter == 0) {

#if 0
                oret = ChangePortParamWithCodecData(buffer_in.data,
                                                    buffer_in.data_size, ports);
                if (oret != OMX_ErrorNone) {
                    LOGE("%s(),%d: exit ChangePortParamWithCodecData failed "
                         "(ret:0x%08x)\n", __func__, __LINE__, oret);
                    goto out;
                }
#endif

                oret = ChangeVcpWithPortParam(vcp,
                                              static_cast<PortVideo *> (ports[INPORT_INDEX]), NULL);
                if (oret != OMX_ErrorNone) {
                    LOGE("%s(),%d: exit ChangeVcpWithPortParam failed "
                         "(ret:0x%08x)\n", __func__, __LINE__, oret);
                    goto out;
                }

                mret = mix_video_configure(mix, vcp, NULL);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGE("%s(), %d: exit, mix_video_configure failed "
                         "(ret:0x%08x)", __func__, __LINE__, mret);
                    oret = OMX_ErrorUndefined;
                    if(mret == MIX_RESULT_NO_MEMORY) {
                        oret = OMX_ErrorInsufficientResources;
                    } else if(mret == MIX_RESULT_NOT_PERMITTED) {
                        oret = (OMX_ERRORTYPE)OMX_ErrorIntelVideoNotPermitted;;
                    }
                    goto out;
                }
                is_mixvideodec_configured = OMX_TRUE;
                LOGV("%s(): mix video configured", __func__);

#if 0
                /*
                 * port reconfigure
                 */
                ports[OUTPORT_INDEX]->ReportPortSettingsChanged();
#endif
            } /* inframe_counter */
        } /* OMX_VIDEO_CodingAVC */
        else if (coding_type == OMX_VIDEO_CodingMPEG4) {
            if (inframe_counter == 0) {
                oret = ChangePortParamWithCodecData(buffer_in.data,
                                                    buffer_in.data_size,
                                                    ports);
                if (oret != OMX_ErrorNone) {
                    LOGE("%s(),%d: exit ChangePortParamWithCodecData failed "
                         "(ret:0x%08x)\n",
                         __func__, __LINE__, oret);
                    goto out;
                }

                oret = ChangeVcpWithPortParam(vcp,
                                              static_cast<PortVideo *>(ports[INPORT_INDEX]),
                                              NULL);
                if (oret != OMX_ErrorNone) {
                    LOGE("%s(),%d: exit ChangeVcpWithPortParam failed "
                         "(ret:0x%08x)\n", __func__, __LINE__, oret);
                    goto out;
                }

                mret = mix_videoconfigparamsdec_set_header(
                           MIX_VIDEOCONFIGPARAMSDEC(vcp), &buffer_in);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGE("%s(), %d: exit, "
                         "mix_videoconfigparamsdec_set_header failed "
                         "(ret:0x%08x)", __func__, __LINE__, mret);
                    oret = OMX_ErrorUndefined;
                    goto out;
                }
                mret = mix_video_configure(mix, vcp, NULL);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGE("%s(), %d: exit, mix_video_configure failed "
                         "(ret:0x%08x)", __func__, __LINE__, mret);
                    oret = OMX_ErrorUndefined;
                    if(mret == MIX_RESULT_NO_MEMORY) {
                        oret = OMX_ErrorInsufficientResources;
                    } else if(mret == MIX_RESULT_NOT_PERMITTED) {
                        oret = (OMX_ERRORTYPE)OMX_ErrorIntelVideoNotPermitted;;
                    }
                    goto out;
                }
                is_mixvideodec_configured = OMX_TRUE;
                LOGV("%s(): mix video configured", __func__);

                /*
                 * port reconfigure
                 */
                ports[OUTPORT_INDEX]->ReportPortSettingsChanged();
                goto out;
            }
        } /* OMX VIDEO_CodingMPEG4 */
    } /* OMX_BUFFERFLAG_ENDOFFRAME && OMX_BUFFERFLAG_CODECCONFIG */

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
                               0, NULL);

    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_buffer_set_data failed (ret:0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    /* decoder */
    MixVideoFrame *frame;

    /* set timestamp */
    outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

    mix_videodecodeparams_set_timestamp(MIX_VIDEODECODEPARAMS(mvp),
                                        outtimestamp);
    LOGV("--- Input timestamp = %"G_GINT64_FORMAT" ---\n", outtimestamp);

    {
        unsigned char *nal = buffer_in.data;
        LOGV("%s(), nal_type = 0x%x", __func__, *nal & 0x1F);
    }

    retry_decode_count = 0;

retry_decode:
    mret = mix_video_decode(mix,
                            mixbuffer_in, 1,
                            MIX_VIDEODECODEPARAMS(mvp));
    if (mret != MIX_RESULT_SUCCESS) {
        if (mret == MIX_RESULT_OUTOFSURFACES) {
            LOGV("%s(),%d: mix_video_decode() failed, "
                 "out of surfaces waits 10000 us and try again\n",
                 __func__, __LINE__);

            if(retry_decode_count ++ > 100) {
                oret = OMX_ErrorUndefined;
                goto out;
            }

            usleep(10000);
            goto retry_decode;
        } else if (mret != MIX_RESULT_DROPFRAME &&
                   mret != MIX_RESULT_ERROR_PROCESS_STREAM &&
                   mret != MIX_RESULT_MISSING_CONFIG) {

            LOGE("%s(),%d: exit, mix_video_decode() failed\n",
                 __func__, __LINE__);

            oret = OMX_ErrorUndefined;
            if(mret == MIX_RESULT_NO_MEMORY) {
                oret = OMX_ErrorInsufficientResources;
            } else if(mret == MIX_RESULT_NOT_PERMITTED) {
                oret = (OMX_ERRORTYPE)OMX_ErrorIntelVideoNotPermitted;;
            }
            goto out;
        }
    }

    {
        MixVideoDecodeParams * decode_params = MIX_VIDEODECODEPARAMS(mvp);
        LOGV("%s(), decode_params->new_sequence = %d", __func__, decode_params->new_sequence);
    }

    /* decide whether or not to send port setting change event */
    if (coding_type == OMX_VIDEO_CodingAVC) {
        if (mret == MIX_RESULT_SUCCESS) {

            MixVideoDecodeParams * decode_params = MIX_VIDEODECODEPARAMS(mvp);
            if (avcDecGotRes == OMX_FALSE || decode_params->new_sequence) {
                LOGV("%s(), avcDecGotRes == OMX_FALSE || decode_params->new_sequence", __func__);

                avcDecGotRes = OMX_TRUE;
                MixVideoConfigParams *config_params = NULL;
                mret = mix_video_get_config(mix, &config_params);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGE("%s(),%d: exit, mix_video_get_config() failed\n",
                         __func__, __LINE__);
                    oret = OMX_ErrorUndefined;
                    goto out;
                }

                PortAvc *avcport =
                    static_cast<PortAvc *> (ports[INPORT_INDEX]);
                OMX_PARAM_PORTDEFINITIONTYPE avcpd;
                memcpy(&avcpd, avcport->GetPortDefinition(),
                       sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

                MixVideoConfigParamsDec *config_params_dec =
                    MIX_VIDEOCONFIGPARAMSDEC(config_params);
                avcDecFrameWidth  = config_params_dec->picture_width -
                    config_params_dec->crop_left - config_params_dec->crop_right;
                avcDecFrameHeight = config_params_dec->picture_height -
                    config_params_dec->crop_top - config_params_dec->crop_bottom;

                if(avcDecodeSettings.nMaxWidth != 0 && avcDecodeSettings.nMaxHeight != 0) {
                    if(avcDecFrameWidth  > avcDecodeSettings.nMaxWidth ||
                            avcDecFrameHeight > avcDecodeSettings.nMaxHeight) {
                        oret = (OMX_ERRORTYPE)OMX_ErrorIntelVideoNotPermitted;
                        goto out;
                    }
                }

                LOGV("%s(), avcDecFrameWidth = %d avcDecFrameHeight = %d",
                     __func__, avcDecFrameWidth, avcDecFrameHeight);
                LOGV("%s(), avcpd.format.video.nFrameWidth = %d avcpd.format.video.nFrameHeight = %d",
                     __func__, avcpd.format.video.nFrameWidth, avcpd.format.video.nFrameHeight);

                if (avcDecFrameWidth != avcpd.format.video.nFrameWidth ||
                        avcDecFrameHeight != avcpd.format.video.nFrameHeight) {

                    LOGV("%s(), calls to ChangePortParamWithCodecData", __func__);
                    oret = ChangePortParamWithCodecData(buffer_in.data,
                                                        buffer_in.data_size, ports);
                    if (oret != OMX_ErrorNone) {
                        LOGE("%s(),%d: exit ChangePortParamWithCodecData failed "
                             "(ret:0x%08x)\n", __func__, __LINE__, oret);
                        goto out;
                    }

                    LOGV("%s(), resolution change is detected!", __func__);
                    LOGV("%s(), calls to ReportPortSettingsChanged", __func__);
                    ports[OUTPORT_INDEX]->ReportPortSettingsChanged();
                }
                mix_videoconfigparams_unref(config_params);
            }
        }
    }

    LOGV("%s(), mix_video_decode() returns 0x%x", __func__, mret);
    if(mret == MIX_RESULT_DROPFRAME ||
            mret == MIX_RESULT_ERROR_PROCESS_STREAM ||
            mret == MIX_RESULT_MISSING_CONFIG ) {
        LOGE("%s(),%d: mix_video_decode() failed, "
             "frame dropped\n",
             __func__, __LINE__);
        /* not an error */
        oret = OMX_ErrorNone;
        retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
        if (mret == MIX_RESULT_ERROR_PROCESS_STREAM) {
            oret = (OMX_ERRORTYPE)OMX_ErrorIntelProcessStream;
        } else if (mret == MIX_RESULT_MISSING_CONFIG) {
            oret = (OMX_ERRORTYPE)OMX_ErrorIntelMissingConfig;
        }
        goto out;
    }

    mret = mix_video_get_frame(mix, &frame);
    if (mret != MIX_RESULT_SUCCESS) {
        if (mret == MIX_RESULT_FRAME_NOTAVAIL) {
            LOGV("%s(), partial frame, waits for next turn "
                 "(ret == 0x%08x)", __func__, mret);
            retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            oret = OMX_ErrorNone;
            goto out;
        }
        else {
            LOGE("%s(), %d mix_video_get_frame() failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto out;
        }
    }

    vaBuf = (VABuffer *)(mixiovec_out[0]->data);
    mret = mix_videoframe_get_frame_id(frame, (ulong *)&vaBuf->surface);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d mix_videoframe_get_frame_id() failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }
    mret = mix_videoframe_get_vadisplay(frame, (void **)&vaBuf->display);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d mix_videoframe_get_vadisplay() failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

#if 1
    mret = mix_videoframe_get_timestamp(frame, (uint64 *)&outtimestamp);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d mix_videoframe_get_timestamp() failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    LOGV("--- Output timestamp = %"G_GINT64_FORMAT"\n ---", outtimestamp);
#endif
    mret = mix_videoframe_get_frame_structure(frame, (uint32 *)&vaBuf->frame_structure);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d mix_videoframe_get_frame_structure() failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    LOGV(" frame_structure = %d \n", vaBuf->frame_structure);

    outfilledlen = sizeof(VABuffer);
    LOGV("In OMX DEC vadisplay = %x surfaceid = %x outfilledlen = %d\n",
         vaBuf->display,  vaBuf->surface, outfilledlen);

release_frame:
    mret = mix_video_release_frame(mix, frame);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d mix_video_release_frame() failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    outflags |= OMX_BUFFERFLAG_ENDOFFRAME;

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
    }

    if (retain[INPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN ||
            retain[INPORT_INDEX] == BUFFER_RETAIN_ACCUMULATE ) {
        inframe_counter++;
//      buffers[INPORT_INDEX]->nFilledLen = 0;
    }

    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
        outframe_counter++;

#if 0
    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
        DumpBuffer(buffers[OUTPORT_INDEX], false);
#endif

    if (coding_type == OMX_VIDEO_CodingAVC) {

        // TODO: add a flag to differentiate VCM and normal decoding. For now, just to max res
        if (avcDecodeSettings.nMaxWidth  == 0 ||
                avcDecodeSettings.nMaxHeight == 0) {
            if (oret == (OMX_ERRORTYPE) OMX_ErrorIntelProcessStream) {
                oret = OMX_ErrorNone;
            }
            if (oret == (OMX_ERRORTYPE) OMX_ErrorIntelMissingConfig) {
                oret = OMX_ErrorNone;
            }
        }
    }

    LOGV_IF(oret == OMX_ErrorNone,
            "%s(),%d: exit, decode is done\n", __func__, __LINE__);

    return oret;
}

/* end of implement ComponentBase::Processor[*] */

/*
 * vcp setting helpers
 */
OMX_ERRORTYPE MrstPsbComponent::__AvcChangePortParamWithCodecData(
    const OMX_U8 *codec_data, OMX_U32 size, PortBase **ports)
{
    PortAvc *avcport = static_cast<PortAvc *>(ports[INPORT_INDEX]);
    PortVideo *rawport = static_cast<PortVideo *>(ports[OUTPORT_INDEX]);

    OMX_PARAM_PORTDEFINITIONTYPE avcpd, rawpd;

    unsigned int width, height, stride, sliceheight;

    int ret;

    memcpy(&avcpd, avcport->GetPortDefinition(),
           sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

#if 1

    width  = avcDecFrameWidth;
    height = avcDecFrameHeight;
    stride = width;
    sliceheight = height;

#else
    LOGV("nFrameWidth = %lu", avcpd.format.video.nFrameWidth);
    LOGV("nFrameHeight = %lu", avcpd.format.video.nFrameHeight);
    LOGV("nStride = %lu", avcpd.format.video.nStride);
    LOGV("nSliceHeight = %lu", avcpd.format.video.nSliceHeight);

    width = avcpd.format.video.nFrameWidth;
    height = avcpd.format.video.nFrameHeight;
    stride = width;
    sliceheight = height;
#endif

    if (avcpd.format.video.nFrameWidth != width) {
        LOGV("%s(): width : %lu != %d", __func__,
             avcpd.format.video.nFrameWidth, width);
        avcpd.format.video.nFrameWidth = width;
    }
    if (avcpd.format.video.nFrameHeight != height) {
        LOGV("%s(): height : %lu != %d", __func__,
             avcpd.format.video.nFrameHeight, height);
        avcpd.format.video.nFrameHeight = height;
    }
    if (avcpd.format.video.nStride != (OMX_S32)stride) {
        LOGV("%s(): stride : %lu != %d", __func__,
             avcpd.format.video.nStride, stride);
        avcpd.format.video.nStride = stride;
    }
    if (avcpd.format.video.nSliceHeight != sliceheight) {
        LOGV("%s(): sliceheight : %ld != %d", __func__,
             avcpd.format.video.nSliceHeight, sliceheight);
        avcpd.format.video.nSliceHeight = sliceheight;
    }

    memcpy(&rawpd, rawport->GetPortDefinition(),
           sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    if (rawpd.format.video.nFrameWidth != width)
        rawpd.format.video.nFrameWidth = width;
    if (rawpd.format.video.nFrameHeight != height)
        rawpd.format.video.nFrameHeight = height;
    if (rawpd.format.video.nStride != (OMX_S32)stride)
        rawpd.format.video.nStride = stride;
    if (rawpd.format.video.nSliceHeight != sliceheight)
        rawpd.format.video.nSliceHeight = sliceheight;

    rawpd.nBufferSize = (stride * sliceheight * 3) >> 1;

    avcport->SetPortDefinition(&avcpd, true);
    rawport->SetPortDefinition(&rawpd, true);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::__Mpeg4ChangePortParamWithCodecData(
    const OMX_U8 *codec_data, OMX_U32 size, PortBase **ports)
{
    PortMpeg4 *mpeg4port = static_cast<PortMpeg4 *>(ports[INPORT_INDEX]);
    PortVideo *rawport = static_cast<PortVideo *>(ports[OUTPORT_INDEX]);

    OMX_PARAM_PORTDEFINITIONTYPE mpeg4pd, rawpd;

    unsigned int width, height, stride, sliceheight;

    memcpy(&mpeg4pd, mpeg4port->GetPortDefinition(),
           sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    LOGV("nFrameWidth = %lu", mpeg4pd.format.video.nFrameWidth);
    LOGV("nFrameHeight = %lu", mpeg4pd.format.video.nFrameHeight);
    LOGV("nStride = %lu", mpeg4pd.format.video.nStride);
    LOGV("nSliceHeight = %lu", mpeg4pd.format.video.nSliceHeight);

    width = mpeg4pd.format.video.nFrameWidth;
    height = mpeg4pd.format.video.nFrameHeight;
    stride = width;
    sliceheight = height;

    if (mpeg4pd.format.video.nFrameWidth != width) {
        LOGV("%s(): width : %lu != %d", __func__,
             mpeg4pd.format.video.nFrameWidth, width);
        mpeg4pd.format.video.nFrameWidth = width;
    }
    if (mpeg4pd.format.video.nFrameHeight != height) {
        LOGV("%s(): height : %lu != %d", __func__,
             mpeg4pd.format.video.nFrameHeight, height);
        mpeg4pd.format.video.nFrameHeight = height;
    }
    if (mpeg4pd.format.video.nStride != (OMX_S32)stride) {
        LOGV("%s(): stride : %lu != %d", __func__,
             mpeg4pd.format.video.nStride, stride);
        mpeg4pd.format.video.nStride = stride;
    }
    if (mpeg4pd.format.video.nSliceHeight != sliceheight) {
        LOGV("%s(): sliceheight : %ld != %d", __func__,
             mpeg4pd.format.video.nSliceHeight, sliceheight);
        mpeg4pd.format.video.nSliceHeight = sliceheight;
    }

    memcpy(&rawpd, rawport->GetPortDefinition(),
           sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    if (rawpd.format.video.nFrameWidth != width)
        rawpd.format.video.nFrameWidth = width;
    if (rawpd.format.video.nFrameHeight != height)
        rawpd.format.video.nFrameHeight = height;
    if (rawpd.format.video.nStride != (OMX_S32)stride)
        rawpd.format.video.nStride = stride;
    if (rawpd.format.video.nSliceHeight != sliceheight)
        rawpd.format.video.nSliceHeight = sliceheight;

    rawpd.nBufferSize = (stride * sliceheight * 3) >> 1;

    mpeg4port->SetPortDefinition(&mpeg4pd, true);
    rawport->SetPortDefinition(&rawpd, true);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::__H263ChangePortParamWithCodecData(
    const OMX_U8 *codec_data, OMX_U32 size, PortBase **ports)
{
    PortH263 *h263port = static_cast<PortH263 *>(ports[INPORT_INDEX]);
    PortVideo *rawport = static_cast<PortVideo *>(ports[OUTPORT_INDEX]);

    OMX_PARAM_PORTDEFINITIONTYPE h263pd, rawpd;

    unsigned int width, height, stride, sliceheight;
    unsigned int display_width, display_height;

    H263HeaderParser h263_header_parser;
    OMX_BOOL result;
    result = h263_header_parser.DecodeH263Header((OMX_U8 *) codec_data,
                                        (int32 *) &width, (int32 *) &height, (int32 *) &display_width,
                                        (int32 *) &display_height);
    if(OMX_FALSE ==result)
		return OMX_ErrorBadParameter;

    memcpy(&h263pd, h263port->GetPortDefinition(),
           sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    LOGV("width = %lu", width);
    LOGV("height = %lu", height);
    LOGV("nFrameWidth = %lu", h263pd.format.video.nFrameWidth);
    LOGV("nFrameHeight = %lu", h263pd.format.video.nFrameHeight);
    LOGV("nStride = %lu", h263pd.format.video.nStride);
    LOGV("nSliceHeight = %lu", h263pd.format.video.nSliceHeight);

//    width = h263pd.format.video.nFrameWidth;
//    height = h263pd.format.video.nFrameHeight;

    h263pd.format.video.nFrameWidth = width;
    h263pd.format.video.nFrameHeight = height;
    h263pd.format.video.nStride = width;
    h263pd.format.video.nSliceHeight = height;

    stride = width;
    sliceheight = height;

    if (h263pd.format.video.nFrameWidth != width) {
        LOGV("%s(): width : %lu != %d", __func__,
             h263pd.format.video.nFrameWidth, width);
        h263pd.format.video.nFrameWidth = width;
    }
    if (h263pd.format.video.nFrameHeight != height) {
        LOGV("%s(): height : %lu != %d", __func__,
             h263pd.format.video.nFrameHeight, height);
        h263pd.format.video.nFrameHeight = height;
    }
    if (h263pd.format.video.nStride != (OMX_S32)stride) {
        LOGV("%s(): stride : %lu != %d", __func__,
             h263pd.format.video.nStride, stride);
        h263pd.format.video.nStride = stride;
    }
    if (h263pd.format.video.nSliceHeight != sliceheight) {
        LOGV("%s(): sliceheight : %ld != %d", __func__,
             h263pd.format.video.nSliceHeight, sliceheight);
        h263pd.format.video.nSliceHeight = sliceheight;
    }

    memcpy(&rawpd, rawport->GetPortDefinition(),
           sizeof(OMX_PARAM_PORTDEFINITIONTYPE));

    if (rawpd.format.video.nFrameWidth != width)
        rawpd.format.video.nFrameWidth = width;
    if (rawpd.format.video.nFrameHeight != height)
        rawpd.format.video.nFrameHeight = height;
    if (rawpd.format.video.nStride != (OMX_S32)stride)
        rawpd.format.video.nStride = stride;
    if (rawpd.format.video.nSliceHeight != sliceheight)
        rawpd.format.video.nSliceHeight = sliceheight;

    rawpd.nBufferSize = (stride * sliceheight * 3) >> 1;

    h263port->SetPortDefinition(&h263pd, true);
    rawport->SetPortDefinition(&rawpd, true);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstPsbComponent::ChangePortParamWithCodecData(
    const OMX_U8 *codec_data,
    OMX_U32 size,
    PortBase **ports)
{
    OMX_ERRORTYPE ret = OMX_ErrorBadParameter;

    if (coding_type == OMX_VIDEO_CodingAVC) {
        ret = __AvcChangePortParamWithCodecData(codec_data, size, ports);
    }
    else if (coding_type == OMX_VIDEO_CodingMPEG4) {
        ret = __Mpeg4ChangePortParamWithCodecData(codec_data, size, ports);
    }
    else if (coding_type == OMX_VIDEO_CodingH263) {
        ret = __H263ChangePortParamWithCodecData(codec_data, size, ports);
    }

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__AvcChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortAvc *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    /* mime type */
    mix_videoconfigparamsdec_set_mime_type(MIX_VIDEOCONFIGPARAMSDEC(vcp),
                                           "video/x-h264");

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__Mpeg4ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortMpeg4 *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    mix_videoconfigparamsdec_set_mime_type(MIX_VIDEOCONFIGPARAMSDEC(vcp),
                                           "video/mpeg");
    mix_videoconfigparamsdec_mp42_set_mpegversion(
        MIX_VIDEOCONFIGPARAMSDEC_MP42(vcp), 4);

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__H263ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortH263 *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    mix_videoconfigparamsdec_set_mime_type(MIX_VIDEOCONFIGPARAMSDEC(vcp),
                                           "video/mpeg");
    mix_videoconfigparamsdec_mp42_set_mpegversion(
        MIX_VIDEOCONFIGPARAMSDEC_MP42(vcp), 4);

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp,
    PortVideo *port,
    bool *vcp_changed)
{
    const OMX_PARAM_PORTDEFINITIONTYPE *pd = port->GetPortDefinition();
    OMX_ERRORTYPE ret;

    if (coding_type == OMX_VIDEO_CodingAVC)
        ret = __AvcChangeVcpWithPortParam(vcp,
                                          static_cast<PortAvc *>(port),
                                          vcp_changed);
    else if (coding_type == OMX_VIDEO_CodingMPEG4)
        ret = __Mpeg4ChangeVcpWithPortParam(vcp,
                                            static_cast<PortMpeg4 *>(port),
                                            vcp_changed);
    else if (coding_type == OMX_VIDEO_CodingH263)
        ret = __H263ChangeVcpWithPortParam(vcp,
                                           static_cast<PortH263 *>(port),
                                           vcp_changed);
    else
        ret = OMX_ErrorBadParameter;


    MixVideoConfigParamsDec *config = MIX_VIDEOCONFIGPARAMSDEC(vcp);

    if (config->frame_rate_num != (pd->format.video.xFramerate >> 16)) {
        LOGV("%s(): framerate : %u != %ld", __func__,
             config->frame_rate_num, pd->format.video.xFramerate >> 16);
        mix_videoconfigparamsdec_set_frame_rate(config,
                                                pd->format.video.xFramerate >> 16,
                                                1);
        if (vcp_changed)
            *vcp_changed = true;
    }

    if ((config->picture_width != pd->format.video.nFrameWidth) ||
            (config->picture_height != pd->format.video.nFrameHeight)) {
        LOGV("%s(): width : %ld != %ld", __func__,
             config->picture_width, pd->format.video.nFrameWidth);
        LOGV("%s(): height : %ld != %ld", __func__,
             config->picture_height, pd->format.video.nFrameHeight);

        mix_videoconfigparamsdec_set_picture_res(config,
                pd->format.video.nFrameWidth,
                pd->format.video.nFrameHeight);
        if (vcp_changed)
            *vcp_changed = true;
    }

    if (coding_type == OMX_VIDEO_CodingAVC) {
        if(avcDecodeSettings.nMaxWidth > config->picture_width &&
                avcDecodeSettings.nMaxHeight > config->picture_height) {
            mix_videoconfigparamsdec_set_picture_res(config,
                    avcDecodeSettings.nMaxWidth, avcDecodeSettings.nMaxHeight);
        }
    }

    /* hard coding */
    mix_videoconfigparamsdec_set_frame_order_mode(
//            MIX_VIDEOCONFIGPARAMSDEC(vcp), MIX_FRAMEORDER_MODE_DECODEORDER);
        MIX_VIDEOCONFIGPARAMSDEC(vcp), MIX_FRAMEORDER_MODE_DISPLAYORDER);
    mix_videoconfigparamsdec_set_buffer_pool_size(
        MIX_VIDEOCONFIGPARAMSDEC(vcp), 8);

    if (coding_type == OMX_VIDEO_CodingAVC) {
        mix_videoconfigparamsdec_set_extra_surface_allocation(
            MIX_VIDEOCONFIGPARAMSDEC(vcp), avcDecodeSettings.nMaxNumberOfReferenceFrame);
    } else {
        mix_videoconfigparamsdec_set_extra_surface_allocation(
            MIX_VIDEOCONFIGPARAMSDEC(vcp), 4);
    }

    return ret;
}

/* end of vcp setting helpers */

OMX_ERRORTYPE MrstPsbComponent::ProcessorFlush(OMX_U32 port_index) {

    LOGV("port_index = %d Flushed!\n", port_index);

    if (port_index == INPORT_INDEX ||
            port_index == OMX_ALL) {
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

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB.Dec";

static const char *g_roles[] =
{
    (const char *)"video_decoder.avc",
    (const char *)"video_decoder.mpeg4",
    (const char *)"video_decoder.h263",
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
