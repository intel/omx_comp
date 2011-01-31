#ifndef __OMXIL_INTEL_H263__
#define __OMXIL_INTEL_H263__

typedef long int32;
typedef unsigned char uint8;
typedef unsigned long uint32;

class H263HeaderParser {

private:	
    OMX_U32 iH263DataBitPos;
    OMX_U32 iH263BitPos;
    OMX_U32 iH263BitBuf;
	
public:
    
    OMX_BOOL DecodeH263Header(OMX_U8* aInputBuffer, int32 *width,
			int32 *height, int32 *display_width, int32 *display_height);	
    void ReadBits(OMX_U8* aStream, uint8 aNumBits, uint32* aOutData);
};

#endif /* __OMXIL_INTEL_H263__ */
