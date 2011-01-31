#define LOG_TAG "wrs-omx-stub-ve-h263-libva "
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

//#include <pv_omxcore.h>
//#include <pv_omxdefs.h>

#include <va/va.h>
#include <va/va_android.h>
#include "vabuffer.h"
#include "libinfodump.h"


#include "h263.h"
#include "OMXH263Component.h"



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


OMXH263Component::OMXH263Component():
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

OMXH263Component::~OMXH263Component()
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
OMX_ERRORTYPE OMXH263Component::ComponentAllocatePorts(void)
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

    ret = __AllocateH263Port(codec_port_index, codec_port_dir);
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

OMX_ERRORTYPE OMXH263Component::__AllocateH263Port(OMX_U32 port_index,
                                                    OMX_DIRTYPE dir)
{
    ENTER_FUN;
    PortH263 *h263port;

    OMX_PARAM_PORTDEFINITIONTYPE h263portdefinition;
    OMX_VIDEO_PARAM_H263TYPE h263portparam;

    LOGV("%s(): enter\n", __func__);

    ports[port_index] = new PortH263;
    if (!ports[port_index]) {
        LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                OMX_ErrorInsufficientResources);
        return OMX_ErrorInsufficientResources;
    }

    h263port = static_cast<PortH263 *>(this->ports[port_index]);

    /* OMX_PARAM_PORTDEFINITIONTYPE */
    memset(&h263portdefinition, 0, sizeof(h263portdefinition));
    SetTypeHeader(&h263portdefinition, sizeof(h263portdefinition));
    h263portdefinition.nPortIndex = port_index;
    h263portdefinition.eDir = dir;
    if (dir == OMX_DirInput) {
        h263portdefinition.nBufferCountActual =
            INPORT_H263_ACTUAL_BUFFER_COUNT;
        h263portdefinition.nBufferCountMin = INPORT_H263_MIN_BUFFER_COUNT;
        h263portdefinition.nBufferSize = INPORT_H263_BUFFER_SIZE;
    } else {
        h263portdefinition.nBufferCountActual =
            OUTPORT_H263_ACTUAL_BUFFER_COUNT;
        h263portdefinition.nBufferCountMin = OUTPORT_H263_MIN_BUFFER_COUNT;
        h263portdefinition.nBufferSize = OUTPORT_H263_BUFFER_SIZE;
    }
    h263portdefinition.bEnabled = OMX_TRUE;
    h263portdefinition.bPopulated = OMX_FALSE;
    h263portdefinition.eDomain = OMX_PortDomainVideo;
    h263portdefinition.format.video.cMIMEType = (OMX_STRING)"video/h263";
    h263portdefinition.format.video.pNativeRender = NULL;
    h263portdefinition.format.video.nFrameWidth = 176;
    h263portdefinition.format.video.nFrameHeight = 144;
    h263portdefinition.format.video.nStride = 0;
    h263portdefinition.format.video.nSliceHeight = 0;
    h263portdefinition.format.video.nBitrate = 64000;
    h263portdefinition.format.video.xFramerate = 15 << 16;
    h263portdefinition.format.video.bFlagErrorConcealment = OMX_FALSE;
    h263portdefinition.format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
    h263portdefinition.format.video.eColorFormat = OMX_COLOR_FormatUnused;
    h263portdefinition.format.video.pNativeWindow = NULL;
    h263portdefinition.bBuffersContiguous = OMX_FALSE;
    h263portdefinition.nBufferAlignment = 0;

    h263port->SetPortDefinition(&h263portdefinition, true);

    /* end of OMX_PARAM_PORTDEFINITIONTYPE */

    /* OMX_VIDEO_PARAM_H263TYPE */
    memset(&h263portparam, 0, sizeof(h263portparam));
    SetTypeHeader(&h263portparam, sizeof(h263portparam));
    h263portparam.nPortIndex = port_index;
    h263portparam.eProfile = OMX_VIDEO_H263ProfileVendorStartUnused;
    h263portparam.eLevel = OMX_VIDEO_H263LevelVendorStartUnused;

    h263port->SetPortH263Param(&h263portparam, true);

    /* end of OMX_VIDEO_PARAM_H263TYPE */

    /* encoder */
    if (dir == OMX_DirOutput)
   {
        /* OMX_VIDEO_PARAM_BITRATETYPE */
        OMX_VIDEO_PARAM_BITRATETYPE bitrateparam;

        memset(&bitrateparam, 0, sizeof(bitrateparam));
        SetTypeHeader(&bitrateparam, sizeof(bitrateparam));

        bitrateparam.nPortIndex = port_index;
        bitrateparam.eControlRate = OMX_Video_ControlRateConstant;
        bitrateparam.nTargetBitrate = 64000;

        h263port->SetPortBitrateParam(&bitrateparam, true);
        /* end of OMX_VIDEO_PARAM_BITRATETYPE */

        /* OMX_VIDEO_CONFIG_PRI_INFOTYPE */
        OMX_VIDEO_CONFIG_PRI_INFOTYPE privateinfoparam;

        memset(&privateinfoparam, 0, sizeof(privateinfoparam));
        SetTypeHeader(&privateinfoparam, sizeof(privateinfoparam));

        privateinfoparam.nPortIndex = port_index;
        privateinfoparam.nCapacity = 0;
        privateinfoparam.nHolder = NULL;

        h263port->SetPortPrivateInfoParam(&privateinfoparam, true);
    }

    LOGV("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, OMX_ErrorNone);
    return OMX_ErrorNone;
}

OMX_ERRORTYPE OMXH263Component::__AllocateRawPort(OMX_U32 port_index,
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
OMX_ERRORTYPE OMXH263Component::ComponentGetParameter(
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
    case OMX_IndexParamVideoH263: {
        OMX_VIDEO_PARAM_H263TYPE *p =
            (OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortH263 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortH263 *>(ports[index]);

        if (!port) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__,
                 OMX_ErrorBadPortIndex);
            return OMX_ErrorBadPortIndex;
        }

        memcpy(p, port->GetPortH263Param(), sizeof(*p));
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
#ifdef COMPONENT_SUPPORT_BUFFER_SHARING
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
#if 0
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

OMX_ERRORTYPE OMXH263Component::ComponentSetParameter(
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
    case OMX_IndexParamVideoH263: {
        OMX_VIDEO_PARAM_H263TYPE *p =
            (OMX_VIDEO_PARAM_H263TYPE *)pComponentParameterStructure;
        OMX_U32 index = p->nPortIndex;
        PortH263 *port = NULL;

        LOGV("%s(): port index : %lu\n", __func__, index);

        ret = CheckTypeHeader(p, sizeof(*p));
        if (ret != OMX_ErrorNone) {
            LOGE("%s(),%d: exit (ret = 0x%08x)\n", __func__, __LINE__, ret);
            return ret;
        }

        if (index < nr_ports)
            port = static_cast<PortH263 *>(ports[index]);

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

        ret = port->SetPortH263Param(p, false);
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
OMX_ERRORTYPE OMXH263Component::ComponentGetConfig(
    OMX_INDEXTYPE nIndex,
    OMX_PTR pComponentConfigStructure)
{
    ENTER_FUN;

    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    OMX_CONFIG_INTRAREFRESHVOPTYPE* pVideoIFrame;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval;

    RtCode aret;

    LOGV("%s() : nIndex = %d\n", __func__, nIndex);

     switch (nIndex)
     {

        default:
        {
            return OMX_ErrorUnsupportedIndex;
        }
    }
    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OMXH263Component::ComponentSetConfig(
    OMX_INDEXTYPE nParamIndex,
    OMX_PTR pComponentConfigStructure)
{
    ENTER_FUN;
    OMX_ERRORTYPE ret = OMX_ErrorUnsupportedIndex;
    OMX_CONFIG_INTRAREFRESHVOPTYPE* pVideoIFrame;
    OMX_VIDEO_CONFIG_AVCINTRAPERIOD *pVideoIDRInterval;
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
OMX_ERRORTYPE OMXH263Component::ProcessorInit(void)
{
    ENTER_FUN;

    RtCode aret =  SUCCESS;
    OMX_ERRORTYPE oret = OMX_ErrorNone;

    OMX_U32 port_index = (OMX_U32)-1;

    encoder = new H263Encoder();

    assert(encoder != NULL);

    LOCK_CONFIG;
    aret = encoder->getConfig(&config);
    assert(aret== SUCCESS);

    port_index = OUTPORT_INDEX;
	
    oret = ChangeH263EncoderConfigWithPortParam(&config,
		    static_cast<PortVideo*>(ports[port_index])
		    );

	    
    aret = encoder->setConfig(config);
    assert(aret==SUCCESS);

    aret = encoder->getConfig(&config);
    assert(aret==SUCCESS);

    LOGV("Effective H263Encoder Configuration ************");
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

OMX_ERRORTYPE OMXH263Component::ProcessorDeinit(void)
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

OMX_ERRORTYPE OMXH263Component::ProcessorStart(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXH263Component::ProcessorStop(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    ports[INPORT_INDEX]->ReturnAllRetainedBuffers();

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXH263Component::ProcessorPause(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

OMX_ERRORTYPE OMXH263Component::ProcessorResume(void)
{
    OMX_ERRORTYPE ret = OMX_ErrorNone;

    LOGV("%s(): enter\n", __func__);

    LOGV("%s(),%d: exit (ret:0x%08x)\n", __func__, __LINE__, ret);
    return ret;
}

/* implement ComponentBase::ProcessorProcess */
OMX_ERRORTYPE OMXH263Component::ProcessorProcess(
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
    if(retain[OUTPORT_INDEX] != BUFFER_RETAIN_GETAGAIN)
    {
        buffers[OUTPORT_INDEX]->nFilledLen = outfilledlen;
        buffers[OUTPORT_INDEX]->nTimeStamp = outtimestamp;
        buffers[OUTPORT_INDEX]->nFlags = outflags;
    
	LOGV("[GENERATE] output buffer (len = %d, timestamp = %ld, flag = %x", 
		    outfilledlen, outtimestamp,  outflags);
    }

    if (retain[INPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN ||
        retain[INPORT_INDEX] == BUFFER_RETAIN_ACCUMULATE )
    {
        inframe_counter++;
    }

    if (retain[OUTPORT_INDEX] == BUFFER_RETAIN_NOT_RETAIN)
        outframe_counter++;

    LOGV_IF(oret == OMX_ErrorNone,
            "%s(),%d: exit, done\n", __func__, __LINE__);

    return oret;
}


OMX_ERRORTYPE OMXH263Component::ChangeH263EncoderConfigWithPortParam(
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
    pconfig->initialQp = 1;
    pconfig->minimalQp = 1;

    pconfig->rateControl = RC_CBR;	//hard code as VBR

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

OMX_ERRORTYPE OMXH263Component::ProcessorFlush(OMX_U32 port_index)
{
    LOGV("port_index = %d Flushed!\n", port_index);
	
    if (port_index == INPORT_INDEX || port_index == OMX_ALL) 
    {
           ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
    }

    return OMX_ErrorNone;
}

void OMXH263Component::handleEncoderCycleEnded(VASurfaceID surface,
					bool b_generated,
					bool b_share_buffer)
{
	LOGV("surface id =%x is released by encoder", surface);
	ports[INPORT_INDEX]->ReturnAllRetainedBuffers();
}

void OMXH263Component::handleConfigChange(bool b_dynamic) 
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

static const char *g_name = (const char *)"OMX.Intel.Mrst.PSB.ve.h263.libva";

static const char *g_roles[] =
{
    (const char *)"video_encoder.h263",
};

OMX_ERRORTYPE wrs_omxil_cmodule_ops_instantiate(OMX_PTR *instance)
{
    ComponentBase *cbase;

    cbase = new OMXH263Component;

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

