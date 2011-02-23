#define LOG_TAG "wrs-omx-stub-ve-mpeg4-libva "
#include <utils/Log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#include <OMX_Core.h>
#include <OMX_Video.h>
#include <OMX_IndexExt.h>
#include <OMX_VideoExt.h>

#include <cmodule.h>
#include <portvideo.h>
#include <componentbase.h>

#ifdef COMPONENT_SUPPORT_OPENCORE
#include <pv_omxcore.h>
#include <pv_omxdefs.h>
#endif

#include <va/va.h>
#include <va/va_android.h>
#include "vabuffer.h"
#include "libinfodump.h"
#include "OMXMPEG4Component.h"


//#define INFODUMP
#ifdef INFODUMP
int counter = 0;
#endif

static void DUMP_BUFFER(char* name, OMX_U8* buf, OMX_U32 len)
{
	int idx;
	int loglen = len < 16 ? len : 16;
	static char logbuf[4096];
	char *plogbuf = logbuf;

	for(idx = 0;idx<loglen;idx++) {
		sprintf(plogbuf, "0x%x ", ((unsigned char*)buf)[idx]);
		plogbuf = logbuf + strlen(logbuf);
	}
	plogbuf = 0;

	LOGV("%s(%p:%d): %s",name,  buf, len, logbuf);
}	


OMXMPEG4Component::OMXMPEG4Component():
	temp_coded_data_buffer(NULL),
	temp_coded_data_buffer_size(0)
{
    ENTER_FUN;

    int pret;

    config_lock = new pthread_mutex_t;
    pret = pthread_mutex_init(config_lock, NULL);
    assert(pret == 0);

    EXIT_FUN;
}

OMXMPEG4Component::~OMXMPEG4Component()
{
    ENTER_FUN;

    if (config_lock != NULL)
    {
        int pret;

	pret = pthread_mutex_destroy(config_lock);
	assert(pret == 0);

	delete config_lock;
    }

    EXIT_FUN;
}


/* core methods & helpers */
OMX_ERRORTYPE OMXMPEG4Component::ComponentAllocatePorts(void)
{
    ENTER_FUN;

    PortBase **ports;

    OMX_U32 codec_port_index, raw_port_index;
    OMX_DIRTYPE codec_port_dir, raw_port_dir;

    OMX_PORT_PARAM_TYPE portparam;

    const char *working_role;

    bool isencoder;

    OMX_ERRORTYPE ret = OMX_ErrorUndefined;

    ports = new PortBase *[NR_PORTS];
    if (!ports)
        return OMX_ErrorInsufficientResources;

    this->nr_ports = NR_PORTS;
    this->ports = ports;

    /* video_[encoder/decoder].[avc/whatever] */

    raw_port_index = INPORT_INDEX;
    codec_port_index = OUTPORT_INDEX;
    raw_port_dir = OMX_DirInput;
    codec_port_dir = OMX_DirOutput;

    ret = __AllocateMPEG4Port(codec_port_index, codec_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    ret = __AllocateRawPort(raw_port_index, raw_port_dir);
    if (ret != OMX_ErrorNone)
        goto free_codecport;

    /* OMX_PORT_PARAM_TYPE */
    memset(&portparam, 0, sizeof(portparam));
    SetTypeHeader(&portparam, sizeof(portparam));
    portparam.nPorts = NR_PORTS;
    portparam.nStartPortNumber = INPORT_INDEX;

    memcpy(&this->portparam, &portparam, sizeof(portparam));
    /* end of OMX_PORT_PARAM_TYPE */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;

free_codecport:
    delete ports[codec_port_index];
    ports[codec_port_index] = NULL;

free_ports:
    delete []ports;
    ports = NULL;

    this->ports = NULL;
    this->nr_ports = 0;

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXMPEG4Component::__AllocateMPEG4Port(OMX_U32 port_index,
                                                    OMX_DIRTYPE dir)
{
    ENTER_FUN;

    PortMpeg4 *mpeg4port;

    OMX_PARAM_PORTDEFINITIONTYPE mpeg4portdefinition;
    OMX_VIDEO_PARAM_MPEG4TYPE mpeg4portparam;

    ports[port_index] = new PortMpeg4;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    mpeg4port = static_cast<PortMpeg4 *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&mpeg4portdefinition, 0, sizeof(mpeg4portdefinition));
    SetTypeHeader(&mpeg4portdefinition, sizeof(mpeg4portdefinition));
    mpeg4portdefinition.nPortIndex = port_index;
    mpeg4portdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        mpeg4portdefinition.nBufferCountActual =
            INPORT_MPEG4_ACTUAL_BUFFER_COUNT;
        mpeg4portdefinition.nBufferCountMin = INPORT_MPEG4_MIN_BUFFER_COUNT;
        mpeg4portdefinition.nBufferSize = INPORT_MPEG4_BUFFER_SIZE;
    } else {
        mpeg4portdefinition.nBufferCountActual =
            OUTPORT_MPEG4_ACTUAL_BUFFER_COUNT;
        mpeg4portdefinition.nBufferCountMin = OUTPORT_MPEG4_MIN_BUFFER_COUNT;
        mpeg4portdefinition.nBufferSize = OUTPORT_MPEG4_BUFFER_SIZE;
    }
    mpeg4portdefinition.bEnabled = OMX_TRUE;
    mpeg4portdefinition.bPopulated = OMX_FALSE;
    mpeg4portdefinition.eDomain = OMX_PortDomainVideo;
    mpeg4portdefinition.format.video.cMIMEType = (OMX_STRING)"video/mpeg4";
    mpeg4portdefinition.format.video.pNativeRender = NULL;
    mpeg4portdefinition.format.video.nFrameWidth = 176;
    mpeg4portdefinition.format.video.nFrameHeight = 144;
    mpeg4portdefinition.format.video.nStride = 0;
    mpeg4portdefinition.format.video.nSliceHeight = 0;
    mpeg4portdefinition.format.video.nBitrate = 64000;
    mpeg4portdefinition.format.video.xFramerate = 15 << 16;
    mpeg4portdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    mpeg4portdefinition.format.video.eCompressionFormat =
        OMX_VIDEO_CodingMPEG4;
    mpeg4portdefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    mpeg4portdefinition.format.video.pNativeWindow = NULL;
    mpeg4portdefinition.bBuffersContiguous = OMX_FALSE;
    mpeg4portdefinition.nBufferAlignment = 0;

    mpeg4port->SetPortDefinition(&mpeg4portdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_MPEG4TYPE */
    memset(&mpeg4portparam, 0, sizeof(mpeg4portparam));
    SetTypeHeader(&mpeg4portparam, sizeof(mpeg4portparam));
    mpeg4portparam.nPortIndex = port_index;
    mpeg4portparam.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
    mpeg4portparam.eLevel = OMX_VIDEO_MPEG4Level5;

    mpeg4port->SetPortMpeg4Param(&mpeg4portparam, true);
    /* end of OMX_VIDEO_PARAM_MPEG4TYPE */


   /* encoder */
    if (dir == OMX_DirOutput) {
        /* OMX_VIDEO_PARAM_BITRATETYPE */
        OMX_VIDEO_PARAM_BITRATETYPE bitrateparam;

        memset(&bitrateparam, 0, sizeof(bitrateparam));
        SetTypeHeader(&bitrateparam, sizeof(bitrateparam));

        bitrateparam.nPortIndex = port_index;
        bitrateparam.eControlRate = OMX_Video_ControlRateConstant;
        bitrateparam.nTargetBitrate = 64000;

        mpeg4port->SetPortBitrateParam(&bitrateparam, true);
        /* end of OMX_VIDEO_PARAM_BITRATETYPE */

        /* OMX_VIDEO_CONFIG_PRI_INFOTYPE */
        OMX_VIDEO_CONFIG_PRI_INFOTYPE privateinfoparam;

        memset(&privateinfoparam, 0, sizeof(privateinfoparam));
        SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));

        privateinfoparam.nPortIndex = port_index;
        privateinfoparam.nCapacity = 0;
        privateinfoparam.nHolder = NULL;

        mpeg4port->SetPortPrivateInfoParam(&privateinfoparam, true);
    }


    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXMPEG4Component::__AllocateRawPort(OMX_U32 port_index,
                                                  OMX_DIRTYPE dir)
{
    PortVideo *rawport;

    OMX_PARAM_PORTDEFINITIONTYPE rawportdefinition;
    OMX_VIDEO_PARAM_PORTFORMATTYPE rawvideoparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortVideo;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
             OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    rawport = static_cast<PortVideo *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&rawportdefinition, 0, sizeof(rawportdefinition));
    SetTypeHeader(&rawportdefinition, sizeof(rawportdefinition));
    rawportdefinition.nPortIndex = port_index;
    rawportdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        rawportdefinition.nBufferCountActual = INPORT_RAW_ACTUAL_BUFFER_COUNT;
        rawportdefinition.nBufferCountMin = INPORT_RAW_MIN_BUFFER_COUNT;
        rawportdefinition.nBufferSize = INPORT_RAW_BUFFER_SIZE;
    }
    else {
        rawportdefinition.nBufferCountActual = OUTPORT_RAW_ACTUAL_BUFFER_COUNT;
        rawportdefinition.nBufferCountMin = OUTPORT_RAW_MIN_BUFFER_COUNT;
        rawportdefinition.nBufferSize = OUTPORT_RAW_BUFFER_SIZE;
    }
    rawportdefinition.bEnabled = OMX_TRUE;
    rawportdefinition.bPopulated = OMX_FALSE;
    rawportdefinition.eDomain = OMX_PortDomainVideo;
    rawportdefinition.format.video.cMIMEType = (char *)"video/raw";
    rawportdefinition.format.video.pNativeRender = NULL;
    rawportdefinition.format.video.nFrameWidth = 176;
    rawportdefinition.format.video.nFrameHeight = 144;
    rawportdefinition.format.video.nStride = 176;
    rawportdefinition.format.video.nSliceHeight = 144;
    rawportdefinition.format.video.nBitrate = 64000;
    rawportdefinition.format.video.xFramerate = 15 << 16;
    rawportdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    rawportdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawportdefinition.format.video.eColorFormat =
        OMX_COLOR_FormatYUV420SemiPlanar;
    rawportdefinition.format.video.pNativeWindow = NULL;
    rawportdefinition.bBuffersContiguous = OMX_FALSE;
    rawportdefinition.nBufferAlignment = 0;

    rawport->SetPortDefinition(&rawportdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_PORTFORMATTYPE */
    rawvideoparam.nPortIndex = port_index;
    rawvideoparam.nIndex = 0;
    rawvideoparam.eCompressionFormat = OMX_VIDEO_CodingUnused;
    rawvideoparam.eColorFormat = OMX_COLOR_FormatYUV420SemiPlanar;

    rawport->SetPortVideoParam(&rawvideoparam, true);

    /* end of OMX_VIDEO_PARAM_PORTFORMATTYPE */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

/* end of core methods & helpers */

/*
 * component methods & helpers
 */
/* Get/SetParameter */
OMX_ERRORTYPE OMXMPEG4Component::ComponentGetParameter(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nParamIndex);

    switch (nParamIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortVideoParam(), sizeof(*p));

	LOGV("p->eColorFormat = %x\n", p->eColorFormat);

        break;
    }
    case OMX_IndexParamVideoMpeg4: {
        OMX_VIDEO_PARAM_MPEG4TYPE *p =
            (OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortMpeg4 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMpeg4 *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortMpeg4Param(), sizeof(*p));
        break;
    }
    case OMX_IndexParamVideoBitrate: {
        OMX_VIDEO_PARAM_BITRATETYPE *p =
            (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortBitrateParam(), sizeof(*p));
        break;
    }
     case OMX_IndexParamVideoProfileLevelQuerySupported:
     {
         OMX_VIDEO_PARAM_PROFILELEVELTYPE *p =
             (OMX_VIDEO_PARAM_PROFILELEVELTYPE *)pComponentParameterStructure;
        PortMpeg4 *port = NULL;

        OMX_U32 index = p->nPortIndex;
    
        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) 
        {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
        {
            port = static_cast<PortMpeg4 *>(ports[index]);
        }
        else
        {
            return OMX_ErrorBadParameter;
        }

        const OMX_VIDEO_PARAM_MPEG4TYPE *mpeg4Param = port->GetPortMpeg4Param();

        p->eProfile = mpeg4Param->eProfile;
        p->eLevel  = mpeg4Param->eLevel;

        break;
     }

#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
#ifdef COMPONENT_SUPPORT_OPENCORE
    case OMX_IndexIntelPrivateInfo: {
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortPrivateInfoParam(), sizeof(*p));
        break;
    }
#endif
#endif
#ifdef COMPONENT_SUPPORT_OPENCORE
    /* PVOpenCore */
    case (OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX: {
        PV_OMXComponentCapabilityFlagsType *p =
            (PV_OMXComponentCapabilityFlagsType *)pComponentParameterStructure;

        p->iIsOMXComponentMultiThreaded = OMX_TRUE;
        p->iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
        p->iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;
        p->iOMXComponentSupportsPartialFrames = OMX_FALSE;
        p->iOMXComponentCanHandleIncompleteFrames = OMX_FALSE;

        p->iOMXComponentUsesNALStartCodes = OMX_FALSE;
        p->iOMXComponentUsesFullAVCFrames = OMX_FALSE;

        break;
    }
#endif
    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXMPEG4Component::ComponentSetParameter(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentParameterStructure)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter (index = 0x%08x)\n", __func__, nIndex);

    switch (nIndex) {
    case OMX_IndexParamVideoPortFormat: {
        OMX_VIDEO_PARAM_PORTFORMATTYPE *p =
            (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortVideoParam(p, false);
        break;
    }
   case OMX_IndexParamVideoMpeg4: {
        OMX_VIDEO_PARAM_MPEG4TYPE *p =
            (OMX_VIDEO_PARAM_MPEG4TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortMpeg4 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortMpeg4 *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        if (port->IsEnabled()) {
            OMX_STATETYPE state;

            CBaseGetState((void *)GetComponentHandle(), &state);
            if (state != OMX_StateLoaded &&
                state != OMX_StateWaitForResources) {
                LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                     OMX_ErrorIncorrectStateOperation);
                return OMX_ErrorIncorrectStateOperation;
            }
        }

        ret = port->SetPortMpeg4Param(p, false);
        break;
    }
    case OMX_IndexParamVideoBitrate: {
        OMX_VIDEO_PARAM_BITRATETYPE *p =
            (OMX_VIDEO_PARAM_BITRATETYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
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

        ret = port->SetPortBitrateParam(p, false);

        break;
    }
    case OMX_IndexIntelPrivateInfo: {
        LOGV("OMX_IndexIntelPrivateInfo");
        OMX_VIDEO_CONFIG_PRI_INFOTYPE *p =
            (OMX_VIDEO_CONFIG_PRI_INFOTYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortVideo *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortVideo *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
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

        ret = port->SetPortPrivateInfoParam(p, false);
        break;
    }
    case OMX_IndexParamVideoBytestream: {
		OMX_VIDEO_PARAM_BYTESTREAMTYPE *p =
				(OMX_VIDEO_PARAM_BYTESTREAMTYPE *) pComponentParameterStructure;
		OMX_U32 index = p->nPortIndex;
		PortVideo *port = NULL;

		ret = CheckTypeHeader(p, sizeof(*p));
		if (ret != OMX_ErrorNone) {
			LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
			return ret;
		}

		if (index < nr_ports)
			port = static_cast<PortVideo *> (ports[index]);

		if (!port) {
			LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
					OMX_ErrorBadPortIndex);
			return OMX_ErrorBadPortIndex;
		}

		if (port->IsEnabled()) {
			OMX_STATETYPE state;
			CBaseGetState((void *) GetComponentHandle(), &state);
			if (state != OMX_StateLoaded && state != OMX_StateWaitForResources) {
				LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
						OMX_ErrorIncorrectStateOperation);
				return OMX_ErrorIncorrectStateOperation;
			}

		}
		break;
	}

    default:
        ret = OMX_ErrorUnsupportedIndex;
    } /* switch */

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* Get/SetConfig */
OMX_ERRORTYPE OMXMPEG4Component::ComponentGetConfig(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    ENTER_FUN;

    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    OMX_CONFIG_INTRAREFRESHVOPTYPE* pVideoIFrame;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval;
    OMX_CONFIG_FRAMERATETYPE *pFrameRate;

    RtCode aret;

    LOGV("%s() : nIndex = %d\n", __func__, nIndex);

    switch (nIndex)
    {
       case OMX_IndexConfigVideoAVCIntraPeriod:
       {
           pVideoIDRInterval = (OMX_VIDEO_CONFIG_AVCINTRAPERIOD *) pComponentConfigStructure;
           if(!pVideoIDRInterval) {
               LOGE("NULL pointer");
               return OMX_ErrorBadParameter;
           }

           ret = CheckTypeHeader(pVideoIDRInterval, sizeof(*pVideoIDRInterval));
           if (ret != OMX_ErrorNone) {
                LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
                return ret;
           }
           if (pVideoIDRInterval->nPortIndex != OUTPORT_INDEX)
           {
               LOGV("Wrong port index");
               return OMX_ErrorBadPortIndex;
           }
           OMX_U32 index = pVideoIDRInterval->nPortIndex;
           PortAvc *port = static_cast<PortAvc *>(ports[index]);

	   LOCK_CONFIG
           RtCode aret;
	   aret = encoder->getConfig(&config);
	   assert(aret == SUCCESS);

           pVideoIDRInterval->nIDRPeriod = config.iDRInterval;
           pVideoIDRInterval->nPFrames = config.intraInterval;
	   UNLOCK_CONFIG

           LOGV("OMX_IndexConfigVideoAVCIntraPeriod : nIDRPeriod = %d, nPFrames = %d", pVideoIDRInterval->nIDRPeriod, pVideoIDRInterval->nPFrames);

           SetTypeHeader(pVideoIDRInterval, sizeof(OMX_VIDEO_CONFIG_AVCINTRAPERIOD));

           break;
       }
       case OMX_IndexConfigVideoFramerate:
       {
            pFrameRate = (OMX_CONFIG_FRAMERATETYPE*)pComponentConfigStructure;
            if(!pFrameRate)
            {
                LOGE("pFrameRate NULL pointer");   
                return OMX_ErrorBadParameter;
            }
          
            OMX_U32 index = pFrameRate->nPortIndex;
          
            if (pFrameRate->nPortIndex != OUTPORT_INDEX)
            {
                LOGE("Wrong port index");
                return OMX_ErrorBadPortIndex;
            }
    
            LOCK_CONFIG
            RtCode aret;
            aret = encoder->getConfig(&config);
            assert(aret == SUCCESS);

            pFrameRate->xEncodeFramerate = config.frameRate;

            UNLOCK_CONFIG
           
            LOGE("OMX_IndexConfigVideoFramerate !xEncodeFramerate =%d ", pFrameRate->xEncodeFramerate);    
            break;
        }
        default:
        {
            return OMX_ErrorUnsupportedIndex;
        }
    }
    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXMPEG4Component::ComponentSetConfig(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentConfigStructure)
{
    ENTER_FUN;
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    OMX_CONFIG_INTRAREFRESHVOPTYPE* pVideoIFrame;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval;
    OMX_CONFIG_FRAMERATETYPE *pFrameRate;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s() : nIndex = %d\n", __func__, nParamIndex);

    switch (nParamIndex)
    {
        case OMX_IndexConfigVideoIntraVOPRefresh:
        {
            LOGV("OMX_IndexConfigVideoIntraVOPRefresh");

            pVideoIFrame = (OMX_CONFIG_INTRAREFRESHVOPTYPE*) pComponentConfigStructure;
            ret = CheckTypeHeader(pVideoIFrame, sizeof(*pVideoIFrame));
            if (ret != OMX_ErrorNone) {
                 LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
                 return ret;
            }
            if (pVideoIFrame->nPortIndex != OUTPORT_INDEX)
            {
                LOGV("Wrong port index");
                return OMX_ErrorBadPortIndex;
            }

            if(pVideoIFrame->IntraRefreshVOP == OMX_TRUE)
            {
                 LOGV("pVideoIFrame->IntraRefreshVOP == OMX_TRUE");

		 LOCK_CONFIG;
	         encoder->setKeyFrame();
	         UNLOCK_CONFIG;
            }
          
            break;
        }
        case OMX_IndexConfigVideoFramerate:
        {
             pFrameRate = (OMX_CONFIG_FRAMERATETYPE*)pComponentConfigStructure;
             if(!pFrameRate)
             {
                 LOGE("pFrameRate NULL pointer");   
                 return OMX_ErrorBadParameter;
             }
           
             OMX_U32 index = pFrameRate->nPortIndex;
           
             if (pFrameRate->nPortIndex != OUTPORT_INDEX)
             {
                 LOGE("Wrong port index");
                 return OMX_ErrorBadPortIndex;
             }

             LOCK_CONFIG
             RtCode aret;
             
             aret = encoder->getConfig(&config);
             assert(aret == SUCCESS);

             config.frameRate = pFrameRate->xEncodeFramerate;
          
             aret = encoder->setDynamicConfig(config);
 
             assert(aret == SUCCESS);
            
             UNLOCK_CONFIG
           
             LOGE("OMX_IndexConfigVideoFramerate ,xEncodeFramerate =%d ", pFrameRate->xEncodeFramerate);    
             break;
        }
        default:
        {
            return OMX_ErrorUnsupportedIndex;
        }
    }
    EXIT_FUN;
    return OMX_ErrorNone;
}


/*
 * implement ComponentBase::Processor[*]
 */
#define DUMP_VALUE(v) LOGV("%s = %d (0x%x)", #v, (unsigned int)(v),(unsigned int)(v))
OMX_ERRORTYPE OMXMPEG4Component::ProcessorInit(void)
{
    ENTER_FUN;

    RtCode aret =  SUCCESS;
    OMX_ERRORTYPE oret = OMX_ErrorNone;

    OMX_U32 port_index = (OMX_U32)-1;

    encoder = new MPEG4Encoder();

    assert(encoder != NULL);

    LOCK_CONFIG;
    aret = encoder->getConfig(&config);
    assert(aret== SUCCESS);

    port_index = OUTPORT_INDEX;
	
    oret = ChangeMPEG4EncoderConfigWithPortParam(&config,
		    static_cast<PortVideo*>(ports[port_index])
		    );

	    
    aret = encoder->setConfig(config);
    assert(aret==SUCCESS);

    aret = encoder->getConfig(&config);
    assert(aret==SUCCESS);

    LOGV("Effective MPEG4Encoder Configuration ************");
    DUMP_VALUE(config.frameWidth);
    DUMP_VALUE(config.frameHeight);
    DUMP_VALUE(config.frameBitrate);
    DUMP_VALUE(config.intraInterval);
    DUMP_VALUE(config.iDRInterval);
    DUMP_VALUE(config.initialQp);
    DUMP_VALUE(config.minimalQp);
    DUMP_VALUE(config.rateControl);
    DUMP_VALUE(config.naluFormat);
    DUMP_VALUE(config.levelIDC);
    DUMP_VALUE(config.bShareBufferMode);
    DUMP_VALUE(config.nSlice);

    UNLOCK_CONFIG

    inframe_counter = 0;
    outframe_counter = 0;

    last_ts = 0;
    last_fps = 0.0;

#ifdef INFODUMP
    libinfodump_init();
    counter = 0;
#endif
    
    temp_coded_data_buffer_size = config.frameWidth * config.frameHeight * 2;
    temp_coded_data_buffer = new OMX_U8 [temp_coded_data_buffer_size];

    encoder->setObserver(this);
    encoder->enableTimingStatistics();
		
    aret = encoder->init();
    assert(aret==SUCCESS);

    encoder->resetStatistics(NULL);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, oret);
    return oret;
}

OMX_ERRORTYPE OMXMPEG4Component::ProcessorDeinit(void)
{
    ENTER_FUN;
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    RtCode aret;

    //----------------------------------------------
    encoder->resetStatistics(&statistics);
    LOGV("Codec Statistics ********************************************");
		
    LOGV("time eclapsed = %f (ms)", statistics.timeEclapsed);		
    LOGV("#frames [encoded = %d|skipped = %d|total = %d]", statistics.frameEncoded,
				statistics.frameSkipped,
				statistics.frameTotal);
    LOGV("#I frames (%d IDR) = %d | #P frames = %d | #B frames %d", statistics.frameI + statistics.frameIDR,
				statistics.frameIDR,
				statistics.frameP,
				statistics.frameB);
    LOGV("average encoding time = %f (ms) - fps = %f", statistics.averEncode, 1000/statistics.averEncode);
    LOGV("average frame size = %f (Bytes) - bitrate = %f(Kbps)", statistics.averFrameSize, 
				statistics.averFrameSize * config.frameRate * 8 / 1024);
    LOGV("libva related details :");
    LOGV("average surface load time = %f (ms)", statistics.averSurfaceLoad);
    LOGV("average surface copy output time = %f (ms)", statistics.averCopyOutput);
    LOGV("average surface sync time = %f (ms)", statistics.averSurfaceSync);
    LOGV("average pure encoding time = %f (ms) - fps = %f", statistics.averEncode - 
				statistics.averSurfaceLoad,
				1000/(statistics.averEncode - statistics.averSurfaceLoad));
    //----------------------------------------------
    
    aret = encoder->deinit();
    assert(aret==SUCCESS);

    delete encoder;

    if (temp_coded_data_buffer != NULL) {
	    delete [] temp_coded_data_buffer;
    };

#ifdef INFODUMP
    libinfodump_destroy();
#endif

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXMPEG4Component::ProcessorStart(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXMPEG4Component::ProcessorStop(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    ports[INPORT_INDEX]->ReturnAllRetainedBuffers();

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXMPEG4Component::ProcessorPause(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXMPEG4Component::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::ProcessorProcess */
OMX_ERRORTYPE OMXMPEG4Component::ProcessorProcess(
    OMX_BUFFERHEADERTYPE **buffers,
    buffer_retain_t *retain,
    OMX_U32 nr_buffers)
{
    OMX_U32 outfilledlen = 0;
    OMX_S64 outtimestamp = 0;
    OMX_U32 outflags = 0;

    OMX_ERRORTYPE oret = OMX_ErrorNone;
    RtCode aret = SUCCESS;

    //intput buffer/output buffer
    void* buffer_in;
    int buffer_in_datasz;
    int buffer_in_buffersz;
    void* buffer_out;
    int buffer_out_datasz;
    int buffer_out_buffersz;

    LOGV("%s(): enter\n", __func__);

    LOGV("Dump Buffer Param (ProcessorInit) ***************************");
    LOGV("Input Buffer = %p, alloc = %d, offset = %d, filled = %d", 
		    buffers[INPORT_INDEX]->pBuffer,
		    buffers[INPORT_INDEX]->nAllocLen,
		    buffers[INPORT_INDEX]->nOffset,
		    buffers[INPORT_INDEX]->nFilledLen);    
    LOGV("Output Buffer = %p, alloc = %d, offset = %d, filled = %d", 
		    buffers[OUTPORT_INDEX]->pBuffer,
		    buffers[OUTPORT_INDEX]->nAllocLen,
		    buffers[OUTPORT_INDEX]->nOffset,
		    buffers[OUTPORT_INDEX]->nFilledLen);

    LOGV_IF(buffers[INPORT_INDEX]->nFlags & OMX_BUFFERFLAG_EOS,
            "%s(),%d: got OMX_BUFFERFLAG_EOS\n", __func__, __LINE__);

    if (!buffers[INPORT_INDEX]->nFilledLen) {
        LOGV("%s(),%d: input buffer's nFilledLen is zero\n",
             __func__, __LINE__);
        goto out;
    }

    buffer_in          = buffers[INPORT_INDEX]->pBuffer + buffers[INPORT_INDEX]->nOffset;
    buffer_in_datasz   = buffers[INPORT_INDEX]->nFilledLen;
    buffer_in_buffersz = buffers[INPORT_INDEX]->nAllocLen - buffers[INPORT_INDEX]->nOffset;

    buffer_out         = buffers[OUTPORT_INDEX]->pBuffer + buffers[OUTPORT_INDEX]->nOffset;
    buffer_out_datasz  = 0;
    buffer_out_buffersz = buffers[OUTPORT_INDEX]->nAllocLen - buffers[OUTPORT_INDEX]->nOffset;

    LOGV("INPUT TIMESTAMP = %ld", buffers[INPORT_INDEX]->nTimeStamp);


encode:
    assert(p->iOMXComponentUsesNALStartCodes == OMX_FALSE);
    assert(p->iOMXComponentUsesFullAVCFrames == OMX_FALSE);


    in.len = static_cast<int>(buffer_in_datasz);	
    out.len = static_cast<int>(buffer_out_buffersz);	
    in.buf = (unsigned char*)(buffer_in);
   out.buf = (unsigned char*)(buffer_out);
		    
    in.timestamp = buffers[INPORT_INDEX]->nTimeStamp;
    in.bCompressed = false;

    LOCK_CONFIG

    aret = encoder->encode(&in, &out);
    assert(aret==SUCCESS);

    UNLOCK_CONFIG

    outfilledlen = out.len;
    outtimestamp = static_cast<OMX_S64>(out.timestamp);
    if (outfilledlen >0)
    {
	outflags |= OMX_BUFFERFLAG_ENDOFFRAME;
	retain[OUTPORT_INDEX] = BUFFER_RETAIN_NOT_RETAIN;
    }
    else
    {
        retain[OUTPORT_INDEX] = BUFFER_RETAIN_GETAGAIN;
    }
   
    retain[INPORT_INDEX] = BUFFER_RETAIN_ACCUMULATE;



out:
    if(retain[OUTPORT_INDEX] != BUFFER_RETAIN_GETAGAIN) {
        buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
        buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;
        buffers[OUTPORT_INDEX]->nFlags = outflags;
    
	LOGV("[GENERATE] output buffer (len = %d, timestamp = %ld, flag = %x", 
		    outfilledlen,
		    outtimestamp,
		    outflags);
    }

    if (retain[INPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN ||
        retain[INPORT_INDEX] == BUFFER_RETAIN_ACCUMULATE ) {
        inframe_counter++;
    }

    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
        outframe_counter++;

    LOGV_IF(oret == OMX_ErrorNone,
            "%s(),%d: exit, done\n", __func__, __LINE__);

    return oret;
}


OMX_ERRORTYPE OMXMPEG4Component::ChangeMPEG4EncoderConfigWithPortParam(
		CodecConfig* pconfig,
		PortVideo *port) 
{

    ENTER_FUN;
    OMX_ERRORTYPE ret = OMX_ErrorNone;
    OMX_VIDEO_CONTROLRATETYPE controlrate;

    const OMX_PARAM_PORTDEFINITIONTYPE *pd = port->GetPortDefinition();
    const OMX_VIDEO_PARAM_BITRATETYPE *bitrate = port->GetPortBitrateParam();

    pconfig->frameWidth = static_cast<int>(pd->format.video.nFrameWidth);
    pconfig->frameHeight = static_cast<int>(pd->format.video.nFrameHeight);
    pconfig->frameRate = static_cast<int>((pd->format.video.xFramerate >> 16));
        
    pconfig->intraInterval = static_cast<int>(pconfig->frameRate / 2);	//just a init value
    pconfig->iDRInterval = -1;						//for opencore, will only accept -1 as iDRInterval

    pconfig->frameBitrate = bitrate->nTargetBitrate;
    
    if ((bitrate->eControlRate == OMX_Video_ControlRateVariable) ||
		    (bitrate->eControlRate == OMX_Video_ControlRateVariableSkipFrames))
    {
	 pconfig->rateControl = RC_VBR;
    }
    else if ((bitrate->eControlRate == OMX_Video_ControlRateConstant) ||
                     (bitrate->eControlRate == OMX_Video_ControlRateConstantSkipFrames)) 
    {
	 pconfig->rateControl = RC_CBR;
    } else
    {
	 pconfig->rateControl = RC_NONE;
    }

    pconfig->nSlice = 1;	//just hard coded for init value, could be adjusted later
    pconfig->initialQp = 15;
    pconfig->minimalQp = 1;

    pconfig->rateControl = RC_VBR;	//hard code as VBR

    const OMX_BOOL *isbuffersharing = port->GetPortBufferSharingInfo();
    const OMX_VIDEO_CONFIG_PRI_INFOTYPE *privateinfoparam = port->GetPortPrivateInfoParam();

    temp_port_private_info = NULL;
    
    if(*isbuffersharing == OMX_TRUE) 
    {
	LOGV("Enable buffer sharing mode");
	if(privateinfoparam->nHolder != NULL)
        {
             int size = static_cast<int>(privateinfoparam->nCapacity);
             unsigned long * p = static_cast<unsigned long*>(privateinfoparam->nHolder);

             int i = 0;
             for(i = 0; i < size; i++)
	     {
                  LOGV("@@@@@ nCapacity = %d, camera frame id array[%d] = 0x%08x @@@@@", size, i, p[i]);
	     }
		
	     temp_port_private_info = const_cast<OMX_VIDEO_CONFIG_PRI_INFOTYPE*>(privateinfoparam);

	     pconfig->bShareBufferMode = true;
	     pconfig->nShareBufferCount = size;
	     pconfig->pShareBufferArray = p;

             //FIXME: CAUTION: free(privateinfoparam->nHolder);  ->  carried out in MPEG4Encoder's handleConfigChange() callback

            }
        } 
        else 
       {
	    	LOGV("Disable buffer sharing mode");
		pconfig->bShareBufferMode = false;
       }

   EXIT_FUN;
   
    return ret;
}

OMX_ERRORTYPE OMXMPEG4Component::ProcessorFlush(OMX_U32 port_index)
{
    LOGV("port_index = %d Flushed!\n", port_index);
	
    if (port_index == INPORT_INDEX || port_index == OMX_ALL) 
    {
           ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
    }

    return OMX_ErrorNone;
}

void OMXMPEG4Component::handleEncoderCycleEnded(VASurfaceID surface,
					bool b_generated,
					bool b_share_buffer)
{
	LOGV("surface id =%x is released by encoder", surface);
	ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
}

void OMXMPEG4Component::handleConfigChange(bool b_dynamic) 
{
    if (b_dynamic == false) 
    {
        LOGV("avc encoder static config changed");

        if (temp_port_private_info != NULL) 
        {
	    free(temp_port_private_info->nHolder);
        }
   }
   else
   {
	LOGV("avc encoder dynamic config changed");
   }
}
/*
 * CModule Interface
 */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB.ve.mpeg4.libva";

static const char *g_roles[] =
{
    (const char *)"video_encoder.mpeg4",
};

OMX_ERRORTYPE wrs_omxil_cmodule_ops_instantiate(OMX_PTR *instance)
{
    ComponentBase *cbase;

    cbase = new OMXMPEG4Component;

    if (!cbase)
    {
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

