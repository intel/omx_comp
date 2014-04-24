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

#include "OMXVideoDecoderBase.h"
#include "vabuffer.h"
#ifdef ANDROID
#include <system/graphics.h>
#include <hardware/gralloc.h>
#endif

static const char* VA_RAW_MIME_TYPE = "video/raw";

OMXVideoDecoderBase::OMXVideoDecoderBase()
    : mVideoDecoder(NULL),
      bNativeBufferEnable(false),
      mGlxPictures(NULL),
      mIsThumbNail(false) {
}

OMXVideoDecoderBase::~OMXVideoDecoderBase() {
    releaseVideoDecoder(mVideoDecoder);

    if (this->ports) {
        if (this->ports[INPORT_INDEX]) {
            delete this->ports[INPORT_INDEX];
            this->ports[INPORT_INDEX] = NULL;
        }

        if (this->ports[OUTPORT_INDEX]) {
            delete this->ports[OUTPORT_INDEX];
            this->ports[OUTPORT_INDEX] = NULL;
        }
    }
}

OMX_ERRORTYPE OMXVideoDecoderBase::InitInputPort(void) {
    this->ports[INPORT_INDEX] = new PortVideo;
    if (this->ports[INPORT_INDEX] == NULL) {
        return OMX_ErrorInsufficientResources;
    }

    PortVideo *port = static_cast<PortVideo *>(this->ports[INPORT_INDEX]);

    // OMX_PARAM_PORTDEFINITIONTYPE
    OMX_PARAM_PORTDEFINITIONTYPE paramPortDefinitionInput;
    memset(&paramPortDefinitionInput, 0, sizeof(paramPortDefinitionInput));
    SetTypeHeader(&paramPortDefinitionInput, sizeof(paramPortDefinitionInput));
    paramPortDefinitionInput.nPortIndex = INPORT_INDEX;
    paramPortDefinitionInput.eDir = OMX_DirInput;
    paramPortDefinitionInput.nBufferCountActual = INPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionInput.nBufferCountMin = INPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionInput.nBufferSize = INPORT_BUFFER_SIZE;
    paramPortDefinitionInput.bEnabled = OMX_TRUE;
    paramPortDefinitionInput.bPopulated = OMX_FALSE;
    paramPortDefinitionInput.eDomain = OMX_PortDomainVideo;
    paramPortDefinitionInput.format.video.cMIMEType = NULL; // to be overridden
    paramPortDefinitionInput.format.video.pNativeRender = NULL;
    paramPortDefinitionInput.format.video.nFrameWidth = 176;
    paramPortDefinitionInput.format.video.nFrameHeight = 144;
    paramPortDefinitionInput.format.video.nStride = 0;
    paramPortDefinitionInput.format.video.nSliceHeight = 0;
    paramPortDefinitionInput.format.video.nBitrate = 64000;
    paramPortDefinitionInput.format.video.xFramerate = 15 << 16;
    // TODO: check if we need to set bFlagErrorConcealment  to OMX_TRUE
    paramPortDefinitionInput.format.video.bFlagErrorConcealment = OMX_FALSE;
    paramPortDefinitionInput.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused; // to be overridden
    paramPortDefinitionInput.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    paramPortDefinitionInput.format.video.pNativeWindow = NULL;
    paramPortDefinitionInput.bBuffersContiguous = OMX_FALSE;
    paramPortDefinitionInput.nBufferAlignment = 0;

    // Derived class must implement this interface and override any field if needed.
    // eCompressionFormat and and cMIMEType must be overridden
    InitInputPortFormatSpecific(&paramPortDefinitionInput);

    port->SetPortDefinition(&paramPortDefinitionInput, true);

    // OMX_VIDEO_PARAM_PORTFORMATTYPE
    OMX_VIDEO_PARAM_PORTFORMATTYPE paramPortFormat;
    memset(&paramPortFormat, 0, sizeof(paramPortFormat));
    SetTypeHeader(&paramPortFormat, sizeof(paramPortFormat));
    paramPortFormat.nPortIndex = INPORT_INDEX;
    paramPortFormat.nIndex = 0;
    paramPortFormat.eCompressionFormat = paramPortDefinitionInput.format.video.eCompressionFormat;
    paramPortFormat.eColorFormat = paramPortDefinitionInput.format.video.eColorFormat;
    paramPortFormat.xFramerate = paramPortDefinitionInput.format.video.xFramerate;

    port->SetPortVideoParam(&paramPortFormat, true);

    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXVideoDecoderBase::InitOutputPort(void) {
    this->ports[OUTPORT_INDEX] = new PortVideo;
    if (this->ports[OUTPORT_INDEX] == NULL) {
        return OMX_ErrorInsufficientResources;
    }

    PortVideo *port = static_cast<PortVideo *>(this->ports[OUTPORT_INDEX]);

    // OMX_PARAM_PORTDEFINITIONTYPE
    OMX_PARAM_PORTDEFINITIONTYPE paramPortDefinitionOutput;

    memset(&paramPortDefinitionOutput, 0, sizeof(paramPortDefinitionOutput));
    SetTypeHeader(&paramPortDefinitionOutput, sizeof(paramPortDefinitionOutput));

    paramPortDefinitionOutput.nPortIndex = OUTPORT_INDEX;
    paramPortDefinitionOutput.eDir = OMX_DirOutput;
    paramPortDefinitionOutput.nBufferCountActual = OUTPORT_ACTUAL_BUFFER_COUNT;
    paramPortDefinitionOutput.nBufferCountMin = OUTPORT_MIN_BUFFER_COUNT;
    paramPortDefinitionOutput.nBufferSize = OUTPORT_BUFFER_SIZE; // bNativeBufferEnable is false by default

    paramPortDefinitionOutput.bEnabled = OMX_TRUE;
    paramPortDefinitionOutput.bPopulated = OMX_FALSE;
    paramPortDefinitionOutput.eDomain = OMX_PortDomainVideo;
    paramPortDefinitionOutput.format.video.cMIMEType = (OMX_STRING)VA_RAW_MIME_TYPE;
    paramPortDefinitionOutput.format.video.pNativeRender = NULL;
    paramPortDefinitionOutput.format.video.nFrameWidth = 176;
    paramPortDefinitionOutput.format.video.nFrameHeight = 144;
    paramPortDefinitionOutput.format.video.nStride = 176;
    paramPortDefinitionOutput.format.video.nSliceHeight = 144;
    paramPortDefinitionOutput.format.video.nBitrate = 64000;
    paramPortDefinitionOutput.format.video.xFramerate = 15 << 16;
    paramPortDefinitionOutput.format.video.bFlagErrorConcealment = OMX_FALSE;
    paramPortDefinitionOutput.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    paramPortDefinitionOutput.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420SemiPlanar;
    paramPortDefinitionOutput.format.video.pNativeWindow = NULL;
    paramPortDefinitionOutput.bBuffersContiguous = OMX_FALSE;
    paramPortDefinitionOutput.nBufferAlignment = 0;

    // no format specific to initialize output port
    InitOutputPortFormatSpecific(&paramPortDefinitionOutput);

    port->SetPortDefinition(&paramPortDefinitionOutput, true);

    // OMX_VIDEO_PARAM_PORTFORMATTYPE
    OMX_VIDEO_PARAM_PORTFORMATTYPE paramPortFormat;
    SetTypeHeader(&paramPortFormat, sizeof(paramPortFormat));
    paramPortFormat.nPortIndex = OUTPORT_INDEX;
    paramPortFormat.nIndex = 0;
    paramPortFormat.eCompressionFormat = paramPortDefinitionOutput.format.video.eCompressionFormat;
    paramPortFormat.eColorFormat = paramPortDefinitionOutput.format.video.eColorFormat;
    paramPortFormat.xFramerate = paramPortDefinitionOutput.format.video.xFramerate;

    port->SetPortVideoParam(&paramPortFormat, true);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::InitOutputPortFormatSpecific(OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionOutput) {
    // no format specific to initialize output port
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorInit(void * parser_handle) {
    OMX_ERRORTYPE ret;
    ret = OMXComponentCodecBase::ProcessorInit(parser_handle);
    CHECK_RETURN_VALUE("OMXComponentCodecBase::ProcessorInit");

    if (mVideoDecoder == NULL) {
        omx_errorLog("ProcessorInit: Video decoder is not created.");
        return OMX_ErrorDynamicResourcesUnavailable;
    }

    VideoConfigBuffer configBuffer;
    ret = PrepareConfigBuffer(&mVideoConfigBuffer);
    CHECK_RETURN_VALUE("PrepareConfigBuffer");
    mVideoConfigBuffer.parser_handle=parser_handle;

    mVideoDecoder->setXDisplay((Display *)mDisplayXPtr);

    //pthread_mutex_lock(&mSerializationLock);
    Decode_Status status = mVideoDecoder->start(&mVideoConfigBuffer);
    //pthread_mutex_unlock(&mSerializationLock);

    if (status == DECODE_FORMAT_CHANGE) {
        HandleFormatChange();
    }
    else if (status != DECODE_SUCCESS) {
        return TranslateDecodeStatus(status);
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorDeinit(void) {
    if (mVideoDecoder == NULL) {
        omx_errorLog("ProcessorDeinit: Video decoder is not created.");
        return OMX_ErrorDynamicResourcesUnavailable;
    }
    mIsThumbNail = false;

    //pthread_mutex_lock(&mSerializationLock);
    mVideoDecoder->stop();
    //pthread_mutex_unlock(&mSerializationLock);

    return OMXComponentCodecBase::ProcessorDeinit();
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorStop(void) {
    // There is no need to return all retained buffers as we don't accumulate buffer
    //this->ports[INPORT_INDEX]->ReturnAllRetainedBuffers();

    // TODO: this is new code
    ProcessorFlush(OMX_ALL);

    return OMXComponentCodecBase::ProcessorStop();
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorFlush(OMX_U32 portIndex) {
    omx_infoLog("Flushing port# %p.", portIndex);
    mIsThumbNail = false;
    if (mVideoDecoder == NULL) {
        omx_errorLog("ProcessorFlush: Video decoder is not created.");
        return OMX_ErrorDynamicResourcesUnavailable;
    }

    // Portbase has returned all retained buffers.
    if (portIndex == INPORT_INDEX || portIndex == OMX_ALL) {
        //pthread_mutex_lock(&mSerializationLock);
        mVideoDecoder->flush();
        //pthread_mutex_unlock(&mSerializationLock);
    } else {
	mVideoDecoder->flushOutport();
    }
    // TODO: do we need to flush output port?
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retains,
    OMX_U32 numberBuffers) {

    OMX_ERRORTYPE ret;
    Decode_Status status;

    // fill render buffer without draining decoder output queue
    omx_verboseLog("calling FillRenderBuffer() 1st time sending=%p", buffers[OUTPORT_INDEX]);
    ret = FillRenderBuffer(&buffers[OUTPORT_INDEX], 0);
    if (ret == OMX_ErrorNone) {
        retains[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
        omx_verboseLog("FillRenderBuffer() 1st time got=%p, returning OUTPORT_INDEX.", buffers[OUTPORT_INDEX]);
        // TODO: continue decoding
        return ret;
    } else if (ret != OMX_ErrorNotReady) {
        return ret;
    }

    VideoDecodeBuffer decodeBuffer;
    // PrepareDecodeBuffer will set retain to either BUFFER_RETAIN_GETAGAIN or BUFFER_RETAIN_NOT_RETAIN
    ret = PrepareDecodeBuffer(buffers[INPORT_INDEX], &retains[INPORT_INDEX], &decodeBuffer);
    if (ret == OMX_ErrorNotReady) {
        retains[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
        return OMX_ErrorNone;
    } else if (ret != OMX_ErrorNone) {
        return ret;
    }

    omx_verboseLog("PrepareDecodeBuffer() returns 0x%x, decodeBuffer.size = %d, .timestamp = %ld",
        ret, decodeBuffer.size, decodeBuffer.timeStamp);

    if (decodeBuffer.size != 0) {
        //pthread_mutex_lock(&mSerializationLock);
        status = mVideoDecoder->decode(&decodeBuffer);
        //pthread_mutex_unlock(&mSerializationLock);

        if (status == DECODE_FORMAT_CHANGE) {

            const OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionInput =
                  this->ports[INPORT_INDEX]->GetPortDefinition();

            if (paramPortDefinitionInput->format.video.eCompressionFormat != OMX_VIDEO_CodingVP8)
            {
                // in AVC case, flush all the buffers from libmix
                omx_errorLog("Get all the buffers from libmix");
                ret = FillRenderBuffer(&buffers[OUTPORT_INDEX], OMX_BUFFERFLAG_EOS);
                if (ret == OMX_ErrorNotReady) {
                    retains[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
                    omx_errorLog("FillRenderBuffer() not ready?");
                }
            }

            // Modify the port settings and send notification
            ret = HandleFormatChange();
            CHECK_RETURN_VALUE("HandleFormatChange");

            // Dont use the output buffer if format is changed.
            // This is temporary workaround for VP8 case. The flush above
            // would have made us lose the key frame in VP8 case. Need to handle
            // dynamic resolution change later
            buffers[OUTPORT_INDEX]->nFilledLen = 0;
            retains[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            retains[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;

            return OMX_ErrorNone;
        } else if (status == DECODE_NO_CONFIG) {
            omx_warnLog("Decoder returns DECODE_NO_CONFIG.");
            retains[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            return OMX_ErrorNone;
        } else if (status == DECODE_NO_REFERENCE) {
            omx_warnLog("Decoder returns DECODE_NO_REFERENCE.");
            retains[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            return OMX_ErrorNone;
	} else if (status == DECODE_NO_SURFACE) {
            omx_warnLog("Decoder returns DECODE_NO_SURFACE.");
            retains[OUTPORT_INDEX] = BUFFER_RETAIN_PUSHBACK;
            buffers[OUTPORT_INDEX]->nFilledLen = 0;
            return OMX_ErrorNone;
        } else if (status != DECODE_SUCCESS && status != DECODE_FRAME_DROPPED) {
            if (checkFatalDecoderError(status)) {
                return TranslateDecodeStatus(status);
            } else {
                // For decoder errors that could be omitted,  not throw error and continue to decode.
                TranslateDecodeStatus(status);

                buffers[OUTPORT_INDEX]->nFilledLen = 0;
                return OMX_ErrorNone;
            }
        }
    }
    // drain the decoder output queue when in EOS state and fill the render buffer
    ret = FillRenderBuffer(&buffers[OUTPORT_INDEX], buffers[INPORT_INDEX]->nFlags);
    if (ret == OMX_ErrorNotReady) {
        if (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS) {
            buffers[OUTPORT_INDEX]->nFlags = OMX_BUFFERFLAG_EOS;
            return OMX_ErrorNone;
        }
        retains[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
        ret = OMX_ErrorNone;
    } else {
       omx_verboseLog("FillRenderBuffer() 2nd time got=%p", buffers[OUTPORT_INDEX]);
        if (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS) {
            buffers[OUTPORT_INDEX]->nFlags = OMX_BUFFERFLAG_EOS;
            return OMX_ErrorNone;
        }
    }
    return ret;
}

OMX_ERRORTYPE OMXVideoDecoderBase::processOutPortBuffers(OMX_BUFFERHEADERTYPE *pOriginalOutBuffer, OMX_BUFFERHEADERTYPE *pNewOutBuffer) {
    OMX_ERRORTYPE ret= OMX_ErrorNone;
    VideoRenderBuffer *renderBuffer =
        (VideoRenderBuffer *)pOriginalOutBuffer->pPlatformPrivate;
    const OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionOutput =
        this->ports[OUTPORT_INDEX]->GetPortDefinition();
    const OMX_VIDEO_PORTDEFINITIONTYPE video_format=paramPortDefinitionOutput->format.video;
    int *picture_id= (int *)pOriginalOutBuffer->pOutputPortPrivate;

    if ( pOriginalOutBuffer != pNewOutBuffer ) {
        // We are not returning the same buffer that we got. Remove this buffer from output port
        omx_verboseLog("%s:original=%p ret=%p", __FUNCTION__, pOriginalOutBuffer, pNewOutBuffer);
        ret = this->ports[OUTPORT_INDEX]->RemoveThisBuffer(pNewOutBuffer);
        if ( OMX_ErrorNone != ret ) {
            omx_errorLog("failed in RemoveThisBuffer= %p", pNewOutBuffer);
            return ret;
        }
        // Put back the original buffer back to the head
        ret = this->ports[OUTPORT_INDEX]->RetainThisBuffer(pOriginalOutBuffer, false);
        if ( OMX_ErrorNone != ret ) {
            omx_errorLog("failed in PushThisBuffer = %p", pOriginalOutBuffer);
            return ret;
        }
    }
    else {
        // Ready to sync and put surfaces if GlxPictures exist
        if(mGlxPictures) {
            omx_verboseLog("va display: %p, va surface: 0x%x, pixmap id: 0x%x, src/dst rect %d x %d",
            renderBuffer->display, renderBuffer->surface, mGlxPictures[*picture_id],
            video_format.nFrameWidth,video_format.nFrameHeight);

            vaSyncSurface(renderBuffer->display,renderBuffer->surface);
            vaPutSurface(renderBuffer->display, renderBuffer->surface,
                    mGlxPictures[*picture_id], 0,0,
                    video_format.nFrameWidth,video_format.nFrameHeight,
                    0,0,video_format.nFrameWidth,video_format.nFrameHeight,
                    NULL,0,0);
        }
    }
    mVideoDecoder->renderDone(renderBuffer);
    pOriginalOutBuffer->pPlatformPrivate = NULL; // surface has been returned back to codec

    return ret;
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorReleaseLock(void) {
	omx_verboseLog("%s entered", __FUNCTION__);
	mVideoDecoder->releaseLock();
	return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorEnableNativeBuffers(void) {
    omx_verboseLog("%s entered VideoConfigBuf %p", __FUNCTION__, &mVideoConfigBuffer);
    mVideoDecoder->enableNativeBuffers();
    bNativeBufferEnable = true;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorPreFillBuffer(OMX_BUFFERHEADERTYPE* pBuffer) {
    OMX_ERRORTYPE ret= OMX_ErrorNone;
#if 0
    if (pBuffer->pPlatformPrivate) {
        /* it is incorrect to call renderDone() in PreFillBuffer.
            a) rendering hasn't been done yet
            b) two threads may come here twice:
                ComponentBase::Work->OMXVideoDecoderBase::getDecodedBuffer and ComponentBase::CBaseFillThisBuffer.
                Calling renderDone() twice will confuse codec; since codec usually recycle surface buffer upon this call.
         */
        omx_verboseLog("%s omxBufferHeader = %p, handle = %p, [%p]->renderDone=true;", __FUNCTION__, pBuffer, pBuffer->pBuffer, pBuffer->pPlatformPrivate);
        mVideoDecoder->renderDone((VideoRenderBuffer*)pBuffer->pPlatformPrivate);
       // pBuffer->pPlatformPrivate = NULL;
        ret = OMX_ErrorNone;
    } else if (bNativeBufferEnable == true) {
        /* nothing is required on chromeos when bNativeBufferEnable is set.
           1) I think bNativeBufferEnable is misused on chromeos. it is used to distinguish between
            'proprietary communication(vaSurface)' and 'non-tunneled communication(MapRawNV12)'.
            in either case, nothing is required on chromeos.
           2) on Android, I guess bNativeBufferEnable means option between external/gralloc and
            native/psb surface memory. so, flagNativeBuffer give driver an chance to make some preparation.
        */
        omx_verboseLog("%s calling flagNativeBuffer()", __FUNCTION__);
        if ((mVideoDecoder->flagNativeBuffer((void *)pBuffer)) != DECODE_SUCCESS)
         ret = OMX_ErrorBadParameter;
    }
#endif
    return ret;
}

OMX_ERRORTYPE OMXVideoDecoderBase::PrepareConfigBuffer(VideoConfigBuffer *p) {
    // default config buffer preparation
    memset(p, 0, sizeof(VideoConfigBuffer));

    const OMX_PARAM_PORTDEFINITIONTYPE *paramPortDefinitionInput = this->ports[INPORT_INDEX]->GetPortDefinition();
    if (paramPortDefinitionInput == NULL) {
        return OMX_ErrorBadParameter;
    }

    p->width = paramPortDefinitionInput->format.video.nFrameWidth;
    p->height = paramPortDefinitionInput->format.video.nFrameHeight;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::PrepareDecodeBuffer(OMX_BUFFERHEADERTYPE *buffer, buffer_retain_t *retain, VideoDecodeBuffer *p) {
    // default decode buffer preparation
    memset(p, 0, sizeof(VideoDecodeBuffer));
    if (buffer->nFilledLen == 0) {
        omx_warnLog("Len of filled data to decode is 0.");
        return OMX_ErrorNone; //OMX_ErrorBadParameter;
    }

    if (buffer->pBuffer == NULL) {
        omx_errorLog("Buffer to decode is empty.");
        return OMX_ErrorBadParameter;
    }

    if (buffer->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
        omx_infoLog("Buffer has OMX_BUFFERFLAG_CODECCONFIG flag.");
    }

    if (buffer->nFlags & OMX_BUFFERFLAG_DECODEONLY) {
        // TODO: Handle OMX_BUFFERFLAG_DECODEONLY : drop the decoded frame without rendering it.
        omx_warnLog("Buffer has OMX_BUFFERFLAG_DECODEONLY flag.");
    }

    p->data = buffer->pBuffer + buffer->nOffset;
    p->size = buffer->nFilledLen;
    p->timeStamp = buffer->nTimeStamp;
    if (buffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) {
        // TODO: OMX_BUFFERFLAG_ENDOFFRAME can be used to indicate end of a NAL unit.
        // setting this flag may cause corruption if buffer does not contain end-of-frame data.
        p->flag = HAS_COMPLETE_FRAME;
    }

    *retain= BUFFER_RETAIN_NOT_RETAIN;
    return OMX_ErrorNone;
}

OMX_BUFFERHEADERTYPE* OMXVideoDecoderBase::getDecodedBuffer( OMX_BUFFERHEADERTYPE *pBuffer, bool draining) {
    omx_verboseLog("%s entered", __FUNCTION__);
    const VideoRenderBuffer *renderBuffer = mVideoDecoder->getOutput(draining);
    if (renderBuffer == NULL) {
        return NULL;
    }

    omx_verboseLog("getOutput returned %p",renderBuffer->surface);
    if(bNativeBufferEnable == true) {
       // decode to Graphic Buffers
        omx_verboseLog("%s, buffer->pPlatformPrivate = %p pBuff=%p omxbuf=%p %s",
              __FUNCTION__, renderBuffer, pBuffer, renderBuffer->nwOMXBufHeader,
              pBuffer==renderBuffer->nwOMXBufHeader?"Same": "Differ");
        //pBuffer = (OMX_BUFFERHEADERTYPE*) renderBuffer->nwOMXBufHeader;
        pBuffer->nFilledLen = sizeof(VideoRenderBuffer);
    } else {
        // memcpy to thumbNail Buffer.
        omx_verboseLog("%s, thumbNail Enabled , pBuffer = %p, pBuffer->pBuffer = %p, renderBuffer = %p",
             __FUNCTION__, pBuffer, pBuffer->pBuffer, renderBuffer);
        MapRawNV12(renderBuffer, pBuffer->pBuffer + pBuffer->nOffset, pBuffer->nAllocLen - pBuffer->nOffset, pBuffer->nFilledLen);
    }
    pBuffer->nFlags = OMX_BUFFERFLAG_ENDOFFRAME;
    pBuffer->nTimeStamp = renderBuffer->timeStamp;

    //pPlatformPrivate used inside ProcessorPreFillBuffer to signal reuse by decoder.
    pBuffer->pPlatformPrivate = (void *)renderBuffer;
        ProcessorPreFillBuffer(pBuffer);
    return pBuffer;
}

OMX_ERRORTYPE OMXVideoDecoderBase::FillRenderBuffer(OMX_BUFFERHEADERTYPE **ppBuffer, OMX_U32 inportBufferFlags) {
    OMX_BUFFERHEADERTYPE *pBufReturn, *pBufReceived;
    bool draining = (inportBufferFlags & OMX_BUFFERFLAG_EOS);
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    omx_verboseLog("%s entered ppBuffer=%p", __FUNCTION__, *ppBuffer);
    pBufReceived = *ppBuffer;

    if ( false == draining) {
        // This is the normal operation
        pBufReturn = getDecodedBuffer(pBufReceived, draining);
        if ( NULL == pBufReturn ) {
            omx_verboseLog("Decoder not ready to return any Buffers");
            return OMX_ErrorNotReady;
        }
         ret = processOutPortBuffers(pBufReceived, pBufReturn);
    } else {
       // EOS. return all the output buffers we have with us
       omx_verboseLog("%s EOS received on buffer[OUTPORT]->nFlags ppBuffer=%p", __FUNCTION__, *ppBuffer);
        bool retainReceivedBuffer = true;
        pBufReceived->nFilledLen = 0;
        pBufReturn = getDecodedBuffer(pBufReceived, draining);
        if ( NULL == pBufReturn ) {
            omx_verboseLog("no Output Buffers to be removed/returned, EOS received.");
            // Special return value to support normal EOS and DynRes extra flush
            return OMX_ErrorNotReady;
        }
        while (pBufReturn != NULL) {
            OMX_BUFFERHEADERTYPE *pPendingBuffer;
            if(pBufReceived != pBufReturn) {
                omx_verboseLog("removing buffer %p ", pBufReturn);
            ret = this->ports[OUTPORT_INDEX]->RemoveThisBuffer(pBufReturn);
            if ( OMX_ErrorNone != ret ) {
            omx_errorLog("%s: removing buffer %p failed with error %p", __FUNCTION__, pBufReturn, ret);
                return ret;
            }
            } else {
               retainReceivedBuffer = false;
            }
            pPendingBuffer = getDecodedBuffer(pBufReceived, draining);
            omx_verboseLog("%s in EOS loop pBuffer=%p ppBuffer=%p pPendingBuffer=%p", __FUNCTION__, pBufReturn, *ppBuffer, pPendingBuffer);
            if ( NULL != pPendingBuffer ) {
                // We have hit the EOS. Give pBuffer
		// ToDo: Fix the return of libva processed buffers. The
		// renderBuffer keeps a reference of the used omx_buffer
		// within its structure but this cannot be maintained when
		// there is no 1:1 relation between omx_buffers and
		// va_surfaces.  ProcessorProcess is the unique method that
		// should return buffers and the buffer should be marked
		// properly.  In this case, the va_surfaces draining should
		// happen but only one omx buffer should be returned.

                //this->ports[OUTPORT_INDEX]->ReturnThisBuffer(pBufReturn);
            } else {
                // Return pBuffer it as part of ppBuffer
                break;
            }
            pBufReturn = pPendingBuffer;
        }
        if(retainReceivedBuffer) {
            ret = this->ports[OUTPORT_INDEX]->RetainThisBuffer(pBufReceived, true);
          if ( OMX_ErrorNone != ret ) {
        omx_errorLog("failed in RetainThisBuffer = %p", pBufReceived);
        return ret;
        }
        }
    }
    *ppBuffer = pBufReturn;
    omx_verboseLog("%s returning %p", __FUNCTION__, *ppBuffer);
    return ret;
}

OMX_ERRORTYPE OMXVideoDecoderBase::HandleFormatChange(void) {
    omx_warnLog("Video format is changed.");
    //pthread_mutex_lock(&mSerializationLock);
    const VideoFormatInfo *formatInfo = mVideoDecoder->getFormatInfo();
    //pthread_mutex_unlock(&mSerializationLock);

    // Sync port definition as it may change.
    OMX_PARAM_PORTDEFINITIONTYPE paramPortDefinitionInput, paramPortDefinitionOutput;

    memcpy(&paramPortDefinitionInput,
        this->ports[INPORT_INDEX]->GetPortDefinition(),
        sizeof(paramPortDefinitionInput));

    memcpy(&paramPortDefinitionOutput,
        this->ports[OUTPORT_INDEX]->GetPortDefinition(),
        sizeof(paramPortDefinitionOutput));

    int width = formatInfo->width;
    int height = formatInfo->height;
    int stride = formatInfo->width;
    int sliceHeight = formatInfo->height;

    int widthCropped = formatInfo->width - formatInfo->cropLeft - formatInfo->cropRight;
    int heightCropped = formatInfo->height - formatInfo->cropTop - formatInfo->cropBottom;
    int strideCropped = widthCropped;
    int sliceHeightCropped = heightCropped;

    omx_verboseLog("Original size = %lu x %lu, new size = %d x %d, cropped size = %d x %d",
        paramPortDefinitionInput.format.video.nFrameWidth,
        paramPortDefinitionInput.format.video.nFrameHeight,
        width, height, widthCropped, heightCropped);


    if (width == paramPortDefinitionInput.format.video.nFrameWidth &&
        height == paramPortDefinitionInput.format.video.nFrameHeight &&
        widthCropped == paramPortDefinitionOutput.format.video.nFrameWidth &&
        heightCropped == paramPortDefinitionOutput.format.video.nFrameHeight) {
        // reporting portsetting change may crash media player.
        omx_warnLog("Change of portsetting is not reported as size is not changed.");
        return OMX_ErrorNone;
    }

    paramPortDefinitionInput.format.video.nFrameWidth = width;
    paramPortDefinitionInput.format.video.nFrameHeight = height;
    paramPortDefinitionInput.format.video.nStride = stride;
    paramPortDefinitionInput.format.video.nSliceHeight = sliceHeight;

    paramPortDefinitionOutput.format.video.nFrameWidth = widthCropped;
    paramPortDefinitionOutput.format.video.nFrameHeight = heightCropped;
    paramPortDefinitionOutput.format.video.nStride = strideCropped;
    paramPortDefinitionOutput.format.video.nSliceHeight = sliceHeightCropped;
    //XXX MapRawNV12 hasn't support crop yet, do not use crop size here
    // nBufferSize is used by client to alloc output buffer (gst-omx for example)
    // chromium doesn't care of it, getDecodedBuffer uses sizeof(VideoRenderBuffer) when bNativeBufferEnable is true
    // even sizeof(VideoRenderBuffer) seems too big, since only one pointer is set to pBuffer->pPlatformPrivate
    int uv_width = (width+1)/2;
    int uv_height = (height+1)/2;
    if (!strcmp(formatInfo->mimeType, "image/jpeg")) {
        // XXX hard code here, we need extend VideoFormatInfo to indicate decoded surface format
        // few software supports 422H, convert it into I420 internally
        paramPortDefinitionOutput.format.video.eColorFormat = (OMX_COLOR_FORMATTYPE)OMX_COLOR_FormatYUV420Planar;
    }
    paramPortDefinitionOutput.nBufferSize = width * height + uv_width*uv_height*2;

    omx_verboseLog("  %s, bNativeBufferEnable: %d, nBufferSize: %d", __FILE__, bNativeBufferEnable, paramPortDefinitionOutput.nBufferSize);
    this->ports[INPORT_INDEX]->SetPortDefinition(&paramPortDefinitionInput, true);
    this->ports[OUTPORT_INDEX]->SetPortDefinition(&paramPortDefinitionOutput, true);

    omx_verboseLog("Call ReportPortSettingsChanged on output port");
    this->ports[OUTPORT_INDEX]->ReportPortSettingsChanged();
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::TranslateDecodeStatus(Decode_Status status) {
    switch (status) {
        case DECODE_NEED_RESTART:
            omx_errorLog("Decoder returned DECODE_NEED_RESTART");
            return (OMX_ERRORTYPE)OMX_ErrorIntelVideoNotPermitted;
        case DECODE_NO_CONFIG:
            omx_errorLog("Decoder returned DECODE_NO_CONFIG");
            return (OMX_ERRORTYPE)OMX_ErrorIntelMissingConfig;
        case DECODE_NO_SURFACE:
            omx_errorLog("Decoder returned DECODE_NO_SURFACE");
            return OMX_ErrorDynamicResourcesUnavailable;
        case DECODE_NO_REFERENCE:
            omx_errorLog("Decoder returned DECODE_NO_REFERENCE");
            return OMX_ErrorDynamicResourcesUnavailable; // TO DO
        case DECODE_NO_PARSER:
            omx_errorLog("Decoder returned DECODE_NO_PARSER");
            return OMX_ErrorDynamicResourcesUnavailable;
        case DECODE_INVALID_DATA:
            omx_errorLog("Decoder returned DECODE_INVALID_DATA");
            return OMX_ErrorBadParameter;
        case DECODE_DRIVER_FAIL:
            omx_errorLog("Decoder returned DECODE_DRIVER_FAIL");
            return OMX_ErrorHardware;
        case DECODE_PARSER_FAIL:
            omx_errorLog("Decoder returned DECODE_PARSER_FAIL");
            return (OMX_ERRORTYPE)OMX_ErrorIntelProcessStream; // OMX_ErrorStreamCorrupt
        case DECODE_MEMORY_FAIL:
            omx_errorLog("Decoder returned DECODE_MEMORY_FAIL");
            return OMX_ErrorInsufficientResources;
        case DECODE_FAIL:
            omx_errorLog("Decoder returned DECODE_FAIL");
            return OMX_ErrorUndefined;
        case DECODE_SUCCESS:
            return OMX_ErrorNone;
        case DECODE_FORMAT_CHANGE:
            omx_warnLog("Decoder returned DECODE_FORMAT_CHANGE");
            return OMX_ErrorNone;
        case DECODE_FRAME_DROPPED:
            omx_infoLog("Decoder returned DECODE_FRAME_DROPPED");
            return OMX_ErrorNone;
        default:
            omx_warnLog("Decoder returned unknown error");
            return OMX_ErrorUndefined;
    }
}

OMX_ERRORTYPE OMXVideoDecoderBase::BuildHandlerList(void) {
    OMXComponentCodecBase::BuildHandlerList();
    AddHandler(OMX_IndexParamVideoPortFormat, GetParamVideoPortFormat, SetParamVideoPortFormat);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamGoogleNativeBuffers, NULL,
                    SetParamVideoGoogleNativeBuffers);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamGoogleNativeBufferUsage,
                    GetParamVideoGoogleNativeBufferUsage, NULL);
    //AddHandler(PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX, GetCapabilityFlags, SetCapabilityFlags);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamGoogleThumbNail,
                    NULL, SetConfigVideoThumbNail);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamIntelXDisplay,
                    NULL, SetConfigXDisplay);
    AddHandler((OMX_INDEXTYPE)OMX_IndexParamIntelGlxPictures,
                    NULL, SetConfigGlxPictures);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::SetParamVideoGoogleNativeBuffers(OMX_PTR pStructure) {
    omx_verboseLog("entered OMXVideoDecoderBase::SetEnableAndroidNativeBuffers(), mEnableNativeWindowSupport enabled.");
    OMX_ERRORTYPE ret= OMX_ErrorNone;
    OMX_GOOGLE_ENABLE_ANDROID_BUFFERS *p =
            (OMX_GOOGLE_ENABLE_ANDROID_BUFFERS *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX_RANGE(p);
    CHECK_SET_PARAM_STATE();

    OMX_PARAM_PORTDEFINITIONTYPE paramPortDefinitionOutput;

    memcpy(&paramPortDefinitionOutput,
        this->ports[OUTPORT_INDEX]->GetPortDefinition(),
        sizeof(paramPortDefinitionOutput));

    bNativeBufferEnable = p->enable;

    //mVideoDecoder->enableNativeBuffers(paramPortDefinitionOutput.format.video.nFrameWidth,
    //    paramPortDefinitionOutput.format.video.nFrameHeight);
    return ret;

}

OMX_ERRORTYPE OMXVideoDecoderBase::GetParamVideoGoogleNativeBufferUsage(OMX_PTR pStructure) {
   omx_verboseLog("entered OMXVideoDecoderBase::GetParamVideoGoogleNativeBufferUsage.");
    OMX_ERRORTYPE ret= OMX_ErrorNone;
    OMX_GOOGLE_ANDROID_BUFFERS_USAGE *p =
            (OMX_GOOGLE_ANDROID_BUFFERS_USAGE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX_RANGE(p);

#ifdef ANDROID
    if (bNativeBufferEnable)
            p->nUsage = GRALLOC_USAGE_HW_RENDER;
#endif

    return ret;
}

OMX_ERRORTYPE OMXVideoDecoderBase::ProcessorUseNativeBuffer(OMX_U32 nPortIndex, OMX_BUFFERHEADERTYPE* pBufHeader) {
 if((nPortIndex  == OUTPORT_INDEX)) {
    omx_verboseLog("entered OMXVideoDecoderBase::ProcessorUseNativeBuffer() pBufHeader = %p, pBufHeader->pBuffer = %p", pBufHeader, pBufHeader->pBuffer);
    mVideoDecoder->getClientNativeWindowBuffer((void *)pBufHeader, (void*) pBufHeader->pBuffer);
    return OMX_ErrorNone;
 } else {
    return OMX_ErrorNone;
 }
}

OMX_ERRORTYPE OMXVideoDecoderBase::GetParamVideoPortFormat(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_PORTFORMATTYPE *p = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX_RANGE(p);
    CHECK_ENUMERATION_RANGE(p->nIndex, 1);

    PortVideo *port = NULL;
    port = static_cast<PortVideo *>(this->ports[p->nPortIndex]);
    memcpy(p, port->GetPortVideoParam(), sizeof(*p));
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::SetParamVideoPortFormat(OMX_PTR pStructure) {
    OMX_ERRORTYPE ret;
    OMX_VIDEO_PARAM_PORTFORMATTYPE *p = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX_RANGE(p);
    CHECK_SET_PARAM_STATE();

    // TODO: do we need to check if port is enabled?
    PortVideo *port = NULL;
    port = static_cast<PortVideo *>(this->ports[p->nPortIndex]);
    port->SetPortVideoParam(p, false);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::MapRawNV12(const VideoRenderBuffer* renderBuffer, OMX_U8 *rawData, OMX_U32 allocSize, OMX_U32& filledSize) {
    VAStatus vaStatus;
    VAImage vaImage;
    int32_t width = this->ports[OUTPORT_INDEX]->GetPortDefinition()->format.video.nFrameWidth;
    int32_t height = this->ports[OUTPORT_INDEX]->GetPortDefinition()->format.video.nFrameHeight;
    int32_t format = this->ports[OUTPORT_INDEX]->GetPortDefinition()->format.video.eColorFormat;
    int32_t uv_width = (width+1)/2;
    int32_t uv_height = (height+1)/2;

    filledSize = 0;

    vaStatus = vaSyncSurface(renderBuffer->display, renderBuffer->surface);
    if (vaStatus != VA_STATUS_SUCCESS) {
        return OMX_ErrorUndefined;
    }

    vaImage.image_id = VA_INVALID_ID;
    vaImage.buf = VA_INVALID_ID;
    vaStatus = vaDeriveImage(renderBuffer->display, renderBuffer->surface, &vaImage);
    if (vaStatus != VA_STATUS_SUCCESS) {
        vaDestroyImage(renderBuffer->display, vaImage.image_id);
        return OMX_ErrorUndefined;
    }

    void *pBuf = NULL;
    vaStatus = vaMapBuffer(renderBuffer->display, vaImage.buf, &pBuf);
    if (vaStatus != VA_STATUS_SUCCESS) {
        vaDestroyImage(renderBuffer->display, vaImage.image_id);
        return OMX_ErrorUndefined;
    }

    omx_verboseLog("output port format: 0x%x, va surface format: 0x%x",
        format, vaImage.format.fourcc);

    int32_t size = width * height + uv_width * uv_height * 2;
    if (width != vaImage.width || height != vaImage.height) {
        omx_errorLog("seems to be up layer bug,  vaImage(%dx%d) resolution is not match to dest(%dx%d)",
                     vaImage.width, vaImage.height, width, height);
        size = 0;
    }
    else if (size > allocSize) {
        omx_errorLog("seems to be up layer bug, no room for nv12 data copy, need %d, have %d", size, allocSize);
        size = 0;
    } else if (size == (int32_t)vaImage.data_size) {
        memcpy(rawData, pBuf, size);
    } else {
        uint8_t *src = (uint8_t*)pBuf;
        uint8_t *dst = rawData;
        int32_t row = 0;
        int32_t uvCopied = false;

        // copy Y data
        for (row = 0; row < height; row++) {
            memcpy(dst, src, width);
            dst += width;
            src += vaImage.pitches[0];
        }

        if (format == OMX_COLOR_FormatYUV420SemiPlanar) {
            // copy interleaved V and  U data
            if (vaImage.format.fourcc  == VA_FOURCC_NV12) {
                src = (uint8_t*)pBuf + vaImage.offsets[1];
                for (row = 0; row < uv_height; row++) {
                    memcpy(dst, src, uv_width*2);
                    dst += uv_width*2;
                    src += vaImage.pitches[1];
                }
                uvCopied = true;
            }
        } else if (format == OMX_COLOR_FormatYUV420Planar) {
            // copy U/V data
            if (vaImage.format.fourcc == VA_FOURCC_422H) {
                int32_t plane = 0;
                for (plane = 1; plane < 3; plane++) {
                    src = (uint8_t*)pBuf + vaImage.offsets[plane];
                    for (row = 0; row < uv_height; row++) {
                        memcpy(dst, src, uv_width);
                        dst += uv_width;
                        src += vaImage.pitches[plane]*2;
                    }
                }
                uvCopied = true;
            }
        }

        if (!uvCopied)
            omx_errorLog("programmer bug, uv data copy from 0x%x to 0x%x format hasn't supported yet",
                vaImage.format.fourcc, format);
    }

    filledSize = size;
    if (vaImage.buf != VA_INVALID_ID) {
        vaUnmapBuffer(renderBuffer->display, vaImage.buf);
    }

    if (vaImage.image_id != VA_INVALID_ID) {
        vaDestroyImage(renderBuffer->display, vaImage.image_id);
    }
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::GetCapabilityFlags(OMX_PTR pStructure) {
#if 0
    OMX_ERRORTYPE ret;
    PV_OMXComponentCapabilityFlagsType *p = (PV_OMXComponentCapabilityFlagsType *)pStructure;

    CHECK_TYPE_HEADER(p);
    CHECK_PORT_INDEX_RANGE(p);

    p->iIsOMXComponentMultiThreaded = OMX_TRUE;
    p->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
    p->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
    p->iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
    p->iOMXComponentSupportsPartialFrames = OMX_TRUE;
    p->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
    p->iOMXComponentUsesNALStartCodes = OMX_FALSE;
    p->iOMXComponentUsesFullAVCFrames = OMX_FALSE;
#endif
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::SetCapabilityFlags(OMX_PTR pStructure) {
    omx_errorLog("SetCapabilityFlags is not supported.");
    return OMX_ErrorUnsupportedSetting;
}

OMX_ERRORTYPE OMXVideoDecoderBase::SetConfigVideoThumbNail(OMX_PTR pStructure) {
   mIsThumbNail = true;
   omx_verboseLog("SetConfigVideoThumbNail() enabling mIsThumbNail to true.");
     return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::SetConfigXDisplay(OMX_PTR pStructure) {
    mDisplayXPtr = pStructure;
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXVideoDecoderBase::SetConfigGlxPictures(OMX_PTR pStructure) {
    mGlxPictures = (Pixmap *)pStructure;
    return OMX_ErrorNone;
}
