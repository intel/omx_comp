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
#define LOG_TAG "OMXVideoEncoderAVC"
#include <utils/Log.h>
#include "OMXVideoEncoderAVC.h"

static const char* AVC_MIME_TYPE = "video/h264";


OMXVideoEncoderAVC::OMXVideoEncoderAVC() {
    LOGV("OMXVideoEncoderAVC is constructed.");
    BuildHandlerList();
}

OMXVideoEncoderAVC::~OMXVideoEncoderAVC() {
    LOGV("OMXVideoEncoderAVC is destructed.");
}

OMX_ERRORTYPE OMXVideoEncoderAVC::InitOutputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionOutput) {
    // OMX_VIDEO_PARAM_AVCTYPE
    memset(&mParamAvc, 0, sizeof(mParamAvc));
    SetTypeHeader(&mParamAvc, sizeof(mParamAvc));
    mParamAvc.nPortIndex = OUTPORT_INDEX;
    mParamAvc.eProfile = OMX_VIDEO_AVCProfileBaseline;
    mParamAvc.eLevel = OMX_VIDEO_AVCLevel1;

    // OMX_NALSTREAMFORMATTYPE
    memset(&mNalStreamFormat, 0, sizeof(mNalStreamFormat));
    SetTypeHeader(&mNalStreamFormat, sizeof(mNalStreamFormat));
    mNalStreamFormat.nPortIndex = OUTPORT_INDEX;
    // TODO: check if this is desired Nalu Format
    mNalStreamFormat.eNaluFormat = OMX_NaluFormatStartCodesSeparateFirstHeader; //OMX_NaluFormatZeroByteInterleaveLength;

    // OMX_VIDEO_CONFIG_AVCINTRAPERIOD
    memset(&mConfigAvcIntraPeriod, 0, sizeof(mConfigAvcIntraPeriod));
    SetTypeHeader(&mConfigAvcIntraPeriod, sizeof(mConfigAvcIntraPeriod));
    mConfigAvcIntraPeriod.nPortIndex = OUTPORT_INDEX;
    // TODO: need to be populated from Video Encoder
    mConfigAvcIntraPeriod.nIDRPeriod = 0;
    mConfigAvcIntraPeriod.nPFrames = 0;

    // OMX_VIDEO_CONFIG_NALSIZE
    memset(&mConfigNalSize, 0, sizeof(mConfigNalSize));
    SetTypeHeader(&mConfigNalSize, sizeof(mConfigNalSize));
    mConfigNalSize.nPortIndex = OUTPORT_INDEX;
    mConfigNalSize.nNaluBytes = 0;

    // OMX_VIDEO_PARAM_INTEL_AVCVUI
#if 0
    memset(&mParamIntelAvcVui, 0, sizeof(mParamIntelAvcVui));
    SetTypeHeader(&mParamIntelAvcVui, sizeof(mParamIntelAvcVui));
    mParamIntelAvcVui.nPortIndex = OUTPORT_INDEX;
    mParamIntelAvcVui.bVuiGeneration = OMX_FALSE;
#endif

    // override OMX_PARAM_PORTDEFINITIONTYPE
    paramPortDefinitionOutput->nBufferCountActual = OUTPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionOutput->nBufferCountMin = OUTPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionOutput->nBufferSize = OUTPORT_BUFFER_SIZE;
    paramPortDefinitionOutput->format.video.cMIMEType = (OMX_STRING)AVC_MIME_TYPE;
    paramPortDefinitionOutput->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    // override OMX_VIDEO_PARAM_PROFILELEVELTYPE
    // TODO: check if profile/level supported is correct
    mParamProfileLevel.eProfile = mParamAvc.eProfile;
    mParamProfileLevel.eLevel = mParamAvc.eLevel;

    // override OMX_VIDEO_PARAM_BITRATETYPE
    mParamBitrate.nTargetBitrate = 192000;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::ProcessorInit(void) {
    return OMXVideoEncoderBase::ProcessorInit();
}

OMX_ERRORTYPE OMXVideoEncoderAVC::ProcessorDeinit(void) {
    return OMXVideoEncoderBase::ProcessorDeinit();
}

OMX_ERRORTYPE OMXVideoEncoderAVC::ProcessorProcess(
        OMX_BUFFERHEADERTYPE **buffers,
        buffer_retain_t *retains,
        OMX_U32 numberBuffers) {

    return OMXVideoEncoderBase::ProcessorProcess(buffers, retains, numberBuffers);
}

OMX_ERRORTYPE OMXVideoEncoderAVC::BuildHandlerList(void) {
    OMXVideoEncoderBase::BuildHandlerList();
    AddHandler(OMX_IndexParamVideoAvc, GetParamVideoAvc, SetParamVideoAvc);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamNalStreamFormat, GetParamNalStreamFormat, SetParamNalStreamFormat);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamNalStreamFormatSupported, GetParamNalStreamFormatSupported, SetParamNalStreamFormatSupported);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamNalStreamFormatSelect, GetParamNalStreamFormatSelect, SetParamNalStreamFormatSelect);
    AddHandler(OMX_IndexConfigVideoAVCIntraPeriod, GetConfigVideoAVCIntraPeriod, SetConfigVideoAVCIntraPeriod);
    AddHandler(OMX_IndexConfigVideoNalSize, GetConfigVideoNalSize, SetConfigVideoNalSize);
    //AddHandler(OMX_IndexParamIntelAVCVUI, GetParamIntelAVCVUI, SetParamIntelAVCVUI);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamVideoBytestream, GetParamVideoBytestream, SetParamVideoBytestream);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamVideoAvc(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_AVCTYPE *p = (OMX_VIDEO_PARAM_AVCTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    memcpy(p, &mParamAvc, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetParamVideoAvc(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_AVCTYPE *p = (OMX_VIDEO_PARAM_AVCTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    // TODO: do we need to check if port is enabled?
    // TODO: see SetPortAvcParam implementation - Can we make simple copy????
    memcpy(&mParamAvc, p, sizeof(mParamAvc));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamNalStreamFormat(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_NALSTREAMFORMATTYPE *p = (OMX_NALSTREAMFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    // TODO: check if this is desired format
    p->eNaluFormat = OMX_NaluFormatStartCodesSeparateFirstHeader; //OMX_NaluFormatStartCodes;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetParamNalStreamFormat(OMX_PTR pStructure) {
    LOGW("SetParamNalStreamFormat is not supported.");
    return OMX_ErrorUnsupportedSetting;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamNalStreamFormatSupported(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_NALSTREAMFORMATTYPE *p = (OMX_NALSTREAMFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    p->eNaluFormat = (OMX_NALUFORMATSTYPE)
            (OMX_NaluFormatStartCodes |
             OMX_NaluFormatStartCodesSeparateFirstHeader);

    // TODO: check if this is desired format
             // OMX_NaluFormatFourByteInterleaveLength |
             //OMX_NaluFormatZeroByteInterleaveLength);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetParamNalStreamFormatSupported(OMX_PTR pStructure) {
    LOGW("SetParamNalStreamFormatSupported is not supported.");
    return OMX_ErrorUnsupportedSetting;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamNalStreamFormatSelect(OMX_PTR pStructure) {
    LOGW("GetParamNalStreamFormatSelect is not supported.");
    return OMX_ErrorUnsupportedSetting;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetParamNalStreamFormatSelect(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_NALSTREAMFORMATTYPE *p = (OMX_NALSTREAMFORMATTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // return OMX_ErrorIncorrectStateOperation if not in Loaded state
    CHECK_SET_PARAM_STATE();

    if (p->eNaluFormat != OMX_NaluFormatStartCodes &&
        p->eNaluFormat != OMX_NaluFormatStartCodesSeparateFirstHeader) {
        //p->eNaluFormat != OMX_NaluFormatFourByteInterleaveLength &&
        //p->eNaluFormat != OMX_NaluFormatZeroByteInterleaveLength) {
        // TODO: check if this is desried
        return OMX_ErrorBadParameter;
    }

    mNalStreamFormat = *p;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetConfigVideoAVCIntraPeriod(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *p = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    // TODO: populate mConfigAvcIntraPeriod from VideoEncoder
    // return OMX_ErrorNotReady if VideoEncoder is not created.
    memcpy(p, &mConfigAvcIntraPeriod, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetConfigVideoAVCIntraPeriod(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *p = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set in either Loaded state (ComponentSetParam) or Executing state (ComponentSetConfig)
    mConfigAvcIntraPeriod = *p;

    // return OMX_ErrorNone if not in Executing state
    // TODO:  return OMX_ErrorIncorrectStateOperation?
    CHECK_SET_CONFIG_STATE();

    // TODO: apply AVC Intra Period configuration in Executing state
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetConfigVideoNalSize(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_CONFIG_NALSIZE *p = (OMX_VIDEO_CONFIG_NALSIZE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, &mConfigNalSize, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetConfigVideoNalSize(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    if (mParamIntelBitrate.eControlRate == OMX_Video_Intel_ControlRateMax) {
        LOGE("SetConfigVideoNalSize failed. Feature is disabled.");
        return OMX_ErrorUnsupportedIndex;
    }
    OMX_VIDEO_CONFIG_NALSIZE *p = (OMX_VIDEO_CONFIG_NALSIZE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set in either Loaded  state (ComponentSetParam) or Executing state (ComponentSetConfig)
    mConfigNalSize = *p;

    // return OMX_ErrorNone if not in Executing state
    // TODO: return OMX_ErrorIncorrectStateOperation?
    CHECK_SET_CONFIG_STATE();

    if (mParamIntelBitrate.eControlRate != OMX_Video_Intel_ControlRateVideoConferencingMode) {
        LOGE("SetConfigVideoNalSize failed. Feature is supported only in VCM.");
        return OMX_ErrorUnsupportedSetting;
    }
    // TODO: apply NAL Size configuration in Executing state
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamIntelAVCVUI(OMX_PTR pStructure) {
#if 0
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_INTEL_AVCVUI *p = (OMX_VIDEO_PARAM_INTEL_AVCVUI *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    memcpy(p, mParamIntelAvcVui, sizeof(*p));
#endif
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetParamIntelAVCVUI(OMX_PTR pStructure) {
#if 0
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_INTEL_AVCVUI *p = (OMX_VIDEO_PARAM_INTEL_AVCVUI *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set only in Loaded state (ComponentSetParam)
    CHECK_SET_PARAM_STATE();

    mParamIntelAvcVui = *p;
#endif
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamVideoBytestream(OMX_PTR pStructure) {
    return OMX_ErrorUnsupportedSetting;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetParamVideoBytestream(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_BYTESTREAMTYPE *p = (OMX_VIDEO_PARAM_BYTESTREAMTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    // set only in Loaded state (ComponentSetParam)
    CHECK_SET_PARAM_STATE();

    if (p->bBytestream == OMX_TRUE) {
        mNalStreamFormat.eNaluFormat = OMX_NaluFormatStartCodes;
    } else {
        // TODO: do we need to override the Nalu format?
        mNalStreamFormat.eNaluFormat = OMX_NaluFormatZeroByteInterleaveLength;
    }

    return OMX_ErrorNone;
}


DECLARE_OMX_COMPONENT("OMX.Intel.VideoEncoder.AVC", "video_encoder.avc", OMXVideoEncoderAVC);
