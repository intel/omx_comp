/*
* Copyright (c) 2012 Intel Corporation.  All rights reserved.
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
#define LOG_TAG "OMXVideoDecoder"
#include "OMXVideoDecoderVP8.h"
using namespace YamiMediaCodec;

// Be sure to have an equal string in VideoDecoderHost.cpp (libmix)
static const char* VP8_MIME_TYPE = "video/x-vnd.on2.vp8";

OMXVideoDecoderVP8::OMXVideoDecoderVP8() {
    omx_verboseLog("OMXVideoDecoderVP8 is constructed.");
    mVideoDecoder = createVideoDecoder(VP8_MIME_TYPE);
    if (!mVideoDecoder) {
        omx_errorLog("createVideoDecoder failed for \"%s\"", VP8_MIME_TYPE);
    }
    BuildHandlerList();
}

OMXVideoDecoderVP8::~OMXVideoDecoderVP8() {
    omx_verboseLog("OMXVideoDecoderVP8 is destructed.");
}

OMX_ERRORTYPE OMXVideoDecoderVP8::InitInputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionInput) {
    // OMX_PARAM_PORTDEFINITIONTYPE
    paramPortDefinitionInput->nBufferCountActual = INPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionInput->nBufferCountMin = INPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionInput->nBufferSize = INPORT_BUFFER_SIZE;
    paramPortDefinitionInput->format.video.cMIMEType = (OMX_STRING)VP8_MIME_TYPE;
    paramPortDefinitionInput->format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE) OMX_VIDEO_CodingVP8;

    // OMX_VIDEO_PARAM_VP8TYPE
    memset(&mParamVp8, 0, sizeof(mParamVp8));
    SetTypeHeader(&mParamVp8, sizeof(mParamVp8));
    mParamVp8.nPortIndex = INPORT_INDEX;
    mParamVp8.eProfile = OMX_VIDEO_VP8ProfileMain;
    mParamVp8.eLevel = OMX_VIDEO_VP8Level_Version0;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderVP8::ProcessorInit(void *parser_handle) {
    return OMXVideoDecoderBase::ProcessorInit(parser_handle);
}

OMX_ERRORTYPE OMXVideoDecoderVP8::ProcessorDeinit(void) {
    return OMXVideoDecoderBase::ProcessorDeinit();
}

OMX_ERRORTYPE OMXVideoDecoderVP8::ProcessorFlush(OMX_U32 portIndex) {
    return OMXVideoDecoderBase::ProcessorFlush(portIndex);
}

OMX_ERRORTYPE OMXVideoDecoderVP8::ProcessorProcess(
        OMX_BUFFERHEADERTYPE **Buffers,
        buffer_retain_t *retains,
        OMX_U32 numberBuffers) {

    return OMXVideoDecoderBase::ProcessorProcess(Buffers, retains, numberBuffers);
}

OMX_ERRORTYPE OMXVideoDecoderVP8::PrepareConfigBuffer(VideoConfigBuffer *p) {
    OMX_ERRORTYPE ret;
    ret = OMXVideoDecoderBase::PrepareConfigBuffer(p);
    CHECK_RETURN_VALUE("OMXVideoDecoderBase::PrepareConfigBuffer");

    p->profile      = VAProfileVP8Version0_3;
    p->flag         = USE_NATIVE_GRAPHIC_BUFFER;
    p->surfaceNumber= 0;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderVP8::PrepareDecodeBuffer(OMX_BUFFERHEADERTYPE *buffer, buffer_retain_t *retain, VideoDecodeBuffer *p) {
    return OMXVideoDecoderBase::PrepareDecodeBuffer(buffer, retain, p);
}

OMX_ERRORTYPE OMXVideoDecoderVP8::BuildHandlerList(void) {
    OMXVideoDecoderBase::BuildHandlerList();
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamVideoVp8, GetParamVideoVp8, SetParamVideoVp8);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderVP8::GetParamVideoVp8(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_VP8TYPE *p = (OMX_VIDEO_PARAM_VP8TYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, INPORT_INDEX);

    memcpy(p, &mParamVp8, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderVP8::SetParamVideoVp8(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_VP8TYPE *p = (OMX_VIDEO_PARAM_VP8TYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, INPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    // TODO: do we need to check if port is enabled?
    memcpy(&mParamVp8, p, sizeof(mParamVp8));
    return OMX_ErrorNone;
}


DECLARE_OMX_COMPONENT("OMX.Intel.VideoDecoder.VP8", "video_decoder.vp8", OMXVideoDecoderVP8);


