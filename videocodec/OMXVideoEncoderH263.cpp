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
#define LOG_TAG "OMXVideoEncoderH263"
#include <utils/Log.h>
#include "OMXVideoEncoderH263.h"

static const char* H263_MIME_TYPE = "video/h263";

OMXVideoEncoderH263::OMXVideoEncoderH263() {
    LOGV("OMXVideoEncoderH263 is constructed.");
    BuildHandlerList();
}

OMXVideoEncoderH263::~OMXVideoEncoderH263() {
    LOGV("OMXVideoEncoderH263 is destructed.");
}

OMX_ERRORTYPE OMXVideoEncoderH263::InitOutputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionOutput) {
    // OMX_VIDEO_PARAM_H263TYPE
    memset(&mParamH263, 0, sizeof(mParamH263));
    SetTypeHeader(&mParamH263, sizeof(mParamH263));
    mParamH263.nPortIndex = OUTPORT_INDEX;
    mParamH263.eProfile = OMX_VIDEO_H263ProfileBaseline;
    // TODO: check eLevel, 10 in Froyo
    mParamH263.eLevel = OMX_VIDEO_H263Level70; //OMX_VIDEO_H263Level10;

    // override OMX_PARAM_PORTDEFINITIONTYPE
    paramPortDefinitionOutput->nBufferCountActual = OUTPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionOutput->nBufferCountMin = OUTPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionOutput->nBufferSize = OUTPORT_BUFFER_SIZE;
    paramPortDefinitionOutput->format.video.cMIMEType = (OMX_STRING)H263_MIME_TYPE;
    paramPortDefinitionOutput->format.video.eCompressionFormat = OMX_VIDEO_CodingH263;

    // override OMX_VIDEO_PARAM_PROFILELEVELTYPE
    // TODO: check if profile/level supported is correct
    mParamProfileLevel.eProfile = mParamH263.eProfile;
    mParamProfileLevel.eLevel = mParamH263.eLevel; //OMX_VIDEO_H263Level70

    // override OMX_VIDEO_PARAM_BITRATETYPE
    mParamBitrate.nTargetBitrate = 64000;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderH263::ProcessorInit(void) {
    return OMXVideoEncoderBase::ProcessorInit();
}

OMX_ERRORTYPE OMXVideoEncoderH263::ProcessorDeinit(void) {
    return OMXVideoEncoderBase::ProcessorDeinit();
}

OMX_ERRORTYPE OMXVideoEncoderH263::ProcessorProcess(
        OMX_BUFFERHEADERTYPE **buffers,
        buffer_retain_t *retains,
        OMX_U32 numberBuffers) {

    return OMXVideoEncoderBase::ProcessorProcess(buffers, retains, numberBuffers);
}

OMX_ERRORTYPE OMXVideoEncoderH263::BuildHandlerList(void) {
    OMXVideoEncoderBase::BuildHandlerList();
    AddHandler(OMX_IndexParamVideoH263, GetParamVideoH263, SetParamVideoH263);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderH263::GetParamVideoH263(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_H263TYPE *p = (OMX_VIDEO_PARAM_H263TYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    memcpy(p, &mParamH263, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderH263::SetParamVideoH263(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_H263TYPE *p = (OMX_VIDEO_PARAM_H263TYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    // TODO: do we need to check if port is enabled?
    // TODO: see SetPortH263Param implementation - Can we make simple copy????
    memcpy(&mParamH263, p, sizeof(mParamH263));
    return OMX_ErrorNone;
}


DECLARE_OMX_COMPONENT("OMX.Intel.VideoEncoder.H263", "video_encoder.h263", OMXVideoEncoderH263);

