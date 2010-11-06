#ifndef __VA_BUFFER_H__
#define __VA_BUFFER_H__

struct VABuffer
{
   VASurfaceID surface;
   VADisplay display;
   unsigned int frame_structure;
};

#endif /* __VA_BUFFER_H__ */

