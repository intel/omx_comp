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


#ifndef OMX_VIDEO_DECODER_BASE_H_
#define OMX_VIDEO_DECODER_BASE_H_


#include "OMXComponentCodecBase.h"
#include "VideoDecoderInterface.h"
#include "VideoDecoderHost.h"

class OMXVideoDecoderBase : public OMXComponentCodecBase {
public:
    OMXVideoDecoderBase();
    virtual ~OMXVideoDecoderBase();

protected:
    virtual OMX_ERRORTYPE InitInputPort(void);
    virtual OMX_ERRORTYPE InitOutputPort(void);
    virtual OMX_ERRORTYPE InitInputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *input) = 0;
    virtual OMX_ERRORTYPE InitOutputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *output);

    virtual OMX_ERRORTYPE ProcessorInit(void);
    virtual OMX_ERRORTYPE ProcessorDeinit(void);
    //virtual OMX_ERRORTYPE ProcessorStart(void);
    virtual OMX_ERRORTYPE ProcessorStop(void);
    //virtual OMX_ERRORTYPE ProcessorPause(void);
    //virtual OMX_ERRORTYPE ProcessorResume(void);
    virtual OMX_ERRORTYPE ProcessorFlush(OMX_U32 portIndex);
    virtual OMX_ERRORTYPE ProcessorProcess(
            OMX_BUFFERHEADERTYPE **buffers,
            buffer_retain_t *retains,
            OMX_U32 numberBuffers);

    virtual OMX_ERRORTYPE PrepareConfigBuffer(VideoConfigBuffer *p);
    virtual OMX_ERRORTYPE PrepareDecodeBuffer(OMX_BUFFERHEADERTYPE *buffer, buffer_retain_t *retain, VideoDecodeBuffer *p);
    virtual OMX_ERRORTYPE FillRenderBuffer(OMX_BUFFERHEADERTYPE *buffer, OMX_U32 inportBufferFlags);
    virtual OMX_ERRORTYPE HandleFormatChange(void);
    virtual OMX_ERRORTYPE TranslateDecodeStatus(Decode_Status status);

    virtual OMX_ERRORTYPE BuildHandlerList(void);
    DECLARE_HANDLER(OMXVideoDecoderBase, ParamVideoPortFormat);
    DECLARE_HANDLER(OMXVideoDecoderBase, CapabilityFlags);

private:
    enum {
        // OMX_PARAM_PORTDEFINITIONTYPE
        INPORT_MIN_BUFFER_COUNT = 1,
        INPORT_ACTUAL_BUFFER_COUNT = 256,
        INPORT_BUFFER_SIZE = 1382400,

        // OMX_PARAM_PORTDEFINITIONTYPE
        OUTPORT_MIN_BUFFER_COUNT = 1,
        OUTPORT_ACTUAL_BUFFER_COUNT = 2,
        OUTPORT_BUFFER_SIZE = 1382400,
    };

protected:
    IVideoDecoder *mVideoDecoder;
};

#endif /* OMX_VIDEO_DECODER_BASE_H_ */
