/*
 * vabuffer.h, vabuffer structure header
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

#ifndef __VA_BUFFER_H__
#define __VA_BUFFER_H__

struct VABuffer
{
    VASurfaceID surface;
    VADisplay display;
    unsigned int frame_structure;
};

#endif /* __VA_BUFFER_H__ */

