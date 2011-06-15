/*
* Copyright (c) 2009-2011 Intel Corporation.  All rights reserved.
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
#define LOG_TAG "OMXVideoEncoderBase"
#include <utils/Log.h>
#include "OMXVideoEncoderBase.h"

static const char* RAW_MIME_TYPE = "video/raw";

OMXVideoEncoderBase::OMXVideoEncoderBase() {
}

OMXVideoEncoderBase::~OMXVideoEncoderBase() {
    if (this->ports) {
        if (this->ports[INPORT_INDEX]) {
            delete this->ports[INPORT_INDEX];
            this->ports[INPORT_INDEX] = NULL;
        }

        if (this->ports[OUTPORT_INDEX]) {
            delete this->ports[OUTPORT_INDEX];
            this->ports[OUTPORT_INDEX] = NULL;
        }
    }
}

OMX_ERRORTYPE OMXVideoEncoderBase::InitInputPort(void) {
    this->ports[INPORT_INDEX] = new PortVideo;
    if (this->ports[INPORT_INDEX] == NULL) {
        return OMX_ErrorInsufficientResources;
    }

    PortVideo *port = static_cast<PortVideo *>(this->ports[INPORT_INDEX]);

    // OMX_PARAM_PORTDEFINITIONTYPE
    OMX_PARAM_PORTDEFINITIONTYPE paramPortDefinitionInput;
    memset(&paramPortDefinitionInput, 0, sizeof(paramPortDefinitionInput));
    SetTypeHeader(&paramPortDefinitionInput, sizeof(paramPortDefinitionInput));
    paramPortDefinitionInput.nPortIndex = INPORT_INDEX;
    paramPortDefinitionInput.eDir = OMX_DirInput;
    paramPortDefinitionInput.nBufferCountActual = INPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionInput.nBufferCountMin = INPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionInput.nBufferSize = INPORT_BUFFER_SIZE;
    paramPortDefinitionInput.bEnabled = OMX_TRUE;
    paramPortDefinitionInput.bPopulated = OMX_FALSE;
    paramPortDefinitionInput.eDomain = OMX_PortDomainVideo;
    paramPortDefinitionInput.format.video.cMIMEType = (OMX_STRING)RAW_MIME_TYPE;
    paramPortDefinitionInput.format.video.pNativeRender = NULL;
    paramPortDefinitionInput.format.video.nFrameWidth = 176;
    paramPortDefinitionInput.format.video.nFrameHeight = 144;
    paramPortDefinitionInput.format.video.nStride = 0;
    paramPortDefinitionInput.format.video.nSliceHeight = 0;
    paramPortDefinitionInput.format.video.nBitrate = 64000;
    paramPortDefinitionInput.format.video.xFramerate = 15 << 16;
    paramPortDefinitionInput.format.video.bFlagErrorConcealment = OMX_FALSE;
    paramPortDefinitionInput.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    paramPortDefinitionInput.format.video.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;
    paramPortDefinitionInput.format.video.pNativeWindow = NULL;
    paramPortDefinitionInput.bBuffersContiguous = OMX_FALSE;
    paramPortDefinitionInput.nBufferAlignment = 0;

    // Nothing specific to initialize input port.
    InitInputPortFormatSpecific(&paramPortDefinitionInput);

    port->SetPortDefinition(&paramPortDefinitionInput, true);

    // OMX_VIDEO_PARAM_PORTFORMATTYPE
    OMX_VIDEO_PARAM_PORTFORMATTYPE paramPortFormat;
    memset(&paramPortFormat, 0, sizeof(paramPortFormat));
    SetTypeHeader(&paramPortFormat, sizeof(paramPortFormat));
    paramPortFormat.nPortIndex = INPORT_INDEX;
    paramPortFormat.nIndex = 0;
    paramPortFormat.eCompressionFormat = paramPortDefinitionInput.format.video.eCompressionFormat;
    paramPortFormat.eColorFormat = paramPortDefinitionInput.format.video.eColorFormat;
    paramPortFormat.xFramerate = paramPortDefinitionInput.format.video.xFramerate;

    port->SetPortVideoParam(&paramPortFormat, true);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXVideoEncoderBase::InitOutputPort(void) {
    this->ports[OUTPORT_INDEX] = new PortVideo;
    if (this->ports[OUTPORT_INDEX] == NULL) {
        return OMX_ErrorInsufficientResources;
    }

    PortVideo *port = static_cast<PortVideo *>(this->ports[OUTPORT_INDEX]);

    // OMX_VIDEO_PARAM_BITRATETYPE
    memset(&mParamBitrate, 0, sizeof(mParamBitrate));
    SetTypeHeader(&mParamBitrate, sizeof(mParamBitrate));
    mParamBitrate.nPortIndex = OUTPORT_INDEX;
    mParamBitrate.eControlRate = OMX_Video_ControlRateConstant;
    mParamBitrate.nTargetBitrate = 192000; // to be overridden

    // OMX_VIDEO_CONFIG_PRI_INFOTYPE
    memset(&mConfigPriInfo, 0, sizeof(mConfigPriInfo));
    SetTypeHeader(&mConfigPriInfo, sizeof(mConfigPriInfo));
    mConfigPriInfo.nPortIndex = OUTPORT_INDEX;
    mConfigPriInfo.nCapacity = 0;
    mConfigPriInfo.nHolder = NULL;

    // OMX_VIDEO_PARAM_INTEL_BITRATETYPE
    memset(&mParamIntelBitrate, 0, sizeof(mParamIntelBitrate));
    SetTypeHeader(&mParamIntelBitrate, sizeof(mParamIntelBitrate));
    mParamIntelBitrate.nPortIndex = OUTPORT_INDEX;
    mParamIntelBitrate.eControlRate = OMX_Video_Intel_ControlRateMax;
    mParamIntelBitrate.nTargetBitrate = 0; // to be overridden ?

    // OMX_VIDEO_CONFIG_INTEL_BITRATETYPE
    memset(&mConfigIntelBitrate, 0, sizeof(mConfigIntelBitrate));
    SetTypeHeader(&mConfigIntelBitrate, sizeof(mConfigIntelBitrate));
    mConfigIntelBitrate.nPortIndex = OUTPORT_INDEX;
    mConfigIntelBitrate.nMaxEncodeBitrate = 4000 * 1024; // Maximum bitrate
    mConfigIntelBitrate.nTargetPercentage = 95; // Target bitrate as percentage of maximum bitrate; e.g. 95 is 95%
    mConfigIntelBitrate.nWindowSize = 1000; // Window size in milliseconds allowed for bitrate to reach target
    mConfigIntelBitrate.nInitialQP = 36;  // Initial QP for I frames
    mConfigIntelBitrate.nMinQP = 18;

    // OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS
    memset(&mConfigIntelSliceNumbers, 0, sizeof(mConfigIntelSliceNumbers));
    SetTypeHeader(&mConfigIntelSliceNumbers, sizeof(mConfigIntelSliceNumbers));
    mConfigIntelSliceNumbers.nPortIndex = OUTPORT_INDEX;
    mConfigIntelSliceNumbers.nISliceNumber = 1;
    mConfigIntelSliceNumbers.nPSliceNumber = 1;

    // OMX_VIDEO_CONFIG_INTEL_AIR
    memset(&mConfigIntelAir, 0, sizeof(mConfigIntelAir));
    SetTypeHeader(&mConfigIntelAir, sizeof(mConfigIntelAir));
    mConfigIntelAir.nPortIndex = OUTPORT_INDEX;
    mConfigIntelAir.bAirEnable = OMX_FALSE;
    mConfigIntelAir.bAirAuto = OMX_FALSE;
    mConfigIntelAir.nAirMBs = 0;
    mConfigIntelAir.nAirThreshold = 0;

    // OMX_CONFIG_FRAMERATETYPE
    memset(&mConfigFramerate, 0, sizeof(mConfigFramerate));
    SetTypeHeader(&mConfigFramerate, sizeof(mConfigFramerate));
    mConfigFramerate.nPortIndex = OUTPORT_INDEX;
    mConfigFramerate.xEncodeFramerate =  0; // Q16 format

#if 0
    // OMX_VIDEO_PARAM_INTEL_ADAPTIVE_SLICE_CONTROL
    memset(&mParamIntelAdaptiveSliceControl, 0, sizeof(mParamIntelAdaptiveSliceControl));
    SetTypeHeader(&mParamIntelAdaptiveSliceControl, sizeof(mParamIntelAdaptiveSliceControl));
    mParamIntelAdaptiveSliceControl.nPortIndex = OUTPORT_INDEX;
    mParamIntelAdaptiveSliceControl.bEnable = OMX_FALSE;
    mParamIntelAdaptiveSliceControl.nMinPSliceNumber = 5;
    mParamIntelAdaptiveSliceControl.nNumPFramesToSkip = 8;
    mParamIntelAdaptiveSliceControl.nSliceSizeThreshold = 1200;
#endif

    // OMX_VIDEO_PARAM_PROFILELEVELTYPE
    memset(&mParamProfileLevel, 0, sizeof(mParamProfileLevel));
    SetTypeHeader(&mParamProfileLevel, sizeof(mParamProfileLevel));
    mParamProfileLevel.nPortIndex = OUTPORT_INDEX;
    mParamProfileLevel.eProfile = 0; // undefined profile, to be overridden
    mParamProfileLevel.eLevel = 0; // undefined level, to be overridden


    // OMX_PARAM_PORTDEFINITIONTYPE
    OMX_PARAM_PORTDEFINITIONTYPE paramPortDefinitionOutput;
    memset(&paramPortDefinitionOutput, 0, sizeof(paramPortDefinitionOutput));
    SetTypeHeader(&paramPortDefinitionOutput, sizeof(paramPortDefinitionOutput));
    paramPortDefinitionOutput.nPortIndex = OUTPORT_INDEX;
    paramPortDefinitionOutput.eDir = OMX_DirOutput;
    paramPortDefinitionOutput.nBufferCountActual = OUTPORT_ACTUAL_BUFFER_COUNT; // to be overridden
    paramPortDefinitionOutput.nBufferCountMin = OUTPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionOutput.nBufferSize = OUTPORT_BUFFER_SIZE; // to be overridden
    paramPortDefinitionOutput.bEnabled = OMX_TRUE;
    paramPortDefinitionOutput.bPopulated = OMX_FALSE;
    paramPortDefinitionOutput.eDomain = OMX_PortDomainVideo;
    paramPortDefinitionOutput.format.video.cMIMEType = NULL; // to be overridden
    paramPortDefinitionOutput.format.video.pNativeRender = NULL;
    paramPortDefinitionOutput.format.video.nFrameWidth = 176;
    paramPortDefinitionOutput.format.video.nFrameHeight = 144;
    paramPortDefinitionOutput.format.video.nStride = 176;
    paramPortDefinitionOutput.format.video.nSliceHeight = 144;
    paramPortDefinitionOutput.format.video.nBitrate = 64000;
    paramPortDefinitionOutput.format.video.xFramerate = 15 << 16;
    paramPortDefinitionOutput.format.video.bFlagErrorConcealment = OMX_FALSE;
    paramPortDefinitionOutput.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused; // to be overridden
    paramPortDefinitionOutput.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    paramPortDefinitionOutput.format.video.pNativeWindow = NULL;
    paramPortDefinitionOutput.bBuffersContiguous = OMX_FALSE;
    paramPortDefinitionOutput.nBufferAlignment = 0;

    InitOutputPortFormatSpecific(&paramPortDefinitionOutput);

    port->SetPortDefinition(&paramPortDefinitionOutput, true);

    // OMX_VIDEO_PARAM_PORTFORMATTYPE
    OMX_VIDEO_PARAM_PORTFORMATTYPE paramPortFormat;
    memset(&paramPortFormat, 0, sizeof(paramPortFormat));
    SetTypeHeader(&paramPortFormat, sizeof(paramPortFormat));
    paramPortFormat.nPortIndex = OUTPORT_INDEX;
    paramPortFormat.nIndex = 0;
    paramPortFormat.eCompressionFormat = paramPortDefinitionOutput.format.video.eCompressionFormat;
    paramPortFormat.eColorFormat = paramPortDefinitionOutput.format.video.eColorFormat;
    paramPortFormat.xFramerate = paramPortDefinitionOutput.format.video.xFramerate;

    port->SetPortVideoParam(&paramPortFormat, true);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::InitInputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionInput) {
    // no format specific to initialize input
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::ProcessorInit(void) {
    OMX_ERRORTYPE ret;
    ret = OMXComponentCodecBase::ProcessorInit();
    CHECK_RETURN_VALUE("OMXComponentCodecBase::ProcessorInit");

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::ProcessorDeinit(void) {
    OMX_ERRORTYPE ret;
    return OMXComponentCodecBase::ProcessorDeinit();
}

OMX_ERRORTYPE OMXVideoEncoderBase::ProcessorStop(void) {
    OMX_ERRORTYPE ret;
    this->ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
    return OMXComponentCodecBase::ProcessorStop();
}

OMX_ERRORTYPE OMXVideoEncoderBase::ProcessorFlush(OMX_U32 portIndex) {
    if (portIndex == INPORT_INDEX || portIndex == OMX_ALL) {
        this->ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retains,
    OMX_U32 numberBuffers) {

    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE OMXVideoEncoderBase::EnableBufferSharing(void)
{
    BufferShareStatus ret;

    if (mBufferSharingState != BUFFER_SHARING_INVALID) {
        LOGE("EnableBufferSharing: Invalid buffer sharing state: %d", mBufferSharingState);
        return OMX_ErrorUndefined;
    }

    mBufferSharingCount = 4;
    mBufferSharingInfo = NULL;
    mBufferSharingLib = BufferShareRegistry::getInstance();

    ret = mBufferSharingLib->encoderRequestToEnableSharingMode();
    if (ret != BS_SUCCESS) {
        LOGE("encoderRequestToEnableSharingMode failed. Error = %d", ret);
        return OMX_ErrorUndefined;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::DisableBufferSharing(void)
{
    BufferShareStatus ret;

    if ((mBufferSharingState != BUFFER_SHARING_INVALID) &&
        (mBufferSharingState != BUFFER_SHARING_LOADED)) {
        LOGE("DisableBufferSharing: Invalid buffer sharing state: %d", mBufferSharingState);
        return OMX_ErrorUndefined;
    }

    if (mBufferSharingState == BUFFER_SHARING_INVALID) {
        return OMX_ErrorNone;
    }

    if (mBufferSharingInfo) {
        delete [] mBufferSharingInfo;
    }
    mBufferSharingInfo = NULL;

    ret = mBufferSharingLib->encoderRequestToDisableSharingMode();
    if (ret != BS_SUCCESS) {
        LOGE("encoderRequestToDisableSharingMode failed. Error = %d", ret);
        return OMX_ErrorUndefined;
    }

    mBufferSharingState = BUFFER_SHARING_INVALID;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::StartBufferSharing(void) {
    // TODO: consolidate the following APIs

    /*
    CheckAndEnableBufferSharingMode
    RequestShareBuffers
    RegisterShareBuffersToPort
    RegisterShareBuffersToLib
    EnterBufferSharingMode
    */
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::StopBufferSharing(void) {
    if ((mBufferSharingState != BUFFER_SHARING_INVALID) &&
        (mBufferSharingState != BUFFER_SHARING_EXECUTING)) {
        LOGE("StopBufferSharing: Invalid buffer sharing state: %d", mBufferSharingState);
        return OMX_ErrorUndefined;
    }

    if (mBufferSharingState == BUFFER_SHARING_INVALID) {
        return OMX_ErrorNone;
    }

    BufferShareStatus ret = mBufferSharingLib->encoderExitSharingMode();
    if (ret != BS_SUCCESS) {
        LOGE("encoderExitSharingMode failed. Error = %d", ret);
        return OMX_ErrorUndefined;
    }

    mBufferSharingState = BUFFER_SHARING_LOADED;
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXVideoEncoderBase::BuildHandlerList(void) {
    OMXComponentCodecBase::BuildHandlerList();
    AddHandler(OMX_IndexParamVideoPortFormat, GetParamVideoPortFormat, SetParamVideoPortFormat);
    AddHandler(OMX_IndexParamVideoBitrate, GetParamVideoBitrate, SetParamVideoBitrate);
    AddHandler((OMX_INDEXTYPE)OMX_IndexIntelPrivateInfo, GetIntelPrivateInfo, SetIntelPrivateInfo);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamIntelBitrate, GetParamIntelBitrate, SetParamIntelBitrate);
    AddHandler((OMX_INDEXTYPE)OMX_IndexConfigIntelBitrate, GetConfigIntelBitrate, SetConfigIntelBitrate);
    AddHandler((OMX_INDEXTYPE)OMX_IndexConfigIntelSliceNumbers, GetConfigIntelSliceNumbers, SetConfigIntelSliceNumbers);
    AddHandler((OMX_INDEXTYPE)OMX_IndexConfigIntelAIR, GetConfigIntelAIR, SetConfigIntelAIR);
    AddHandler(OMX_IndexConfigVideoFramerate, GetConfigVideoFramerate, SetConfigVideoFramerate);
    AddHandler(OMX_IndexConfigVideoIntraVOPRefresh, GetConfigVideoIntraVOPRefresh, SetConfigVideoIntraVOPRefresh);
    //AddHandler(OMX_IndexParamIntelAdaptiveSliceControl, GetParamIntelAdaptiveSliceControl, SetParamIntelAdaptiveSliceControl);
    AddHandler(OMX_IndexParamVideoProfileLevelQuerySupported, GetParamVideoProfileLevelQuerySupported, SetParamVideoProfileLevelQuerySupported);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetParamVideoPortFormat(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_PORTFORMATTYPE *p = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX_RANGE(p);
    CHECK_ENUMERATION_RANGE(p->nIndex, 1);

    PortVideo *port = NULL;
    port = static_cast<PortVideo *>(this->ports[p->nPortIndex]);
    memcpy(p, port->GetPortVideoParam(), sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetParamVideoPortFormat(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_PORTFORMATTYPE *p = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX_RANGE(p);
    CHECK_SET_PARAM_STATE();

    // TODO: do we need to check if port is enabled?
    PortVideo *port = NULL;
    port = static_cast<PortVideo *>(this->ports[p->nPortIndex]);
    port->SetPortVideoParam(p, false);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetParamVideoBitrate(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_BITRATETYPE *p = (OMX_VIDEO_PARAM_BITRATETYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, &mParamBitrate, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetParamVideoBitrate(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_BITRATETYPE *p = (OMX_VIDEO_PARAM_BITRATETYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    // This disables other type of bitrate control mechanism
    // TODO: check if it is desired
    mParamIntelBitrate.eControlRate = OMX_Video_Intel_ControlRateMax;

    // TODO: can we override  mParamBitrate.nPortIndex (See SetPortBitrateParam)
    mParamBitrate.eControlRate = p->eControlRate;
    mParamBitrate.nTargetBitrate = p->nTargetBitrate;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetIntelPrivateInfo(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_CONFIG_PRI_INFOTYPE *p = (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, &mConfigPriInfo, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetIntelPrivateInfo(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_CONFIG_PRI_INFOTYPE *p = (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // OMX_VIDEO_CONFIG_PRI_INFOTYPE is static parameter?
    CHECK_SET_PARAM_STATE();

    // TODO: can we override  mConfigPriInfo.nPortIndex (See SetPortPrivateInfoParam)

    if(p->nHolder != NULL) {
        // TODO: do we need to free nHolder?
        if (mConfigPriInfo.nHolder) {
            free(mConfigPriInfo.nHolder);
        }
        mConfigPriInfo.nCapacity = p->nCapacity;
        // TODO: nCapacity is in 8-bit unit or 32-bit unit?
        // TODO: check memory allocation
        mConfigPriInfo.nHolder = (OMX_PTR)malloc(sizeof(OMX_U32) * p->nCapacity);
        memcpy(mConfigPriInfo.nHolder, p->nHolder, sizeof(OMX_U32) * p->nCapacity);
    } else {
        mConfigPriInfo.nCapacity = 0;
        mConfigPriInfo.nHolder = NULL;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetParamIntelBitrate(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_INTEL_BITRATETYPE *p = (OMX_VIDEO_PARAM_INTEL_BITRATETYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, &mParamIntelBitrate, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetParamIntelBitrate(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_INTEL_BITRATETYPE *p = (OMX_VIDEO_PARAM_INTEL_BITRATETYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    mParamIntelBitrate = *p;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetConfigIntelBitrate(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *p = (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, &mConfigIntelBitrate, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetConfigIntelBitrate(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    if (mParamIntelBitrate.eControlRate == OMX_Video_Intel_ControlRateMax) {
        LOGE("SetConfigIntelBitrate failed. Feature is disabled.");
        return OMX_ErrorUnsupportedIndex;
    }
    OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *p = (OMX_VIDEO_CONFIG_INTEL_BITRATETYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set in either Loaded state (ComponentSetParam) or Executing state (ComponentSetConfig)
    mConfigIntelBitrate = *p;

    // return OMX_ErrorNone if not in Executing state
    // TODO: return OMX_ErrorIncorrectStateOperation?
    CHECK_SET_CONFIG_STATE();

    if (mParamIntelBitrate.eControlRate != OMX_Video_Intel_ControlRateVideoConferencingMode) {
        LOGE("SetConfigIntelBitrate failed. Feature is supported only in VCM.");
        return OMX_ErrorUnsupportedSetting;
    }
    // TODO: apply Bitrate configuration in Executing state
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXVideoEncoderBase::GetConfigIntelSliceNumbers(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *p = (OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, &mConfigIntelSliceNumbers, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetConfigIntelSliceNumbers(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    if (mParamIntelBitrate.eControlRate == OMX_Video_Intel_ControlRateMax) {
        LOGE("SetConfigIntelSliceNumbers failed. Feature is disabled.");
        return OMX_ErrorUnsupportedIndex;
    }
    OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *p = (OMX_VIDEO_CONFIG_INTEL_SLICE_NUMBERS *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set in either Loaded  state (ComponentSetParam) or Executing state (ComponentSetConfig)
    mConfigIntelSliceNumbers = *p;

    // return OMX_ErrorNone if not in Executing state
    // TODO: return OMX_ErrorIncorrectStateOperation?
    CHECK_SET_CONFIG_STATE();

    if (mParamIntelBitrate.eControlRate != OMX_Video_Intel_ControlRateVideoConferencingMode) {
        LOGE("SetConfigIntelSliceNumbers failed. Feature is supported only in VCM.");
        return OMX_ErrorUnsupportedSetting;
    }
    // TODO: apply Slice numbers configuration in Executing state
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXVideoEncoderBase::GetConfigIntelAIR(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_CONFIG_INTEL_AIR *p = (OMX_VIDEO_CONFIG_INTEL_AIR *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, &mConfigIntelAir, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetConfigIntelAIR(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    if (mParamIntelBitrate.eControlRate == OMX_Video_Intel_ControlRateMax) {
        LOGE("SetConfigIntelAIR failed. Feature is disabled.");
        return OMX_ErrorUnsupportedIndex;
    }
    OMX_VIDEO_CONFIG_INTEL_AIR *p = (OMX_VIDEO_CONFIG_INTEL_AIR *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set in either Loaded  state (ComponentSetParam) or Executing state (ComponentSetConfig)
    mConfigIntelAir = *p;

    // return OMX_ErrorNone if not in Executing state
    // TODO: return OMX_ErrorIncorrectStateOperation?
    CHECK_SET_CONFIG_STATE();

    if (mParamIntelBitrate.eControlRate != OMX_Video_Intel_ControlRateVideoConferencingMode) {
        LOGE("SetConfigIntelAIR failed. Feature is supported only in VCM.");
        return OMX_ErrorUnsupportedSetting;
    }
    // TODO: apply AIR configuration in Executing state
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetConfigVideoFramerate(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_CONFIG_FRAMERATETYPE *p = (OMX_CONFIG_FRAMERATETYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, &mConfigFramerate, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetConfigVideoFramerate(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    if (mParamIntelBitrate.eControlRate == OMX_Video_Intel_ControlRateMax) {
        LOGE("SetConfigVideoFramerate failed. Feature is disabled.");
        return OMX_ErrorUnsupportedIndex;
    }
    OMX_CONFIG_FRAMERATETYPE *p = (OMX_CONFIG_FRAMERATETYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set in either Loaded state  (ComponentSetParam) or Executing state (ComponentSetConfig)
    mConfigFramerate = *p;

    // return OMX_ErrorNone if not in Executing state
    // TODO, return OMX_ErrorIncorrectStateOperation?
    CHECK_SET_CONFIG_STATE();

    if (mParamIntelBitrate.eControlRate != OMX_Video_Intel_ControlRateVideoConferencingMode) {
        LOGE("SetConfigIntelAIR failed. Feature is supported only in VCM.");
        return OMX_ErrorUnsupportedSetting;
    }
    // TODO: apply Framerate configuration in Executing state
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetConfigVideoIntraVOPRefresh(OMX_PTR pStructure) {
    LOGW("GetConfigVideoIntraVOPRefresh is not supported.");
    return OMX_ErrorUnsupportedSetting;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetConfigVideoIntraVOPRefresh(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_CONFIG_INTRAREFRESHVOPTYPE *p = (OMX_CONFIG_INTRAREFRESHVOPTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // return OMX_ErrorNone if not in Executing state
    // TODO: return OMX_ErrorIncorrectStateOperation?
    CHECK_SET_CONFIG_STATE();

    // TODO: apply VOP refresh configuration in Executing state
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetParamIntelAdaptiveSliceControl(OMX_PTR pStructure) {
#if 0
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_INTEL_ADAPTIVE_SLICE_CONTROL *p = (OMX_VIDEO_PARAM_INTEL_ADAPTIVE_SLICE_CONTROL *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, mParamIntelAdaptiveSliceControl, sizeof(*p));
#endif
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetParamIntelAdaptiveSliceControl(OMX_PTR pStructure) {
#if 0
    OMX_ERRORTYPE ret;
    if (mParamIntelBitrate.eControlRate == OMX_Video_Intel_ControlRateMax) {
        LOGE("SetParamIntelAdaptiveSliceControl failed. Feature is disabled.");
        return OMX_ErrorUnsupportedIndex;
    }
    OMX_VIDEO_PARAM_INTEL_ADAPTIVE_SLICE_CONTROL *p = (OMX_VIDEO_PARAM_INTEL_ADAPTIVE_SLICE_CONTROL *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set only in Loaded state (ComponentSetParam)
    CHECK_SET_PARAM_STATE();

    mParamIntelAdaptiveSliceControl = *p;
#endif
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::GetParamVideoProfileLevelQuerySupported(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_PROFILELEVELTYPE *p = (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // assign values instead of memory coping to avoid nProfileIndex being overridden
    p->eProfile = mParamProfileLevel.eProfile;
    p->eLevel = mParamProfileLevel.eLevel;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderBase::SetParamVideoProfileLevelQuerySupported(OMX_PTR pStructure) {
    LOGW("SetParamVideoProfileLevelQuerySupported is not supported.");
    return OMX_ErrorUnsupportedSetting;
}

