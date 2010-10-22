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

#include <cmodule.h>
#include <portvideo.h>
#include <componentbase.h>

#include <pv_omxcore.h>

#include <gthread.h>
#include <glib.h>


#ifdef __cplusplus
extern "C" {
#endif

#include <mixdisplayandroid.h>
#include <mixvideo.h>
#include <mixvideoconfigparamsdec_h264.h>
#include <mixvideoconfigparamsenc_h264.h>
#include <mixvideoconfigparamsdec_mp42.h>
#include <mixvideoconfigparamsenc_h263.h>

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

        /* OMX_VIDEO_CONFIG_PRI_INFOTYPE */
        OMX_VIDEO_CONFIG_PRI_INFOTYPE privateinfoparam;

        memset(&privateinfoparam, 0, sizeof(privateinfoparam));
        SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));

        privateinfoparam.nPortIndex = port_index;
        privateinfoparam.nCapacity = 0;
        privateinfoparam.nHolder = NULL;

        avcport->SetPortPrivateInfoParam(&privateinfoparam, true);

        /* end of OMX_VIDEO_PARAM_BITRATETYPE */
    }

    avcEncIDRPeriod = 0;
    avcEncPFrames = 0;

    avcEncNaluFormatType = OMX_NaluFormatZeroByteInterleaveLength;

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

	LOGV("p->eColorFormat = %x\n", p->eColorFormat);

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
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAvc *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
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

        if (coding_type == OMX_VIDEO_CodingAVC) {
            p->iOMXComponentUsesNALStartCodes = OMX_FALSE;
            p->iOMXComponentUsesFullAVCFrames = OMX_FALSE;
        }

        break;
    }

    case OMX_IndexParamNalStreamFormat:
    case OMX_IndexParamNalStreamFormatSupported: {
    	OMX_NALSTREAMFORMATTYPE *p =
           (OMX_NALSTREAMFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s() : OMX_IndexParamNalStreamFormat or OMX_IndexParamNalStreamFormatSupported");

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

        LOGV("%s() : about to get native or supported nal format", __func__);

        if (port->IsEnabled()) {
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
            if(codec_mode == MIX_CODEC_MODE_ENCODE &&
               coding_type == OMX_VIDEO_CodingAVC) {
            	 if(nParamIndex == OMX_IndexParamNalStreamFormat) {
                     p->eNaluFormat = OMX_NaluFormatStartCodes;
                     LOGV("OMX_IndexParamNalStreamFormat 0x%x", p->eNaluFormat);
            	 } else {
                     p->eNaluFormat = (OMX_NALUFORMATSTYPE)(OMX_NaluFormatStartCodes |
                    		          OMX_NaluFormatFourByteInterleaveLength |
                    		          OMX_NaluFormatZeroByteInterleaveLength);
                     LOGV("OMX_IndexParamNalStreamFormatSupported 0x%x", p->eNaluFormat);
            	 }
            }
        }
        break;
    }

    case OMX_IndexConfigVideoAVCIntraPeriod: {
		OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval =
				(OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentParameterStructure;
		OMX_U32 index = pVideoIDRInterval->nPortIndex;

        LOGV("%s() : OMX_IndexConfigVideoAVCIntraPeriod", __func__);

    	if(!vcp) {
                LOGV("%s() : vcp is NULL", __func__);
		return OMX_ErrorNotReady;
    	}

		ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
		if (ret != OMX_ErrorNone) {
			LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
			return ret;
		}

		if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX) {
			LOGV("Wrong port index");
			return OMX_ErrorBadPortIndex;
		}

		PortAvc *port = static_cast<PortAvc *> (ports[index]);
		if (!port) {
			LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
					OMX_ErrorBadPortIndex);
			return OMX_ErrorBadPortIndex;
		}

		if (port->IsEnabled()) {
			if (codec_mode == MIX_CODEC_MODE_ENCODE && coding_type
					== OMX_VIDEO_CodingAVC) {
				ret = ComponentGetConfig(OMX_IndexConfigVideoAVCIntraPeriod,
					pComponentParameterStructure);
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
    case OMX_IndexParamVideoAvc: {
        OMX_VIDEO_PARAM_AVCTYPE *p =
            (OMX_VIDEO_PARAM_AVCTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAvc *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAvc *>(ports[index]);

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
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMpeg4 *>(ports[index]);

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

        LOGV("OMX_IndexIntelPrivateInfo");
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
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortPrivateInfoParam(p, false);
        break;
    }

    case OMX_IndexParamVideoBytestream: {
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
				LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
						OMX_ErrorIncorrectStateOperation);
				return OMX_ErrorIncorrectStateOperation;
			}

			if (codec_mode == MIX_CODEC_MODE_ENCODE && coding_type
					== OMX_VIDEO_CodingAVC) {

				if (p->bBytestream == OMX_TRUE) {
					avcEncNaluFormatType = OMX_NaluFormatStartCodes;
				} else {
					avcEncNaluFormatType
							= OMX_NaluFormatZeroByteInterleaveLength;
				}
			}
		}
		break;
	}
	case OMX_IndexParamNalStreamFormatSelect: {

                LOGV("%s() : OMX_IndexParamNalStreamFormatSelect", __func__);
		OMX_NALSTREAMFORMATTYPE *p =
				(OMX_NALSTREAMFORMATTYPE *) pComponentParameterStructure;
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

		if (p->eNaluFormat == OMX_NaluFormatStartCodes || p->eNaluFormat
				== OMX_NaluFormatFourByteInterleaveLength || p->eNaluFormat
				== OMX_NaluFormatZeroByteInterleaveLength) {

			if (port->IsEnabled()) {
				OMX_STATETYPE state;
				CBaseGetState((void *) GetComponentHandle(), &state);
				if (state != OMX_StateLoaded && state
						!= OMX_StateWaitForResources) {
					LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
							OMX_ErrorIncorrectStateOperation);
					return OMX_ErrorIncorrectStateOperation;
				}

				if (codec_mode == MIX_CODEC_MODE_ENCODE && coding_type
						== OMX_VIDEO_CodingAVC) {

					avcEncNaluFormatType = p->eNaluFormat;
                                        LOGV("OMX_IndexParamNalStreamFormatSelect : 0x%x", avcEncNaluFormatType);
				}
			}
		}
		break;
	}

    case OMX_IndexConfigVideoAVCIntraPeriod: {

                LOGV("%s() : OMX_IndexConfigVideoAVCIntraPeriod", __func__);

		OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval =
				(OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentParameterStructure;
		if (!pVideoIDRInterval) {
			LOGE("NULL pointer");
			return OMX_ErrorBadParameter;
		}

		OMX_U32 index = pVideoIDRInterval->nPortIndex;
		if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX) {
			LOGV("Wrong port index");
			return OMX_ErrorBadPortIndex;
		}

		PortAvc *port = static_cast<PortAvc *> (ports[index]);
		if (!port) {
			LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
					OMX_ErrorBadPortIndex);
			return OMX_ErrorBadPortIndex;
		}

		if (!port->IsEnabled()) {
                        LOGV("%s() : port is not enabled", __func__);
			return OMX_ErrorNotReady;
		}

		if (codec_mode != MIX_CODEC_MODE_ENCODE) {
			LOGE("Wrong codec mode");
			return OMX_ErrorUnsupportedIndex;
		}
		if (coding_type != OMX_VIDEO_CodingAVC) {
			LOGE("Wrong coding type");
			return OMX_ErrorUnsupportedIndex;
		}

		ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
		if (ret != OMX_ErrorNone) {
			LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
			return ret;
		}

                avcEncIDRPeriod = pVideoIDRInterval->nIDRPeriod;
                avcEncPFrames   = pVideoIDRInterval->nPFrames;
                LOGV("%s() : OMX_IndexConfigVideoAVCIntraPeriod : avcEncIDRPeriod = %d avcEncPFrames = %d",
                          __func__, avcEncIDRPeriod, avcEncPFrames);

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
                LOGE("NULL pointer");
                return OMX_ErrorBadParameter;
            }

            ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
            if (ret != OMX_ErrorNone) {
                 LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
                 return ret;
            }
            if(codec_mode != MIX_CODEC_MODE_ENCODE) {
                 LOGE("Wrong codec mode");
                 return OMX_ErrorUnsupportedIndex;
            }
            if(coding_type != OMX_VIDEO_CodingAVC) {
                 LOGE("Wrong coding type");
                 return OMX_ErrorUnsupportedIndex;
            }
            if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX)
            {
                LOGV("Wrong port index");
                return OMX_ErrorBadPortIndex;
            }
            OMX_U32 index = pVideoIDRInterval->nPortIndex;
            PortAvc *port = static_cast<PortAvc *>(ports[index]);

            if(!mix) {
                LOGE("MixVideo is not created");
                return OMX_ErrorUndefined;
            }

            MixVideoConfigParams *mixbaseconfig = NULL;
            MIX_RESULT mret = mix_video_get_config(mix, &mixbaseconfig);
            if(mret != MIX_RESULT_SUCCESS) {
                LOGE("Failed to get config");
                return OMX_ErrorUndefined;
            }

            guint intra_period = 0;
            mret = mix_videoconfigparamsenc_get_intra_period(MIX_VIDEOCONFIGPARAMSENC(mixbaseconfig), &intra_period);
            if(mret != MIX_RESULT_SUCCESS) {
                LOGE("Failed to get intra period");
                return OMX_ErrorUndefined;
            }

            guint idr_interval = 0;
            mret = mix_videoconfigparamsenc_h264_get_IDR_interval(MIX_VIDEOCONFIGPARAMSENC_H264(mixbaseconfig), &idr_interval);
            if(mret != MIX_RESULT_SUCCESS) {
                LOGE("Failed to get IDR interval");
                return OMX_ErrorUndefined;
            }

            mix_videoconfigparams_unref(mixbaseconfig);

            pVideoIDRInterval->nIDRPeriod = idr_interval;
            pVideoIDRInterval->nPFrames = intra_period;

            LOGV("OMX_IndexConfigVideoAVCIntraPeriod : nIDRPeriod = %d, nPFrames = %d", pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);

            SetTypeHeader(pVideoIDRInterval, sizeof(OMX_VIDEO_CONFIG_AVCINTRAPERIOD));
        }
        break;

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
    LOGV("%s(): enter\n", __func__);

    LOGV("%s() : nIndex = %d\n", __func__, nParamIndex);

    switch (nParamIndex)
    {
        case OMX_IndexConfigVideoIntraVOPRefresh:
        {
            pVideoIFrame = (OMX_CONFIG_INTRAREFRESHVOPTYPE*) pComponentConfigStructure;
            ret = CheckTypeHeader(pVideoIFrame, sizeof(*pVideoIFrame));
            if (ret != OMX_ErrorNone) {
                 LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
                 return ret;
            }
            if(codec_mode != MIX_CODEC_MODE_ENCODE) {
                 LOGV("Wrong code mode");
                 return OMX_ErrorUnsupportedIndex;
            }
            if(coding_type != OMX_VIDEO_CodingAVC) {
                 LOGV("Wrong coding type");
                 return OMX_ErrorUnsupportedIndex;
            }
            if (pVideoIFrame->nPortIndex != OUTPORT_INDEX)
            {
                LOGV("Wrong port index");
                return OMX_ErrorBadPortIndex;
            }

            LOGV("OMX_IndexConfigVideoIntraVOPRefresh");
            if(pVideoIFrame->IntraRefreshVOP == OMX_TRUE) {
                 LOGV("pVideoIFrame->IntraRefreshVOP == OMX_TRUE");

                MixEncDynamicParams encdynareq;
                oscl_memset(&encdynareq, 0, sizeof(encdynareq));
                encdynareq.force_idr = TRUE;
                MIX_RESULT mret;
                if(mix) {
                    mret = mix_video_set_dynamic_enc_config (mix, MIX_ENC_PARAMS_FORCE_KEY_FRAME, &encdynareq);
                    if(mret != MIX_RESULT_SUCCESS) {
                         LOGV("Failed to set IDR interval");
                    }
                }
            }
        }
        break;

        case OMX_IndexConfigVideoAVCIntraPeriod:
        {
            pVideoIDRInterval = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentConfigStructure;

            ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
            if (ret != OMX_ErrorNone) {
                 LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
                 return ret;
            }
            if(codec_mode != MIX_CODEC_MODE_ENCODE) {
                 LOGV("Wrong codec mode");
                 return OMX_ErrorUnsupportedIndex;
            }
            if(coding_type != OMX_VIDEO_CodingAVC) {
                 LOGV("Wrong coding type");
                 return OMX_ErrorUnsupportedIndex;
            }
            if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX)
            {
                LOGV("Wrong port index");
                return OMX_ErrorBadPortIndex;
            }
            OMX_U32 index = pVideoIDRInterval->nPortIndex;
            PortAvc *port = static_cast<PortAvc *>(ports[index]);

            LOGV("OMX_IndexConfigVideoAVCIntraPeriod : nIDRPeriod = %d, nPFrames = %d", pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);

            MixEncDynamicParams encdynareq;
            oscl_memset(&encdynareq, 0, sizeof(encdynareq));
            encdynareq.idr_interval = pVideoIDRInterval->nIDRPeriod;
            encdynareq.intra_period = pVideoIDRInterval->nPFrames;

            LOGV("nIDRPeriod = %d  nPFrames = %d", pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);

            MIX_RESULT mret;
            if(mix) {

                  // Ignore the return code
                  mret = mix_video_set_dynamic_enc_config (mix, MIX_ENC_PARAMS_IDR_INTERVAL, &encdynareq);
                  if(mret != MIX_RESULT_SUCCESS) {
                       LOGV("Failed to set IDR interval");
                  }

                  mret = mix_video_set_dynamic_enc_config (mix, MIX_ENC_PARAMS_GOP_SIZE, &encdynareq);
                  if(mret != MIX_RESULT_SUCCESS) {
                       LOGV("Failed to set GOP size");
                  }
            }
        }
        break;

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

    guint major, minor;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

    if(!g_thread_supported()) {
         g_thread_init(NULL);
    }
    g_type_init();


    /*
     * common codes
     */
    mix = mix_video_new();
    if (!mix) {
        LOGE("%s(),%d: exit, mix_video_new failed", __func__, __LINE__);
        goto error_out;
    }

    mix_video_get_version(mix, &major, &minor);
    LOGV("MixVideo version: %d.%d", major, minor);

    /* decoder */
    if (codec_mode == MIX_CODEC_MODE_DECODE) {
	if (coding_type == OMX_VIDEO_CodingAVC) {
            vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsdec_h264_new());
        }
        else if (coding_type == OMX_VIDEO_CodingMPEG4 || coding_type == OMX_VIDEO_CodingH263) {
            vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsdec_mp42_new());
        }

        mvp = MIX_PARAMS(mix_videodecodeparams_new());
        port_index = INPORT_INDEX;
    }
    /* encoder */
    else {
	 if (coding_type == OMX_VIDEO_CodingAVC) {
            vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsenc_h264_new());
	}
	else if (coding_type == OMX_VIDEO_CodingH263) {
	    vcp = MIX_VIDEOCONFIGPARAMS(mix_videoconfigparamsenc_h263_new());
	}

        mvp = MIX_PARAMS(mix_videoencodeparams_new());
        port_index = OUTPORT_INDEX;
    }

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

    if (codec_mode == MIX_CODEC_MODE_DECODE) {
        mret = mix_videodecodeparams_set_discontinuity(
            MIX_VIDEODECODEPARAMS(mvp), FALSE);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(),%d: exit, mix_videodecodeparams_set_discontinuity "
                 "(ret == 0x%08x)", __func__, __LINE__, mret);
            goto error_out;
        }
    }

    mret = mix_video_initialize(mix, codec_mode, vip, NULL);
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(),%d: exit, mix_video_initialize failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto error_out;
    }

    if (codec_mode == MIX_CODEC_MODE_ENCODE) {
        LOGV("mix_video_configure");
        mret = mix_video_configure(mix, vcp, NULL);
        if (mret != MIX_RESULT_SUCCESS) {
            LOGE("%s(), %d: exit, mix_video_configure failed "
                 "(ret:0x%08x)", __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto error_out;
        }
        LOGV("%s(): mix video configured", __func__);
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

    if (codec_mode != MIX_CODEC_MODE_DECODE &&
        (coding_type == OMX_VIDEO_CodingAVC || coding_type == OMX_VIDEO_CodingH263)) {

        ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
    }

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

static OMX_U32 avc_get_nal_size_from_length_prefix(OMX_U8 *data, OMX_U32 data_size,
                                OMX_U8 **pstart, OMX_U32 *nal_size)
{
    OMX_U8 *p = data;

    for(int i = 0; i < 16; i++) {
        LOGV("p[%d] = 0x%x\n", i, p[i]);
    }

    *nal_size = ((*p) << 24) + ((*(p+1)) << 16) + ((*(p+2)) << 8) + ((*(p+3)));
    *pstart = p + 4;

    LOGV("^^^^ data_size = %d nal_size = %d ^^^^^\n", data_size, *nal_size);

    return true;
}



/* FIXME - move to video_parser.c */
static OMX_U32 avc_get_nal_size(OMX_U8 *data, OMX_U32 data_size,
                                OMX_U8 **pstart, OMX_U32 *nal_size)
{
    OMX_U8 *p = data;
    OMX_U32 start = 0, end = 0, zero = 0;
    bool found = false;

    if (!data || !data_size || !pstart || !nal_size)
        return false;

    *pstart = NULL;
    *nal_size = 0;

    while (end < data_size) {
        //LOGV("%s(): %lu:0x%02x\n", __func__, end, p[end]);
        if (p[end] != 0x00)
            break;
        end++;
    }

    if (p[end] != 0x01)
        goto out;

    found = true;

    //LOGV("%s():found start code, start here %lu\n", __func__, end+1);

    end++;
    start = end;
    if (start < data_size)
        *pstart = p + start;
    else
        goto out;

    while (end < data_size) {
        //LOGV("%s(): %lu:0x%02x\n", __func__, end, p[end]);

        if (zero == 3 && p[end] == 0x01) {
            end -= 3;
            //LOGV("%s():found start code cut here %lu\n", __func__, end);
            break;
        }

        if (p[end] == 0x00)
            zero++;
        else
            zero = 0;
        end++;
    }

out:
    *nal_size = end - start;
    return found;
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

    LOGV("%s(): enter\n", __func__);

#if 0
    if (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG)
        DumpBuffer(buffers[INPORT_INDEX], true);
    else
        DumpBuffer(buffers[INPORT_INDEX], false);
#endif

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
    buffer_in.buffer_size = buffers[INPORT_INDEX]->nAllocLen - buffers[INPORT_INDEX]->nOffset;

    buffer_out.data =
        buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    buffer_out.data_size = 0;
    buffer_out.buffer_size = buffers[OUTPORT_INDEX]->nAllocLen - buffers[OUTPORT_INDEX]->nOffset;
    mixiovec_out[0] = &buffer_out;

    if(codec_mode == MIX_CODEC_MODE_DECODE && is_mixvideodec_configured) {
    	MixVideoFrame *frame = NULL;
        VABuffer *vaBuf = (VABuffer *)(mixiovec_out[0]->data);
        uint32 frame_structure = 0;

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

        mret = mix_videoframe_get_frame_id(frame, (gulong *)&vaBuf->surface);
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

        mret = mix_videoframe_get_timestamp(frame, (guint64 *)&outtimestamp);
        if (mret != MIX_RESULT_SUCCESS) {
              LOGE("%s(), %d mix_videoframe_get_timestamp() failed (ret == 0x%08x)",
                                 __func__, __LINE__, mret);
              oret = OMX_ErrorUndefined;
              goto local_cleanup;
        }

        LOGV("--- Output timestamp = %"G_GINT64_FORMAT"\n ---", outtimestamp);

        mret = mix_videoframe_get_frame_structure(frame, (guint32 *)&frame_structure);
        if (mret != MIX_RESULT_SUCCESS) {
              LOGE("%s(), %d mix_videoframe_get_frame_structure() failed (ret == 0x%08x)",
                                 __func__, __LINE__, mret);
              oret = OMX_ErrorUndefined;
              goto local_cleanup;
        }

        LOGV(" frame_structure = %d \n", frame_structure);

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

    if ((codec_mode == MIX_CODEC_MODE_DECODE) && (coding_type == OMX_VIDEO_CodingH263)) {

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

	LOGV("^^^ end of frame and codec config ^^^");

	if (coding_type == OMX_VIDEO_CodingAVC) {
            if (inframe_counter == 0) {
                unsigned int width, height, stride, sliceheight;

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

                mret = mix_video_configure(mix, vcp, NULL);
                if (mret != MIX_RESULT_SUCCESS) {
                    LOGE("%s(), %d: exit, mix_video_configure failed "
                         "(ret:0x%08x)", __func__, __LINE__, mret);
                    oret = OMX_ErrorUndefined;
                    goto out;
                }
                is_mixvideodec_configured = OMX_TRUE;
                LOGV("%s(): mix video configured", __func__);

                /*
                 * port reconfigure
                 */
                ports[OUTPORT_INDEX]->ReportPortSettingsChanged();

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
    if(codec_mode != MIX_CODEC_MODE_DECODE &&
          (coding_type == OMX_VIDEO_CodingAVC || coding_type == OMX_VIDEO_CodingH263)) {
          mret = mix_buffer_set_data(mixbuffer_in[0],
                              buffer_in.data, buffer_in.data_size,
                              (gulong)this, AvcEncMixBufferCallback);
    } else {
          mret = mix_buffer_set_data(mixbuffer_in[0],
                              buffer_in.data, buffer_in.data_size,
                              0, NULL);
    }
    if (mret != MIX_RESULT_SUCCESS) {
        LOGE("%s(), %d: exit, mix_buffer_set_data failed (ret:0x%08x)",
                __func__, __LINE__, mret);
        oret = OMX_ErrorUndefined;
        goto out;
    }

    /* decoder */
    if (codec_mode == MIX_CODEC_MODE_DECODE) {

    	MixVideoFrame *frame;

        /* set timestamp */
        outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

        mix_videodecodeparams_set_timestamp(MIX_VIDEODECODEPARAMS(mvp),
                                            outtimestamp);
        LOGV("--- Input timestamp = %"G_GINT64_FORMAT" ---\n", outtimestamp);

	int retry_decode_count = 0;

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
            }
#if 0
            else if (mret == MIX_RESULT_DROPFRAME) {
                LOGE("%s(),%d: exit, mix_video_decode() failed, "
                     "frame dropped\n",
                     __func__, __LINE__);
                /* not an error */
                goto out;
            }
            else {
                LOGE("%s(),%d: exit, mix_video_decode() failed\n",
                     __func__, __LINE__);
                oret = OMX_ErrorUndefined;
                goto out;
            }
#endif
            else if (mret != MIX_RESULT_DROPFRAME) {
                LOGE("%s(),%d: exit, mix_video_decode() failed\n",
                     __func__, __LINE__);
                oret = OMX_ErrorUndefined;
                goto out;
            }
        }

        if(mret == MIX_RESULT_DROPFRAME) {
            LOGE("%s(),%d: mix_video_decode() failed, "
                    "frame dropped\n",
                     __func__, __LINE__);
            /* not an error */
            retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
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

        VABuffer *vaBuf = (VABuffer *)(mixiovec_out[0]->data);
        mret = mix_videoframe_get_frame_id(frame, (gulong *)&vaBuf->surface);
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
        mret = mix_videoframe_get_timestamp(frame, (guint64 *)&outtimestamp);
        if (mret != MIX_RESULT_SUCCESS) {
              LOGE("%s(), %d mix_videoframe_get_timestamp() failed (ret == 0x%08x)",
                                 __func__, __LINE__, mret);
              oret = OMX_ErrorUndefined;
              goto out;
        }

        LOGV("--- Output timestamp = %"G_GINT64_FORMAT"\n ---", outtimestamp);
#endif
        uint32 frame_structure = 0;
        mret = mix_videoframe_get_frame_structure(frame, (guint32 *)&frame_structure);
        if (mret != MIX_RESULT_SUCCESS) {
              LOGE("%s(), %d mix_videoframe_get_frame_structure() failed (ret == 0x%08x)",
                                 __func__, __LINE__, mret);
              oret = OMX_ErrorUndefined;
              goto out;
        }

        LOGV(" frame_structure = %d \n", frame_structure);

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
    }
    /* encoder */
    else {

       if (coding_type != OMX_VIDEO_CodingAVC || (coding_type
				== OMX_VIDEO_CodingAVC && avcEncNaluFormatType
				!= OMX_NaluFormatZeroByteInterleaveLength)) {

			mret = mix_video_encode(mix, mixbuffer_in, 1, mixiovec_out, 1,
					MIX_VIDEOENCODEPARAMS(mvp));

			if (mret != MIX_RESULT_SUCCESS) {
				LOGE(
						"%s(), %d: exit, mix_video_encode failed (ret == 0x%08x)\n",
						__func__, __LINE__, mret);
				oret = OMX_ErrorUndefined;
				goto out;
			}

			outfilledlen = mixiovec_out[0]->data_size;
			outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;
			outflags |= OMX_BUFFERFLAG_ENDOFFRAME;

			// The buffer will be relaseed in AvcEncMixBufferCallback()
			if (coding_type == OMX_VIDEO_CodingH263 || coding_type
					== OMX_VIDEO_CodingAVC) {
				retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;
			}

		} else {

			LOGV("in buffer = 0x%x ts = %lld",
					buffers[INPORT_INDEX]->pBuffer
							+ buffers[INPORT_INDEX]->nOffset,
					buffers[INPORT_INDEX]->nTimeStamp);

			OMX_U8 *val = NULL;
			bool _flag = false;
			if (avc_enc_frame_size_left == 0) {

				LOGV("begin to call mix_video_encode()");

				mret = mix_video_encode(mix, mixbuffer_in, 1, mixiovec_out, 1,
						MIX_VIDEOENCODEPARAMS(mvp));
				if (mret != MIX_RESULT_SUCCESS) {
					LOGE(
							"%s(), %d: exit, mix_video_encode failed (ret == 0x%08x)\n",
							__func__, __LINE__, mret);
					oret = OMX_ErrorUndefined;
					goto out;
				}

				_flag = true;

				LOGV("mixiovec_out[0]->data_size = %d\n",
						mixiovec_out[0]->data_size);

				avc_enc_frame_size_left = mixiovec_out[0]->data_size;

				val = buffers[OUTPORT_INDEX]->pBuffer
						+ buffers[OUTPORT_INDEX]->nOffset;
			} else {
				val = avc_enc_buffer + avc_enc_buffer_offset;
			}

			OMX_U32 nal_size = ((*val) << 24) + ((*(val + 1)) << 16) + ((*(val
					+ 2)) << 8) + ((*(val + 3)));

			if (avc_enc_frame_size_left < nal_size + 4) {

				LOGE("nal_size + 4 > avc_enc_frame_size_left");
				oret = OMX_ErrorUndefined;
				goto out;
			} else if (avc_enc_frame_size_left == nal_size + 4) {

				if (!_flag) {
					memcpy(buffers[OUTPORT_INDEX]->pBuffer
							+ buffers[OUTPORT_INDEX]->nOffset, val, 4
							+ nal_size);
				}

				avc_enc_frame_size_left = 0;
				buffers[OUTPORT_INDEX]->nOffset += 4;
				outfilledlen = nal_size;

				retain[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
				outflags |= OMX_BUFFERFLAG_ENDOFFRAME;

				// The buffer will be relaseed in AvcEncMixBufferCallback()
				retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;

			} else {

				avc_enc_frame_size_left -= (4 + nal_size);

				if (_flag) {
					if (!avc_enc_buffer || avc_enc_buffer_length
							< avc_enc_frame_size_left) {
						if (avc_enc_buffer) {
							delete[] avc_enc_buffer;
							LOGV("delete avc_enc_buffer");
						}
						avc_enc_buffer = new OMX_U8[avc_enc_frame_size_left
								+ 1024];
						LOGV("new avc_enc_buffer");
						if (!avc_enc_buffer) {
							oret = OMX_ErrorUndefined;
							goto out;
						}
						avc_enc_buffer_length = avc_enc_frame_size_left + 1024;
					}
					memcpy(avc_enc_buffer, val + 4 + nal_size,
							avc_enc_frame_size_left);
					avc_enc_buffer_offset = 0;

				} else {

					memcpy(buffers[OUTPORT_INDEX]->pBuffer
							+ buffers[OUTPORT_INDEX]->nOffset, val, 4
							+ nal_size);

					avc_enc_buffer_offset += (4 + nal_size);
				}

				buffers[OUTPORT_INDEX]->nOffset += 4;
				outfilledlen = nal_size;
				retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
				retain[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
			}

#if 1
			OMX_U8 *nal = val + 4;

			if (((*nal & 0x1F) == 0x07) || ((*nal & 0x1F) == 0x08)) {

				outflags |= OMX_BUFFERFLAG_CODECCONFIG;

				LOGV(" nal type = %x\n", *nal & 0x1F);
			}
#endif
			outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;
			//             outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
		}


        if (mixbuffer_in[0]) {
             mix_video_release_mixbuffer(mix, mixbuffer_in[0]);
             mixbuffer_in[0] = NULL;
        }
   }

#if SHOW_FPS
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

    LOGV_IF(oret == OMX_ErrorNone,
            "%s(),%d: exit, done\n", __func__, __LINE__);

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

#if 0
    /*
     * FIXME
     *  - parsing codec data (sps/pps) and geting parameters
     */
    #error "no h264 codec data parser"
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
    h263_header_parser.DecodeH263Header((OMX_U8 *) codec_data,
			(int32 *) &width, (int32 *) &height, (int32 *) &display_width,
			(int32 *) &display_height);

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

    /* decoder */
    if (codec_mode == MIX_CODEC_MODE_DECODE) {
        /* mime type */
        mix_videoconfigparamsdec_set_mime_type(MIX_VIDEOCONFIGPARAMSDEC(vcp),
                                               "video/x-h264");
    }
    /* encoder */
    else {
        MixVideoConfigParamsEnc *config = MIX_VIDEOCONFIGPARAMSENC(vcp);

        mix_videoconfigparamsenc_set_encode_format(
            config, MIX_ENCODE_TARGET_FORMAT_H264);
        mix_videoconfigparamsenc_set_profile(config, MIX_PROFILE_H264BASELINE);
        mix_videoconfigparamsenc_set_mime_type(config, "video/x-h264");
        mix_videoconfigparamsenc_h264_set_bus(
            MIX_VIDEOCONFIGPARAMSENC_H264(config), 0);
        mix_videoconfigparamsenc_h264_set_dlk(
            MIX_VIDEOCONFIGPARAMSENC_H264(config), FALSE);

        if (avcEncNaluFormatType == OMX_NaluFormatStartCodes) {

            mix_videoconfigparamsenc_h264_set_slice_num(
                 MIX_VIDEOCONFIGPARAMSENC_H264(config), 10);

            mix_videoconfigparamsenc_h264_set_delimiter_type(
                 MIX_VIDEOCONFIGPARAMSENC_H264(config), MIX_DELIMITER_ANNEXB);
        } else {

            mix_videoconfigparamsenc_h264_set_slice_num(
                MIX_VIDEOCONFIGPARAMSENC_H264(config), 1);

            mix_videoconfigparamsenc_h264_set_delimiter_type(
                 MIX_VIDEOCONFIGPARAMSENC_H264(config),
                 MIX_DELIMITER_LENGTHPREFIX);
        }

        if(avcEncIDRPeriod == 0) {
            avcEncIDRPeriod = 2;
        }

        LOGV("%s() : avcEncIDRPeriod = %d", __func__, avcEncIDRPeriod);

        mix_videoconfigparamsenc_h264_set_IDR_interval(
           MIX_VIDEOCONFIGPARAMSENC_H264(config), avcEncIDRPeriod);
    }

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__Mpeg4ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortMpeg4 *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (codec_mode == MIX_CODEC_MODE_DECODE) {
        mix_videoconfigparamsdec_set_mime_type(MIX_VIDEOCONFIGPARAMSDEC(vcp),
                                               "video/mpeg");
        mix_videoconfigparamsdec_mp42_set_mpegversion(
            MIX_VIDEOCONFIGPARAMSDEC_MP42(vcp), 4);
    }
    else {
        ret = OMX_ErrorBadParameter;
    }

    return ret;
}

OMX_ERRORTYPE MrstPsbComponent::__H263ChangeVcpWithPortParam(
    MixVideoConfigParams *vcp, PortH263 *port, bool *vcp_changed)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    if (codec_mode == MIX_CODEC_MODE_DECODE) {
        mix_videoconfigparamsdec_set_mime_type(MIX_VIDEOCONFIGPARAMSDEC(vcp),
                                               "video/mpeg");
        mix_videoconfigparamsdec_mp42_set_mpegversion(
            MIX_VIDEOCONFIGPARAMSDEC_MP42(vcp), 4);
    }
 /* encoder */
    else {
        MixVideoConfigParamsEnc *config = MIX_VIDEOCONFIGPARAMSENC(vcp);

        mix_videoconfigparamsenc_set_encode_format(
            config, MIX_ENCODE_TARGET_FORMAT_H263);
        mix_videoconfigparamsenc_set_profile(config, MIX_PROFILE_H263BASELINE);
        mix_videoconfigparamsenc_set_mime_type(config, "video/x-h263");
        mix_videoconfigparamsenc_h263_set_dlk(
            MIX_VIDEOCONFIGPARAMSENC_H263(config), FALSE);
    }

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

    /* common */

    /* decoder */
    if (codec_mode == MIX_CODEC_MODE_DECODE) {
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

	LOGV("^^^ config->picture_width = %d pd->format.video.nFrameWidth = %d ^^^",
				config->picture_width, pd->format.video.nFrameWidth);
		LOGV("^^^ config->picture_height = %d pd->format.video.nFrameHeight = %d ^^^",
				config->picture_height, pd->format.video.nFrameHeight);

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

        /* hard coding */
        mix_videoconfigparamsdec_set_frame_order_mode(
//            MIX_VIDEOCONFIGPARAMSDEC(vcp), MIX_FRAMEORDER_MODE_DECODEORDER);
            MIX_VIDEOCONFIGPARAMSDEC(vcp), MIX_FRAMEORDER_MODE_DISPLAYORDER);
        mix_videoconfigparamsdec_set_buffer_pool_size(
            MIX_VIDEOCONFIGPARAMSDEC(vcp), 8);
        mix_videoconfigparamsdec_set_extra_surface_allocation(
            MIX_VIDEOCONFIGPARAMSDEC(vcp), 4);
    }
    /* encoder */
    else {
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

        /* This is temporary change for h264 slice number until IMG interface is fixed. */
        if (coding_type == OMX_VIDEO_CodingAVC && config->picture_height > 480)
        {
             mix_videoconfigparamsenc_h264_set_slice_num(
                     MIX_VIDEOCONFIGPARAMSENC_H264(config), 1);
        }

        if (config->frame_rate_num != (pd->format.video.xFramerate >> 16)) {
            LOGV("%s(): framerate : %u != %ld", __func__,
                 config->frame_rate_num, pd->format.video.xFramerate >> 16);

            mix_videoconfigparamsenc_set_frame_rate(config,
                                            pd->format.video.xFramerate >> 16,
                                            1);
            if (vcp_changed)
                *vcp_changed = true;
        }

        if(avcEncPFrames == 0) {
             avcEncPFrames = config->frame_rate_num / 2;
        }

        LOGV("%s() : avcEncPFrames = %d", __func__, avcEncPFrames);
        mix_videoconfigparamsenc_set_intra_period(config, avcEncPFrames);

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
       LOGV("@@@@@ about to set buffer sharing @@@@@");

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

        /* hard coding */
        mix_videoconfigparamsenc_set_raw_format(config,
                                                MIX_RAW_TARGET_FORMAT_YUV420);
        if(coding_type == OMX_VIDEO_CodingAVC) {
	        mix_videoconfigparamsenc_set_init_qp(config, 24);
	}
	else if (coding_type == OMX_VIDEO_CodingH263) {
		 mix_videoconfigparamsenc_set_init_qp(config, 15);
	}
	mix_videoconfigparamsenc_set_min_qp(config, 1);
        mix_videoconfigparamsenc_set_buffer_pool_size(config, 8);
        mix_videoconfigparamsenc_set_drawable(config, 0x0);
        mix_videoconfigparamsenc_set_need_display(config, FALSE);
    }

    return ret;
}

/* end of vcp setting helpers */

OMX_ERRORTYPE MrstPsbComponent::ProcessorFlush(OMX_U32 port_index) {

	LOGV("port_index = %d Flushed!\n", port_index);

	if (codec_mode == MIX_CODEC_MODE_DECODE && coding_type
	        == OMX_VIDEO_CodingAVC && (port_index == INPORT_INDEX || port_index
		== OMX_ALL)) {
		mix_video_flush( mix);
	}
        if (codec_mode != MIX_CODEC_MODE_DECODE &&
              (coding_type == OMX_VIDEO_CodingAVC || coding_type == OMX_VIDEO_CodingH263) &&
              (port_index == INPORT_INDEX || port_index == OMX_ALL)) {
                ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
                mix_video_flush( mix);
        }
        return OMX_ErrorNone;
}

void MrstPsbComponent::AvcEncMixBufferCallback(gulong token, guchar *data) {
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

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB";

static const char *g_roles[] =
{
    (const char *)"video_decoder.avc",
    (const char *)"video_encoder.avc",
    (const char *)"video_decoder.mpeg4",
    (const char *)"video_decoder.h263",
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
    instantiate: wrs_omxil_cmodule_ops_instantiate,
};

struct wrs_omxil_cmodule_s WRS_OMXIL_CMODULE_SYMBOL = {
    name: g_name,
    roles: &g_roles[0],
    nr_roles: ARRAY_SIZE(g_roles),
    ops: &cmodule_ops,
};
