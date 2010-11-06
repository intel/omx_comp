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
//#define LOG_NDEBUG 0
#define LOG_TAG "avcsps"
#include <utils/Log.h>

#include <oscl_mem.h>
#include <avcsps.h>

typedef struct
{
    uint8 *data;
    uint32 numBytes;
    uint32 bytePos;
    uint32 bitBuf;
    uint32 dataBitPos;
    uint32  bitPos;
} mp4StreamType;

#define PV_CLZ(A,B) while (((B) & 0x8000) == 0) {(B) <<=1; A++;}

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



int16 ShowBits(
    mp4StreamType *pStream,           /* Input Stream */
    uint8 ucNBits,          /* nr of bits to read */
    uint32 *pulOutData      /* output target */
)
{
    uint8 *bits;
    uint32 dataBitPos = pStream->dataBitPos;
    uint32 bitPos = pStream->bitPos;
    uint32 dataBytePos;

    uint i;

    if (ucNBits > (32 - bitPos))    /* not enough bits */
    {
        dataBytePos = dataBitPos >> 3; /* Byte Aligned Position */
        bitPos = dataBitPos & 7; /* update bit position */
        if (dataBytePos > pStream->numBytes - 4)
        {
            pStream->bitBuf = 0;
            for (i = 0; i < pStream->numBytes - dataBytePos; i++)
            {
                pStream->bitBuf |= pStream->data[dataBytePos+i];
                pStream->bitBuf <<= 8;
            }
            pStream->bitBuf <<= 8 * (3 - i);
        }
        else
        {
            bits = &pStream->data[dataBytePos];
            pStream->bitBuf = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
        }
        pStream->bitPos = bitPos;
    }

    bitPos += ucNBits;

    *pulOutData = (pStream->bitBuf >> (32 - bitPos)) & mask[(uint16)ucNBits];


    return 0;
}

int16 FlushBits(
    mp4StreamType *pStream,           /* Input Stream */
    uint8 ucNBits                      /* number of bits to flush */
)
{
    uint8 *bits;
    uint32 dataBitPos = pStream->dataBitPos;
    uint32 bitPos = pStream->bitPos;
    uint32 dataBytePos;


    if ((dataBitPos + ucNBits) > (uint32)(pStream->numBytes << 3))
        return (-2); // Buffer over run

    dataBitPos += ucNBits;
    bitPos     += ucNBits;

    if (bitPos > 32)
    {
        dataBytePos = dataBitPos >> 3;    /* Byte Aligned Position */
        bitPos = dataBitPos & 7; /* update bit position */
        bits = &pStream->data[dataBytePos];
        pStream->bitBuf = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
    }

    pStream->dataBitPos = dataBitPos;
    pStream->bitPos     = bitPos;

    return 0;
}

int16 ReadBits(
    mp4StreamType *pStream,           /* Input Stream */
    uint8 ucNBits,                     /* nr of bits to read */
    uint32 *pulOutData                 /* output target */
)
{
    uint8 *bits;
    uint32 dataBitPos = pStream->dataBitPos;
    uint32 bitPos = pStream->bitPos;
    uint32 dataBytePos;


    if ((dataBitPos + ucNBits) > (pStream->numBytes << 3))
    {
        *pulOutData = 0;
        return (-2); // Buffer over run 
    }
    
    //  dataBitPos += ucNBits;

    if (ucNBits > (32 - bitPos))    /* not enough bits */
    {
        dataBytePos = dataBitPos >> 3;    /* Byte Aligned Position */
        bitPos = dataBitPos & 7; /* update bit position */
        bits = &pStream->data[dataBytePos];
        pStream->bitBuf = (bits[0] << 24) | (bits[1] << 16) | (bits[2] << 8) | bits[3];
    }

    pStream->dataBitPos += ucNBits;
    pStream->bitPos      = (unsigned char)(bitPos + ucNBits);

    *pulOutData = (pStream->bitBuf >> (32 - pStream->bitPos)) & mask[(uint16)ucNBits];

    return 0;
}


void ue_v(mp4StreamType *psBits, uint32 *codeNum)
{
    uint32 temp;
    uint tmp_cnt;
    int32 leading_zeros = 0;
    ShowBits(psBits, 16, &temp);

    tmp_cnt = temp  | 0x1;

    PV_CLZ(leading_zeros, tmp_cnt)

    if (leading_zeros < 8)
    {
        *codeNum = (temp >> (15 - (leading_zeros << 1))) - 1;

        FlushBits(psBits, (leading_zeros << 1) + 1);

    }
    else
    {
        ReadBits(psBits, (leading_zeros << 1) + 1, &temp);
        *codeNum = temp - 1;

    }
}


void se_v(mp4StreamType *psBits, int32 *value)
{
    int32 leadingZeros = 0;
    uint32 temp;

    OSCL_UNUSED_ARG(value);

    ReadBits(psBits, 1, &temp);
    while (!temp)
    {
        leadingZeros++;
        if (ReadBits(psBits, 1, &temp))
        {
            break;
        }
    }
    ReadBits(psBits, leadingZeros, &temp);
}

void scaling_list(mp4StreamType *psBits, uint32 sizeOfScalingList) {
    uint32 lastScale = 8;
    uint32 nextScale = 8;
    int32 delta_scale;
    uint32 j = 0;

    for (j = 0; j < sizeOfScalingList; j++) {
        if(nextScale != 0) {
          /* delta_scale */
          se_v(psBits, &delta_scale);
          nextScale = ( lastScale + delta_scale + 256 ) % 256 ;
        }
        lastScale = ( nextScale == 0 ) ? lastScale : nextScale;
    }
}

int16 DecodeSPS(mp4StreamType *psBits, int32 *width, int32 *height, int32 *display_width, int32 *display_height, int32 *profile_idc, int32 *level_idc)
{

    uint32 temp;
    int32 temp0;
    uint left_offset, right_offset, top_offset, bottom_offset;
    uint i;

    ReadBits(psBits, 8, &temp);

    if ((temp & 0x1F) != 7) {
          return -1;
    }

    /* Read profile_idc */
    ReadBits(psBits, 8, &temp);
    *profile_idc = temp;

    /* Read Constraint_set0-3_flag */
    ReadBits(psBits, 1, &temp);
    ReadBits(psBits, 1, &temp);
    ReadBits(psBits, 1, &temp);
    ReadBits(psBits, 1, &temp);

    LOGV("intel: --- profile_idc = %d ---\n", temp);

    /* reserved_zero_4bits */
    ReadBits(psBits, 4, &temp);

    /* level_idc */ 
    ReadBits(psBits, 8, &temp);
    *level_idc = temp;

    if (temp > 51)
        return -1;

    /* seq_parameter_set_id */
    ue_v(psBits, &temp);
    if(*profile_idc == 100 || *profile_idc == 110 || 
       *profile_idc == 122 || *profile_idc == 144 )
    {
    /* chroma_format_idc */ 
       ue_v(psBits, &temp);
       if(temp == 3) {
       /* residual_colour_transform_flag */
          ReadBits(psBits, 1, &temp);
       }
       /* bit_depth_luma_minus8 */
       ue_v(psBits, &temp);
  
       /* bit_depth_chroma_minus8 */
       ue_v(psBits, &temp);
   
       /* qpprime_y_zero_transform_bypass_flag */
       ReadBits(psBits, 1, &temp);

       /* seq_scaling_matrix_present_flag */
       ReadBits(psBits, 1, &temp);
 
       if(temp) {
           int i = 0;
           for(i = 0; i < 8; i++) {
               ReadBits(psBits, 1, &temp);
               if(temp) {
                  if(i < 6 ) {
                     scaling_list(psBits, 16);
                  } else {
                     scaling_list(psBits, 64);
                  }
              } 
           }
       }
    }

    /* log2_max_frame_num_minus4 */
    ue_v(psBits, &temp);

    /* pic_order_cnt_type */
    ue_v(psBits, &temp);

    if (temp == 0)
    {
        ue_v(psBits, &temp);
    }
    else if (temp == 1)
    {
        ReadBits(psBits, 1, &temp);
        se_v(psBits, &temp0);
        se_v(psBits, &temp0);
        ue_v(psBits, &temp);

        for (i = 0; i < temp; i++)
        {
            se_v(psBits, &temp0);
        }
    }

    /* num_ref_frames */
    ue_v(psBits, &temp);

    /* gaps_in_frame_num_value_allowed_flag */
    ReadBits(psBits, 1, &temp);


    /* pic_width_in_mbs_minus1 */
    ue_v(psBits, &temp);
    *display_width = *width = (temp + 1) << 4;

    /* pic_height_in_map_units_minus1 */
    ue_v(psBits, &temp);
    *display_height = *height = (temp + 1) << 4;


    LOGV("---intel: 1 *display_width = %d *display_height = %d\n", *display_width, *display_height);

    /* frame_mbs_only_flag */
    ReadBits(psBits, 1, &temp);

    if (!temp)
    {
       /* mb_adaptive_frame_field_flag */
        ReadBits(psBits,1, &temp);
    }

    /* direct_8x8_inference_flag */
    ReadBits(psBits, 1, &temp);
    
    /* frame_cropping_flag */
    ReadBits(psBits, 1, &temp);

    if (temp)
    {
        ue_v(psBits, (uint32*)&left_offset);
        ue_v(psBits, (uint32*)&right_offset);
        ue_v(psBits, (uint32*)&top_offset);
        ue_v(psBits, (uint32*)&bottom_offset);

        *display_width = *width - 2 * (right_offset + left_offset);
        *display_height = *height - 2 * (top_offset + bottom_offset);

        LOGV("---intel: 2 *display_width = %d *display_height = %d\n", *display_width, *display_height);
    }

    /*  no need to check further */
#if USE_LATER
    ReadBits(psBits, 1, &temp);
    if (temp)
    {
        if (!DecodeVUI(psBits))
        {
            return MP4_INVALID_VOL_PARAM;
        }
    }
#endif
    return 0; // return 0 for success
}


int avc_parse_sps(unsigned char *buf, unsigned int size, int *display_width, int *display_height) {

    mp4StreamType psBits;
    psBits.data = buf;
    psBits.numBytes = size;
    psBits.bitBuf = 0;
    psBits.bitPos = 32;
    psBits.bytePos = 0;
    psBits.dataBitPos = 0;

    int32 profile_idc = 0;;
    int32 level_idc   = 0;
    int32 width  = 0;
    int32 height = 0;
    
    return DecodeSPS(&psBits, &width, &height, display_width, display_height, &profile_idc, &level_idc);
}
