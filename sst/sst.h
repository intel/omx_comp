#ifndef __OMX_INTEL_MRST_SST
#define __OMX_INTEL_MRST_SST

#include <OMX_Core.h>
#include <OMX_Component.h>

#include <cmodule.h>
#include <portbase.h>
#include <componentbase.h>

class MrstSstComponent : public ComponentBase
{
public:
    /*
     * constructor & destructor
     */
    MrstSstComponent();
    ~MrstSstComponent();

private:
    /*
     * component methods & helpers
     */
    /* implement ComponentBase::ComponentAllocatePorts */
    virtual OMX_ERRORTYPE ComponentAllocatePorts(void);

    /* implement ComponentBase::ComponentGet/SetPatameter */
    virtual OMX_ERRORTYPE
        ComponentGetParameter(OMX_INDEXTYPE nParamIndex,
                              OMX_PTR pComponentParameterStructure);
    virtual OMX_ERRORTYPE
        ComponentSetParameter(OMX_INDEXTYPE nIndex,
                              OMX_PTR pComponentParameterStructure);

    /* implement ComponentBase::ComponentGet/SetConfig */
    virtual OMX_ERRORTYPE
        ComponentGetConfig(OMX_INDEXTYPE nIndex,
                           OMX_PTR pComponentConfigStructure);
    virtual OMX_ERRORTYPE
        ComponentSetConfig(OMX_INDEXTYPE nIndex,
                           OMX_PTR pComponentConfigStructure);

    /* implement ComponentBase::Processor[*] */
    virtual OMX_ERRORTYPE ProcessorInit(void);  /* Loaded to Idle */
    virtual OMX_ERRORTYPE ProcessorDeinit(void);/* Idle to Loaded */
    virtual OMX_ERRORTYPE ProcessorStart(void); /* Idle to Executing/Pause */
    virtual OMX_ERRORTYPE ProcessorStop(void);  /* Executing/Pause to Idle */
    virtual OMX_ERRORTYPE ProcessorPause(void); /* Executing to Pause */
    virtual OMX_ERRORTYPE ProcessorResume(void);/* Pause to Executing */
    virtual void ProcessorProcess(OMX_BUFFERHEADERTYPE **buffers,
                                  bool *retain,
                                  OMX_U32 nr_buffers);

    OMX_ERRORTYPE __AllocateMp3Port(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocatePcmPort(OMX_U32 port_index, OMX_DIRTYPE dir);
    OMX_ERRORTYPE __AllocateMp3RolePorts(bool isencoder);

    /* end of component methods & helpers */

    /* constant */
    /* ports */
    const static OMX_U32 NR_PORTS = 2;
    const static OMX_U32 INPORT_INDEX = 0;
    const static OMX_U32 OUTPORT_INDEX = 1;

    /* buffer */
    const static OMX_U32 INPORT_MP3_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 INPORT_MP3_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_MP3_BUFFER_SIZE = 1024;
    const static OMX_U32 OUTPORT_MP3_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_MP3_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_MP3_BUFFER_SIZE = 1024;
    const static OMX_U32 INPORT_PCM_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 INPORT_PCM_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 INPORT_PCM_BUFFER_SIZE = 1024;
    const static OMX_U32 OUTPORT_PCM_ACTUAL_BUFFER_COUNT = 2;
    const static OMX_U32 OUTPORT_PCM_MIN_BUFFER_COUNT = 1;
    const static OMX_U32 OUTPORT_PCM_BUFFER_SIZE = 1024;
};

/*
 * CModule Interface
 */
#ifdef __cplusplus
extern "C" {
#endif

OMX_ERRORTYPE omx_component_module_instantiate(OMX_PTR *);
OMX_ERRORTYPE omx_component_module_query_name(OMX_STRING, OMX_U32);
OMX_ERRORTYPE omx_component_module_query_roles(OMX_U32 *, OMX_U8 **);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __OMX_INTEL_MRST_SST */
