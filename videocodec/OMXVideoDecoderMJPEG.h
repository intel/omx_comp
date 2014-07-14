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



#ifndef OMX_VIDEO_DECODER_MJPEG_H_
#define OMX_VIDEO_DECODER_MJPEG_H_


#include "OMXVideoDecoderBase.h"
using namespace YamiMediaCodec;

class OMXVideoDecoderMJPEG : public OMXVideoDecoderBase {
public:
    OMXVideoDecoderMJPEG();
    virtual ~OMXVideoDecoderMJPEG();

protected:
   virtual OMX_ERRORTYPE InitInputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionInput);
   virtual OMX_ERRORTYPE PrepareConfigBuffer(VideoConfigBuffer *p);
   virtual OMX_ERRORTYPE BuildHandlerList(void);
   DECLARE_HANDLER(OMXVideoDecoderMJPEG, ParamVideoMjpeg);

private:
    enum {
        INPORT_MIN_BUFFER_COUNT = 1,
        INPORT_ACTUAL_BUFFER_COUNT = 5,
        INPORT_BUFFER_SIZE = 1382400,
    };

    OMX_VIDEO_PARAM_PORTFORMATTYPE mParamMjpeg;
};

#endif /* OMX_VIDEO_DECODER_MJPEG_H_ */
