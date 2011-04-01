/* ------------------------------------------------------------------
 * Copyright (C) 1998-2009 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
/*
 * Portions Copyright 2011, Intel Corportion
 */

#ifndef __OMXIL_INTEL_H263__
#define __OMXIL_INTEL_H263__

typedef int int32;
typedef unsigned int uint32;
typedef unsigned char uint8;
typedef unsigned short uint16;

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
