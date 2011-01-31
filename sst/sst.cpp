/*
 * sst.cpp, omx sst component file
 *
 * Copyright (c) 2009-2010 Wind River Systems, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include <OMX_Core.h>

#include <audio_parser.h>

#include <cmodule.h>
#include <portaudio.h>
#include <componentbase.h>

//#include <pv_omxcore.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <mixaudio.h>
#include <mixacpaac.h>
#include <mixacpmp3.h>

#ifdef __cplusplus
} /* extern "C" */
#endif

#include "sst.h"

//#define LOG_NDEBUG 0

#define LOG_TAG "mrst_sst"
#include <log.h>

/*
 * TEST FEATURE
 *   - need to be removed
 */
#define TEST_USE_AAC_ENCODING_RAW 1 /* 1: RAW, 0: ATDS */
/* 1:variable, 0:44100,12800,2 */
#define TEST_USE_AAC_ENCODING_VARIABLE_PARAMETER 1

/*
 * only decode mode
 *   0 : Decode Return
 *   1 : Direct Render
 */
#define SST_RENDER_MODE_DIRECT_RENDER 1

/*
 * MixAudioStreamCtrl
 */

#define SST_USE_STREAM_CTRL_WORKQUEUE 1

static const char *mix_audio_command_string
[MixAudioStreamCtrl::MIX_STREAM_RESUME+2] = {
    "MIX_STREAM_START",
    "MIX_STREAM_STOP_DROP",
    "MIX_STREAM_STOP_DRAIN",
    "MIX_STREAM_PAUSE",
    "MIX_STREAM_RESUME",
    "MIX_STREAM_UNKNOWN",
};

inline static const char *
get_mix_audio_command_string(MixAudioStreamCtrl::mix_audio_command_t command)
{
    if (command > MixAudioStreamCtrl::MIX_STREAM_RESUME)
        command = (MixAudioStreamCtrl::mix_audio_command_t)
            (MixAudioStreamCtrl::MIX_STREAM_RESUME+1);

    return mix_audio_command_string[command];
}

MixAudioStreamCtrl::MixAudioStreamCtrl(MixAudio *mix)
{
    __queue_init(&q);
    pthread_mutex_init(&lock, NULL);

    this->mix = mix;

    StartWork(true);
}

MixAudioStreamCtrl::~MixAudioStreamCtrl()
{
    mix_audio_command_t *temp;

    CancelScheduledWork(this);
    StopWork();

    while ((temp = PopCommand()))
        free(temp);

    pthread_mutex_destroy(&lock);
}

void MixAudioStreamCtrl::SendCommand(mix_audio_command_t command)
{
    mix_audio_command_t *copy;

    if (command > MIX_STREAM_RESUME)
        return;

    copy = (mix_audio_command_t *)malloc(sizeof(*copy));
    if (!copy)
        return;
    *copy = command;

    LOGV("send mix audio stream command %s\n",
         get_mix_audio_command_string(command));

    pthread_mutex_lock(&lock);
    queue_push_tail(&q, copy);
    pthread_mutex_unlock(&lock);

    ScheduleWork();
}

MixAudioStreamCtrl::mix_audio_command_t *MixAudioStreamCtrl::PopCommand(void)
{
    mix_audio_command_t *command;

    pthread_mutex_lock(&lock);
    command = (mix_audio_command_t *)queue_pop_head(&q);
    pthread_mutex_unlock(&lock);

    return command;
}

void MixAudioStreamCtrl::Work(void)
{
    mix_audio_command_t *command;
    MIX_RESULT ret;

    command = PopCommand();
    if (command) {
        switch ((int)*command) {
        case MIX_STREAM_START:
            usleep(50000);
            ret = mix_audio_start(mix);
            break;
        case MIX_STREAM_STOP_DROP:
            ret = mix_audio_stop_drop(mix);
            break;
        case MIX_STREAM_STOP_DRAIN:
            ret = mix_audio_stop_drain(mix);
            break;
        case MIX_STREAM_PAUSE:
            ret = mix_audio_pause(mix);
            break;
        case MIX_STREAM_RESUME:
            ret = mix_audio_resume(mix);
            break;
        default:
            ret = MIX_RESULT_FAIL;
            LOGE("cannot handle mix audio stream command (%d)", *command);
        }

        if (MIX_SUCCEEDED(ret))
            LOGV("done mix audio stream command %s\n",
                 get_mix_audio_command_string(*command));
        else
            LOGE("failed mix audio stream command %s (ret : 0x%08x)\n",
                 get_mix_audio_command_string(*command), ret);

        free(command);
    }
}

/* end of MixAudioStreamCtrl */

/*
 * constructor & destructor
 */
MrstSstComponent::MrstSstComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = void)\n", __func__, __LINE__);
}

MrstSstComponent::~MrstSstComponent()
{
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = void)\n", __func__, __LINE__);
}

/* end of constructor & destructor */

/* core methods & helpers */
OMX_ERRORTYPE MrstSstComponent::ComponentAllocatePorts(void)
{
    PortBase **ports;

    OMX_U32 codec_port_index, pcm_port_index;
    OMX_DIRTYPE codec_port_dir, pcm_port_dir;

    OMX_PORT_PARAM_TYPE portparam;

    bool isencoder;
    const char *working_role;

    OMX_U32 i;
    OMX_ERRORTYPE ret;

    LOGV("%s(): enter\n", __func__);

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;

    this->nr_ports = NR_PORTS;
    this->ports = ports;

    working_role = GetWorkingRole();
    if (!strncmp(working_role, "audio_decoder", strlen("audio_decoder")))
        isencoder = false;
    else
        isencoder = true;

    if (isencoder) {
        pcm_port_index = INPORT_INDEX;
        codec_port_index = OUTPORT_INDEX;
        pcm_port_dir = OMX_DirInput;
        codec_port_dir = OMX_DirOutput;
    }
    else {
        codec_port_index = INPORT_INDEX;
        pcm_port_index = OUTPORT_INDEX;
        codec_port_dir = OMX_DirInput;
        pcm_port_dir = OMX_DirOutput;
    }

    working_role = strpbrk(working_role, ".");
    if (!working_role)
        return OMX_ErrorUndefined;
    working_role++;

    if (!strcmp(working_role, "mp3")) {
        ret = __AllocateMp3Port(codec_port_index, codec_port_dir);
        coding_type = OMX_AUDIO_CodingMP3;
    }
    else if(!strcmp(working_role, "aac")) {
        ret = __AllocateAacPort(codec_port_index, codec_port_dir);
        coding_type = OMX_AUDIO_CodingAAC;
    }
    else
        ret = OMX_ErrorUndefined;

    if (ret != OMX_ErrorNone)
        goto free_ports;

    ret = __AllocatePcmPort(pcm_port_index, pcm_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    /* OMX_PORT_PARAM_TYPE */
    memset(&portparam, 0, sizeof(portparam));
    SetTypeHeader(&portparam, sizeof(portparam));
    portparam.nPorts = NR_PORTS;
    portparam.nStartPortNumber = INPORT_INDEX;

    memcpy(&this->portparam, &portparam, sizeof(portparam));
    /* end of OMX_PORT_PARAM_TYPE */

    codec_mode = isencoder ? MIX_CODING_ENCODE : MIX_CODING_DECODE;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

free_codecport:
    delete ports[codec_port_index];
    ports[codec_port_index] = NULL;

free_ports:
    coding_type = OMX_AUDIO_CodingUnused;

    delete []ports;
    ports = NULL;

    this->ports = NULL;
    this->nr_ports = 0;

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::__AllocateMp3Port(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortMp3 *mp3port;

    OMX_PARAM_PORTDEFINITIONTYPE mp3portdefinition;
    OMX_AUDIO_PARAM_MP3TYPE mp3portparam;
    OMX_U32 i;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortMp3;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    mp3port = static_cast<PortMp3 *>(this->ports[port_index]);

    /* MP3 - OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&mp3portdefinition, 0, sizeof(mp3portdefinition));
    SetTypeHeader(&mp3portdefinition, sizeof(mp3portdefinition));
    mp3portdefinition.nPortIndex = port_index;
    mp3portdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        mp3portdefinition.nBufferCountActual = INPORT_MP3_ACTUAL_BUFFER_COUNT;
        mp3portdefinition.nBufferCountMin = INPORT_MP3_MIN_BUFFER_COUNT;
        mp3portdefinition.nBufferSize = INPORT_MP3_BUFFER_SIZE;
    }
    else {
        mp3portdefinition.nBufferCountActual = OUTPORT_MP3_ACTUAL_BUFFER_COUNT;
        mp3portdefinition.nBufferCountMin = OUTPORT_MP3_MIN_BUFFER_COUNT;
        mp3portdefinition.nBufferSize = OUTPORT_MP3_BUFFER_SIZE;
    }
    mp3portdefinition.bEnabled = OMX_TRUE;
    mp3portdefinition.bPopulated = OMX_FALSE;
    mp3portdefinition.eDomain = OMX_PortDomainAudio;
    mp3portdefinition.format.audio.cMIMEType = (char *)"audio/mpeg";
    mp3portdefinition.format.audio.pNativeRender = NULL;
    mp3portdefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
    mp3portdefinition.format.audio.eEncoding = OMX_AUDIO_CodingMP3;
    mp3portdefinition.bBuffersContiguous = OMX_FALSE;
    mp3portdefinition.nBufferAlignment = 0;

    mp3port->SetPortDefinition(&mp3portdefinition, true);

    /* end of MP3 - OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_AUDIO_PARAM_MP3TYPE */
    memset(&mp3portparam, 0, sizeof(mp3portparam));
    SetTypeHeader(&mp3portparam, sizeof(mp3portparam));
    mp3portparam.nPortIndex = port_index;
    mp3portparam.nChannels = 2;
    mp3portparam.nBitRate = 128000;
    mp3portparam.nSampleRate = 44100;
    mp3portparam.nAudioBandWidth = 0;
    mp3portparam.eChannelMode = OMX_AUDIO_ChannelModeStereo;
    mp3portparam.eFormat = OMX_AUDIO_MP3StreamFormatMP1Layer3;

    mp3port->SetPortMp3Param(&mp3portparam, true);
    /* end of OMX_AUDIO_PARAM_MP3TYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__AllocateAacPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortAac *aacport;

    OMX_PARAM_PORTDEFINITIONTYPE aacportdefinition;
    OMX_AUDIO_PARAM_AACPROFILETYPE aacportparam;
    OMX_U32 i;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortAac;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    aacport = static_cast<PortAac *>(this->ports[port_index]);

    /* AAC - OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&aacportdefinition, 0, sizeof(aacportdefinition));
    SetTypeHeader(&aacportdefinition, sizeof(aacportdefinition));
    aacportdefinition.nPortIndex = port_index;
    aacportdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        aacportdefinition.nBufferCountActual = INPORT_AAC_ACTUAL_BUFFER_COUNT;
        aacportdefinition.nBufferCountMin = INPORT_AAC_MIN_BUFFER_COUNT;
        aacportdefinition.nBufferSize = INPORT_AAC_BUFFER_SIZE;
    }
    else {
        aacportdefinition.nBufferCountActual = OUTPORT_AAC_ACTUAL_BUFFER_COUNT;
        aacportdefinition.nBufferCountMin = OUTPORT_AAC_MIN_BUFFER_COUNT;
        aacportdefinition.nBufferSize = OUTPORT_AAC_BUFFER_SIZE;
    }
    aacportdefinition.bEnabled = OMX_TRUE;
    aacportdefinition.bPopulated = OMX_FALSE;
    aacportdefinition.eDomain = OMX_PortDomainAudio;
    aacportdefinition.format.audio.cMIMEType = (char *)"audio/mp4";
    aacportdefinition.format.audio.pNativeRender = NULL;
    aacportdefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
    aacportdefinition.format.audio.eEncoding = OMX_AUDIO_CodingAAC;
    aacportdefinition.bBuffersContiguous = OMX_FALSE;
    aacportdefinition.nBufferAlignment = 0;

    aacport->SetPortDefinition(&aacportdefinition, true);

    /* end of AAC - OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_AUDIO_PARAM_AACPROFILETYPE */
    memset(&aacportparam, 0, sizeof(aacportparam));
    SetTypeHeader(&aacportparam, sizeof(aacportparam));
    aacportparam.nPortIndex = port_index;
    aacportparam.nChannels = 2;
    aacportparam.nBitRate = 128000;
    aacportparam.nSampleRate = 44100;
    aacportparam.nAudioBandWidth = 0;
    aacportparam.nFrameLength = 1024; /* default for LC */
    aacportparam.nAACtools = OMX_AUDIO_AACToolNone;
    aacportparam.nAACERtools = OMX_AUDIO_AACERNone;
    aacportparam.eAACProfile = OMX_AUDIO_AACObjectLC;
    aacportparam.eChannelMode = OMX_AUDIO_ChannelModeStereo;

    aacport->SetPortAacParam(&aacportparam, true);
    /* end of OMX_AUDIO_PARAM_AACPROFILETYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__AllocatePcmPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortPcm *pcmport;

    OMX_PARAM_PORTDEFINITIONTYPE pcmportdefinition;
    OMX_AUDIO_PARAM_PCMMODETYPE pcmportparam;
    OMX_U32 i;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortPcm;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }
    pcmport = static_cast<PortPcm *>(this->ports[port_index]);

    /* PCM - OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&pcmportdefinition, 0, sizeof(pcmportdefinition));
    SetTypeHeader(&pcmportdefinition, sizeof(pcmportdefinition));
    pcmportdefinition.nPortIndex = port_index;
    pcmportdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        pcmportdefinition.nBufferCountActual = INPORT_PCM_ACTUAL_BUFFER_COUNT;
        pcmportdefinition.nBufferCountMin = INPORT_PCM_MIN_BUFFER_COUNT;
        pcmportdefinition.nBufferSize = INPORT_PCM_BUFFER_SIZE;
    }
    else {
        pcmportdefinition.nBufferCountActual = OUTPORT_PCM_ACTUAL_BUFFER_COUNT;
        pcmportdefinition.nBufferCountMin = OUTPORT_PCM_MIN_BUFFER_COUNT;
        pcmportdefinition.nBufferSize = OUTPORT_PCM_BUFFER_SIZE;
    }
    pcmportdefinition.bEnabled = OMX_TRUE;
    pcmportdefinition.bPopulated = OMX_FALSE;
    pcmportdefinition.eDomain = OMX_PortDomainAudio;
    pcmportdefinition.format.audio.cMIMEType = (char *)"raw";
    pcmportdefinition.format.audio.pNativeRender = NULL;
    pcmportdefinition.format.audio.bFlagErrorConcealment = OMX_FALSE;
    pcmportdefinition.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    pcmportdefinition.bBuffersContiguous = OMX_FALSE;
    pcmportdefinition.nBufferAlignment = 0;

    pcmport->SetPortDefinition(&pcmportdefinition, true);
    /* end of PCM - OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_AUDIO_PARAM_PCMMODETYPE */
    memset(&pcmportparam, 0, sizeof(pcmportparam));
    SetTypeHeader(&pcmportparam, sizeof(pcmportparam));
    pcmportparam.nPortIndex = port_index;
    pcmportparam.nChannels = 2;
    pcmportparam.eNumData = OMX_NumericalDataUnsigned;
    pcmportparam.eEndian = OMX_EndianLittle;
    pcmportparam.bInterleaved = OMX_FALSE;
    pcmportparam.nBitPerSample = 16;
    pcmportparam.nSamplingRate = 44100;
    pcmportparam.ePCMMode = OMX_AUDIO_PCMModeLinear;
    pcmportparam.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    pcmportparam.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

    pcmport->SetPortPcmParam(&pcmportparam, true);
    /* end of OMX_AUDIO_PARAM_PCMMODETYPE */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

/* end of core methods & helpers */

/*
 * component methods & helpers
 */
/* Get/SetParameter */
OMX_ERRORTYPE MrstSstComponent::ComponentGetParameter(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nParamIndex);

    switch (nParamIndex) {
    case OMX_IndexParamAudioPortFormat: {
        OMX_AUDIO_PARAM_PORTFORMATTYPE *p =
            (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAudio *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAudio *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortAudioParam(), sizeof(*p));
        break;
    }
    case OMX_IndexParamAudioPcm: {
        OMX_AUDIO_PARAM_PCMMODETYPE *p =
            (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortPcm *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortPcm *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortPcmParam(), sizeof(*p));
        break;
    }
    case OMX_IndexParamAudioMp3: {
        OMX_AUDIO_PARAM_MP3TYPE *p =
            (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortMp3 *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMp3 *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortMp3Param(), sizeof(*p));
        break;
    }
    case OMX_IndexParamAudioAac: {
        OMX_AUDIO_PARAM_AACPROFILETYPE *p =
            (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAac *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAac *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortAacParam(), sizeof(*p));
        break;
    }
#if 0
    case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX: {
        PV_OMXComponentCapabilityFlagsType *p =
            (PV_OMXComponentCapabilityFlagsType *)pComponentParameterStructure;

        p->iIsOMXComponentMultiThreaded = OMX_TRUE;
        p->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
        p->iOMXComponentUsesNALStartCodes = OMX_TRUE;
        p->iOMXComponentSupportsPartialFrames = OMX_FALSE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_TRUE;
        p->iOMXComponentUsesFullAVCFrames = OMX_FALSE;
        break;
    }
#endif
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ComponentSetParameter(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nIndex);

    switch (nIndex) {
    case OMX_IndexParamAudioPortFormat: {
        OMX_AUDIO_PARAM_PORTFORMATTYPE *p =
            (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAudio *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortPcm *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortAudioParam(p, false);
        break;
    }
    case OMX_IndexParamAudioPcm: {
        OMX_AUDIO_PARAM_PCMMODETYPE *p =
            (OMX_AUDIO_PARAM_PCMMODETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortPcm *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortPcm *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortPcmParam(p, false);
        break;
    }
    case OMX_IndexParamAudioMp3: {
        OMX_AUDIO_PARAM_MP3TYPE *p =
            (OMX_AUDIO_PARAM_MP3TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortMp3 *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMp3 *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortMp3Param(p, false);
        break;
    }
    case OMX_IndexParamAudioAac: {
        OMX_AUDIO_PARAM_AACPROFILETYPE *p =
            (OMX_AUDIO_PARAM_AACPROFILETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortAac *port = NULL;

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortAac *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortAacParam(p, false);
        break;
    }
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* Get/SetConfig */
OMX_ERRORTYPE MrstSstComponent::ComponentGetConfig(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ComponentSetConfig(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentConfigStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::Processor[*] */
OMX_ERRORTYPE MrstSstComponent::ProcessorInit(void)
{
    MixAudio *mix;
    MixAudioConfigParams *acp;
    bool acp_changed;
    MixIOVec *mixio_in, *mixio_out;
    OMX_ERRORTYPE oret;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

    g_type_init();

    mix = mix_audio_new();
    if (!mix) {
        LOGE("%s(),%d: exit, mix_audio_new failed", __func__, __LINE__);
        goto error_out;
    }

    if (coding_type == OMX_AUDIO_CodingMP3) {
        MixAudioConfigParamsMP3 *acp_mp3 = mix_acp_mp3_new();
        if (!acp_mp3) {
            LOGE("%s(),%d: exit, mix_acp_mp3_new failed", __func__, __LINE__);
            goto free_mix;
        }
        acp = MIX_AUDIOCONFIGPARAMS(acp_mp3);
    }
    else if (coding_type == OMX_AUDIO_CodingAAC) {
        MixAudioConfigParamsAAC *acp_aac = mix_acp_aac_new();
        if (!acp_aac) {
            LOGE("%s(),%d: exit, mix_acp_aac_new failed", __func__, __LINE__);
            goto free_mix;
        }
        acp = MIX_AUDIOCONFIGPARAMS(acp_aac);
    }
    else {
        LOGE("%s(),%d: exit, unkown role (ret == 0x%08x)\n",
             __func__, __LINE__, OMX_ErrorInvalidState);
        goto free_mix;
    }

    if (codec_mode == MIX_CODING_DECODE) {
#if SST_RENDER_MODE_DIRECT_RENDER
        MIX_ACP_DECODEMODE(acp) = MIX_DECODE_DIRECTRENDER;
#else
        MIX_ACP_DECODEMODE(acp) = MIX_DECODE_DECODERETURN;
#endif
        oret = ChangeAcpWithPortParam(acp, ports[INPORT_INDEX], &acp_changed);
        if (oret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit, ChangeAcpWithPortParam failed "
                 "(ret == 0x%08x)\n", __func__, __LINE__, oret);
            goto free_acp;
        }
    }
    else if (codec_mode == MIX_CODING_ENCODE) {
        MIX_ACP_DECODEMODE(acp) = MIX_DECODE_NULL;
        oret = ChangeAcpWithPortParam(acp, ports[OUTPORT_INDEX], &acp_changed);
        if (oret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit, ChangeAcpWithPortParam failed "
                 "(ret == 0x%08x)\n", __func__, __LINE__, oret);
            goto free_acp;
        }
    }

    mret = mix_acp_set_streamname(acp, GetWorkingRole());
    if (!MIX_SUCCEEDED(mret)) {
        LOGE("%s(),%d: exit, mix_acp_set_streamname failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto free_acp;
    }

    mret = mix_audio_initialize(mix, codec_mode, NULL, NULL);
    if (!(MIX_SUCCEEDED(mret))) {
        LOGE("%s(),%d: exit, mix_audio_initialize failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto free_acp;
    }
    LOGV("%s(): mix audio initialized", __func__);

    LOGV("%s: call mix_audio_configure() with the following acp\n",
         __func__);
    LOGV("%s:   decode mode      : %d\n", __func__, acp->decode_mode);
    LOGV("%s:   stream name      : %s\n", __func__, acp->stream_name);
    LOGV("%s:   nr of channels   : %d\n", __func__, MIX_ACP_NUM_CHANNELS(acp));
    LOGV("%s:   bitrate          : %d\n", __func__, MIX_ACP_BITRATE(acp));
    LOGV("%s:   samplerate       : %d\n", __func__, MIX_ACP_SAMPLE_FREQ(acp));
    if (coding_type == OMX_AUDIO_CodingMP3) {
        LOGV("%s:   format           : %d\n", __func__,
             MIX_ACP_MP3_MPEG_FORMAT(acp));
        LOGV("%s:   layer            : %d\n", __func__,
             MIX_ACP_MP3_MPEG_LAYER(acp));
        LOGV("%s:   crc              : %d\n", __func__, MIX_ACP_MP3_CRC(acp));
    }
    else if (coding_type == OMX_AUDIO_CodingAAC) {
        LOGV("%s:   aot              : %d", __func__,
             mix_acp_aac_get_aot(MIX_AUDIOCONFIGPARAMSAAC(acp)));
        LOGV("%s:   aac profile      : %d", __func__,
             mix_acp_aac_get_aac_profile(MIX_AUDIOCONFIGPARAMSAAC(acp)));
        LOGV("%s:   MPEG id          : %d", __func__,
             mix_acp_aac_get_mpeg_id(MIX_AUDIOCONFIGPARAMSAAC(acp)));
        LOGV("%s:   bitstream format : %d", __func__,
             mix_acp_aac_get_bit_stream_format(MIX_AUDIOCONFIGPARAMSAAC(acp)));
        LOGV("%s:   bitrate type     : %d", __func__,
             mix_acp_aac_get_bit_rate_type(MIX_AUDIOCONFIGPARAMSAAC(acp)));
    }

    mret = mix_audio_configure(mix, acp, NULL);
    if (!MIX_SUCCEEDED(mret)) {
        LOGE("%s(),%d: exit, mix_audio_configure failed (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto deinitialize_mix;
    }
    LOGV("%s(): mix audio configured", __func__);

#if SST_USE_STREAM_CTRL_WORKQUEUE
    if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
        (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL)) {
        mix_stream_ctrl = new MixAudioStreamCtrl(mix);
        if (!mix_stream_ctrl) {
            LOGE("%s(),%d: exit, failed to new MixAudioStreamCtrl",
                 __func__, __LINE__);
            goto deinitialize_mix;
        }
    }
#endif

    mixio_in = (MixIOVec *)malloc(sizeof(MixIOVec));
    if (!mixio_in) {
        LOGE("%s(),%d: exit, failed to allocate mbuffer (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto deinitialize_mix;
    }

    mixio_out = (MixIOVec *)malloc(sizeof(MixIOVec));
    if (!mixio_out) {
        LOGE("%s(),%d: exit, failed to allocate mbuffer (ret == 0x%08x)",
             __func__, __LINE__, mret);
        goto free_mixio_in;
    }

    this->mix = mix;
    this->acp = acp;
    this->mixio_in = mixio_in;
    this->mixio_out = mixio_out;

    LOGV("%s(),%d: exit, done\n", __func__, __LINE__);
    return OMX_ErrorNone;

free_mixio_in:
    free(mixio_in);

deinitialize_mix:
    mix_audio_deinitialize(mix);

free_acp:
    mix_acp_unref(acp);

free_mix:
    mix_audio_unref(mix);

error_out:
    LOGV("%s(),%d: exit, (ret = 0x%08x)\n", __func__, __LINE__, oret);
    return OMX_ErrorUndefined;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorDeinit(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

#if SST_USE_STREAM_CTRL_WORKQUEUE
    if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
        (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL))
        delete mix_stream_ctrl;
#endif

    if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
        (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL))
        mix_audio_stop_drop(mix);

    mix_audio_deinitialize(mix);
    LOGV("%s(): mix audio deinitialized", __func__);

    mix_acp_unref(acp);
    mix_audio_unref(mix);

    free(mixio_in);
    free(mixio_out);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorStart(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorStop(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
        (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL))
        mix_audio_stop_drop(mix);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorPause(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    //if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
    //    (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL))
    //    mix_audio_pause(mix)

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE MrstSstComponent::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    LOGV("%s(): enter\n", __func__);

    //if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
    //    (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL))
    //    mix_audio_resume(mix);

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::ProcessorProcess */
OMX_ERRORTYPE MrstSstComponent::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retain,
    OMX_U32 nr_buffers)
{
    OMX_U64 consumed = 0, produced = 0;
    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;
    OMX_U32 outflags = 0;

    MixStreamState mstream_state = MIX_STREAM_NULL;
    MixState mstate;
    bool acp_changed = false;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    MIX_RESULT mret;

    LOGV("%s(): enter\n", __func__);

#if !LOG_NDEBUG
    DumpBuffer(buffers[INPORT_INDEX], false);
#endif

    LOGV_IF(buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS,
            "%s(),%d: got OMX_BUFFERFLAG_EOS\n", __func__, __LINE__);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGV("%s(),%d: exit, input buffer's nFilledLen is zero (ret = void)\n",
             __func__, __LINE__);
        goto out;
    }

    mixio_in->data = buffers[INPORT_INDEX]->pBuffer +
        buffers[INPORT_INDEX]->nOffset;
    mixio_in->data_size = buffers[INPORT_INDEX]->nFilledLen;
    mixio_in->buffer_size = buffers[INPORT_INDEX]->nAllocLen;

    mixio_out->data = buffers[OUTPORT_INDEX]->pBuffer +
        buffers[OUTPORT_INDEX]->nOffset;
    mixio_out->data_size = 0;
    mixio_out->buffer_size = buffers[OUTPORT_INDEX]->nAllocLen;

    if (codec_mode == MIX_CODING_DECODE) {
        if (((coding_type == OMX_AUDIO_CodingMP3) &&
             (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_SYNCFRAME))
            ||
            ((coding_type == OMX_AUDIO_CodingAAC) &&
             (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG))) {

            oret = ChangeAcpWithConfigHeader(mixio_in->data,
                                             &acp_changed);
            if (oret != OMX_ErrorNone) {
                LOGE("%s(),%d: exit, ret == 0x%08x\n", __func__, __LINE__,
                     oret);
                goto out;
            }
        }

        /*
         * reconfigure acp with parsed parameter
         * 1. if started, stop
         * 2. reset mix audio
         * 3. configure
         */
        if (acp_changed) {
            mix_audio_get_state(mix, &mstate);
            if (mstate == MIX_STATE_CONFIGURED) {
                if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
                    (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL)) {
                    mix_audio_get_stream_state(mix, &mstream_state);
                    if (mstream_state == MIX_STREAM_PLAYING)
                        mix_audio_stop_drop(mix);
                }
                mix_audio_deinitialize(mix);
                mix_audio_initialize(mix, codec_mode, NULL, NULL);
            }

            mret = mix_audio_configure(mix, acp, NULL);
            if (!MIX_SUCCEEDED(mret)) {
                LOGE("%s(),%d: exit, mix_audio_configure failed "
                     "(ret == 0x%08x)", __func__, __LINE__, mret);
                oret = OMX_ErrorUndefined;
                goto out;
            }
            LOGV("%s(): mix audio configured", __func__);

            /*
             * port reconfigure
             */
            ChangePortParamWithAcp();
            goto out;
        }
    }
    else {
        if (coding_type == OMX_AUDIO_CodingAAC) {
#if TEST_USE_AAC_ENCODING_RAW
            if (codecdata) {
                memcpy(buffers[OUTPORT_INDEX]->pBuffer, codecdata, 2);

                free(codecdata);
                codecdata = NULL;

                outfilledlen = 2;
                outflags |=
                    OMX_BUFFERFLAG_CODECCONFIG | OMX_BUFFERFLAG_ENDOFFRAME;

                retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
                goto out;
            }
#endif
        }
    }

    if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
        (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL)) {
        mix_audio_get_stream_state(mix, &mstream_state);
        if (mstream_state != MIX_STREAM_PLAYING) {
#if SST_USE_STREAM_CTRL_WORKQUEUE
            if ((MIX_ACP_DECODEMODE(acp) == MIX_DECODE_DIRECTRENDER) ||
                (MIX_ACP_DECODEMODE(acp) == MIX_DECODE_NULL))
                mix_stream_ctrl->SendCommand(
                    MixAudioStreamCtrl::MIX_STREAM_START);
#else
            LOGV("%s(): mix current stream state = %d, call mix_audio_start\n",
                 __func__, mstream_state);
            mret = mix_audio_start(mix);
            if (!MIX_SUCCEEDED(mret)) {
                LOGE("%s(),%d: faild to mix_audio_start (ret == 0x%08x)",
                     __func__, __LINE__, mret);
                oret = OMX_ErrorUndefined;
                goto out;
            }
#endif
        }
    }

    if (codec_mode == MIX_CODING_DECODE) {
        if (buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_CODECCONFIG) {
            retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
            goto out;
        }

        //LOGV("%s(): call mix_audio_decode()\n", __func__);
        mret = mix_audio_decode(mix,
                                (const MixIOVec *)mixio_in, 1, &consumed,
                                mixio_out, 1);
        if (!MIX_SUCCEEDED(mret)) {
            LOGE("%s(), %d: exit, mix_audio_decode failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto out;
        }
        produced = mixio_out->data_size;
        //LOGV("%s(): returned from mix_audio_decode()\n", __func__);
    }
    else {
        //LOGV("%s(): call mix_audio_capture_encode()\n", __func__);
        mret = mix_audio_capture_encode(mix, mixio_out, 1);
        if (!MIX_SUCCEEDED(mret)) {
            LOGE("%s(), %d: exit, mix_audio_decode failed (ret == 0x%08x)",
                 __func__, __LINE__, mret);
            oret = OMX_ErrorUndefined;
            goto out;
        }
        //LOGV("%s(): returned from mix_audio_capture_encode()\n", __func__);

        outflags |= OMX_BUFFERFLAG_ENDOFFRAME;

        /* direct encode workaround */
        consumed = mixio_in->data_size;
        produced = mixio_out->data_size;

#if 0
        mix_audio_get_timestamp(mix, (OMX_U64 *)&outtimestamp);
        outtimestamp *= 1000;
        buffers[INPORT_INDEX]->nTimeStamp = outtimestamp;
#endif
    }

    outfilledlen = produced;
    LOGV("%s(): %lu bytes produced\n", __func__, outfilledlen);

    buffers[INPORT_INDEX]->nFilledLen -= (OMX_U32)consumed;
    if (buffers[INPORT_INDEX]->nFilledLen) {
        LOGV("%s(): input buffer NOT fully consumed %lu bytes consumed\n",
             __func__, (OMX_U32)consumed);

        if (!consumed && !produced) {
            LOGV("%s(): just return\n", __func__);
            goto out;
        }

        LOGV("%s(): %lu bytes remained, waits for next turn\n",
             __func__, buffers[INPORT_INDEX]->nFilledLen);

        buffers[INPORT_INDEX]->nOffset += (OMX_U32)consumed;
        retain[INPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
        /* FIXME */
        outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;
    }
    else {
        buffers[INPORT_INDEX]->nOffset = 0;
        outtimestamp = buffers[INPORT_INDEX]->nTimeStamp;

        LOGV("%s(): %lu bytes fully consumed\n", __func__,
             (OMX_U32)consumed);
    }

out:
    buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
    buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;
    buffers[OUTPORT_INDEX]->nFlags = outflags;

#if !LOG_NDEBUG
    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
        DumpBuffer(buffers[OUTPORT_INDEX], false);
#endif

    LOGV("%s(),%d: exit, done\n", __func__, __LINE__);

    return oret;
}

/* end of implement ComponentBase::Processor[*] */

/* end of component methods & helpers */

/*
 * acp setting helpers
 */

static inline OMX_ERRORTYPE __Mp3ChangeAcp(MixAudioConfigParams *acp,
                                           bool *acp_changed,
                                           int channel, int samplingrate,
                                           int bitrate,
                                           int version, int layer, int crc)
{
    if (!acp_changed)
        return OMX_ErrorBadParameter;

    if ((MIX_ACP_NUM_CHANNELS(acp) != channel) ||
        /* mix_audio doesn't care bitrate change */
        //(MIX_ACP_BITRATE(acp) != bitrate) ||
        (MIX_ACP_SAMPLE_FREQ(acp) != samplingrate) ||
        (MIX_ACP_MP3_CRC(acp) != crc) ||
        (MIX_ACP_MP3_MPEG_FORMAT(acp) != version) ||
        (MIX_ACP_MP3_MPEG_LAYER(acp) != layer)) {

        *acp_changed = true;

        LOGV_IF(MIX_ACP_NUM_CHANNELS(acp) != channel,
                "%s(): channel : %d != %d\n", __func__,
                MIX_ACP_NUM_CHANNELS(acp), channel);
        LOGV_IF(MIX_ACP_BITRATE(acp) != bitrate,
                "%s(): bitrate : %d != %d\n", __func__,
                MIX_ACP_BITRATE(acp), bitrate);
        LOGV_IF(MIX_ACP_SAMPLE_FREQ(acp) != samplingrate,
                "%s(): frequency : %d != %d\n", __func__,
                MIX_ACP_SAMPLE_FREQ(acp), samplingrate);
        LOGV_IF(MIX_ACP_MP3_CRC(acp) != crc,
                "%s(): crc : %d != %d\n", __func__, MIX_ACP_MP3_CRC(acp), crc);
        LOGV_IF(MIX_ACP_MP3_MPEG_FORMAT(acp) != version,
                "%s(): version : %d != %d\n", __func__,
                MIX_ACP_MP3_MPEG_FORMAT(acp), version);
        LOGV_IF(MIX_ACP_MP3_MPEG_LAYER(acp) != layer,
                "%s(): layer : %d != %d\n", __func__,
                MIX_ACP_MP3_MPEG_LAYER(acp), layer);

        MIX_ACP_NUM_CHANNELS(acp) = channel;
        MIX_ACP_BITRATE(acp) = bitrate;
        MIX_ACP_SAMPLE_FREQ(acp) = samplingrate;

        MIX_ACP_MP3_CRC(acp) = crc;
        MIX_ACP_MP3_MPEG_FORMAT(acp) = version;
        MIX_ACP_MP3_MPEG_LAYER(acp) = layer;

        LOGV("%s(): mp3 configration parameter has been changed\n", __func__);
        LOGV("%s():   format : %d\n", __func__, MIX_ACP_MP3_MPEG_FORMAT(acp));
        LOGV("%s():   layer : %d\n", __func__, MIX_ACP_MP3_MPEG_LAYER(acp));
        LOGV("%s():   crc : %d\n", __func__, MIX_ACP_MP3_CRC(acp));
        LOGV("%s():   frequency : %d\n", __func__, MIX_ACP_SAMPLE_FREQ(acp));
        LOGV("%s():   bitrate : %d\n", __func__, MIX_ACP_BITRATE(acp));
        LOGV("%s():   channel : %d\n", __func__, MIX_ACP_NUM_CHANNELS(acp));
    }

    mix_acp_set_op_align(acp, MIX_ACP_OUTPUT_ALIGN_LSB);
    mix_acp_set_bps(acp, MIX_ACP_BPS_16);

    return OMX_ErrorNone;
}

static inline OMX_ERRORTYPE __AacChangeAcp(MixAudioConfigParams *acp,
                                           bool *acp_changed,
                                           int channel, int frequency,
                                           int bitrate, int aot)
{
    if (!acp_changed)
        return OMX_ErrorBadParameter;

    if ((MIX_ACP_NUM_CHANNELS(acp) != channel) ||
        (MIX_ACP_SAMPLE_FREQ(acp) != frequency) ||
        (mix_acp_aac_get_aot(MIX_AUDIOCONFIGPARAMSAAC(acp)) !=
         (unsigned)aot) ||
        (mix_acp_aac_get_aac_profile(MIX_AUDIOCONFIGPARAMSAAC(acp)) !=
         (aot - 1))) {

        *acp_changed = true;

        LOGV_IF("%s(): channel : %d != %d\n", __func__,
                MIX_ACP_NUM_CHANNELS(acp), channel);
        LOGV_IF("%s(): samplingrate : %d != %d\n", __func__,
                MIX_ACP_SAMPLE_FREQ(acp), frequency);
        LOGV_IF("%s(): aot : %d != %d\n", __func__,
                mix_acp_aac_get_aot(MIX_AUDIOCONFIGPARAMSAAC(acp)), aot);
        LOGV_IF("%s(): profile : %d != %d\n", __func__,
                mix_acp_aac_get_aac_profile(MIX_AUDIOCONFIGPARAMSAAC(acp)),
                (aot - 1));

        MIX_ACP_NUM_CHANNELS(acp) = channel;
        MIX_ACP_SAMPLE_FREQ(acp) = frequency;
        MIX_ACP_BITRATE(acp) = bitrate;

        MIX_ACP_AAC_SAMPLE_RATE(acp) = frequency;
        MIX_ACP_AAC_CHANNELS(acp) = channel;

        /* 1:Main, 2:LC, 3:SSR, 4:SBR */
        mix_acp_aac_set_aot(MIX_AUDIOCONFIGPARAMSAAC(acp), aot);
        /* 0:Main, 1:LC, 2:SSR, 3:SBR */
        mix_acp_aac_set_aac_profile(MIX_AUDIOCONFIGPARAMSAAC(acp),
                                    (MixAACProfile)(aot - 1));

        LOGV("%s(): audio configration parameter has been chagned\n",
             __func__);
        LOGV("%s():   aot : %d\n", __func__,
             mix_acp_aac_get_aot(MIX_AUDIOCONFIGPARAMSAAC(acp)));
        LOGV("%s():   frequency : %d\n", __func__, MIX_ACP_SAMPLE_FREQ(acp));
        LOGV("%s():   channel : %d\n", __func__, MIX_ACP_NUM_CHANNELS(acp));
    }

    mix_acp_aac_set_mpeg_id(MIX_AUDIOCONFIGPARAMSAAC(acp), MIX_AAC_MPEG_4_ID);
    MIX_ACP_AAC_CRC(acp) = 0; /* 0:disabled, 1:enabled */
    /* 0:CBR, 1:VBR */
    mix_acp_aac_set_bit_rate_type(MIX_AUDIOCONFIGPARAMSAAC(acp),
                                  (MixAACBitrateType)0);
#if TEST_USE_AAC_ENCODING_RAW
    mix_acp_aac_set_bit_stream_format(MIX_AUDIOCONFIGPARAMSAAC(acp),
                                      MIX_AAC_BS_RAW);
#else /* encoding: adts, decoding:raw */
    if (MIX_ACP_DECODEMODE(acp) != MIX_DECODE_NULL) /* decoding */
        mix_acp_aac_set_bit_stream_format(MIX_AUDIOCONFIGPARAMSAAC(acp),
                                          MIX_AAC_BS_RAW);
    else /* encoding */
        mix_acp_aac_set_bit_stream_format(MIX_AUDIOCONFIGPARAMSAAC(acp),
                                          MIX_AAC_BS_ADTS);
#endif

    mix_acp_set_op_align(acp, MIX_ACP_OUTPUT_ALIGN_LSB);
    mix_acp_set_bps(acp, MIX_ACP_BPS_16);

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__Mp3ChangeAcpWithConfigHeader(
    const unsigned char *buffer, bool *acp_changed)
{
    int version, layer, crc, bitrate, samplingrate, channel,
        mode_extension, frame_length, frame_duration;
    int ret;

    ret = mp3_header_parse(buffer,
                           &version, &layer,
                           &crc, &bitrate,
                           &samplingrate, &channel,
                           &mode_extension, &frame_length, &frame_duration);
    if (ret)
        return OMX_ErrorBadParameter;

    if (version == MP3_HEADER_VERSION_1)
        version = 1;
    else if ((version == MP3_HEADER_VERSION_2) ||
             (version == MP3_HEADER_VERSION_25))
        version = 2;
    else
        return OMX_ErrorUndefined;

    if (layer == MP3_HEADER_LAYER_1)
        layer = 1;
    else if (layer == MP3_HEADER_LAYER_2)
        layer = 2;
    else if (layer == MP3_HEADER_LAYER_3)
        layer = 3;
    else
        return OMX_ErrorUndefined;

    if (crc == MP3_HEADER_CRC_PROTECTED)
        crc = 1;
    else if (crc == MP3_HEADER_NOT_PROTECTED)
        crc = 0;
    else
        return OMX_ErrorUndefined;

    if ((channel == MP3_HEADER_STEREO) ||
        (channel == MP3_HEADER_JOINT_STEREO) ||
        (channel == MP3_HEADER_DUAL_CHANNEL))
        channel = 2;
    else if (channel == MP3_HEADER_SINGLE_CHANNEL)
        channel = 1;
    else
        return OMX_ErrorUndefined;

    return __Mp3ChangeAcp(acp, acp_changed,
                          channel, samplingrate, bitrate,
                          version, layer, crc);
}

OMX_ERRORTYPE MrstSstComponent::__AacChangeAcpWithConfigHeader(
    const unsigned char *buffer, bool *acp_changed)
{
    int aot, frequency, channel;
    int ret;
    OMX_ERRORTYPE oret;

    ret = audio_specific_config_parse(buffer,
                                      &aot, &frequency, &channel);
    if (ret)
        return OMX_ErrorBadParameter;

    if (aot < 1 || aot > 4)
        return OMX_ErrorUndefined;

    oret = __AacChangeAcp(acp, acp_changed, channel, frequency, 128000, aot);
    if (oret != OMX_ErrorNone) {
        LOGE("%s(): __AacChangAcp() failed (ret : 0x%08x)\n", __func__, oret);
        return oret;
    }

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::ChangeAcpWithConfigHeader(
    const unsigned char *buffer,
    bool *acp_changed)
{
    OMX_ERRORTYPE ret;

    if (coding_type == OMX_AUDIO_CodingMP3)
        ret = __Mp3ChangeAcpWithConfigHeader(buffer, acp_changed);
    else if (coding_type == OMX_AUDIO_CodingAAC)
        ret = __AacChangeAcpWithConfigHeader(buffer, acp_changed);
    else
        return OMX_ErrorBadParameter;

    if (ret != OMX_ErrorNone)
        return ret;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__Mp3ChangeAcpWithPortParam(
    MixAudioConfigParams *acp, PortMp3 *port, bool *acp_changed)
{
    OMX_AUDIO_PARAM_MP3TYPE p;
    int format;
    OMX_ERRORTYPE ret;

    memcpy(&p, port->GetPortMp3Param(), sizeof(OMX_AUDIO_PARAM_MP3TYPE));

    ret = ComponentBase::CheckTypeHeader(&p, sizeof(p));
    if (ret != OMX_ErrorNone)
        return ret;

    LOGV("%s(): eFormat = %d\n", __func__, p.eFormat);
    LOGV("%s(): nBitRate = %lu\n", __func__, p.nBitRate);
    LOGV("%s(): nSampleRate = %lu\n", __func__, p.nSampleRate);
    LOGV("%s(): nChannels = %lu\n", __func__, p.nChannels);

    if (p.eFormat == OMX_AUDIO_MP3StreamFormatMP1Layer3)
        format = 1;
    else if ((p.eFormat == OMX_AUDIO_MP3StreamFormatMP2Layer3) ||
          (p.eFormat == OMX_AUDIO_MP3StreamFormatMP2_5Layer3))
        format = 2;
    else
        return OMX_ErrorBadParameter;

    return __Mp3ChangeAcp(acp, acp_changed,
                          p.nChannels, p.nSampleRate, p.nBitRate,
                          format, 3, 0);
}

OMX_ERRORTYPE MrstSstComponent::__AacChangeAcpWithPortParam(
    MixAudioConfigParams *acp, PortAac *port, bool *acp_changed)
{
    OMX_AUDIO_PARAM_AACPROFILETYPE p;
    int aot;
    OMX_ERRORTYPE ret;

    memcpy(&p, port->GetPortAacParam(), sizeof(p));

    ret = ComponentBase::CheckTypeHeader(&p, sizeof(p));
    if (ret != OMX_ErrorNone)
        return ret;

    LOGV("%s(): eAACProfile = %d\n", __func__, p.eAACProfile);
    LOGV("%s(): nSampleRate = %lu\n", __func__, p.nSampleRate);
    LOGV("%s(): nBitRate = %lu\n", __func__, p.nBitRate);
    LOGV("%s(): nChannels = %lu\n", __func__, p.nChannels);

    if (p.eAACProfile == OMX_AUDIO_AACObjectMain)
        aot = 1;
    else if (p.eAACProfile == OMX_AUDIO_AACObjectLC)
        aot = 2;
    else if (p.eAACProfile == OMX_AUDIO_AACObjectSSR)
        aot = 3;
    else
        return OMX_ErrorBadParameter;

#if TEST_USE_AAC_ENCODING_RAW
 #if TEST_USE_AAC_ENCODING_VARIABLE_PARAMETER
    if (codec_mode == MIX_CODING_ENCODE) {
        /* freed at ProcessorProcess */
        codecdata = (unsigned char *)calloc(2, sizeof(unsigned char));
        if (codecdata)
            audio_specific_config_bitcoding(codecdata,
                                            aot, p.nSampleRate, p.nChannels);
    }

    return __AacChangeAcp(acp, acp_changed, p.nChannels, p.nSampleRate,
                          p.nBitRate, aot);
 #else /* 44100,12800,2 */
    if (codec_mode == MIX_CODING_ENCODE) {
        /* freed at ProcessorProcess */
        codecdata = (unsigned char *)calloc(2, sizeof(unsigned char));
        if (codecdata)
            audio_specific_config_bitcoding(codecdata,
                                            aot, 44100, 2);
    }

    return __AacChangeAcp(acp, acp_changed, 2, 44100,
                          128000, aot);
 #endif
#else /* ADTS */
    if (codec_mode == MIX_CODING_ENCODE)
        codecdata = NULL;

 #if TEST_USE_AAC_ENCODING_VARIABLE_PARAMETER
    return __AacChangeAcp(acp, acp_changed, p.nChannels, p.nSampleRate,
                          p.nBitRate, aot);
 #else /* 44100,12800,2 */
    return __AacChangeAcp(acp, acp_changed, 2, 44100,
                          128000, aot);
 #endif
#endif
}

OMX_ERRORTYPE MrstSstComponent::ChangeAcpWithPortParam(
    MixAudioConfigParams *acp,
    PortBase *port,
    bool *acp_changed)
{
    OMX_ERRORTYPE ret;

    if (coding_type == OMX_AUDIO_CodingMP3)
        ret = __Mp3ChangeAcpWithPortParam(
            acp, static_cast<PortMp3 *>(port), acp_changed);
    else if (coding_type == OMX_AUDIO_CodingAAC)
        ret = __AacChangeAcpWithPortParam(
            acp, static_cast<PortAac *>(port), acp_changed);
    else
        return OMX_ErrorBadParameter;

    if (ret != OMX_ErrorNone)
        return ret;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__PcmChangePortParamWithAcp(
    MixAudioConfigParams *acp, PortPcm *port)
{
    OMX_AUDIO_PARAM_PCMMODETYPE p;
    OMX_ERRORTYPE ret;

    memcpy(&p, port->GetPortPcmParam(), sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));

    if ((MIX_ACP_NUM_CHANNELS(acp) != (int)p.nChannels) ||
        (MIX_ACP_SAMPLE_FREQ(acp) != (int)p.nSamplingRate)) {

        p.nChannels = MIX_ACP_NUM_CHANNELS(acp);
        p.nSamplingRate = MIX_ACP_SAMPLE_FREQ(acp);
    }

    port->SetPortPcmParam(&p, false);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__Mp3ChangePortParamWithAcp(
    MixAudioConfigParams *acp, PortMp3 *port)
{
    OMX_AUDIO_PARAM_MP3TYPE p;
    OMX_ERRORTYPE ret;

    memcpy(&p, port->GetPortMp3Param(), sizeof(OMX_AUDIO_PARAM_MP3TYPE));

    if ((MIX_ACP_NUM_CHANNELS(acp) != (int)p.nChannels) ||
        (MIX_ACP_BITRATE(acp) != (int)p.nBitRate) ||
        (MIX_ACP_SAMPLE_FREQ(acp) != (int)p.nSampleRate)) {

        p.nChannels = MIX_ACP_NUM_CHANNELS(acp);
        p.nBitRate = MIX_ACP_BITRATE(acp);
        p.nSampleRate = MIX_ACP_SAMPLE_FREQ(acp);
    }

    port->SetPortMp3Param(&p, false);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE MrstSstComponent::__AacChangePortParamWithAcp(
    MixAudioConfigParams *acp, PortAac *port)
{
    OMX_AUDIO_PARAM_AACPROFILETYPE p;
    OMX_ERRORTYPE ret;

    memcpy(&p, port->GetPortAacParam(), sizeof(p));

    if ((MIX_ACP_NUM_CHANNELS(acp) != (int)p.nChannels) ||
        (MIX_ACP_SAMPLE_FREQ(acp) != (int)p.nSampleRate)) {

        p.nChannels = MIX_ACP_NUM_CHANNELS(acp);
        p.nSampleRate = MIX_ACP_SAMPLE_FREQ(acp);
    }

    port->SetPortAacParam(&p, false);
    return OMX_ErrorNone;
}

/* used only decode mode */
OMX_ERRORTYPE MrstSstComponent::ChangePortParamWithAcp(void)
{
    OMX_ERRORTYPE ret;

    if (coding_type == OMX_AUDIO_CodingMP3)
        ret = __Mp3ChangePortParamWithAcp(
            acp, static_cast<PortMp3 *>(ports[INPORT_INDEX]));
    else if (coding_type == OMX_AUDIO_CodingAAC)
        ret = __AacChangePortParamWithAcp(
            acp, static_cast<PortAac *>(ports[INPORT_INDEX]));
    else
        return OMX_ErrorBadParameter;

    if (ret != OMX_ErrorNone)
        return ret;

    ret = __PcmChangePortParamWithAcp(
        acp, static_cast<PortPcm *>(ports[OUTPORT_INDEX]));
    if (ret != OMX_ErrorNone)
        return ret;

    LOGV("%s(): report OMX_EventPortSettingsChanged event on %luth port",
         __func__, OUTPORT_INDEX);

    ret = static_cast<PortPcm *>(ports[OUTPORT_INDEX])->
        ReportPortSettingsChanged();

    LOGV("%s(): returned from event handler (ret : 0x%08x)\n", __func__, ret);

    return OMX_ErrorNone;
}

/* end of acp setting helpers */

/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.SST";

static const char *g_roles[] =
{
    (const char *)"audio_decoder.mp3",
    (const char *)"audio_decoder.aac",
    (const char *)"audio_encoder.aac",
};

OMX_ERRORTYPE wrs_omxil_cmodule_ops_instantiate(OMX_PTR *instance)
{
    ComponentBase *cbase;

    cbase = new MrstSstComponent;
    if (!cbase) {
        *instance = NULL;
        return OMX_ErrorInsufficientResources;
    }

    *instance = cbase;
    return OMX_ErrorNone;
}

struct wrs_omxil_cmodule_ops_s cmodule_ops = {
    instantiate: wrs_omxil_cmodule_ops_instantiate,
};

struct wrs_omxil_cmodule_s WRS_OMXIL_CMODULE_SYMBOL = {
    name: g_name,
    roles: &g_roles[0],
    nr_roles: ARRAY_SIZE(g_roles),
    ops: &cmodule_ops,
};
