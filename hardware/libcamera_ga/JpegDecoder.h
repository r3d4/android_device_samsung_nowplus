/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
/* ==============================================================================
*             Texas Instruments OMAP (TM) Platform Software
*  (c) Copyright Texas Instruments, Incorporated.  All Rights Reserved.
*
*  Use of this software is controlled by the terms and conditions found
*  in the license agreement under which this software has been supplied.
* ============================================================================ */

#include <stdio.h>
#include <string.h>
#include <semaphore.h>

#include <utils/Log.h>

extern "C" {
    #include "OMX_Component.h"
    #include "OMX_IVCommon.h"
}

class JpegDecoder
{

public:

    enum JPEGDEC_State
    {
        STATE_LOADED,                                   // 0
        STATE_IDLE,                                         // 1
        STATE_EXECUTING,                              // 2
        STATE_FILL_BUFFER_DONE_CALLED,      // 3
        STATE_EMPTY_BUFFER_DONE_CALLED,     // 4
        STATE_ERROR,                                        // 5
        STATE_EXIT                                             // 6
    };

    typedef struct JpegDecoderParams
    {
        //nWidth;
        //nHeight;
        //nQuality;
        //nComment;
        //nThumbnailWidth;
        //nThumbnailHeight;
        //APP0
        //APP1
        //APP13
        //CustomQuantizationTable
        //CustomHuffmanTable
        //nCropWidth
        //nCropHeight
    }JpegDecoderParams;

    int yuvsize;
    sem_t *semaphore;
    JPEGDEC_State iState;
    JPEGDEC_State iLastState;

    ~JpegDecoder();
    JpegDecoder();
    bool decodeImage(void* outputBuffer, int outBuffSize, void *inputBuffer, int inBuffSize, int width, int height, int quality, int mIsPixelFmt420p);    
    bool SetJpegDecodeParameters(JpegDecoderParams * jep) {memcpy(&jpegDecParams, jep, sizeof(JpegDecoderParams)); return true;}
    void Run();
    void PrintState();
    void FillBufferDone(OMX_U8* pBuffer, OMX_U32 size);
    bool StartFromLoadedState();
    void EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData);

private:

    OMX_HANDLETYPE pOMXHandle;
    OMX_BUFFERHEADERTYPE *pInBuffHead;
    OMX_BUFFERHEADERTYPE *pOutBuffHead;
    OMX_PARAM_PORTDEFINITIONTYPE InPortDef;
    OMX_PARAM_PORTDEFINITIONTYPE OutPortDef;
    JpegDecoderParams jpegDecParams;
    void* mOutputBuffer;
    int mOutBuffSize;
    void *mInputBuffer;
    int mInBuffSize;
    int mWidth;
    int mHeight;
    int mQuality;
    int mIsPixelFmt420p;
};

OMX_ERRORTYPE JPEGE_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead);
OMX_ERRORTYPE JPEGE_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE JPEGE_EventHandler(OMX_HANDLETYPE hComponent,
                                            								OMX_PTR pAppData,
                                            									OMX_EVENTTYPE eEvent,
                                            										OMX_U32 nData1,
                                            											OMX_U32 nData2,
                                            												OMX_PTR pEventData);

