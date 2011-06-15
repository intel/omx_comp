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
#define LOG_TAG "OMXVideoEncoderMPEG4"
#include <utils/Log.h>
#include "OMXVideoEncoderMPEG4.h"

static const char* MPEG4_MIME_TYPE = "video/mpeg4";

OMXVideoEncoderMPEG4::OMXVideoEncoderMPEG4() {
    LOGV("OMXVideoEncoderMPEG4 is constructed.");
    BuildHandlerList();
}

OMXVideoEncoderMPEG4::~OMXVideoEncoderMPEG4() {
    LOGV("OMXVideoEncoderMPEG4 is destructed.");
}

OMX_ERRORTYPE OMXVideoEncoderMPEG4::InitOutputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionOutput) {
    // OMX_VIDEO_PARAM_MPEG4TYPE
    memset(&mParamMpeg4, 0, sizeof(mParamMpeg4));
    SetTypeHeader(&mParamMpeg4, sizeof(mParamMpeg4));
    mParamMpeg4.nPortIndex = OUTPORT_INDEX;
    mParamMpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    // TODO: Check eLevel (Level3 in Froyo)
    mParamMpeg4.eLevel = OMX_VIDEO_MPEG4Level5; //OMX_VIDEO_MPEG4Level3;

    // override OMX_PARAM_PORTDEFINITIONTYPE
    paramPortDefinitionOutput->nBufferCountActual = OUTPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionOutput->nBufferCountMin = OUTPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionOutput->nBufferSize = OUTPORT_BUFFER_SIZE;
    paramPortDefinitionOutput->format.video.cMIMEType = (OMX_STRING)MPEG4_MIME_TYPE;
    paramPortDefinitionOutput->format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;

    // override OMX_VIDEO_PARAM_PROFILELEVELTYPE
    // TODO: check if profile/level supported is correct
    mParamProfileLevel.eProfile = mParamMpeg4.eProfile;
    mParamProfileLevel.eLevel = mParamMpeg4.eLevel; //OMX_VIDEO_MPEG4Level5;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderMPEG4::ProcessorInit(void) {
    return OMXVideoEncoderBase::ProcessorInit();
}

OMX_ERRORTYPE OMXVideoEncoderMPEG4::ProcessorDeinit(void) {
    return OMXVideoEncoderBase::ProcessorDeinit();
}

OMX_ERRORTYPE OMXVideoEncoderMPEG4::ProcessorProcess(
        OMX_BUFFERHEADERTYPE **buffers,
        buffer_retain_t *retains,
        OMX_U32 numberBuffers) {

    return OMXVideoEncoderBase::ProcessorProcess(buffers, retains, numberBuffers);
}

OMX_ERRORTYPE OMXVideoEncoderMPEG4::BuildHandlerList(void) {
    OMXVideoEncoderBase::BuildHandlerList();
    AddHandler(OMX_IndexParamVideoMpeg4, GetParamVideoMpeg4, SetParamVideoMpeg4);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderMPEG4::GetParamVideoMpeg4(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_MPEG4TYPE *p = (OMX_VIDEO_PARAM_MPEG4TYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    memcpy(p, &mParamMpeg4, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderMPEG4::SetParamVideoMpeg4(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_MPEG4TYPE *p = (OMX_VIDEO_PARAM_MPEG4TYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    // TODO: do we need to check if port is enabled?
    // TODO: see SetPortMpeg4Param implementation - Can we make simple copy????
    memcpy(&mParamMpeg4, p, sizeof(mParamMpeg4));
    return OMX_ErrorNone;
}


DECLARE_OMX_COMPONENT("OMX.Intel.VideoEncoder.MPEG4", "video_encoder.mpeg4", OMXVideoEncoderMPEG4);


