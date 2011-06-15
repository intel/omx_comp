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


#ifndef OMX_VIDEO_ENCODER_AVC_H_
#define OMX_VIDEO_ENCODER_AVC_H_


#include "OMXVideoEncoderBase.h"

class OMXVideoEncoderAVC : public OMXVideoEncoderBase {
public:
    OMXVideoEncoderAVC();
    virtual ~OMXVideoEncoderAVC();

protected:
    virtual OMX_ERRORTYPE InitOutputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionOutput);
    virtual OMX_ERRORTYPE ProcessorInit(void);
    virtual OMX_ERRORTYPE ProcessorDeinit(void);
    virtual OMX_ERRORTYPE ProcessorProcess(
            OMX_BUFFERHEADERTYPE **buffers,
            buffer_retain_t *retains,
            OMX_U32 numberBuffers);

   virtual OMX_ERRORTYPE BuildHandlerList(void);

   DECLARE_HANDLER(OMXVideoEncoderAVC, ParamVideoAvc);
   DECLARE_HANDLER(OMXVideoEncoderAVC, ParamNalStreamFormat);
   DECLARE_HANDLER(OMXVideoEncoderAVC, ParamNalStreamFormatSupported);
   DECLARE_HANDLER(OMXVideoEncoderAVC, ParamNalStreamFormatSelect);
   DECLARE_HANDLER(OMXVideoEncoderAVC, ConfigVideoAVCIntraPeriod);
   DECLARE_HANDLER(OMXVideoEncoderAVC, ConfigVideoNalSize);
   DECLARE_HANDLER(OMXVideoEncoderAVC, ParamIntelAVCVUI);
   DECLARE_HANDLER(OMXVideoEncoderAVC, ParamVideoBytestream);


private:
    enum {
        // OMX_PARAM_PORTDEFINITIONTYPE
        OUTPORT_MIN_BUFFER_COUNT = 1,
        OUTPORT_ACTUAL_BUFFER_COUNT = 10,
        OUTPORT_BUFFER_SIZE = 1382400,
        // for OMX_VIDEO_PARAM_INTEL_AVC_DECODE_SETTINGS
        NUM_REFERENCE_FRAME = 4,
    };

    OMX_VIDEO_PARAM_AVCTYPE mParamAvc;
    OMX_NALSTREAMFORMATTYPE mNalStreamFormat;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD mConfigAvcIntraPeriod;
    OMX_VIDEO_CONFIG_NALSIZE mConfigNalSize;
    //OMX_VIDEO_PARAM_INTEL_AVCVUI mParamIntelAvcVui;
};

#endif /* OMX_VIDEO_ENCODER_AVC_H_ */
