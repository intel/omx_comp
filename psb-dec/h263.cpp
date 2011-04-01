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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <OMX_Core.h>

//#include "oscl_base.h"
//#include "oscl_types.h"

#include "h263.h"

OMX_BOOL H263HeaderParser::DecodeH263Header(OMX_U8* aInputBuffer, int32 *width,
        int32 *height, int32 *display_width, int32 *display_height) {
    uint32 codeword;
    int32   extended_PTYPE = 0;
    int32 UFEP = 0;
    int32 custom_PFMT = 0;

    //Reset the data bit position to the start of the stream
    iH263DataBitPos = 0;
    iH263BitPos = 0;
    //BitBuf contains the first 4 bytes of the aInputBuffer
    iH263BitBuf = (aInputBuffer[0] << 24) | (aInputBuffer[1] << 16) | (aInputBuffer[2] << 8) | aInputBuffer[3];

    ReadBits(aInputBuffer, 22, &codeword);
    if (codeword !=  0x20)
    {
        return OMX_FALSE;
    }

    ReadBits(aInputBuffer, 8, &codeword);

    ReadBits(aInputBuffer, 1, &codeword);
    if (codeword == 0) return OMX_FALSE;

    ReadBits(aInputBuffer, 1, &codeword);
    if (codeword == 1) return OMX_FALSE;

    ReadBits(aInputBuffer, 1, &codeword);
    if (codeword == 1) return OMX_FALSE;

    ReadBits(aInputBuffer, 1, &codeword);
    if (codeword == 1) return OMX_FALSE;

    ReadBits(aInputBuffer, 1, &codeword);
    if (codeword == 1) return OMX_FALSE;

    /* source format */
    ReadBits(aInputBuffer, 3, &codeword);
    switch (codeword)
    {
    case 1:
        *width = 128;
        *height = 96;
        break;

    case 2:
        *width = 176;
        *height = 144;
        break;

    case 3:
        *width = 352;
        *height = 288;
        break;

    case 4:
        *width = 704;
        *height = 576;
        break;

    case 5:
        *width = 1408;
        *height = 1152;
        break;

    case 7:
        extended_PTYPE = 1;
        break;
    default:
        /* Msg("H.263 source format not legal\n"); */
        return OMX_FALSE;
    }

    if (extended_PTYPE == 0)
    {
        *display_width = *width;
        *display_height = *height;
        return OMX_TRUE;
    }

    /* source format */
    ReadBits(aInputBuffer, 3, &codeword);
    UFEP = codeword;
    if (UFEP == 1)
    {
        ReadBits(aInputBuffer, 3, &codeword);
        switch (codeword)
        {
        case 1:
            *width = 128;
            *height = 96;

            break;

        case 2:
            *width = 176;
            *height = 144;

            break;

        case 3:
            *width = 352;
            *height = 288;

            break;

        case 4:
            *width = 704;
            *height = 576;

            break;

        case 5:
            *width = 1408;
            *height = 1152;

            break;

        case 6:
            custom_PFMT = 1;
            break;
        default:
            /* Msg("H.263 source format not legal\n"); */
            return OMX_FALSE;
        }
        if (custom_PFMT == 0)
        {
            *display_width = *width;
            *display_height = *height;
            return OMX_TRUE;
        }


        ReadBits(aInputBuffer, 1, &codeword);
        ReadBits(aInputBuffer, 1, &codeword);
        if (codeword) return OMX_FALSE;
        ReadBits(aInputBuffer, 1, &codeword);
        if (codeword) return OMX_FALSE;
        ReadBits(aInputBuffer, 1, &codeword);
        if (codeword) return OMX_FALSE;
        ReadBits(aInputBuffer, 3, &codeword);
        ReadBits(aInputBuffer, 3, &codeword);
        if (codeword) return OMX_FALSE;             /* RPS, ISD, AIV */
        ReadBits(aInputBuffer, 1, &codeword);
        ReadBits(aInputBuffer, 4, &codeword);
        if (codeword != 8) return OMX_FALSE;
    }

    if (UFEP == 0 || UFEP == 1)
    {
        ReadBits(aInputBuffer, 3, &codeword);
        if (codeword > 1) return OMX_FALSE;
        ReadBits(aInputBuffer, 1, &codeword);
        if (codeword) return OMX_FALSE;
        ReadBits(aInputBuffer, 1, &codeword);
        if (codeword) return OMX_FALSE;
        ReadBits(aInputBuffer, 1, &codeword);
        ReadBits(aInputBuffer, 3, &codeword);
        if (codeword != 1) return OMX_FALSE;
    }
    else
    {
        return OMX_FALSE;
    }

    ReadBits(aInputBuffer, 1, &codeword);
    if (codeword) return OMX_FALSE; /* CPM */
    if (custom_PFMT == 1 && UFEP == 1)
    {
        OMX_U32 DisplayWidth, Width, DisplayHeight, Height, Resolution;

        ReadBits(aInputBuffer, 4, &codeword);
        if (codeword == 0) return OMX_FALSE;
        if (codeword == 0xf)
        {
            ReadBits(aInputBuffer, 8, &codeword);
            ReadBits(aInputBuffer, 8, &codeword);
        }
        ReadBits(aInputBuffer, 9, &codeword);
        DisplayWidth = (codeword + 1) << 2;
        Width = (DisplayWidth + 15) & -16;

        ReadBits(aInputBuffer, 1, &codeword);
        if (codeword != 1) return OMX_FALSE;
        ReadBits(aInputBuffer, 9, &codeword);
        if (codeword == 0) return OMX_FALSE;
        DisplayHeight = codeword << 2;
        Height = (DisplayHeight + 15) & -16;

        Resolution = Width * Height;

        *width = Width;
        *height = Height;
        *display_width = DisplayWidth;
        *display_height = DisplayHeight;
    }

    return OMX_TRUE;
}

static const uint32 mask[33] =
{
    0x00000000, 0x00000001, 0x00000003, 0x00000007,
    0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
    0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
    0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
    0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
    0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
    0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
    0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
    0xffffffff
};

void H263HeaderParser::ReadBits(OMX_U8* aStream,     /* Input Stream */
                                uint8 aNumBits,        /* nr of bits to read */
                                uint32* aOutData       /* output target */
                               )
{
    uint8 *bits;
    uint32 dataBitPos = iH263DataBitPos;
    uint32 bitPos = iH263BitPos;
    uint32 dataBytePos;

    if (aNumBits > (32 - bitPos))    /* not enough bits */
    {
        dataBytePos = dataBitPos >> 3;    /* Byte Aligned Position */
        bitPos = dataBitPos & 7; /* update bit position */
        bits = &aStream[dataBytePos];
        iH263BitBuf = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
    }

    iH263DataBitPos += aNumBits;
    iH263BitPos      = (unsigned char)(bitPos + aNumBits);

    *aOutData = (iH263BitBuf >> (32 - iH263BitPos)) & mask[(uint16)aNumBits];

    return;
}
