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


#include "OMXVideoDecoderAVC.h"

// Be sure to have an equal string in VideoDecoderHost.cpp (libmix)
static const char* AVC_MIME_TYPE = "video/h264";
#define INVALID_PTS (OMX_S64)-1


OMXVideoDecoderAVC::OMXVideoDecoderAVC()
{
    omx_verboseLog("OMXVideoDecoderAVC is constructed.");
    mVideoDecoder = createVideoDecoder(AVC_MIME_TYPE);
    if (!mVideoDecoder) {
        omx_errorLog("createVideoDecoder failed for \"%s\"", AVC_MIME_TYPE);
    }
    BuildHandlerList();
}

OMXVideoDecoderAVC::~OMXVideoDecoderAVC() {
    omx_verboseLog("OMXVideoDecoderAVC is destructed.");
}

OMX_ERRORTYPE OMXVideoDecoderAVC::InitInputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionInput) {
    //OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS
    memset(&mDecodeSettings, 0, sizeof(mDecodeSettings));
    SetTypeHeader(&mDecodeSettings, sizeof(mDecodeSettings));
    mDecodeSettings.nMaxNumberOfReferenceFrame = NUM_REFERENCE_FRAME;

    // OMX_PARAM_PORTDEFINITIONTYPE
    paramPortDefinitionInput->nBufferCountActual = INPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionInput->nBufferCountMin = INPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionInput->nBufferSize = INPORT_BUFFER_SIZE;
    paramPortDefinitionInput->format.video.cMIMEType = (OMX_STRING)AVC_MIME_TYPE;
    paramPortDefinitionInput->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;

    // OMX_VIDEO_PARAM_AVCTYPE
    memset(&mParamAvc, 0, sizeof(mParamAvc));
    SetTypeHeader(&mParamAvc, sizeof(mParamAvc));
    mParamAvc.nPortIndex = INPORT_INDEX;
    // TODO: check eProfile/eLevel
    mParamAvc.eProfile = OMX_VIDEO_AVCProfileHigh; //OMX_VIDEO_AVCProfileBaseline;
    mParamAvc.eLevel = OMX_VIDEO_AVCLevel41; //OMX_VIDEO_AVCLevel1;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderAVC::ProcessorInit(void *parser_handle) {
    return OMXVideoDecoderBase::ProcessorInit(parser_handle);
}

OMX_ERRORTYPE OMXVideoDecoderAVC::ProcessorDeinit(void) {
    return OMXVideoDecoderBase::ProcessorDeinit();
}

OMX_ERRORTYPE OMXVideoDecoderAVC::ProcessorFlush(OMX_U32 portIndex) {
    return OMXVideoDecoderBase::ProcessorFlush(portIndex);
}

OMX_ERRORTYPE OMXVideoDecoderAVC::ProcessorProcess(
        OMX_BUFFERHEADERTYPE **buffers,
        buffer_retain_t *retains,
        OMX_U32 numberBuffers) {

    return OMXVideoDecoderBase::ProcessorProcess(buffers, retains, numberBuffers);
}

OMX_ERRORTYPE OMXVideoDecoderAVC::PrepareConfigBuffer(VideoConfigBuffer *p) {
    OMX_ERRORTYPE ret;
    ret = OMXVideoDecoderBase::PrepareConfigBuffer(p);
    CHECK_RETURN_VALUE("OMXVideoDecoderBase::PrepareConfigBuffer");

    omx_warnLog("AVC Video decoder used in Video Conferencing Mode.");

    // For video conferencing application
    p->width = mDecodeSettings.nMaxWidth;
    p->height = mDecodeSettings.nMaxHeight;
    p->profile = VAProfileH264High;
    p->surfaceNumber =  0; //mDecodeSettings.nMaxNumberOfReferenceFrame + EXTRA_REFERENCE_FRAME;
    p->flag = USE_NATIVE_GRAPHIC_BUFFER;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderAVC::PrepareDecodeBuffer(OMX_BUFFERHEADERTYPE *buffer, buffer_retain_t *retain, VideoDecodeBuffer *p) {
    return OMXVideoDecoderBase::PrepareDecodeBuffer(buffer, retain, p);
    /*
        it is problemtic and usually unnecessary to AccumulateBuffer data for a complete frame.
        1. usually media frame work send complete frame here (either gst or chromeos).
            even if it is not complete frame, we can depend on codec to connect/split buffers
        2. it is problemtic if we simple accumalate buffer here: when buffer is required to
            be retained by OMXVideoDecoderBase, it becomes inconsistent.
        3. it is usually useless to accumulate buffer only, without split it o frame boundary.
        4. it is not reliable to detect a new frame by time stamp difference
    */
}

OMX_ERRORTYPE OMXVideoDecoderAVC::BuildHandlerList(void) {
    OMXVideoDecoderBase::BuildHandlerList();
    AddHandler(OMX_IndexParamVideoAvc, GetParamVideoAvc, SetParamVideoAvc);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamIntelAVCDecodeSettings, GetParamIntelAVCDecodeSettings, SetParamIntelAVCDecodeSettings);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderAVC::GetParamVideoAvc(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_AVCTYPE *p = (OMX_VIDEO_PARAM_AVCTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, INPORT_INDEX);

    memcpy(p, &mParamAvc, sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderAVC::SetParamVideoAvc(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_AVCTYPE *p = (OMX_VIDEO_PARAM_AVCTYPE *)pStructure;
    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, INPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    // TODO: do we need to check if port is enabled?
    // TODO: see SetPortAvcParam implementation - Can we make simple copy????
    memcpy(&mParamAvc, p, sizeof(mParamAvc));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderAVC::GetParamIntelAVCDecodeSettings(OMX_PTR pStructure) {
    return OMX_ErrorNotImplemented;
}

OMX_ERRORTYPE OMXVideoDecoderAVC::SetParamIntelAVCDecodeSettings(OMX_PTR pStructure) {
    omx_warnLog("SetParamIntelAVCDecodeSettings");

    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS *p = (OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, INPORT_INDEX);
    CHECK_SET_PARAM_STATE();

    if(p->nMaxNumberOfReferenceFrame == 0) {
        // TODO: check if we just return in this case.
        p->nMaxNumberOfReferenceFrame = NUM_REFERENCE_FRAME;
    }
    omx_infoLog("Maximum width = %lu, height = %lu, dpb = %lu", p->nMaxWidth, p->nMaxHeight, p->nMaxNumberOfReferenceFrame);
    mDecodeSettings = *p;

    return OMX_ErrorNone;
}


DECLARE_OMX_COMPONENT("OMX.Intel.VideoDecoder.AVC", "video_decoder.avc", OMXVideoDecoderAVC);
