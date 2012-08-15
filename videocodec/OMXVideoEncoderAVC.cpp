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


#include "OMXVideoEncoderAVC.h"

static const char *AVC_MIME_TYPE = "video/h264";

OMXVideoEncoderAVC::OMXVideoEncoderAVC() {
    BuildHandlerList();
    mEncoderVideo =  createVideoEncoder(AVC_MIME_TYPE);
    if (!mEncoderVideo) omx_errorLog("OMX_ErrorInsufficientResources");

    mEncAVCParams = new VideoParamsAVC();
    if (!mEncAVCParams) omx_errorLog("OMX_ErrorInsufficientResources");
}

OMXVideoEncoderAVC::~OMXVideoEncoderAVC() {
    if(mEncAVCParams) {
        delete mEncAVCParams;
        mEncAVCParams = NULL;
    }
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
    mFirstFrame = OMX_TRUE;
    return  OMXVideoEncoderBase::ProcessorInit();
}

OMX_ERRORTYPE OMXVideoEncoderAVC::ProcessorDeinit(void) {
    return OMXVideoEncoderBase::ProcessorDeinit();
}


OMX_ERRORTYPE OMXVideoEncoderAVC::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retains,
    OMX_U32 numberBuffers) {

    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;
    OMX_U32 outflags = 0;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    Encode_Status ret = ENCODE_SUCCESS;

    VideoEncOutputBuffer outBuf;
    VideoEncRawBuffer inBuf;

    if(buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS)
        omx_verboseLog("%s(),%d: got OMX_BUFFERFLAG_EOS\n", __func__, __LINE__);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        omx_errorLog("%s(),%d: input buffer's nFilledLen is zero\n",  __func__, __LINE__);
        goto out;
    }

    inBuf.data = buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset;
    inBuf.size = buffers[INPORT_INDEX]->nFilledLen;

    omx_verboseLog("inBuf.data=%x, size=%d",(unsigned)inBuf.data, inBuf.size);

    outBuf.data = buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    outBuf.dataSize = 0;
    outBuf.bufferSize = buffers[OUTPORT_INDEX]->nAllocLen - buffers[OUTPORT_INDEX]->nOffset;

    if( inBuf.size<=0) {
        omx_errorLog("The Input buf size is 0\n");
        return OMX_ErrorBadParameter;
    }

    omx_verboseLog("in buffer = 0x%x ts = %lld",
         buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset,
         buffers[INPORT_INDEX]->nTimeStamp);

    if(mGetBufDone) {
        // encode and setConfig need to be thread safe
        pthread_mutex_lock(&mSerializationLock);
        ret = mEncoderVideo->encode(&inBuf);
        pthread_mutex_unlock(&mSerializationLock);

        CHECK_ENCODE_STATUS("encode");
        mGetBufDone = OMX_FALSE;

        // This is for buffer contention, we won't release current buffer
        // but the last input buffer
        ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
    }

    switch (mNalStreamFormat.eNaluFormat) {
        case OMX_NaluFormatStartCodes:

            outBuf.format = OUTPUT_EVERYTHING;
            ret = mEncoderVideo->getOutput(&outBuf);
            CHECK_ENCODE_STATUS("getOutput");

            omx_verboseLog("output data size = %d", outBuf.dataSize);
            outfilledlen = outBuf.dataSize;
            outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;


            if (outBuf.flag & ENCODE_BUFFERFLAG_SYNCFRAME) {
                outflags |= OMX_BUFFERFLAG_SYNCFRAME;
            }

            if(outBuf.flag & ENCODE_BUFFERFLAG_ENDOFFRAME) {
                outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
                mGetBufDone = OMX_TRUE;
                retains[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;

            } else {
                retains[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;  //get again

            }

            if (outfilledlen > 0) {
                retains[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
            } else {
                retains[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            }

            break;
        case OMX_NaluFormatOneNaluPerBuffer:

            outBuf.format = OUTPUT_ONE_NAL;
            ret = mEncoderVideo->getOutput(&outBuf);
            CHECK_ENCODE_STATUS("getOutput");
            // Return code could not be ENCODE_BUFFER_TOO_SMALL
            // If we don't return error, we will have dead lock issue
            if (ret == ENCODE_BUFFER_TOO_SMALL) {
                return OMX_ErrorUndefined;
            }

            omx_verboseLog("output codec data size = %d", outBuf.dataSize);

            outfilledlen = outBuf.dataSize;
            outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

            if (outBuf.flag & ENCODE_BUFFERFLAG_SYNCFRAME) {
                outflags |= OMX_BUFFERFLAG_SYNCFRAME;
            }

            if(outBuf.flag & ENCODE_BUFFERFLAG_ENDOFFRAME) {
                outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
                mGetBufDone = OMX_TRUE;
                retains[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;

            } else {
                retains[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;  //get again
            }

            if (outfilledlen > 0) {
                retains[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
            } else {
                retains[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            }

            break;
        case OMX_NaluFormatStartCodesSeparateFirstHeader:

            if(mFirstFrame) {
                omx_verboseLog("mFirstFrame\n");
                outBuf.format = OUTPUT_CODEC_DATA;
                ret = mEncoderVideo->getOutput(&outBuf);
                CHECK_ENCODE_STATUS("getOutput");

                // Return code could not be ENCODE_BUFFER_TOO_SMALL
                // If we don't return error, we will have dead lock issue
                if (ret == ENCODE_BUFFER_TOO_SMALL) {
                    return OMX_ErrorUndefined;
                }

                omx_verboseLog("output codec data size = %d", outBuf.dataSize);

                outflags |= OMX_BUFFERFLAG_CODECCONFIG;
                outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
                outflags |= OMX_BUFFERFLAG_SYNCFRAME;

                outfilledlen = outBuf.dataSize;
                mFirstFrame = OMX_FALSE;
                retains[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;  //get again
            } else {
                outBuf.format = OUTPUT_EVERYTHING;
                ret = mEncoderVideo->getOutput(&outBuf);
                CHECK_ENCODE_STATUS("getOutput");

                omx_verboseLog("output data size = %d", outBuf.dataSize);


                outfilledlen = outBuf.dataSize;
                outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

                if (outBuf.flag & ENCODE_BUFFERFLAG_SYNCFRAME) {
                    outflags |= OMX_BUFFERFLAG_SYNCFRAME;
                }

                if(outBuf.flag & ENCODE_BUFFERFLAG_ENDOFFRAME) {
                    omx_verboseLog("Get buffer done\n");
                    outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
                    mGetBufDone = OMX_TRUE;
                    retains[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;

                } else {
                    retains[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;  //get again

                }

            }


            break;
    }


out:
    omx_verboseLog("output buffers = %p:%d, flag = %x", buffers[OUTPORT_INDEX]->pBuffer, outfilledlen, outflags);

    if(retains[OUTPORT_INDEX] != BUFFER_RETAIN_GETAGAIN) {
        buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
        buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;
        buffers[OUTPORT_INDEX]->nFlags = outflags;
    }

    if (retains[INPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN ||
            retains[INPORT_INDEX] == BUFFER_RETAIN_ACCUMULATE ) {
        inFrameCnt ++;
    }

    if (retains[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN) outFrameCnt  ++;
#if 0
    if (avcEncParamIntelBitrateType.eControlRate != OMX_Video_Intel_ControlRateVideoConferencingMode) {
        if (oret == (OMX_ERRORTYPE) OMX_ErrorIntelExtSliceSizeOverflow) {
            oret = OMX_ErrorNone;
        }
    }
#endif
    if(oret == OMX_ErrorNone)
	omx_verboseLog("%s(),%d: exit, encode is done\n", __func__, __LINE__);

    return oret;

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

    mPFrames = p->nPFrames;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamNalStreamFormat(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_NALSTREAMFORMATTYPE *p = (OMX_NALSTREAMFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    // TODO: check if this is desired format
    p->eNaluFormat = mNalStreamFormat.eNaluFormat; //OMX_NaluFormatStartCodes;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetParamNalStreamFormat(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_NALSTREAMFORMATTYPE *p = (OMX_NALSTREAMFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);

    omx_verboseLog("p->eNaluFormat =%d\n",p->eNaluFormat);
    if(p->eNaluFormat != OMX_NaluFormatStartCodes &&
            p->eNaluFormat != OMX_NaluFormatStartCodesSeparateFirstHeader &&
            p->eNaluFormat != OMX_NaluFormatOneNaluPerBuffer) {
        omx_errorLog("Format not support\n");
        return OMX_ErrorUnsupportedSetting;
    }
    mNalStreamFormat.eNaluFormat = p->eNaluFormat;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamNalStreamFormatSupported(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_NALSTREAMFORMATTYPE *p = (OMX_NALSTREAMFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX(p, OUTPORT_INDEX);
    p->eNaluFormat = (OMX_NALUFORMATSTYPE)
                     (OMX_NaluFormatStartCodes |
                      OMX_NaluFormatStartCodesSeparateFirstHeader |
                      OMX_NaluFormatOneNaluPerBuffer);

    // TODO: check if this is desired format
    // OMX_NaluFormatFourByteInterleaveLength |
    //OMX_NaluFormatZeroByteInterleaveLength);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::SetParamNalStreamFormatSupported(OMX_PTR pStructure) {
    omx_warnLog("SetParamNalStreamFormatSupported is not supported.");
    return OMX_ErrorUnsupportedSetting;
}

OMX_ERRORTYPE OMXVideoEncoderAVC::GetParamNalStreamFormatSelect(OMX_PTR pStructure) {
    omx_warnLog("GetParamNalStreamFormatSelect is not supported.");
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
            p->eNaluFormat != OMX_NaluFormatStartCodesSeparateFirstHeader &&
            p->eNaluFormat != OMX_NaluFormatOneNaluPerBuffer) {
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
        omx_errorLog("SetConfigVideoNalSize failed. Feature is disabled.");
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
        omx_errorLog("SetConfigVideoNalSize failed. Feature is supported only in VCM.");
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
