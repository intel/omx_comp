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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define LOG_TAG "OMXVideoDecoderMJPEG"
#include "OMXVideoDecoderMJPEG.h"
using namespace YamiMediaCodec;

// Be sure to have an equal string in VideoDecoderHost.cpp
// OMX defines OMX_IMAGE_CodingJPEG in OMX_Image.h and OMX_VIDEO_CodingMJPEG in OMX_Video.h,
// we choose OMX_VIDEO_CodingMJPEG for the component type.
// most media framework doesn't distinguish mjpeg jpeg; so we still use "image/jpeg" as mimetype.
static const char* MJPEG_MIME_TYPE = "image/jpeg";

OMXVideoDecoderMJPEG::OMXVideoDecoderMJPEG() {
    omx_verboseLog("OMXVideoDecoderMJPEG is constructed.");
    mVideoDecoder = createVideoDecoder(MJPEG_MIME_TYPE);
    if (!mVideoDecoder) {
        omx_errorLog("createVideoDecoder failed for \"%s\"", MJPEG_MIME_TYPE);
    }
    BuildHandlerList();
}

OMXVideoDecoderMJPEG::~OMXVideoDecoderMJPEG() {
    omx_verboseLog("OMXVideoDecoderMJPEG is destructed.");
}

OMX_ERRORTYPE OMXVideoDecoderMJPEG::InitInputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionInput) {
    paramPortDefinitionInput->nBufferCountActual = INPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionInput->nBufferCountMin = INPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionInput->nBufferSize = INPORT_BUFFER_SIZE;
    paramPortDefinitionInput->format.video.cMIMEType = (OMX_STRING)MJPEG_MIME_TYPE;
    paramPortDefinitionInput->format.video.eCompressionFormat = (OMX_VIDEO_CODINGTYPE) OMX_VIDEO_CodingMJPEG;

    memset(&mParamMjpeg, 0, sizeof(mParamMjpeg));
    SetTypeHeader(&mParamMjpeg, sizeof(mParamMjpeg));
    mParamMjpeg.nPortIndex = INPORT_INDEX;
    mParamMjpeg.eCompressionFormat = OMX_VIDEO_CodingMJPEG;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderMJPEG::PrepareConfigBuffer(VideoConfigBuffer *p) {
    OMX_ERRORTYPE ret;
    ret = OMXVideoDecoderBase::PrepareConfigBuffer(p);
    CHECK_RETURN_VALUE("OMXVideoDecoderBase::PrepareConfigBuffer");

    p->profile      = VAProfileJPEGBaseline;
    p->flag         = 0; // USE_NATIVE_GRAPHIC_BUFFER;
    p->surfaceNumber= 0;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderMJPEG::BuildHandlerList(void) {
    OMXVideoDecoderBase::BuildHandlerList();
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamVideoPortFormat, GetParamVideoMjpeg, SetParamVideoMjpeg);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderMJPEG::GetParamVideoMjpeg(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_PORTFORMATTYPE *p = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, INPORT_INDEX);

    memcpy(p, &mParamMjpeg, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderMJPEG::SetParamVideoMjpeg(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_PORTFORMATTYPE *p = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, INPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    // TODO: do we need to check if port is enabled?
    memcpy(&mParamMjpeg, p, sizeof(mParamMjpeg));
    return OMX_ErrorNone;
}

DECLARE_OMX_COMPONENT("OMX.Intel.VideoDecoder.MJPEG", "video_decoder.mjpeg", OMXVideoDecoderMJPEG);

