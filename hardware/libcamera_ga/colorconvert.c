/*
 * colorconvert.c
 *
 * Color converter code
 *
 * Copyright (C) 2009-2010 Texas Instruments Incorporated - http://www.ti.com/
 *
 * This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation version 2.1 of the License.
 *
 * This program is distributed “as is?WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/vt.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h> 
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <OMX_Index.h>
#include <OMX_Types.h>
#include <OMX_Core.h>
#include <OMX_Audio.h>
#include <OMX_VPP.h>
#include <OMX_IVCommon.h>
#include <OMX_Component.h>
#include "colorconvert.h"
#include <signal.h>

#include <utils/Log.h>
/* DSP recovery includes */
#include <qosregistry.h>
#include <qosti.h>
#include <dbapi.h>
#include <DSPManager.h>
#include <DSPProcessor.h>
#include <DSPProcessor_OEM.h>

#define KHRONOS_1_2

//#define APP_DEBUG

#ifdef  APP_DEBUG
#define APP_DPRINT(...)    fprintf(stderr,__VA_ARGS__)
#else
#define APP_DPRINT(...)
#endif

#define LOG_FUNCTION_NAME       LOGV("%s() ###### ENTER %s() ######",  __FILE__,  __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT  LOGV("%s() ###### EXIT %s() ######", __FILE__, __FUNCTION__);

#define DEFAULT_WIDTH      (176)
#define DEFAULT_HEIGHT     (144)

int done = 0;
FILE *Fin,*Fout;
typedef struct _OMX_VPPCustomTYPE
{
    OMX_INDEXTYPE VPPCustomSetZoomFactor;
    OMX_INDEXTYPE VPPCustomSetZoomLimit;
    OMX_INDEXTYPE VPPCustomSetZoomSpeed;
    OMX_INDEXTYPE VPPCustomSetZoomXoffsetFromCenter16;
    OMX_INDEXTYPE VPPCustomSetZoomYoffsetFromCenter16;
    OMX_INDEXTYPE VPPCustomSetFrostedGlassOvly;
    OMX_INDEXTYPE VPPCustomSetColorRange;
    OMX_INDEXTYPE VPP_CustomRGB4ColorFormat;
} OMX_VPPCustomTYPE;

typedef struct EVENT_PRIVATE {
    OMX_EVENTTYPE eEvent;
    OMX_U32 nData1;
    OMX_U32 nData2;
    OMX_PTR pAppData;
    OMX_PTR eInfo;
} EVENT_PRIVATE;
static OMX_ERRORTYPE VPP_SetZoom(OMX_HANDLETYPE pHandle, int speed, int factor, int limit, int xoff, int yoff);
static OMX_ERRORTYPE VPP_SetContrast(OMX_HANDLETYPE pHandle, int Contrast);
static OMX_ERRORTYPE VPP_FrostedGlassEffect(OMX_HANDLETYPE pHandle, int IsOverlay);
static OMX_ERRORTYPE VPP_SetCrop(OMX_HANDLETYPE pHandle, int XStart, int XSize, int YStart, int YSize);
static OMX_ERRORTYPE VPP_SetMirroring(OMX_HANDLETYPE pHandle, int IsRGBOutput);
static OMX_ERRORTYPE VPP_SetRotationAngle(OMX_HANDLETYPE pHandle, int IsRGBOutput,int Angle);
static OMX_ERRORTYPE VPP_SetDithering(OMX_HANDLETYPE pHandle, int IsRGBOutput);
static OMX_ERRORTYPE VPP_SetColorRange(OMX_HANDLETYPE pHandle, int nColorRange); 
OMX_BOOL VPP_Test_Check_Frames(int YUVRGB, int inFrames, int OvlyFrames,int outRGBFrames,int outYUVFrames);

#ifdef DSP_MMU_FAULT_HANDLING
int LoadBaseImage();
#endif

OMX_STRING strAmrDecoder = "OMX.TI.VPP";

int IpBuf_Pipe[2];
int OvlyBuf_Pipe[2];
int OpRGBBuf_Pipe[2];
int OpYUVBuf_Pipe[2];
int Event_Pipe[2];
int nRGBFillBufferDones=0;
int nYUVFillBufferDones=0;
int nInputEmptyBufferDones=0;
int nOvlyEmptyBufferDones=0;

/* Error flag */
OMX_BOOL bError = OMX_FALSE;

struct timeval base;
struct timeval newer;
/*struct timezone tz;*/

static COMPONENT_PORTINDEX_DEF MyVppPortDef;
static OMX_VPPCustomTYPE MyVPPCustomDef;

static long int nTotalTime = 0;

    OMX_ERRORTYPE           error = OMX_ErrorNone;
    OMX_HANDLETYPE pHandle;
    OMX_U32 AppData = 100;
    OMX_PARAM_PORTDEFINITIONTYPE* pCompPrivateStruct = NULL;
    OMX_BUFFERHEADERTYPE* InputBufHeader[MAX_VPP_BUFFERS];
    OMX_BUFFERHEADERTYPE* OvlyBufHeader[MAX_VPP_BUFFERS]; 
    OMX_BUFFERHEADERTYPE* OutRGBBufHeader[MAX_VPP_BUFFERS]; 
    OMX_BUFFERHEADERTYPE* OutYUVBufHeader[MAX_VPP_BUFFERS];
     
    OMX_COMPONENTTYPE *pComponent;
    OMX_STATETYPE state;
    int retval;
  
    char* szInFile = NULL;
    char* szOutFile = NULL;
    FILE* fIn = NULL;
    FILE* fOut = NULL;
    FILE* fYuvOut = NULL;
    FILE* fInOvelay = NULL;
    char stringRGB[30];
    char stringYUV[30];
    char overlaystring[30];

    OMX_U16 inputwidth=0;
    OMX_U16 inputheight=0;
    OMX_U16 outputwidth=0;
    OMX_U16 outputheight=0;
    OMX_U16 inputcolor;
    OMX_U16 rgboutputcolor;
    OMX_U16 yuvoutputcolor;
    int Isoverlay;
    int IsYUVRGB;
    int bitrate=0;
    int iCurrentFrameIn = 0;
    int iCurrentOvlyFrameIn=0;
    int iCurrentRGBFrameOut = 0;
    int iCurrentYUVFrameOut = 0;
    int DEINIT_FLAG = 0;
    int nTypeofAllocation;
    int feature_param[4]={0,0,0,0};   /*Initialize array*/
    int feature;  /*Feature selected, only scaling, zoom, contrast, frosted glass effect, cropping, mirror and rotation*/ 
    OMX_COLOR_FORMATTYPE nColor;        /*Used to pass Color Format for input and output ports*/
    fd_set rfds;
    int fdmax;
    OMX_U8 *pInBuffer = NULL;
    OMX_U8 *pYUVBuffer = NULL;
    OMX_U8 *pRGBBuffer = NULL;
    int nRead = 0;
    int frmCount = 0;
    int nCounter =0;
    int count_stop_restart=0;   /* Executing-->Idle-->Executing*/
    int count_stop_load=0;  /* Loaded-->Idle-->Executing-->Idle-->Loaded */
    int max_count_stop_restart=0;
    int max_count_stop_load=0;
    OMX_BUFFERHEADERTYPE* pBuffer = NULL;  /*To Hold Input Buffers*/
    OMX_BUFFERHEADERTYPE* pBuf = NULL;     /*To Hold Output Buffers*/
    OMX_PARAM_PORTDEFINITIONTYPE *portinput;
    int nFillThisBufferYUV=0;
    int nFillThisBufferRGB=0;
    int nTimeouts =0;
    int bPauseResume=OMX_FALSE;
    int bStopRestart=OMX_FALSE;
    int bStopNotFree=OMX_FALSE;
    int bStopExit=OMX_FALSE;
    int MAX_VPP_BUFFERS_IN_USE = MAX_VPP_BUFFERS;
    sigset_t set;	
    
/* safe routine to get the maximum of 2 integers */
int maxint(int a, int b)
{
         return (a>b) ? a : b;
}

/* This method will wait for the component to get to the state
* specified by the DesiredState input. */
static OMX_ERRORTYPE WaitForState(OMX_HANDLETYPE* pHandle, OMX_STATETYPE DesiredState)
{
    OMX_STATETYPE CurState = OMX_StateInvalid;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    int nCnt = 0;
    OMX_COMPONENTTYPE *pComponent = (OMX_COMPONENTTYPE *)pHandle;

    eError = pComponent->GetState(pHandle, &CurState);
    LOGV("VPPTEST:: desire - %d, current - %d\n", DesiredState, CurState);
    while( (eError == OMX_ErrorNone) 
        && (CurState != DesiredState) 
        && (CurState != OMX_StateInvalid)) {
        sched_yield();
        if ( nCnt++ >= 0xEFFFFFFE ) {
            LOGE( "VPPTEST:: Resend Idle State Command\n");
            break;
        }
        eError = pComponent->GetState(pHandle, &CurState);
        LOGV("VPPTEST:: desire - %d, current - %d, Cnt - %d\n", DesiredState, CurState, nCnt);
    }

    if (CurState == OMX_StateInvalid && DesiredState != OMX_StateInvalid) {
        LOGE( "WaitForState :: InvalidState\n");
        eError = OMX_ErrorInvalidState;
    } 
    return eError;
}

void EventHandler(OMX_HANDLETYPE hComponent,OMX_PTR pAppData, OMX_EVENTTYPE eEvent,
OMX_U32 nData1,OMX_U32 nData2, OMX_STRING eInfo)
{
    
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    EVENT_PRIVATE MyEvent;

    MyEvent.eEvent = eEvent;
    MyEvent.nData1 = nData1;
    MyEvent.nData2 = nData2;
    MyEvent.pAppData = pAppData;
    MyEvent.eInfo = eInfo;

    switch (eEvent) {
    case OMX_EventCmdComplete:           
        break;
    case OMX_EventError:
        if(nData1 == OMX_ErrorHardware) {
            printf("%d: App: ErrorNotofication Came: \
                        \nComponent Name : %d : Error Num %lx: String :%s\n",
            __LINE__,*((int *)(pAppData)), nData1, eInfo);
            eError = OMX_SendCommand(hComponent, OMX_CommandStateSet, OMX_StateInvalid, NULL);
            if(eError != OMX_ErrorNone) {
                printf ("Error from SendCommand-Invalid State function\n");
            }
        } else {
            printf("%d: App: ErrorNotofication Came: \
                        \nComponent Name : %d : Error Num %lx: String :%s\n",
                        __LINE__,*((int *)(pAppData)), nData1, eInfo);
        }
        break;
    case OMX_EventMax:
        break;   
    case OMX_EventMark:
        break;   
    default:
        break;
    }

    write(Event_Pipe[1], &MyEvent, sizeof(EVENT_PRIVATE));

}

#ifndef UNDER_CE
long GetProfiletime()
{
    long int nFrameTime = 0;
    /*struct timeval older;*/
    int nStatus ;
    base.tv_sec = newer.tv_sec;
    base.tv_usec = newer.tv_usec;
    nStatus = gettimeofday(&newer, NULL);
    /*printf("base.tv_sec = %ld, base.tv_usec %ld\n", base.tv_sec, base.tv_usec);*/
    nFrameTime = (newer.tv_sec-base.tv_sec) * 1000000 + (newer.tv_usec-base.tv_usec);
    nTotalTime = nTotalTime + nFrameTime;
    return nFrameTime;

}
#endif /*#ifndef UNDER_CE*/

void FillBufferDone (OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{

    /*PROFILE POINT*/

#ifndef UNDER_CE
    long int pftime = 0;
    pftime = GetProfiletime();
    APP_DPRINT("total time for frame %d \n", nTotalTime);
    APP_DPRINT("total time for each frame %d \n",pftime);
#endif

    if(pBuffer->nOutputPortIndex == MyVppPortDef.rgboutput_port) {  
        write(OpRGBBuf_Pipe[1], &pBuffer, sizeof(pBuffer));
        nRGBFillBufferDones++;
    } else if(pBuffer->nOutputPortIndex == MyVppPortDef.yuvoutput_port) {  
        write(OpYUVBuf_Pipe[1], &pBuffer, sizeof(pBuffer));
        done = 0;
        nYUVFillBufferDones++;
    } else {
        printf("VPPTEST:: Incorrect Output Port Index\n");
    }
        

}


void EmptyBufferDone(OMX_HANDLETYPE hComponent, OMX_PTR ptr, OMX_BUFFERHEADERTYPE* pBuffer)
{
    if(pBuffer->nInputPortIndex == MyVppPortDef.input_port) {
        //write(IpBuf_Pipe[1], &pBuffer, sizeof(pBuffer));
        //done++;
        nInputEmptyBufferDones++;
    } else if(pBuffer->nInputPortIndex == MyVppPortDef.overlay_port) {
        //write(OvlyBuf_Pipe[1], &pBuffer,sizeof(pBuffer));
        nOvlyEmptyBufferDones++;
    } else {
        printf("VPPTEST:: Incorrect Input Port Index\n");  
    }
}


static OMX_ERRORTYPE GetComponentPortDef(OMX_HANDLETYPE pHandleVPP, COMPONENT_PORTINDEX_DEF *pVppPortDef)
{
    OMX_PORT_PARAM_TYPE *pTempPortType = NULL;
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    OMX_PARAM_PORTDEFINITIONTYPE *pTempVidPortDef = NULL;
    int i;
    OMX_BOOL found_input = OMX_FALSE;

    pTempPortType = calloc(1, sizeof(OMX_PORT_PARAM_TYPE));
    if (!pTempPortType ){
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    
    eError = OMX_GetParameter(pHandleVPP, OMX_IndexParamVideoInit, pTempPortType);
    if (eError != OMX_ErrorNone) {
        printf("VPPTEST:: Error at %d\n",__LINE__);
        eError = OMX_ErrorBadParameter;
        goto EXIT;
    }

    APP_DPRINT("VPP_JPEG_DISPLAY:: ports of VPP %lu, start port %lu\n", pTempPortType->nPorts, pTempPortType->nStartPortNumber);

    pTempVidPortDef = calloc (1,sizeof (OMX_PARAM_PORTDEFINITIONTYPE)); 
    if (!pTempVidPortDef) {
        eError = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    
    found_input = OMX_FALSE;
    for (i = pTempPortType->nStartPortNumber; i < pTempPortType->nPorts; i ++) {
        pTempVidPortDef->nPortIndex = i;
        eError = OMX_GetParameter (pHandleVPP, OMX_IndexParamPortDefinition, pTempVidPortDef);
        if ( eError != OMX_ErrorNone ) {
            printf("VPPTEST:: Error at %d\n",__LINE__);
            eError = OMX_ErrorBadParameter;
            goto EXIT;
        }      
            
        if ((pTempVidPortDef->eDir == OMX_DirInput) &&(found_input == OMX_FALSE)) {
            /* found input */
            pVppPortDef->input_port= i;
            found_input = OMX_TRUE;
            continue;
        }
        if ((pTempVidPortDef->eDir == OMX_DirInput) && (found_input == OMX_TRUE)) {
            /* found ovelay port */
            pVppPortDef->overlay_port = i;
            continue;
        }
        if ((pTempVidPortDef->eDir == OMX_DirOutput) &&
            (pTempVidPortDef->format.video.eColorFormat == OMX_COLOR_Format16bitRGB565)) {
            /* found RGB output */
            pVppPortDef->rgboutput_port = i;
            continue;
        }
        if ((pTempVidPortDef->eDir == OMX_DirOutput) &&
            ((pTempVidPortDef->format.video.eColorFormat == OMX_COLOR_FormatYCbYCr)||
            (pTempVidPortDef->format.video.eColorFormat == OMX_COLOR_FormatCbYCrY))) {
             /* found YUV output */
            pVppPortDef->yuvoutput_port = i;
            continue;
        }         
    }
    
    APP_DPRINT("VPPTEST:: input port is %d\n", pVppPortDef->input_port);
    APP_DPRINT("VPPTEST:: overlay port is %d\n", pVppPortDef->overlay_port);    
    APP_DPRINT("VPPTEST:: RGB output port is %d\n", pVppPortDef->rgboutput_port);   
    APP_DPRINT("VPPTEST:: YUV output port is %d\n", pVppPortDef->yuvoutput_port);

EXIT:
    if (pTempPortType) {
        free(pTempPortType);
        pTempPortType = NULL;
    }

    if (pTempVidPortDef) {
        free(pTempVidPortDef);
        pTempVidPortDef = NULL;
    }
    return eError;
}



static OMX_ERRORTYPE GetVPPCustomDef(OMX_HANDLETYPE pHandleVPP)
{
    OMX_ERRORTYPE eError = OMX_ErrorNone;
    
    eError = OMX_GetExtensionIndex(pHandleVPP, "OMX.TI.VPP.Param.ZoomFactor", &(MyVPPCustomDef.VPPCustomSetZoomFactor));
    if(eError != OMX_ErrorNone) {
        LOGE("VPPTEST:: Error in OMX_GetExtensionIndex function\n");
        goto EXIT;
    }
    APP_DPRINT("OMX.TI.VPP.Param.ZoomFactor is %x\n", MyVPPCustomDef.VPPCustomSetZoomFactor);

    eError = OMX_GetExtensionIndex(pHandleVPP, "OMX.TI.VPP.Param.ZoomLimit", &(MyVPPCustomDef.VPPCustomSetZoomLimit));
    if(eError != OMX_ErrorNone) {
        LOGE("VPPTEST:: Error in OMX_GetExtensionIndex function\n");
        goto EXIT;
    }
    APP_DPRINT("OMX.TI.VPP.Param.ZoomLimit is %x\n", MyVPPCustomDef.VPPCustomSetZoomLimit);

    eError = OMX_GetExtensionIndex(pHandleVPP, "OMX.TI.VPP.Param.ZoomSpeed", &(MyVPPCustomDef.VPPCustomSetZoomSpeed));
    if(eError != OMX_ErrorNone) {
        LOGE("Error in OMX_GetExtensionIndex function\n");
        goto EXIT;
    }
    APP_DPRINT("OMX.TI.VPP.Param.ZoomSpeed is %x\n", MyVPPCustomDef.VPPCustomSetZoomSpeed);

    eError = OMX_GetExtensionIndex(pHandleVPP, "OMX.TI.VPP.Param.ZoomXoffsetFromCenter16", &(MyVPPCustomDef.VPPCustomSetZoomXoffsetFromCenter16));
    if(eError != OMX_ErrorNone) {
        LOGE("VPPTEST:: Error in OMX_GetExtensionIndex function\n");
        goto EXIT;
    }
    APP_DPRINT("OMX.TI.VPP.Param.ZoomXoffsetFromCenter16 is %x\n", MyVPPCustomDef.VPPCustomSetZoomXoffsetFromCenter16);
    
    eError = OMX_GetExtensionIndex(pHandleVPP, "OMX.TI.VPP.Param.ZoomYoffsetFromCenter16", &(MyVPPCustomDef.VPPCustomSetZoomYoffsetFromCenter16));
    if(eError != OMX_ErrorNone) {
        LOGE("Error in OMX_GetExtensionIndex function\n");
        goto EXIT;
    }
    APP_DPRINT("OMX.TI.VPP.Param.ZoomYoffsetFromCenter16 is %x\n", MyVPPCustomDef.VPPCustomSetZoomYoffsetFromCenter16);
    
    eError = OMX_GetExtensionIndex(pHandleVPP, "OMX.TI.VPP.Param.FrostedGlassOvly", &(MyVPPCustomDef.VPPCustomSetFrostedGlassOvly));
    if(eError != OMX_ErrorNone) {
        LOGE("VPPTEST:: Error in OMX_GetExtensionIndex function\n");
        goto EXIT;
    }
    APP_DPRINT("OMX.TI.VPP.Param.FrostedGlassOvly is %x\n", (MyVPPCustomDef.VPPCustomSetFrostedGlassOvly));


    eError = OMX_GetExtensionIndex(pHandleVPP, "OMX.TI.VPP.Param.VideoColorRange", &(MyVPPCustomDef.VPPCustomSetColorRange));
    if(eError != OMX_ErrorNone) {
        LOGE("VPPTEST:: Error in OMX_GetExtensionIndex function\n");
        goto EXIT;
    }
    APP_DPRINT("OMX.TI.VPP.Param.FrostedGlassOvly is %x\n", (MyVPPCustomDef.VPPCustomSetColorRange));
    
    eError = OMX_GetExtensionIndex(pHandleVPP, "OMX.TI.VPP.Param.RGB4ColorFormat", &(MyVPPCustomDef.VPP_CustomRGB4ColorFormat));
    if(eError != OMX_ErrorNone) {
        LOGE("VPPTEST:: Error in OMX_GetExtensionIndex function\n");
        goto EXIT;
    }
    APP_DPRINT("OMX.TI.VPP.Param.ZoomFactor is %x\n", MyVPPCustomDef.VPP_CustomRGB4ColorFormat);

EXIT:
    return eError;

}

#ifndef UNDER_CE
int fill_data (OMX_BUFFERHEADERTYPE *pBuf, FILE *fIn)
#else
int fill_data (OMX_BUFFERHEADERTYPE *pBuf, HANDLE fIn)
#endif
{
    int nRead;
    static int totalRead = 0;
    OMX_U8 *pTmp = NULL;
    pTmp = pBuf->pBuffer;
    APP_DPRINT(" :: ------------- before File Read -------------- %p\n",pBuf);
    APP_DPRINT(" :: ------------- before File Read -------------- %d\n",pBuf->nAllocLen);
    APP_DPRINT(" :: ------------- before File Read -------------- %p\n",pBuf->pBuffer);
#ifndef UNDER_CE
    nRead = fread(pBuf->pBuffer, 1, pBuf->nAllocLen, fIn);
#else
    ReadFile(fIn, pBuf->pBuffer, pBuf->nAllocLen, &nRead, NULL);
#endif
    APP_DPRINT("\n%d :: ------------- App File Read --------------\n",__LINE__);
    APP_DPRINT("App: Read %d bytes from file\n", nRead);
    APP_DPRINT("App: pBuf->nAllocLen = %ld\n",pBuf->nAllocLen);
    APP_DPRINT("%d :: ------------- App File Read --------------\n\n",__LINE__);

    pBuf->nFilledLen = nRead;
    totalRead += nRead;
    return nRead;
}



static OMX_ERRORTYPE VPP_SetZoom(OMX_HANDLETYPE pHandle, int speed, int factor, int limit, int xoff, int yoff)
{
    int nZoomSpeed = speed;
    int nZoomFactor = factor<<10;
    int nZoomLimit = limit<<10;
    int nXoff = xoff<<4;
    int nYoff=yoff<<4;

    OMX_ERRORTYPE error = OMX_ErrorNone;

    LOG_FUNCTION_NAME

    error = OMX_SetConfig (pHandle, MyVPPCustomDef.VPPCustomSetZoomFactor, &nZoomFactor);
    if(error != OMX_ErrorNone) {         
        LOGE("Error configuring Zoom Factor\n");                          
        return error;
    }
    
    error = OMX_SetConfig (pHandle,MyVPPCustomDef.VPPCustomSetZoomSpeed,&nZoomSpeed);
    if(error != OMX_ErrorNone) {         
        LOGE("Error configuring Zoom Factor\n");                          
        return error;
    }

    error = OMX_SetConfig (pHandle,MyVPPCustomDef.VPPCustomSetZoomLimit,&nZoomLimit);
    if(error != OMX_ErrorNone) {                                                     
        LOGE("Error configuring Zoom Limit/n");
        return error;
    }
    
    error = OMX_SetConfig(pHandle,MyVPPCustomDef.VPPCustomSetZoomXoffsetFromCenter16,&nXoff);
    if(error != OMX_ErrorNone) {                                 
        LOGE("Error Configuring Horizontal Zoom Offset\n.");
        return error;
    }   
    
    error = OMX_SetConfig(pHandle,MyVPPCustomDef.VPPCustomSetZoomYoffsetFromCenter16,&nYoff);
    if(error != OMX_ErrorNone) {                                                                 
        LOGE("Error Configuring Vertical Zoom Offset");                               
        return error;
    }

    LOG_FUNCTION_NAME_EXIT

    return error;
}



static OMX_ERRORTYPE VPP_SetContrast(OMX_HANDLETYPE pHandle, int Contrast)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    OMX_CONFIG_CONTRASTTYPE *pConfigNewContrast = malloc(sizeof(OMX_CONFIG_CONTRASTTYPE));

    LOG_FUNCTION_NAME
        
    if(pConfigNewContrast == NULL) {
        error = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(pConfigNewContrast,0,sizeof(OMX_CONFIG_CONTRASTTYPE));
    pConfigNewContrast->nContrast = Contrast;
    error=OMX_SetConfig(pHandle,OMX_IndexConfigCommonContrast,pConfigNewContrast);
    if(error != OMX_ErrorNone) {
        LOGE("VPPTEST:: VPPTest Error at %d\n",__LINE__);
        error = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    if ( error != OMX_ErrorNone) {
        if(pConfigNewContrast) {
            free(pConfigNewContrast);
            pConfigNewContrast = NULL;
        }
    }

    LOG_FUNCTION_NAME_EXIT
        
    return error;
}



static OMX_ERRORTYPE VPP_FrostedGlassEffect(OMX_HANDLETYPE pHandle, int IsOverlay)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;

    LOG_FUNCTION_NAME
        
    if(IsOverlay) {
        int FrostedGlassEffect=1;           
        error=OMX_SetConfig(pHandle,MyVPPCustomDef.VPPCustomSetFrostedGlassOvly,&FrostedGlassEffect);
        if(error != OMX_ErrorNone) {
            LOGV("VPPTEST:: VPPTest Error at %d\n",__LINE__);
            error = OMX_ErrorBadParameter;
            return error;
        }
    } else {
        LOGE("VPPTEST:: Frosted Glass Effect is only upon overlayed images.\n");
        error=OMX_ErrorBadParameter;
    }

    LOG_FUNCTION_NAME_EXIT
        
    return error;
}



static OMX_ERRORTYPE VPP_SetCrop(OMX_HANDLETYPE pHandle, int XStart, int XSize, int YStart, int YSize)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    OMX_CONFIG_RECTTYPE *pConfigNewCropWindow = malloc (sizeof(OMX_CONFIG_RECTTYPE));

    LOG_FUNCTION_NAME
        
    if(pConfigNewCropWindow == NULL) {
        error = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(pConfigNewCropWindow, 0, sizeof(OMX_CONFIG_RECTTYPE));
    pConfigNewCropWindow->nLeft   = XStart;
    pConfigNewCropWindow->nTop    = YStart;
    pConfigNewCropWindow->nWidth  = XSize;
    pConfigNewCropWindow->nHeight = YSize;
    error = OMX_SetConfig (pHandle, OMX_IndexConfigCommonInputCrop, pConfigNewCropWindow);
    if (error != OMX_ErrorNone) {
        LOGE("VPPTest Error at %d\n",__LINE__);
        error = OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:
    if(error != OMX_ErrorNone){
        free(pConfigNewCropWindow);
        pConfigNewCropWindow = NULL;
    }

    LOG_FUNCTION_NAME_EXIT
    
    return error;
}

static OMX_ERRORTYPE VPP_SetMirroring(OMX_HANDLETYPE pHandle, int IsRGBOutput)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    OMX_CONFIG_MIRRORTYPE * pConfigNewMirror = NULL;

    LOG_FUNCTION_NAME
        
    if(IsRGBOutput) {
        pConfigNewMirror = malloc(sizeof(OMX_CONFIG_MIRRORTYPE));
        if(pConfigNewMirror == NULL) {
            error = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        memset(pConfigNewMirror,0,sizeof(OMX_CONFIG_MIRRORTYPE));
        pConfigNewMirror->nPortIndex = MyVppPortDef.rgboutput_port;
        pConfigNewMirror->eMirror = OMX_MirrorHorizontal;
        error=OMX_SetConfig(pHandle,OMX_IndexConfigCommonMirror,pConfigNewMirror);
        if(error != OMX_ErrorNone) {
            LOGE("VPPTest Error at %d\n",__LINE__);
            error = OMX_ErrorBadParameter;
            goto EXIT;
        }
    } else {
        LOGE("VPPTEST:: Need to use RGB as Output, Mirror Setting unchanged.\n");
        error=OMX_ErrorBadParameter;
        goto EXIT;
    }
EXIT:

    if(pConfigNewMirror){
        free(pConfigNewMirror);
        pConfigNewMirror = NULL;
    }

    LOG_FUNCTION_NAME_EXIT
        
    return error;
}

static OMX_ERRORTYPE VPP_SetRotationAngle(OMX_HANDLETYPE pHandle, int IsRGBOutput,int Angle)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    OMX_CONFIG_ROTATIONTYPE *pConfigNewRotation = malloc(sizeof(OMX_CONFIG_ROTATIONTYPE));

    LOG_FUNCTION_NAME
        
    if(pConfigNewRotation == NULL){
        error = OMX_ErrorInsufficientResources;
        goto EXIT;
    }
    memset(pConfigNewRotation,0,sizeof(OMX_CONFIG_ROTATIONTYPE));
    if(Angle == 90 || Angle == 180 || Angle == 270) {
        pConfigNewRotation->nRotation = Angle;
    } else {
        LOGE("VPPTEST:: Not a valid Rotation Angle, Working with Default Rotation Angle\n");
        goto EXIT;
    }
    
    switch(IsRGBOutput){
    case 0:  /*YUV output only*/ 
        pConfigNewRotation->nPortIndex = MyVppPortDef.yuvoutput_port;
        error=OMX_SetConfig(pHandle,OMX_IndexConfigCommonRotate,pConfigNewRotation);
        if(error != OMX_ErrorNone) {
            LOGE("VPPTest Error at %d\n",__LINE__);
            error = OMX_ErrorBadParameter;
            goto EXIT;
        }
        break;          
    case 1: /*RGB output only*/
        pConfigNewRotation->nPortIndex = MyVppPortDef.rgboutput_port;
        error=OMX_SetConfig(pHandle,OMX_IndexConfigCommonRotate,pConfigNewRotation);
        if(error != OMX_ErrorNone) {
            LOGE("VPPTest Error at %d\n",__LINE__);
            error = OMX_ErrorBadParameter;
            goto EXIT;
        }
        break;
    case 2: /*Simultaneous outputs*/
        pConfigNewRotation->nPortIndex = MyVppPortDef.rgboutput_port;
        error=OMX_SetConfig(pHandle,OMX_IndexConfigCommonRotate,pConfigNewRotation);
        if(error != OMX_ErrorNone) {
            LOGE("VPPTest Error at %d\n",__LINE__);
            error = OMX_ErrorBadParameter;
            goto EXIT;
        }
        pConfigNewRotation->nPortIndex = MyVppPortDef.yuvoutput_port;
        error=OMX_SetConfig(pHandle,OMX_IndexConfigCommonRotate,pConfigNewRotation);
        if(error != OMX_ErrorNone) {
            LOGE("VPPTest Error at %d\n",__LINE__);
            error = OMX_ErrorBadParameter;
            goto EXIT;
        }
        break;
    default:  
        break;
    }
EXIT:
    if(pConfigNewRotation){
        free(pConfigNewRotation);
        pConfigNewRotation = NULL;
    }

    LOG_FUNCTION_NAME_EXIT
        
    return error;

}



static OMX_ERRORTYPE VPP_SetDithering(OMX_HANDLETYPE pHandle, int IsRGBOutput)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;
    OMX_CONFIG_DITHERTYPE * pConfigNewDither = NULL;

    LOG_FUNCTION_NAME

    if(IsRGBOutput) {
        pConfigNewDither = malloc(sizeof(OMX_CONFIG_DITHERTYPE));
        if(pConfigNewDither == NULL) {
            error = OMX_ErrorInsufficientResources;
            goto EXIT;
        }
        memset(pConfigNewDither,0,sizeof(OMX_CONFIG_DITHERTYPE));
        pConfigNewDither->nPortIndex = MyVppPortDef.rgboutput_port;
        pConfigNewDither->eDither = OMX_DitherErrorDiffusion;
        error=OMX_SetConfig(pHandle,OMX_IndexConfigCommonDithering,pConfigNewDither);
        if(error != OMX_ErrorNone) {
            LOGE("VPPTest Error at %d\n",__LINE__);
            error = OMX_ErrorBadParameter;
            goto EXIT;
        }
    } else {
        LOGE("VPPTEST:: Need to use RGB as Output, Dithering not possible.\n");
        error=OMX_ErrorBadParameter;
        goto EXIT;
    }

EXIT:

    if(pConfigNewDither) {
        free(pConfigNewDither);
        pConfigNewDither = NULL;
    }

    LOG_FUNCTION_NAME_EXIT
        
    return error;
}

static OMX_ERRORTYPE VPP_SetColorRange(OMX_HANDLETYPE pHandle, int nColorRange)
{
    OMX_ERRORTYPE error = OMX_ErrorNone;

    LOG_FUNCTION_NAME
    
    if(nColorRange<0 || nColorRange>3) {
        LOGE("VPPTEST:: Not a valid option in Color Range Conversion\n");
        error = OMX_ErrorBadParameter;
        return error;
    }
    
    error=OMX_SetConfig(pHandle, MyVPPCustomDef.VPPCustomSetColorRange, &nColorRange);
    if(error != OMX_ErrorNone) {
        LOGE("VPPTest Error at %d\n",__LINE__);
        error = OMX_ErrorBadParameter;
        return error;
    }       

    LOG_FUNCTION_NAME_EXIT

    return error;
}

OMX_BOOL VPP_Test_Check_Frames(int YUVRGB, int inFrames, int OvlyFrames,int outRGBFrames,int outYUVFrames)
{
    LOG_FUNCTION_NAME

    if(YUVRGB == 0) {
        if(outYUVFrames < (inFrames-1) || outYUVFrames < (OvlyFrames-1)) {
            return OMX_TRUE;
        } else {
            return OMX_FALSE;
        }
    } else if(YUVRGB == 1) {
        if(outRGBFrames < (inFrames-1) || outRGBFrames < (OvlyFrames-1)) {
            return OMX_TRUE;
        } else {
            return OMX_FALSE;
        }
    } else {
        if( (outRGBFrames < (inFrames-1) || outRGBFrames < (OvlyFrames-1)) ||
            (outYUVFrames < (inFrames-1) || outYUVFrames < (OvlyFrames-1))) {
            return OMX_TRUE;
        }  
        else{
            return OMX_FALSE;
        }
    }

    LOG_FUNCTION_NAME_EXIT
}

#ifdef DSP_MMU_FAULT_HANDLING

int LoadBaseImage() {
    unsigned int uProcId = 0;	/* default proc ID is 0. */
    unsigned int index = 0;
    
    struct DSP_PROCESSORINFO dspInfo;
    DSP_HPROCESSOR hProc;
    DSP_STATUS status = DSP_SOK;
    unsigned int numProcs;
    char* argv[2];

    LOG_FUNCTION_NAME
   
    argv[0] = "/lib/dsp/baseimage.dof";
    
    status = (DBAPI)DspManager_Open(0, NULL);
    if (DSP_FAILED(status)) {
        LOGE("DSPManager_Open failed \n");
        return -1;
    } 
    
    while (DSP_SUCCEEDED(DSPManager_EnumProcessorInfo(index,&dspInfo,
        (unsigned int)sizeof(struct DSP_PROCESSORINFO),&numProcs))) {
        if ((dspInfo.uProcessorType == DSPTYPE_55) || 
            (dspInfo.uProcessorType == DSPTYPE_64)) {
            uProcId = index;
            status = DSP_SOK;
            break;
        }
        index++;
    }
    
    status = DSPProcessor_Attach(uProcId, NULL, &hProc);
    if (DSP_SUCCEEDED(status)) {
        status = DSPProcessor_Stop(hProc);
        if (DSP_SUCCEEDED(status)) {
            status = DSPProcessor_Load(hProc,1,(const char **)argv,NULL);
            if (DSP_SUCCEEDED(status)) {
                status = DSPProcessor_Start(hProc);
                if (DSP_SUCCEEDED(status)) {
                } else {
                }
            } else {
            }
            DSPProcessor_Detach(hProc);
        } else {
        }
    } else {
    }
    LOGE("Baseimage Loaded\n");

    LOG_FUNCTION_NAME_EXIT

    return 0;
}
#endif


OMX_CALLBACKTYPE AmrCaBa = {(void *)EventHandler, (void*) EmptyBufferDone, (void*)FillBufferDone};

int ColorConvert_Init(int width, int height, int infmt, int outyuvfmt, int outrgbfmt, int rotateAngle)
{
    inputwidth = width;
    inputheight = height;
    inputcolor = infmt;
    outputwidth = width;
    outputheight = height;     
    yuvoutputcolor = outyuvfmt;
    rgboutputcolor = outrgbfmt;    
    Isoverlay = 0;   
    nTypeofAllocation = 0; 
    MAX_VPP_BUFFERS_IN_USE = 1;
    
    LOG_FUNCTION_NAME
    
    if(yuvoutputcolor && !rgboutputcolor) {
        IsYUVRGB = 0;     /*Only YUV output*/
    } else if(!yuvoutputcolor && rgboutputcolor) {
        IsYUVRGB = 1;     /*Only RGB output*/
    } else if(yuvoutputcolor && rgboutputcolor) {
        IsYUVRGB = 2;     /*Simultaneous output*/
    } else {
        IsYUVRGB = 0;     /*Only YUV output*/   
    }
    
#ifdef UNDER_CE    
   /* check to see that the input file exists */
    struct stat sb = {0};
    int status = stat(argv[1], &sb);
    if( status != 0 ) {
        LOGE( "Cannot find file %s. (%u)\n", argv[1], errno);
        goto EXIT;
    }
#endif
 
    /* Create a pipe used to queue data from the callback. */
    retval = pipe(IpBuf_Pipe);
    if(retval != 0) {
        LOGE( "Error:Fill Data Pipe failed to open\n");
        goto EXIT;
    }
    
    /* Create a pipe used to queue data from the callback. */
    retval = pipe(OvlyBuf_Pipe);
    if(retval != 0) {
        LOGE( "Error:Fill Data Pipe failed to open\n");
        goto EXIT;
    }
    
    retval = pipe(OpRGBBuf_Pipe);
    if(retval != 0) {
        LOGE( "Error:Empty Data Pipe failed to open\n");
        goto EXIT;
    }

    retval = pipe(OpYUVBuf_Pipe);
    if(retval != 0) {
        LOGE( "Error:Empty Data Pipe failed to open\n");
        goto EXIT;
    }

    /* Create a pipe used to handle events from the callback. */
    retval = pipe(Event_Pipe);
    if(retval != 0) {
        LOGE("Error:Fill Data Pipe failed to open\n");
        goto EXIT;
    }

    /* save off the "max" of the handles for the selct statement */
    fdmax = maxint(IpBuf_Pipe[0], OvlyBuf_Pipe[0]);
    fdmax = maxint(fdmax, OpRGBBuf_Pipe[0]);
    fdmax = maxint(fdmax, OpYUVBuf_Pipe[0]);
    fdmax = maxint(fdmax, Event_Pipe[0]);

#ifdef DSP_MMU_FAULT_HANDLING
    /* LOAD BASE IMAGE FIRST TIME */
    //LoadBaseImage();
#endif

    TIOMX_Deinit();

    error = TIOMX_Init();
    if(error != OMX_ErrorNone) {
        LOGE("%d :: Error returned by TIOMX_init()\n",__LINE__);
        goto EXIT;
    }

    /********************************************************************************************************/
    /* Load the VPP Component */
    error = TIOMX_GetHandle(&pHandle,strAmrDecoder,&AppData, &AmrCaBa);
    if( (error != OMX_ErrorNone) || (pHandle == NULL) ) {
        LOGE("Error in Get Handle function\n");
        goto EXIT;
    }
    
    if(bStopNotFree) {
        max_count_stop_load=20;
    } else {
        max_count_stop_load=1;
    }

    /********************************Component is loaded here, we can loop after this point****************************************/
    for(count_stop_load=0; count_stop_load<max_count_stop_load; count_stop_load++) {
        error = GetComponentPortDef(pHandle, &MyVppPortDef);
        if (error != OMX_ErrorNone) {
            LOGE("Error in Get Handle function\n");
            goto EXIT;
        }

        error = GetVPPCustomDef(pHandle);
        if(error != OMX_ErrorNone) {
            LOGE("Error in Get Handle function\n");
            goto EXIT;
        }

        LOGE("VPPTEST:: Input Height: %d, Input Width %d\n",inputheight, inputwidth); 
    
        /*Select Input Format and Buffer Size accordingly*/
        switch(inputcolor) {
        case 1: /*YUV 420 Packed Planar*/
            nColor = OMX_COLOR_FormatYUV420PackedPlanar; 
            LOGV("VPPTEST:: Input YUV420\n");
            break;
        case 2: /*YUV 422 Interleaved (little-endian)*/
            nColor = OMX_COLOR_FormatCbYCrY; 
            LOGV("VPPTEST:: Input YUV422 Interleaved (little-endian)\n");
            break;
        case 3: /*YUV 422 Interleaved (big-endian)*/
            nColor = OMX_COLOR_FormatYCbYCr;  
            LOGV("VPPTEST:: Input YUV422 Interleaved (big-endian)\n");
            break;
        case 4: /*RGB24 8:8:8*/
            nColor = OMX_COLOR_Format24bitRGB888; 
            LOGV("VPPTEST:: Input RGB24\n");
            break;
        case 5 : /*RGB16 5:6:5*/ 
            nColor = OMX_COLOR_Format16bitRGB565; 
            LOGV("VPPTEST:: Input RGB16\n");
            break;
        case 6 : /*RGB12 5:6:5*/
            nColor = OMX_COLOR_Format12bitRGB444; 
            LOGV("VPPTEST:: Input RGB12\n");
            break;
        case 7 : /*RGB8 3:3:2*/
            nColor = OMX_COLOR_Format8bitRGB332; 
            LOGV("VPPTEST:: Input RGB8\n");
            break;
        case 8 : /*RGB4  Look-Up-Table*/
            nColor = MyVPPCustomDef.VPP_CustomRGB4ColorFormat; 
            LOGV("VPPTEST:: Input RGB4\n");
            break;
        case 9 : /*GRAY8 */
            nColor = OMX_COLOR_FormatL8; 
            LOGV("VPPTEST:: Input GRAY8\n");
            break;
        case 10 : /* GRAY4 */
            nColor = OMX_COLOR_FormatL4; 
            LOGV("VPPTEST:: Input GRAY4\n");
            break;
        case 11: /* GRAY2 */
            nColor = OMX_COLOR_FormatL2; 
            LOGV("VPPTEST:: Input GRAY2\n");
            break;
        case 12 : /* Monochrome */
            nColor = OMX_COLOR_FormatMonochrome; 
            LOGV("VPPTEST:: Input Monochrome\n");
            break;
        default:
            LOGV("VPPTEST:: Not a valid option for Input Format\n");
            goto EXIT;
            break;
        }
    
        LOGE("VPPTEST:: %d : GetHandle Done..........\n",__LINE__);

        pCompPrivateStruct = malloc (sizeof (OMX_PARAM_PORTDEFINITIONTYPE));
        if(!pCompPrivateStruct) {
            LOGE("VPPTEST:: eError = OMX_ErrorInsufficientResources");
            error = OMX_ErrorInsufficientResources;
            goto EXIT;
        }

        pCompPrivateStruct->nSize = sizeof (OMX_PARAM_PORTDEFINITIONTYPE);
        pCompPrivateStruct->nVersion.s.nVersionMajor = 0x1; 
        pCompPrivateStruct->nVersion.s.nVersionMinor = 0x0; 
        pCompPrivateStruct->nVersion.s.nRevision = 0x0;     
        pCompPrivateStruct->nVersion.s.nStep = 0x0;
        pCompPrivateStruct->nPortIndex = MyVppPortDef.input_port;    
        pCompPrivateStruct->eDir = OMX_DirInput;
        pCompPrivateStruct->nBufferCountMin = 1;
        pCompPrivateStruct->nBufferCountActual = MAX_VPP_BUFFERS_IN_USE; 
        pCompPrivateStruct->bEnabled = OMX_TRUE;
        pCompPrivateStruct->bPopulated = OMX_FALSE;
        pCompPrivateStruct->eDomain = OMX_PortDomainVideo;
        pCompPrivateStruct->format.video.nFrameHeight = inputheight;
        pCompPrivateStruct->format.video.nFrameWidth = inputwidth;
        pCompPrivateStruct->format.video.eColorFormat = nColor;
        pCompPrivateStruct->format.video.nBitrate = bitrate;
        
        //Send input port config
        error = OMX_SetParameter (pHandle,OMX_IndexParamPortDefinition, pCompPrivateStruct);
        if (error != OMX_ErrorNone) {
            goto EXIT;
        } 

        if(Isoverlay ){  /* At the moment, only qcif overlay frame is supported*/
            error = OMX_GetParameter (pHandle,OMX_IndexParamPortDefinition, pCompPrivateStruct);
            if(error != OMX_ErrorNone) {
                goto EXIT;
            }

            pCompPrivateStruct->nPortIndex = MyVppPortDef.overlay_port;
            pCompPrivateStruct->eDir = OMX_DirInput;
            pCompPrivateStruct->nBufferCountActual = MAX_VPP_BUFFERS_IN_USE; 
            pCompPrivateStruct->format.video.nFrameHeight = DEFAULT_HEIGHT;
            pCompPrivateStruct->format.video.nFrameWidth = DEFAULT_WIDTH;
            pCompPrivateStruct->format.video.eColorFormat = OMX_COLOR_Format24bitRGB888;//OMX_COLOR_FormatYCbYCr 
            error = OMX_SetParameter (pHandle,OMX_IndexParamPortDefinition, pCompPrivateStruct);
            if (error != OMX_ErrorNone ){
                goto EXIT;
            }
            LOGV("VPPTEST:: Overlay Enabled\n");
    
        } else {
            OMX_SendCommand(pHandle, OMX_CommandPortDisable, MyVppPortDef.overlay_port, NULL);
        }
    
        /**************************** FOR OUTPUT PORTS ***************************/
        LOGE("VPPTEST:: Output height: %d, Output Width: %d\n",outputheight, outputwidth);
    
        if(IsYUVRGB){ /*Select output Format and Buffer Size accordingly*/
            switch (rgboutputcolor) {
            case 1: /*RGB24 8:8:8*/
                nColor = OMX_COLOR_Format24bitRGB888; /* changed from BGR*/ 
                LOGV("VPPTEST:: %d::Output RGB24\n",__LINE__);
                break;
            case 2: /*RGB16 5:6:5*/
                nColor = OMX_COLOR_Format16bitRGB565;
                LOGV("VPPTEST:: Output RGB16\n");
                break;
            case 3:/*RGB12 4:4:4*/ 
                nColor = OMX_COLOR_Format12bitRGB444;
                LOGV("VPPTEST:: Output RGB12\n");
                break;
            case 4:/*RGB8 3:3:2*/
                nColor = OMX_COLOR_Format8bitRGB332;
                LOGV("VPPTEST:: Output RGB8\n");
                break;
            case 5: /*RGB4*/
                nColor = MyVPPCustomDef.VPP_CustomRGB4ColorFormat;
                LOGV("VPPTEST:: Output RGB4\n");
                break;
            case 6: /*GRAY8 */
                nColor = OMX_COLOR_FormatL8;
                LOGV("VPPTEST:: Output GRAY8\n");
                break;
            case 7:/*GRAY4*/
                nColor = OMX_COLOR_FormatL4;
                LOGV("VPPTEST:: Output GRAY4\n");
                break;
            case 8: /*GRAY2*/
                nColor = OMX_COLOR_FormatL2;
                LOGV("VPPTEST:: Output GRAY2\n");
                break;
            case 9: /*Monochrome*/
                nColor = OMX_COLOR_FormatMonochrome;
                LOGV("VPPTEST:: Output Monochrome\n");
                break;
            case 10: /*RGB32*/
                nColor = OMX_COLOR_Format32bitARGB8888; 
                LOGV("VPPTEST:: Output RGB32\n");
                break;
            default:
                nColor = OMX_COLOR_Format16bitRGB565;
                LOGV("VPPTEST:: Not a valid option, default to RGB16\n");
                break;
            }
        }   
        
        /*Send output port config for RGB port*/
        
        if(IsYUVRGB){
            LOGV("VPPTEST:: configuring RGB port \n");
            error = OMX_GetParameter (pHandle,OMX_IndexParamPortDefinition,
                                                   pCompPrivateStruct);
            if (error != OMX_ErrorNone) {
                goto EXIT;
            }
            pCompPrivateStruct->nPortIndex = MyVppPortDef.rgboutput_port;
            pCompPrivateStruct->eDir = OMX_DirOutput;
            pCompPrivateStruct->nBufferCountActual = MAX_VPP_BUFFERS_IN_USE;
            pCompPrivateStruct->format.video.nFrameHeight = outputheight;
            pCompPrivateStruct->format.video.nFrameWidth = outputwidth;
            pCompPrivateStruct->format.video.eColorFormat = nColor ;/*OMX_COLOR_FormatUnused */
            error = OMX_SetParameter (pHandle,OMX_IndexParamPortDefinition,
                                                   pCompPrivateStruct);
            if (error != OMX_ErrorNone) {
                goto EXIT;
            }
            LOGE("VPPTEST:: RGB port has been configured\n");
        } else {
            OMX_SendCommand(pHandle, OMX_CommandPortDisable, MyVppPortDef.rgboutput_port, NULL);
        }
    
        /*Send output port config for YUV port*/
        if(IsYUVRGB == 0 || IsYUVRGB == 2) {
            switch (yuvoutputcolor) {
            case 1: /*YUV 420 Packed Planar*/
                nColor = OMX_COLOR_FormatYUV420PackedPlanar;
                LOGV("VPPTEST:: Output YUV420 Planar\n");
                break;
            case 2: /*YUV 422 Interleaved (little-endian)*/
                nColor = OMX_COLOR_FormatCbYCrY; 
                LOGV("VPPTEST:: Output YUV422 YUYV\n");
                break;
            case 3: /*YUV 422 Interleaved (big-endian)*/
                nColor = OMX_COLOR_FormatYCbYCr; 
                LOGV("VPPTEST:: Output YUV422 UYVY\n");         
                break;
            default:
                LOGV("VPPTEST:: Not a valid option, default to YUV420 planar\n");
                nColor = OMX_COLOR_FormatYUV420PackedPlanar;
                break;
            }
            error = OMX_GetParameter (pHandle,OMX_IndexParamPortDefinition,
                        pCompPrivateStruct);
            if (error != OMX_ErrorNone) {
                goto EXIT;
            }
            pCompPrivateStruct->nPortIndex = MyVppPortDef.yuvoutput_port;
            pCompPrivateStruct->eDir = OMX_DirOutput;
            pCompPrivateStruct->nBufferCountActual = MAX_VPP_BUFFERS_IN_USE;
            pCompPrivateStruct->format.video.nFrameHeight = outputheight;
            pCompPrivateStruct->format.video.nFrameWidth = outputwidth;
            pCompPrivateStruct->format.video.eColorFormat = nColor;
    
            LOGV("VPPTEST:: Configuring YUV output port\n");
            error = OMX_SetParameter (pHandle,OMX_IndexParamPortDefinition,
                        pCompPrivateStruct);
            if (error != OMX_ErrorNone) {
                goto EXIT;
            }
            LOGV("VPPTEST:: YUV output port has been configured\n");
        } else {
            OMX_SendCommand(pHandle, OMX_CommandPortDisable, MyVppPortDef.yuvoutput_port, NULL);
        }
        
        APP_DPRINT ("Basic Function:: Sending OMX_StateIdle Command (%d)\n", __LINE__);

        LOGE("VPPTEST:: Num buffers %d\n", MAX_VPP_BUFFERS_IN_USE);
        /*Input Buffer Allocation*/
        pCompPrivateStruct->nPortIndex = MyVppPortDef.input_port;
        error = OMX_GetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
        if (error != OMX_ErrorNone) {
            goto EXIT;
        }
        if(nTypeofAllocation ==0) {
            for(nCounter=0; nCounter<MAX_VPP_BUFFERS_IN_USE; nCounter++) {  /*MultiBuffer*/
                error = OMX_AllocateBuffer(pHandle, &InputBufHeader[nCounter], MyVppPortDef.input_port, (void *)NULL, pCompPrivateStruct->nBufferSize); 
                if(error != OMX_ErrorNone) {
                    LOGE("VPPTEST:: VPPTEST:: OMX_AllocateBuffer failed !!\n");
                    goto EXIT;
                }
            }
        } else {
            pInBuffer = malloc(pCompPrivateStruct->nBufferSize +256);
            if(pInBuffer == NULL) {
                error = OMX_ErrorInsufficientResources;
                goto EXIT;
            }
            pInBuffer += 128;
            for(nCounter=0; nCounter<MAX_VPP_BUFFERS_IN_USE; nCounter++) { /*MultiBuffer*/
                error = OMX_UseBuffer(pHandle, &InputBufHeader[nCounter], 
                            MyVppPortDef.input_port, 
                            (void *)NULL, 
                            pCompPrivateStruct->nBufferSize, 
                            pInBuffer); 
                if(error != OMX_ErrorNone) {
                    LOGE("VPPTEST:: OMX_UseBuffer failed !!\n");
                    goto EXIT;
                }
            }
        }

        /*Overlay Buffer Allocation*/
        pCompPrivateStruct->nPortIndex = MyVppPortDef.overlay_port;
        error = OMX_GetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
        if (error != OMX_ErrorNone) {
            goto EXIT;
        }
        
        if(Isoverlay) {
            for(nCounter=0; nCounter<MAX_VPP_BUFFERS_IN_USE; nCounter++) {  /*MultiBuffer*/
                error = OMX_AllocateBuffer(pHandle, &OvlyBufHeader[nCounter], 
                            MyVppPortDef.overlay_port, 
                            (void *)NULL, 
                            pCompPrivateStruct->nBufferSize); 
                if(error != OMX_ErrorNone) {
                    LOGE ("VPPTEST:: OMX_AllocateBuffer failed !!\n");
                    goto EXIT;
                }
            }
        }
    
        /*RGB Buffer Allocation*/
        pCompPrivateStruct->nPortIndex = MyVppPortDef.rgboutput_port;
        error = OMX_GetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
        if (error != OMX_ErrorNone) {
            goto EXIT;
        }
        if(IsYUVRGB){
            if(nTypeofAllocation == 0) {
                for(nCounter=0; nCounter<MAX_VPP_BUFFERS_IN_USE; nCounter++) {  /*MultiBuffer*/
                    error = OMX_AllocateBuffer(pHandle, &OutRGBBufHeader[nCounter], 
                                MyVppPortDef.rgboutput_port, 
                                (void *)NULL, 
                                pCompPrivateStruct->nBufferSize); 
                    if(error != OMX_ErrorNone) {
                        LOGE ("VPPTEST:: OMX_AllocateBuffer failed !!\n");
                        goto EXIT;
                    }
                }
            } else {
                pRGBBuffer = malloc(pCompPrivateStruct->nBufferSize + 256);
                if(pRGBBuffer == NULL) {
                    error = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
                pRGBBuffer += 128;
                for(nCounter = 0; nCounter<MAX_VPP_BUFFERS_IN_USE; nCounter++) { /*MultiBuffer*/
                    error = OMX_UseBuffer(pHandle, &OutRGBBufHeader[nCounter], 
                                MyVppPortDef.rgboutput_port, 
                                (void *)NULL, 
                                pCompPrivateStruct->nBufferSize, 
                                pRGBBuffer); 
                    if(error != OMX_ErrorNone) {
                        LOGE ("VPPTEST:: OMX_UseBuffer failed !!\n");
                        goto EXIT;
                    }
                }
            }
        }
        
        /*YUV Buffer Allocation*/
        pCompPrivateStruct->nPortIndex = MyVppPortDef.yuvoutput_port;
        error = OMX_GetParameter (pHandle, OMX_IndexParamPortDefinition, pCompPrivateStruct);
        if (error != OMX_ErrorNone) {
            goto EXIT;
        }
        if(IsYUVRGB == 0 || IsYUVRGB == 2) {
            if(nTypeofAllocation ==0){ 
                for(nCounter=0; nCounter<MAX_VPP_BUFFERS_IN_USE; nCounter++) {  /*MultiBuffer*/
                    error = OMX_AllocateBuffer(pHandle, &OutYUVBufHeader[nCounter], 
                                MyVppPortDef.yuvoutput_port, 
                                (void *)NULL, 
                                pCompPrivateStruct->nBufferSize); 
                    if(error != OMX_ErrorNone) {
                        LOGE ("VPPTEST:: OMX_AllocateBuffer failed !!\n");
                        goto EXIT;
                    }
                }
            } else {
                pYUVBuffer = malloc(pCompPrivateStruct->nBufferSize +256);
                if(pYUVBuffer == NULL) {
                    error = OMX_ErrorInsufficientResources;
                    goto EXIT;
                }
                pYUVBuffer += 128;
                for(nCounter=0; nCounter<MAX_VPP_BUFFERS_IN_USE; nCounter++) {  /*MultiBuffer*/
                    error = OMX_UseBuffer(pHandle, &OutYUVBufHeader[nCounter], 
                                MyVppPortDef.yuvoutput_port, 
                                (void *)NULL, 
                                pCompPrivateStruct->nBufferSize, 
                                pYUVBuffer); 
                    if(error != OMX_ErrorNone) {
                        LOGE("VPPTEST:: OMX_UseBuffer failed !!\n");
                        goto EXIT;
                    }
                }
            }
        }

        error = OMX_SendCommand(pHandle, OMX_CommandStateSet, OMX_StateIdle, NULL);
        if(error != OMX_ErrorNone) {
            LOGE ("VPPTEST:: Error from SendCommand-Idle(Init) State function\n");
            goto EXIT;
        } 
        LOGV("VPPTEST:: Sending component to Idle State (%d)\n", __LINE__);
        
        /* Wait for startup to complete */
        error = WaitForState(pHandle, OMX_StateIdle);
        if(error != OMX_ErrorNone) {
            LOGE ("VPPTEST:: Error:  VPP->WaitForState reports an error %X\n", error);
            goto EXIT;
        }
        state = OMX_StateIdle; 
        LOGV("Component is in Idle State\n");
            
        /*******HERE THE COMPONENT IS ALREADY IN IDLE STATE AND BUFFERS HAVE BEEN ALLOCATED********/
        if(bStopRestart) {
            max_count_stop_restart=20;
        } else {
            max_count_stop_restart=1;
        }
    
        for(count_stop_restart=0; count_stop_restart<max_count_stop_restart; count_stop_restart++) { /*Executing-->Idle-->Executing*/        
            sched_yield();
            iCurrentFrameIn = 0;
            iCurrentOvlyFrameIn = 0;
            iCurrentRGBFrameOut = 0;
            iCurrentYUVFrameOut = 0;
            nFillThisBufferYUV = 0;
            nFillThisBufferRGB = 0;

#if 1   // This code is required if we need to have the rotation
            if(rotateAngle) {
                error = VPP_SetRotationAngle(pHandle, IsYUVRGB, rotateAngle);
                LOGV("VPP rorate %d\n", rotateAngle);
            }
#endif  

            /***********************************OPEN THE NEEDED FILES****************************************************/
       
            /**** ALL THE Configurables Features of VPP will happen here (Zoom, Contrast, Cropping, etc.*/
            APP_DPRINT("Configurating features...\n");
           
            error = OMX_ErrorNone;         
            
            if (error != OMX_ErrorNone) {
                error = OMX_ErrorBadParameter;
                goto EXIT;
            }
        
            APP_DPRINT ("Basic Function:: Sending OMX_StateExecuting Command\n");
            error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateExecuting, NULL);
            if(error != OMX_ErrorNone) {
                LOGE ("VPPTEST:: Error from SendCommand-Executing State function\n");
                goto EXIT;
            }
            pComponent = (OMX_COMPONENTTYPE *)pHandle;       
            error = WaitForState(pHandle, OMX_StateExecuting);
            if(error != OMX_ErrorNone) {
                LOGE ("VPPTEST:: Error:  VPP->WaitForState reports an error %X\n", error);
                goto EXIT;
            }
            LOGV("VPPTEST:: Component is now in Executing state\n");
            state = OMX_StateExecuting;
          
#ifndef UNDER_CE             
            /*PROFILE POINT */
            gettimeofday(&base, NULL);
            newer.tv_sec = base.tv_sec;
            newer.tv_usec = base.tv_usec;
            APP_DPRINT("sec: %ld, usec: %ld\n", base.tv_sec, base.tv_usec);
            /*profiletime = GetProfiletime());*/
#endif
        }

    }
    
    return 0;
EXIT:
    
    LOGE ("ERROR :: INIT Free the Component handle\n"); 
    error = TIOMX_FreeHandle(pHandle);
    if( (error != OMX_ErrorNone)) {
        LOGE("Error in Free Handle function\n");
        //goto EXIT;
    }
    //fprintf (stderr,"Free Handle returned Successfully \n");

#ifdef DSP_MMU_FAULT_HANDLING
    if(bError) {
        //LoadBaseImage();
    }
#endif

    /* De-Initialize OMX Core */
    error = TIOMX_Deinit();
    if (error != OMX_ErrorNone) {
        LOGE("VPPTEST:: Failed to de-init OMX Core!\n");
        //goto EXIT;
    }
    
    if(error != OMX_ErrorNone) {
        if(pCompPrivateStruct) {
            free(pCompPrivateStruct);
            pCompPrivateStruct = NULL;
        }
        if(pInBuffer) {
            pInBuffer -= 128;
            free(pInBuffer);
            pInBuffer = NULL;
        }
        if(pRGBBuffer) {
            pRGBBuffer -= 128;
            free(pRGBBuffer);
            pRGBBuffer = NULL;
        }
        if(pYUVBuffer) {
            pYUVBuffer -= 128;
            free(pYUVBuffer);
            pYUVBuffer = NULL;
        }
    }

    LOG_FUNCTION_NAME_EXIT

    return error;
}


int fill_frame (OMX_BUFFERHEADERTYPE *pBuf, char *buffer)
{
    int nRead;
    static int totalRead = 0;
    OMX_U8 *pTmp = NULL;

    LOG_FUNCTION_NAME
        
    pTmp = pBuf->pBuffer;
    APP_DPRINT(" :: ------------- before File Read -------------- %p\n",pBuf);
    APP_DPRINT(" :: ------------- before File Read -------------- %d\n",pBuf->nAllocLen);
    APP_DPRINT(" :: ------------- before File Read -------------- %p\n",pBuf->pBuffer);

    memcpy(pBuf->pBuffer,buffer, pBuf->nAllocLen);
    nRead=pBuf->nAllocLen;

    APP_DPRINT("\n%d :: ------------- App File Read --------------\n",__LINE__);
    APP_DPRINT ("App: Read %d bytes from file\n", nRead);
    APP_DPRINT ("App: pBuf->nAllocLen = %ld\n",pBuf->nAllocLen);
    APP_DPRINT("%d :: ------------- App File Read --------------\n\n",__LINE__);

    pBuf->nFilledLen = nRead;
    totalRead += nRead;

    LOG_FUNCTION_NAME_EXIT
        
    return nRead;
}


int ColorConvert_Process(char *inbuffer,char *outbuffer)
{
    LOG_FUNCTION_NAME

    for(nCounter=0; nCounter<1; nCounter++) { /*MultiBuffer*/
        /*Send Input Buffers to the Component */
        /*Provided that at least one frame will be read*/
        nRead = fill_frame (InputBufHeader[nCounter],inbuffer);
        if(nRead == 0) {
            LOGE("There is no data on input file\n");
            break; /*If there is no data send no more buffers to the component*/
            /*Exit for loop*/
        } else {
            ++iCurrentFrameIn;
            error = OMX_EmptyThisBuffer(pHandle, InputBufHeader[nCounter]);  /*INPUT port*/
            if (error != OMX_ErrorNone) {
                LOGE ("VPPTEST:: Error from OMX_EmptyThisBuffer function 0x%X\n",error);
                goto EXIT;
            }
        }


        if(IsYUVRGB == 0 || IsYUVRGB == 2) {  /*Send YUV output buffers to component*/
            OutYUVBufHeader[nCounter]->nFilledLen = 0;
            done = 1;
            error = OMX_FillThisBuffer(pHandle,OutYUVBufHeader[nCounter]);
            if (error != OMX_ErrorNone) {
                LOGE ("VPPTEST:: Error from OMX_FillThisBuffer function 0x%X\n",error);
                goto EXIT;
            }
            nFillThisBufferYUV++;
        }
    }

    while(done);      
    read(OpYUVBuf_Pipe[0], &pBuf, sizeof(pBuf));
    memcpy(outbuffer,pBuf->pBuffer, pBuf->nFilledLen);

    LOG_FUNCTION_NAME_EXIT

    return 0;
EXIT:

    error = TIOMX_FreeHandle(pHandle);
    if( (error != OMX_ErrorNone)) {
        LOGE("Error in Free Handle function\n");
        //goto EXIT;
    }

#ifdef DSP_MMU_FAULT_HANDLING
    if(bError) {
        //LoadBaseImage();
    }
#endif
    /* De-Initialize OMX Core */
    error = TIOMX_Deinit();
    if (error != OMX_ErrorNone) {
        LOGE("VPPTEST:: Failed to de-init OMX Core!\n");
        //goto EXIT;
    }

    if(error != OMX_ErrorNone) {
        if(pCompPrivateStruct) {
            free(pCompPrivateStruct);
            pCompPrivateStruct = NULL;
        }
        if(pInBuffer ) {
            pInBuffer -= 128;
            free(pInBuffer);
            pInBuffer = NULL;
        }
        if(pRGBBuffer) {
            pRGBBuffer -= 128;
            free(pRGBBuffer);
            pRGBBuffer = NULL;
        }
        if(pYUVBuffer) {
            pYUVBuffer -= 128;
            free(pYUVBuffer);
            pYUVBuffer = NULL;
        }
    }

    LOG_FUNCTION_NAME_EXIT
        
    return error;

}

int ColorConvert_Deinit()
{
    int retval = 0;

    OMX_STATETYPE MyState;
    OMX_GetState(pHandle, &MyState);

    LOG_FUNCTION_NAME

    if(MyState != OMX_StateInvalid) {
        APP_DPRINT ("%d :: App: State Of Component Is Idle Now\n",__LINE__);
        APP_DPRINT("Sending the StateLoaded Command\n");
        /*Disable the ports before freeing the buffers*/
    }

    if (MyState == OMX_StateExecuting) {
        error = OMX_SendCommand(pHandle,OMX_CommandStateSet, OMX_StateIdle, NULL);
        if ( error != OMX_ErrorNone ) {
            LOGE("Error from SendCommand-Idle(nStop) State function\n");
            //goto EXIT;
        }

        error = WaitForState(pHandle, OMX_StateIdle);
        if(error != OMX_ErrorNone) {
            LOGE ("VPPTEST:: Error:  VPP->WaitForState reports an error %X\n", error);
            //goto EXIT;
        }
    }

    for(nCounter=0; nCounter<1; nCounter++) {  /* MultiBuffer*/
        error = OMX_FreeBuffer(pHandle, MyVppPortDef.input_port, InputBufHeader[nCounter]) ;
        if(error != OMX_ErrorNone) {
            LOGE("VPPTEST:: free buffer failed !!\n");
            //goto EXIT;
        }

        if(IsYUVRGB ==0 || IsYUVRGB ==2) {
            error = OMX_FreeBuffer(pHandle, MyVppPortDef.yuvoutput_port, OutYUVBufHeader[nCounter]);
            if(error != OMX_ErrorNone) {
                LOGE("VPPTEST:: OMX_FreeBuffer failed !!\n");
                //goto EXIT;
            }
        }
    }

    if(nTypeofAllocation == 1) {
        if(pInBuffer) {
            free (pInBuffer-128);
            pInBuffer = NULL;
        }
        if(pYUVBuffer) {
            free(pYUVBuffer-128);
            pYUVBuffer = NULL;
        }
        if(pRGBBuffer) {
            free(pRGBBuffer-128);
            pYUVBuffer = NULL;
        }
    }

    retval = close(IpBuf_Pipe[0]);
    if(retval != 0) {
        LOGE ("Error:Fill Data Pipe failed to close 0\n");
    }
    retval = close(IpBuf_Pipe[1]);
    if(retval != 0) {
        LOGE ("Error:Fill Data Pipe failed to close 1\n");
    }

    retval = close(OvlyBuf_Pipe[0]);
    if(retval != 0) {
        LOGE ("Error:Fill Data Pipe failed to close 0\n");
    }
    retval = close(OvlyBuf_Pipe[1]);
    if(retval != 0) {
        LOGE ("Error:Fill Data Pipe failed to close 1\n");
    }

    retval = close(OpRGBBuf_Pipe[0]);
    if(retval != 0) {
        LOGE ("Error:Empty Data Pipe failed to close 0\n");
    }
    retval = close(OpRGBBuf_Pipe[1]);
    if(retval != 0) {
        LOGE ("Error:Empty Data Pipe failed to close 1\n");
    }

    retval = close(OpYUVBuf_Pipe[0]);
    if(retval != 0) {
        LOGE ("Error:Empty Data Pipe failed to close 0\n");
    }
    retval = close(OpYUVBuf_Pipe[1]);
    if(retval != 0) {
        LOGE ("Error:Empty Data Pipe failed to close 1\n");
    }

    /* Create a pipe used to handle events from the callback. */
    retval = close(Event_Pipe[0]);
    if(retval != 0) {
        LOGE ("Error:Fill Data Pipe failed to close 0\n");
    }
    retval = close(Event_Pipe[1]);
    if(retval != 0) {
        LOGE ("Error:Fill Data Pipe failed to close 1\n");
    }
    if(pCompPrivateStruct) {
        free(pCompPrivateStruct);	/*Free all m(allocated) resources to avoid memory leaks*/	   
        pCompPrivateStruct = NULL;
    }

EXIT:

    error = TIOMX_FreeHandle(pHandle);

#ifdef DSP_MMU_FAULT_HANDLING
    /*
    if(bError) {
        LoadBaseImage();
    }
    */
#endif
    /* De-Initialize OMX Core */
    error = TIOMX_Deinit();
#if 1
    if(error != OMX_ErrorNone) {
        if(pCompPrivateStruct) {
            free(pCompPrivateStruct);
            pCompPrivateStruct = NULL;
        }
        if(pInBuffer) {
            pInBuffer -= 128;
            free(pInBuffer);
            pInBuffer = NULL;
        }
        if(pRGBBuffer) {
            pRGBBuffer -= 128;
            free(pRGBBuffer);
            pRGBBuffer = NULL;
        }
        if(pYUVBuffer) {
            pYUVBuffer -= 128;
            free(pYUVBuffer);
            pYUVBuffer = NULL;
        }
    }
#endif

    LOG_FUNCTION_NAME_EXIT

    return error;
}
