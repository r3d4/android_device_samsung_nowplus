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
/*
TODO:

Memcpy into bm upon receiving fillbufferdone

Replace Allocate Buffer with UseBuffer(using bm) for output buffer.

Better Error handling
*/

#include <stdio.h>
#include <string.h>
#include <semaphore.h>
#include "../../omx/image/src/openmax_il/jpeg_enc/inc/OMX_JpegEnc_CustomCmd.h"

#include <utils/Log.h>
//#include <OMX_JpegEnc_CustomCmd.h>

extern "C" {
    #include "OMX_Component.h"
    #include "OMX_IVCommon.h"
}

class JpegEncoder
{

public:

    enum JPEGENC_State
    {
        STATE_LOADED,                                   // 0
        STATE_IDLE,                                         // 1
        STATE_EXECUTING,                              // 2
        STATE_FILL_BUFFER_DONE_CALLED,      // 3
        STATE_EMPTY_BUFFER_DONE_CALLED,     // 4
        STATE_ERROR,                                        // 5
        STATE_EXIT                                             // 6
    };

    typedef struct JpegEncoderParams
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
    }JpegEncoderParams;
        //[ 20100110 Ratnesh 
	typedef struct IMAGE_INFO {
        int nWidth;
        int nHeight ;
        int nFormat;
        int nComment;
        char* pCommentString;
        int nThumbnailWidth_app0;
        int nThumbnailHeight_app0;
        int nThumbnailWidth_app1;
        int nThumbnailHeight_app1;
        int nThumbnailWidth_app5;
        int nThumbnailHeight_app5;
        int nThumbnailWidth_app13;
        int nThumbnailHeight_app13;
        int nDRI;
        OMX_BOOL bAPP0;
        OMX_BOOL bAPP1;
        OMX_BOOL bAPP5;
        OMX_BOOL bAPP13;
    } IMAGE_INFO;
    //]

    int jpegSize;
    sem_t *semaphore;
    JPEGENC_State iState;
    JPEGENC_State iLastState;

    ~JpegEncoder();
    JpegEncoder();
    bool encodeImage(void* outputBuffer, int outBuffSize, void *inputBuffer, int inBuffSize, int width, int height, int quality, int mIsPixelFmt420p);    
	//[20100110 Ratnesh Modified to take EXIF Info
    bool encodeImage(void* outputBuffer, int outBuffSize, void *inputBuffer, int inBuffSize, unsigned char* pExifBuf, int ExifSize,int width, int height, int ThumbWidth, int ThumbHeight, int quality, int mIsPixelFmt420p);    
	//]
    bool SetJpegEncodeParameters(JpegEncoderParams * jep) {memcpy(&jpegEncParams, jep, sizeof(JpegEncoderParams)); return true;}
    void Run();
    void PrintState();
    void FillBufferDone(OMX_U8* pBuffer, OMX_U32 size);
    bool StartFromLoadedState();
    bool StartFromLoadedState(unsigned char* pExifBuf, int ExifSize, int ThumbWidth, int ThumbHeight); //[ 20100110 Ratnesh Modified for EXIF
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
    JpegEncoderParams jpegEncParams;
    void* mOutputBuffer;
    int mOutBuffSize;
    void *mInputBuffer;
    int mInBuffSize;
    int mWidth;
    int mHeight;
    int mQuality;
	int mIsPixelFmt420p;
};

OMX_ERRORTYPE OMX_JPEGE_FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffHead);
OMX_ERRORTYPE OMX_JPEGE_EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer);
OMX_ERRORTYPE OMX_JPEGE_EventHandler(OMX_HANDLETYPE hComponent,
                                            OMX_PTR pAppData,
                                            OMX_EVENTTYPE eEvent,
                                            OMX_U32 nData1,
                                            OMX_U32 nData2,
                                            OMX_PTR pEventData);

