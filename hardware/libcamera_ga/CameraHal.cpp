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
/**
* @file CameraHal.cpp
*
* This file maps the Camera Hardware Interface to V4L2.
*
*/
#undef LOG_TAG
#define LOG_TAG "CameraHal"

#include "CameraHal.h"
#include <cutils/properties.h>
#include <pthread.h>

#define HD_WIDTH       1280
#define HD_HEIGHT      720
#define WIDE_WIDTH     1280
#define WIDE_HEIGHT    720
#define QQVGA_WIDTH    160
#define QQVGA_HEIGHT   120
#define QCIF_WIDTH     176
#define QCIF_HEIGHT    144
#define WQQVGA_WIDTH   200
#define WQQVGA_HEIGHT  120
#define QVGA_WIDTH     320
#define QVGA_HEIGHT    240
#define CIF_WIDTH      352
#define CIF_HEIGHT     288
#define WQVGA_WIDTH    400
#define WQVGA_HEIGHT   240
#define VGA_WIDTH      640
#define VGA_HEIGHT     480
#define D1_WIDTH       720
#define D1_HEIGHT      480
#define WVGA_WIDTH     800
#define WVGA_HEIGHT    480

namespace android {
/*****************************************************************************/

/*
 * This is the overlay_t object, it is returned to the user and represents
 * an overlay. here we use a subclass, where we can store our own state.
 * This handles will be passed across processes and possibly given to other
 * HAL modules (for instance video decode modules).
 */
struct overlay_true_handle_t : public native_handle {
    /* add the data fields we need here, for instance: */
    int ctl_fd;
    int shared_fd;
    int width;
    int height;
    int format;
    int num_buffers;
    int shared_size;
    int device_id;    /* Video pipe 1 and video pipe 2 */

};

/* Defined in liboverlay */
typedef struct {
    int fd;
    size_t length;
    uint32_t offset;
    void *ptr;
    int nQueueToOverlay;   
} mapping_data_t;

static const char* wb_value[] = {
	"minus1",           
	"auto",         	// M4MO_WB_AUTO					1
	"incandescent",		// M4MO_WB_INCANDESCENT			2
	"fluorescent",		// M4MO_WB_FLUORESCENT			3
	"daylight",     	// M4MO_WB_DAYLIGHT				4
	"cloudy-daylight",	// M4MO_WB_CLOUDY				5
	"shade",	    	// M4MO_WB_SHADE				6
	"horizon",      	// M4MO_WB_HORIZON				7
	"maxplus1"		

};

#define MAX_WBLIGHTING_EFFECTS 8

static const char* color_effects[] = {    
	"minus1",			// to match camera_effect_t 
	"none", 			// M4MO_EFFECT_OFF				    1
	"sepia", 			// M4MO_EFFECT_SEPIA				2
	"mono",             // M4MO_EFFECT_GRAY					3
	"red",              // M4MO_EFFECT_RED				    4
	"green",            // M4MO_EFFECT_GREEN				5
	"blue",             // M4MO_EFFECT_BLUE					6
	"pink", 			// M4MO_EFFECT_PINK					7
	"yellow", 			// M4MO_EFFECT_YELLOW			    8
	"purple",			// M4MO_EFFECT_PURPLE			    9
	"whiteboard", 		// M4MO_EFFECT_ANTIQUE			    10
	"negative", 		// M4MO_EFFECT_NEGATIVE				11
	"solarize",         // M4MO_EFFECT_SOLARIZATION1		12
	"solar2",           // M4MO_EFFECT_SOLARIZATION2		13
	"solar3",          	//  M4MO_EFFECT_SOLARIZATION3		14
	"solar4",           // M4MO_EFFECT_SOLARIZATION4		15
	"blackboard",      	// M4MO_EFFECT_EMBOSS			    16
	"posterize",        // M4MO_EFFECT_OUTLINE			    17
	"aqua",             // M4MO_EFFECT_AQUA			   		18  
	"maxplus" 			
};   

#define MAX_COLOR_EFFECTS 19

static const char* scene_mode[] = {
	"minus1", 			// to match camera_effect_t 
	"auto",             // M4MO_SCENE_NORMAL			1
	"portrait", 		// M4MO_SCENE_PORTRAIT			2
	"landscape", 		// M4MO_SCENE_LANDSCAPE			3
	"sports",			// M4MO_SCENE_SPORTS			4
	"party",     		// M4MO_SCENE_PARTY_INDOOR		5
	"beach",            // M4MO_SCENE_BEACH_SNOW		6
	"sunset", 			// M4MO_SCENE_SUNSET			7
	"dusk-dawn", 		// M4MO_SCENE_DUSK_DAWN			8
	"fall", 		// M4MO_SCENE_FALL_COLOR		9
	"night", 			// M4MO_SCENE_NIGHT				10
	"fireworks", 		// M4MO_SCENE_FIREWORK			11	
	"text", 			// M4MO_SCENE_TEXT				12
	"candlelight", 		// M4MO_SCENE_CANDLELIGHT		13
	"back-light", 		// M4MO_SCENE_BACKLIGHT			14   
	"maxplus" 			
};

#define MAX_SCENE_MODE 15

static const char* flash_mode[] = {
	"minus1", 			// to match camera_effect_t 
	"off", 				// M4MO_FLASH_CAPTURE_OFF		1
	"on",				// M4MO_FLASH_CAPTURE_ON		2
	"auto", 			// M4MO_FLASH_CAPTURE_AUTO		3
	"torch",            // M4MO_FLASH_TORCH		        4
	"maxplus" 			// CAMERA_FLASH_MAX_PLUS_1
};

#define MAX_FLASH_MODE 5

static const char* focus_mode[] = {
	"minus1", 			// to match focus_mode 
	"auto",				//M4MO_AF_MODE_NORMAL 	1
	"macro",		 	//M4MO_AF_MODE_MACRO  	2
	"maxplus" 			//CAMERA_FOCUS_MAX_PLUS_1
};

#define MAX_FOCUS_MODE 3

static const char* metering_mode[] = {
	"minus1", 			// to match metering 
	"matrix",			//M4MO_PHOTOMETRY_AVERAGE		1  
	"center", 			//M4MO_PHOTOMETRY_CENTER		2
	"spot", 			//M4MO_PHOTOMETRY_SPOT			3
	"maxplus" 			//CAMERA_METERING_MAX_PLUS_1
};

#define MAX_METERING 4

static const char* anti_banding_values[] = {
    "off",	//CAMERA_ANTIBANDING_OFF, as defined in qcamera/common/camera.h
    "60hz",	//CAMERA_ANTIBANDING_60HZ,
    "50hz",	//CAMERA_ANTIBANDING_50HZ,
    "auto",	//CAMERA_ANTIBANDING_AUTO,
    "max"		//CAMERA_MAX_ANTIBANDING,
};

#define MAX_ANTI_BANDING_VALUES 5

static const char* iso_mode[] = {
	"minus1", 			// to match metering
	"auto",   			//M4MO_ISO_AUTO					1
	"50",     			//M4MO_ISO_50					2
	"100",    			//M4MO_ISO_100					3
	"200",    			//M4MO_ISO_200					4
	"400",    			//M4MO_ISO_400					5
	"800",     			//M4MO_ISO_800					6
	"maxplus"
};

#define MAX_ISO_VALUES 7	

#define MAX_ZOOM 				12

#define CLEAR(x) memset (&(x), 0, sizeof (x))


#define AF_START           		1
#define AF_STOP            		2
#define AF_RELEASE          	3

#define FLASH_AF_OFF          	1
#define FLASH_AF_ON           	2
#define FLASH_AF_AUTO         	3
#define FLASH_AF_TORCH        	4

#define FLASH_MOVIE_OFF       	1
#define FLASH_MOVIE_ON        	2
#define FLASH_MOVIE_AUTO		3

#define ISO_AUTO        		1	// M4MO_ISO_AUTO				1
#define ISO_50          		2   // M4MO_ISO_50					2
#define ISO_100         		3   // M4MO_ISO_100				3
#define ISO_200         		4   // M4MO_ISO_200				4
#define ISO_400         		5   // M4MO_ISO_400				5
#define ISO_800         		6   // M4MO_ISO_800				6

#define JPEG_SUPERFINE        	1
#define JPEG_FINE             	2
#define JPEG_NORMAL           	3
#define JPEG_ECONOMY          	4
#define JPEG_SUPERFINE_limit  	75   
#define JPEG_FINE_limit       	50
#define JPEG_NORMAL_limit     	25

#define YUV_SUPERFINE   		100
#define YUV_FINE        		75
#define YUV_ECONOMY     		50
#define YUV_NORMAL      		25

#define DISPLAYFPS 				15

int CameraHal::camera_device = NULL;
wp<CameraHardwareInterface> CameraHal::singleton;

CameraHal::CameraHal(int cameraId)
    :mParameters(),
    mCameraIndex(0),
    mOverlay(NULL),
    mPreviewRunning(0),
    mPreviewNotReady(0),
    mRecordingFrameSize(0),
    mNotifyCb(0),
    mDataCb(0),
    mDataCbTimestamp(0),
    mCallbackCookie(0),
    mMsgEnabled(0),
    mRecordRunning(0),
    mRecordOverlay(0),
    mVideoBufferCount(0),
    nOverlayBuffersQueued(0),
    nCameraBuffersQueued(0),   
    mfirstTime(0),
    pictureNumber(0),
    file_index(0),
    mflash(2),
    mcapture_mode(1),
    mAFState(0),
    mcaf(0),
    j(0),
    myuv(3),
    mMMSApp(0),
    mCurrentTime(0),
    mCaptureFlag(0),
    mCamera_Mode(1),
    mYcbcrQuality(100),
    mASDMode(0),
    mPreviewFrameSkipValue(0),
    mCamMode(1),
    mCounterSkipFrame(0),
    mSkipFrameNumber(0),
    mPassedFirstFrame(0),
    mDSSActive (0),
    mPreviousGPSLatitude(0),
    mPreviousGPSLongitude(0),
    mPreviousGPSAltitude(0),
    mPreviousGPSTimestamp(0)    
#ifdef SKT_FMC_DM
    ,mSkipFrameCount(0)
    ,mCurSecurity(0)
#endif
#if VPP_THREAD
#if VPP_CONVERSION
    ,isStart_convert(0)
#endif
#endif
#if OMAP_SCALE_CDD
    ,isStart_Scale (0)
    ,isStart_Rotate (0)
#endif
{
    LOG_FUNCTION_NAME
    
    gettimeofday(&ppm_start, NULL);
    
    mCameraIndex = cameraId;

    LOGE("%s: mCameraIndex=%d",__func__,mCameraIndex);

    if(CameraCreate() != NO_ERROR) {
        LOGE("ERROR CameraCreate()\n");        
        mNotifyCb(CAMERA_MSG_ERROR,CAMERA_DEVICE_ERROR_FOR_EXIT,0,mCallbackCookie);
    }

    initDefaultParameters();

    mPreviewThread = new PreviewThread(this);
    mPreviewThread->run("CameraPreviewThread", PRIORITY_URGENT_DISPLAY);

    LOG_FUNCTION_NAME_EXIT
}

CameraHal::~CameraHal()
{
    int err = 0;

    LOG_FUNCTION_NAME

    if(mPreviewThread != NULL) {
        Message msg;
        msg.command = PREVIEW_KILL;
        previewThreadCommandQ.put(&msg);
        previewThreadAckQ.get(&msg);
    }

    // don't hold the lock while waiting for the thread to quit
    if (mPreviewThread != 0) {
        mPreviewThread->requestExitAndWait();
        mPreviewThread.clear();
    }
    
    mPreviewRunning = false;
    mPreviewNotReady = false;
    mRecordRunning = false;
    mRecordOverlay = false;

    mCallbackCookie = NULL;

    if (mOverlay != NULL) {
        mOverlay->destroy();
        mOverlay = NULL;
        nOverlayBuffersQueued = 0;
    }
    
    CameraDestroy();

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::initDefaultParameters()
{
    LOG_FUNCTION_NAME

    CameraParameters p;
        
    mPreviousWB = 1;
    mPreviousEffect = 1;
    mPreviousAntibanding = 0;
    mPreviousSceneMode = 1;
    mPreviousFlashMode = 1;
    mPreviousExposure = 5;
    mPreviousZoom = 0;
    mPreviousIso = 0;
    mPreviousContrast = 4;
    mPreviousSaturation = 4;
    mPreviousSharpness = 4;
    mPreviousWdr = 1;
    mPreviousAntiShake = 1;
    mPreviousFocus = 1;
    mPreviousMetering = 2;
    mPreviousPretty = 1;
    mPreviousQuality = 100;
    mPreviousAngle = 0;
    mPreviousFlag = 1;
    mPreviousGPSLatitude = 0;
    mPreviousGPSLongitude = 0;
    mPreviousGPSAltitude = 0;
    mPreviousGPSTimestamp = 0;

    if (mCameraIndex == MAIN_CAMERA) {
        //Support Info
        p.set(p.KEY_SUPPORTED_PREVIEW_SIZES, "320x240,640x480");
        p.set(p.KEY_SUPPORTED_PREVIEW_FORMATS, "yuv422i-uyvy,yuv420sp");
        p.set(p.KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(7000,35000)");
        p.set(p.KEY_SUPPORTED_PREVIEW_FRAME_RATES, "30");
        p.set(p.KEY_SUPPORTED_PICTURE_SIZES, "2560x1920,2048x1536,1600x1200,1280x960");
        p.set(p.KEY_SUPPORTED_PICTURE_FORMATS, "jpeg");
        p.set(p.KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, "160x120,0x0");    
        p.set(p.KEY_SUPPORTED_WHITE_BALANCE, "auto,daylight,cloudy-daylight,incandescent,fluorescent");
        p.set(p.KEY_SUPPORTED_ANTIBANDING, "off");
        p.set(p.KEY_SUPPORTED_EFFECTS, "none,purple,negative,sepia,aqua,green,blue,pink,yellow,mono,red,");
        p.set(p.KEY_SUPPORTED_SCENE_MODES,"auto,sunset,candlelight,beach,text,night,landscape,fireworks,portrait,fall,party,action");    
        p.set(p.KEY_SUPPORTED_FLASH_MODES, "off,on,auto,torch");
        p.set(p.KEY_SUPPORTED_FOCUS_MODES, "auto,macro");
        p.set(p.KEY_ZOOM_SUPPORTED, "true");
    
        //Preview
        p.setPreviewSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
        p.set(p.KEY_PREVIEW_FPS_RANGE, "7000,30000");
        p.set(p.KEY_PREVIEW_FORMAT, "yuv420sp");
    
        //Picture
        p.setPictureSize(PICTURE_WIDTH, PICTURE_HEIGHT);
        p.set(p.KEY_PICTURE_FORMAT, "jpeg");
        p.set(p.KEY_JPEG_QUALITY, "100");    

        //Video
        p.set(p.KEY_VIDEO_FRAME_FORMAT, "yuv422i-yuyv");
    
        //Thumbnail
        p.set(p.KEY_JPEG_THUMBNAIL_WIDTH, "160");
        p.set(p.KEY_JPEG_THUMBNAIL_HEIGHT, "120");
        p.set(p.KEY_JPEG_THUMBNAIL_QUALITY, "100");   
    
        //Setting
        p.set(p.KEY_MAX_ZOOM, "12");
        p.set(p.KEY_ZOOM, "0");    
        p.set(p.KEY_ZOOM_RATIOS, "100,125,150,175,200,225,250,275,300,325,350,375,400");
        p.set(p.KEY_EFFECT, "none");
        p.set(p.KEY_WHITE_BALANCE, "auto");
        p.set(p.KEY_ANTIBANDING, "off");
        p.set(p.KEY_SCENE_MODE, "auto");
        p.set(p.KEY_FLASH_MODE, "off");
        p.set(p.KEY_FOCUS_MODE, "auto");
        p.set(p.KEY_FOCAL_LENGTH, "3.546");
        p.set(p.KEY_HORIZONTAL_VIEW_ANGLE, "51.2");
        p.set(p.KEY_VERTICAL_VIEW_ANGLE, "39.4");    
        p.set(p.KEY_FOCUS_DISTANCES, "0.10,1.20,Infinity");
        p.set(p.KEY_EXPOSURE_COMPENSATION, "0");
        p.set(p.KEY_MAX_EXPOSURE_COMPENSATION, "4");
        p.set(p.KEY_MIN_EXPOSURE_COMPENSATION, "-4");
        p.set(p.KEY_EXPOSURE_COMPENSATION_STEP, "1");
        p.set(p.KEY_ROTATION, "0");
        p.set(p.KEY_GPS_LATITUDE, "0");
        p.set(p.KEY_GPS_LONGITUDE, "0");
        p.set(p.KEY_GPS_ALTITUDE, "0");
        p.set(p.KEY_GPS_TIMESTAMP, "0");
        p.set(p.KEY_GPS_PROCESSING_METHOD, "GPS");
    
        //Add OEM
        p.set("camera-id", mCameraIndex);
        p.set("exposure-compensation", "0");
        p.set("iso","0");
        p.set("contrast","4");
        p.set("saturation","4");
        p.set("sharpness","4");
        p.set("wdr","1");
        p.set("anti-shake","0");
        p.set("metering","center");    
        p.set("videoinput","0");
        p.set("CamcorderPreviewMode","0");
        p.set("vtmode","0");
        p.set("pretty","1");
        p.set("previewframeskip","0");
        p.set("twosecondreviewmode","0");
        p.set("previewflashon","0");
        p.set("setcrop","0");
        p.set("samsungcamera","0");
        p.set("previewframeskip", "0");
    } else {
        //Support Info
        p.set(p.KEY_SUPPORTED_PREVIEW_SIZES, "320x240,640x480");
        p.set(p.KEY_SUPPORTED_PREVIEW_FORMATS, "yuv422i-uyvy,yuv420sp");
        p.set(p.KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(7000,30000)");
        p.set(p.KEY_SUPPORTED_PREVIEW_FRAME_RATES, "15");
        p.set(p.KEY_SUPPORTED_PICTURE_SIZES, "640x480");
        p.set(p.KEY_SUPPORTED_PICTURE_FORMATS, "jpeg");
        p.set(p.KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, "160x120,0x0");    
        p.set(p.KEY_SUPPORTED_WHITE_BALANCE, "auto,daylight,cloudy-daylight,incandescent,fluorescent");
        p.set(p.KEY_SUPPORTED_ANTIBANDING, "off");
        p.set(p.KEY_SUPPORTED_EFFECTS, "none,negative,sepia,aqua,green-tint,mono,red-tint");        
        p.set(p.KEY_SUPPORTED_FLASH_MODES, "off");
        p.set(p.KEY_SUPPORTED_FOCUS_MODES, "fixed");
        p.set(p.KEY_ZOOM_SUPPORTED, "false");
    
        //Preview
        p.setPreviewSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
        p.set(p.KEY_PREVIEW_FPS_RANGE, "7000,30000");
        p.set(p.KEY_PREVIEW_FORMAT, "yuv420sp");
    
        //Picture
        p.setPictureSize(PREVIEW_WIDTH, PREVIEW_HEIGHT);
        p.set(p.KEY_PICTURE_FORMAT, "jpeg");
        p.set(p.KEY_JPEG_QUALITY, "100");    

        // Video
        p.set(p.KEY_VIDEO_FRAME_FORMAT, "yuv422i-yuyv");
    
        //Thumbnail
        p.set(p.KEY_JPEG_THUMBNAIL_WIDTH, "160");
        p.set(p.KEY_JPEG_THUMBNAIL_HEIGHT, "120");
        p.set(p.KEY_JPEG_THUMBNAIL_QUALITY, "100");   
    
        //Setting
        p.set(p.KEY_ZOOM, "0");    
        p.set(p.KEY_EFFECT, "none");
        p.set(p.KEY_WHITE_BALANCE, "auto");    
        p.set(p.KEY_ANTIBANDING, "off");
        p.set(p.KEY_FLASH_MODE, "off");
        p.set(p.KEY_FOCUS_MODE,"fixed");
        p.set(p.KEY_FOCAL_LENGTH, "1.300");
        p.set(p.KEY_HORIZONTAL_VIEW_ANGLE, "51.2");
        p.set(p.KEY_VERTICAL_VIEW_ANGLE, "39.4"); 
        p.set(p.KEY_FOCUS_DISTANCES, "0.20,0.25,Infinity");
        p.set(p.KEY_EXPOSURE_COMPENSATION, "0");
        p.set(p.KEY_MAX_EXPOSURE_COMPENSATION, "4");
        p.set(p.KEY_MIN_EXPOSURE_COMPENSATION, "-4");
        p.set(p.KEY_EXPOSURE_COMPENSATION_STEP, "1");
        p.set(p.KEY_ROTATION, "0");
        p.set(p.KEY_GPS_LATITUDE, "0");
        p.set(p.KEY_GPS_LONGITUDE, "0");
        p.set(p.KEY_GPS_ALTITUDE, "0");
        p.set(p.KEY_GPS_TIMESTAMP, "0");
        p.set(p.KEY_GPS_PROCESSING_METHOD, "GPS");
    
        //Add OEM
        p.set("camera-id", mCameraIndex);
        p.set("exposure-compensation", "0");
        p.set("videoinput", "0");
        p.set("CamcorderPreviewMode", "0");
        p.set("vtmode", "0");
        p.set("pretty", "1");
        p.set("previewframeskip", "0");
        p.set("twosecondreviewmode", "0");
        p.set("previewflashon", "0");
        p.set("setcrop", "0");
        p.set("samsungcamera", "0");
        p.set("previewframeskip", "0");
    }

    p.set("AppShutterSound", 0); // for 3'rd party app
   
    setParameters(p);

    if (mCameraIndex == MAIN_CAMERA) {
        setWB(getWBLighting());
        setEffect(getEffect());
        setSceneMode(getSceneMode());
        setFlashMode(getFlashMode());
        setExposure(getExposure());
        setZoom(getZoomValue());
        setISO(getISO());
        setContrast(getContrast());
        setSaturation(getSaturation());
        setSharpness(getSharpness());
        setWDRMode(getWDRMode());
        setAntiShakeMode(getAntiShakeMode());
        setFocusMode(getFocusMode());
        setMetering(getMetering());
    } else {
        setWB(getWBLighting());
        setEffect(getEffect());    
        setFlashMode(getFlashMode());
        setFocusMode(getFocusMode());
        setPrettyMode(getPrettyValue());
        setBrightness(getBrightness());
    }
    
    setRotate(getRotate());
    setJpegMainimageQuality(getJpegMainimageQuality());
    setJpegThumbnailSize(getJpegThumbnailSize());
    setGPSLatitude(getGPSLatitude());
    setGPSLongitude(getGPSLongitude());
    setGPSAltitude(getGPSAltitude());
    setGPSTimestamp(getGPSTimestamp());
    setGPSProcessingMethod(getGPSProcessingMethod());

    LOG_FUNCTION_NAME_EXIT
}


static void debugShowFPS()
{
    static int mFrameCount = 0;
    static int mLastFrameCount = 0;
    static nsecs_t mLastFpsTime = 0;
    static float mFps = 0;
    
    if (mFrameCount++ == 150) {
        nsecs_t now = systemTime();
        nsecs_t diff = now - mLastFpsTime;
        mFps =  (mFrameCount * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        LOGI("preview fps=%2.2f", mFps);
        mFrameCount = 0;
    }

    // XXX: mFPS has the value we want
}

static void debugRecordingFPS()
{
    static int mFrameCount = 0;
    static int mLastFrameCount = 0;
    static nsecs_t mLastFpsTime = 0;
    static float mFps = 0;

    if (mFrameCount++ == 150) 
    {
        nsecs_t now = systemTime();
        nsecs_t diff = now - mLastFpsTime;
        mFps =  (mFrameCount * float(s2ns(1))) / diff;
        mLastFpsTime = now;
        LOGI("recording fps=%2.2f", mFps);
        mFrameCount = 0;
    }

    // XXX: mFPS has the value we want
}
void CameraHal::previewThread()
{
    Message  msg;
    bool     shouldLive = true;
    bool     has_message = false;
    int      frameCount = 0 ;

    struct v4l2_control vc;
    CLEAR(vc);    

    LOG_FUNCTION_NAME

    while(shouldLive) {    

        // init message command 
        memset(&msg, 0, sizeof(msg));
        
        has_message = false;
            
        if( mPreviewRunning ) {            
            if(mAFState) {
                vc.id = V4L2_CID_AF;
                vc.value = 0;
                if (ioctl (camera_device, VIDIOC_G_CTRL, &vc) < 0) {
                    LOGE("getautofocus fail\n");
                }

                if(vc.value == 2) {
                    mNotifyCb(CAMERA_MSG_FOCUS,true,0,mCallbackCookie);
                    mAFState = false;
                } else if(vc.value == 3) {
                    mNotifyCb(CAMERA_MSG_FOCUS,false,0,mCallbackCookie);
                    mAFState = false;
                }
            } 
#ifndef NOWPLUS_MOD
			else {
                if(mASDMode) {
                    frameCount++;
                    if(!(frameCount%20)) {
                        vc.id = V4L2_CID_SCENE;
                        vc.value = 0;
                        if (ioctl (camera_device, VIDIOC_G_CTRL, &vc) < 0)
                            LOGE("getscene fail\n");

                        mNotifyCb(CAMERA_MSG_ASD,vc.value,0,mCallbackCookie);
                        frameCount = 0;
                    }
                    
                }
            }
#endif
            //process 1 preview frame
            nextPreview();

            // Get message command
            if( !previewThreadCommandQ.isEmpty() ) {
                previewThreadCommandQ.get(&msg);
                has_message = true;           
            }
        } else {
            // Get message command
            previewThreadCommandQ.get(&msg);
            has_message = true;
        }

        if(!has_message) {
            continue;
        }

        switch(msg.command) {
        case PREVIEW_START:
            LOGD("Receive Command: PREVIEW_START\n");   
            if(camera_device < 0) {
                LOGD("WARNING! DEVICE NOT OPEN!\n");
                msg.command = PREVIEW_NACK;
            } else if(!mPreviewRunning) {
#ifdef TIMECHECK
                PPM("CONFIGURING CAMERA TO RESTART PREVIEW\n");
#endif
                    if (CameraStart() < 0) {
                        LOGE("ERROR CameraStart()\n");      
                        msg.command = PREVIEW_NACK;
                    } else {
                        mPreviewRunning = true;
                        msg.command = PREVIEW_ACK;
                    }
#ifdef TIMECHECK
                PPM("PREVIEW STARTED AFTER CAPTURING\n");
#endif
            } else {
                LOGE("Preview already running!\n"); 
                msg.command = PREVIEW_ACK;
            }
            LOGD("PREVIEW_START %s\n", msg.command == PREVIEW_NACK ? "Fail" : "Success!");
            previewThreadAckQ.put(&msg);
            break;

        case PREVIEW_STOP:
            LOGD("Receive Command: PREVIEW_STOP\n");
            if(camera_device < 0) {
                LOGD("WARNING! DEVICE NOT OPEN!\n");
                msg.command = PREVIEW_NACK;
            } else if(mPreviewRunning) {
                if( CameraStop() < 0) {
                    LOGE("ERROR CameraStop()\n"); 
                    msg.command = PREVIEW_NACK;
                } else  {
                    msg.command = PREVIEW_ACK;
                }
            } else {
                LOGE("Preview already stop!\n");
                msg.command = PREVIEW_ACK;
            }
            LOGD("PREVIEW_STOP %s\n", msg.command == PREVIEW_NACK ? "Fail" : "Success!");
            previewThreadAckQ.put(&msg);
            break;

        case PREVIEW_AF_START:
            LOGD("Receive Command: PREVIEW_AF_START\n");
            if(camera_device < 0) {
                LOGD("WARNING! DEVICE NOT OPEN!\n");
                msg.command = PREVIEW_NACK;
            } else if (!mPreviewRunning) {
                LOGD("Preview not yet!\n");
                msg.command = PREVIEW_ACK;            
            } else {
                if(mMsgEnabled & CAMERA_MSG_FOCUS) {
                    vc.id = V4L2_CID_AF;
                    vc.value = AF_START;
                    if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {   
                        LOGE("setautofocus fail\n");                        
                        mAFState = false;  
                        msg.command = PREVIEW_NACK;
                    } else {
                        mAFState = true;
                        msg.command = PREVIEW_ACK;
                    }
                } else {
                    msg.command = PREVIEW_ACK;
                }
            }
            LOGD("PREVIEW_AF_START %s\n", msg.command == PREVIEW_NACK ? "Fail" : "Success!");             
            previewThreadAckQ.put(&msg);
            break;

        case PREVIEW_AF_CANCEL:
            LOGD("Receive Command: PREVIEW_AF_CANCEL\n");
            if(camera_device < 0) {
                LOGD("WARNING! DEVICE NOT OPEN!\n");
                msg.command = PREVIEW_NACK;
            } else if (!mPreviewRunning) {
                LOGD("Preview not yet!\n");
                msg.command = PREVIEW_ACK;            
            } else {
                vc.id = V4L2_CID_AF;
                vc.value = AF_STOP;
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("release autofocus fail\n");
                    msg.command = PREVIEW_NACK;
                } else {
                    mAFState = false; 
                    msg.command = PREVIEW_ACK;               
                }
            }
            LOGD("PREVIEW_AF_CANCEL %s\n", msg.command == PREVIEW_NACK ? "Fail" : "Success!");             
            previewThreadAckQ.put(&msg);
            break;

        case PREVIEW_CAPTURE:
            LOGD("Receive Command: PREVIEW_CAPTURE\n");
#ifdef TIMECHECK
            PPM("RECEIVED COMMAND TO TAKE A PICTURE\n");
#endif
            //In burst mode the preview is not reconfigured between each picture 
            //so it can not be based on it to decide wheter the state is incorrect or not
            if(camera_device < 0) {
                LOGE("WARNING DEVICE NOT OPEN!\n");
                msg.command = CAPTURE_NACK;
            } else if( mCaptureFlag ) {
                msg.command = PREVIEW_ACK;
                LOGE("Capture already start!\n");
            } else {
                //capture flag enable
                mCaptureFlag = 1;
                if(mPreviewRunning) {
                    if( CameraStop() < 0) {
                        LOGE("ERROR CameraStop()\n");
                    }          
                }

#ifdef TIMECHECK
                PPM("STOPPED PREVIEW\n");
#endif
                if( startCapture() < 0) {
                    LOGE("ERROR startCapture()\n");    
                    msg.command = CAPTURE_NACK;
                } else {
                    msg.command = CAPTURE_ACK;
                }
            }
            LOGD("PREVIEW_CAPTURE %s\n", msg.command == PREVIEW_NACK ? "Fail" : "Success!");  
            previewThreadAckQ.put(&msg);  
            break;

        case PREVIEW_CAPTURE_CANCEL:
            LOGD("Receive Command: PREVIEW_CAPTURE_CANCEL\n");
            if(camera_device < 0) {
                LOGE("WARNING DEVICE NOT OPEN!\n");
                msg.command = PREVIEW_NACK;
            } else {
                msg.command = PREVIEW_ACK;
            }            
            LOGD("PREVIEW_CAPTURE_CANCEL %s\n", msg.command == PREVIEW_NACK ? "Fail" : "Success!"); 
            previewThreadAckQ.put(&msg); 
            break;

        case PREVIEW_KILL:
            LOGD("Receive Command: PREVIEW_KILL\n");
            shouldLive = false;
            if(camera_device < 0) {
                LOGD("WARNING! DEVICE NOT OPEN!\n");
                msg.command = PREVIEW_NACK;
            } else if(mPreviewRunning) {
                if( CameraStop() < 0) {
                    LOGE("ERROR CameraStop()\n");
                    msg.command = PREVIEW_NACK;
                } else {
                    msg.command = PREVIEW_ACK;
                }
            } else {
                msg.command = PREVIEW_ACK;
            }
            LOGD("PREVIEW_KILL %s\n", msg.command == PREVIEW_NACK ? "Fail!" : "Success!"); 
            previewThreadAckQ.put(&msg);
            break;
        }
    }

    LOG_FUNCTION_NAME_EXIT
}

int CameraHal::CameraCreate()
{
    int err = 0;

    LOG_FUNCTION_NAME    
    
    if(!camera_device) {   
        if(mCameraIndex == MAIN_CAMERA) {
            camera_device = open(VIDEO_DEVICE, O_RDWR);
            mCamera_Mode = CAMERA_MODE_JPEG;        
        } else {
            camera_device = open(VIDEO_DEVICE5, O_RDWR);
            mCamera_Mode = CAMERA_MODE_YUV;
        }
    
        if (camera_device < 0) {
            LOGE ("Could not open the camera device: %s\n",  strerror(errno) );
            err = -1;
            goto exit;
        }
    } else {
        LOGE ("Already open the camera device.\n");
        err = -1;
        goto exit;        
    }

#if OMAP_SCALE_CDD
    if(mCameraIndex == VGA_CAMERA) {
        if(!isStart_Scale) {
            if ( scale_init(PREVIEW_WIDTH, PREVIEW_HEIGHT, PREVIEW_WIDTH, PREVIEW_HEIGHT, PIX_YUV422I, PIX_YUV422I) < 0 ) {
                LOGE("scale_init() failed");
                err = -1;
                goto exit; 
            } else {
                isStart_Scale = true;
                LOGD("scale_init() Done");
            }
        }

#if USE_NEON
        if(!isStart_Rotate) {
            pTIrtn = NULL;
            Neon_Rotate = NULL;
            neon_args = NULL;

            //Get the handle of rotation shared library.      
            pTIrtn = dlopen("librotation.so", RTLD_LOCAL | RTLD_LAZY);
            if (!pTIrtn) {
                LOGE("Open NEON Rotation Library Failed \n");
                err = -1;
                goto exit;
            } else {
                LOGD("Loaded NEON Rotation Library\n");
            }
            
            Neon_Rotate = (NEON_fpo) dlsym(pTIrtn, "Neon_RotateCYCY");            
            if (Neon_Rotate == NULL) {
                LOGE("Couldnot find  Neon_RotateCYCY symbol\n");
                dlclose(pTIrtn);
                pTIrtn = NULL;
                err = -1;
                goto exit;
            } 
            
            neon_args = (NEON_FUNCTION_ARGS*)malloc(sizeof(NEON_FUNCTION_ARGS));                
            if(neon_args == NULL) {
                LOGE("Couldnot mem allocation Neon_arg\n");
                goto exit;
            }

            isStart_Rotate = true;
        }
#endif        
    }
#endif

#if JPEG_ENCODER
    if( ICaptureCreate() < 0) {
        LOGE("ICaptureCreate() fail!!\n");
        err = -1;
        goto exit;
    } else {
        LOGD("ICaptureCreate() OK\n");
    }
#endif    

#if VPP_THREAD
    if (pthread_mutexattr_init(&attr) != 0) {
        LOGE("Failed to initialize overlay mutex attr\n");
    }
    
    if (pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) != 0) {
       LOGE("Failed to set the overlay mutex attr to be shared across-processes\n");
    }

    if (pthread_mutex_init(&lock, &attr) != 0) {
        LOGE("Failed to initialize overlay mutex\n");
    }

    if( pipe(vppPipe) != 0 ) {
        LOGE("Failed creating pipe\n");
    }
#endif

    LOG_FUNCTION_NAME_EXIT
        
    return NO_ERROR;

exit:
    return err;
}


int CameraHal::CameraDestroy()
{
    int err = 0;

    LOG_FUNCTION_NAME

#if VPP_THREAD
#if VPP_CONVERSION
    if(close(vppPipe[0]) != 0) {
        LOGE ("Error:vppPipe failed to close 0\n");
    }

    if(close(vppPipe[1]) != 0) {
        LOGE ("Error:vppPipe failed to close 1\n");
    }

    if (pthread_mutex_destroy(&lock)) {
        LOGE("Failed to uninitialize overlay mutex!\n");
    }

    if (pthread_mutexattr_destroy(&attr)) {
        LOGE("Failed to uninitialize the overlay mutex attr!\n");
    }
#endif
#endif

#if JPEG_ENCODER
    if( ICaptureDestroy() < 0) {
        LOGE("ICaptureDestroy() fail!!\n");
    } else {
        LOGD("ICaptureDestroy() OK\n");
    }
#endif    

#if OMAP_SCALE_CDD
    if(mCameraIndex == VGA_CAMERA) {
#if USE_NEON
        if(isStart_Rotate) {
            if(neon_args) {
                free((NEON_FUNCTION_ARGS *)neon_args);
                neon_args = NULL;
            }   

            if(Neon_Rotate) {
                Neon_Rotate = NULL;
            }

            if(pTIrtn != NULL) {
                LOGD("Unloaded NEON Rotation Library\n");
                dlclose(pTIrtn);  
                pTIrtn = NULL;
            }
            isStart_Rotate = false;
        }
#endif        
        if(isStart_Scale) {
            err = scale_deinit();
            if( err ) {
                LOGE("scale_deinit() failed\n");
            } else {
                LOGD("scale_deinit() OK\n");
            }        
            isStart_Scale = false;
        }
    }
#endif

    if(camera_device) {
        close(camera_device);
        camera_device = NULL;
    }

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;

exit:
    return err;    
}

int CameraHal::CameraConfigure()
{
    struct v4l2_format format;
    
    int camcorderMode     = getCamcorderPreviewValue(); // Camera/Camcorder
    int mVTMode           = getVTMode(); //Vtmode
    int samsungCameraMode = getSamsungCameraValue(); // Samsung/3rd Party

    int err = 0;

    LOG_FUNCTION_NAME    

    switch(camcorderMode) {
    case 0:
        if(mVTMode) // 1: HDVT, 2: VT
            mCamMode = VT_MODE;
        else {
            if(samsungCameraMode)
                mCamMode = CAMERA_MODE;
            else
                mCamMode = Trd_PART;
        }
            break;

    case 1:
        if(mVTMode) // 1: HDVT, 2: VT
            mCamMode = VT_MODE;
        else {
            if(samsungCameraMode)
                mCamMode = CAMCORDER_MODE;
            else
                mCamMode = Trd_PART;
        }
        break;

    default:
        LOGE ("invalid value \n");
        break;
    }
    HAL_PRINT("\nHAL_DBG: CameraConfigure (mCamMode : %d)\n", mCamMode);

    mParameters.getPreviewSize(&mPreviewWidth, &mPreviewHeight);
    HAL_PRINT("\nHAL_DBG: CameraConfigure (Preview Size : %d X %d)\n", mPreviewWidth, mPreviewHeight);

    /* Set 720P */
    if ((mPreviewWidth == HD_WIDTH) && (mPreviewHeight == HD_HEIGHT)) {
        mOverlay->setParameter(CACHEABLE_BUFFERS, 1);
        mOverlay->setParameter(MAINTAIN_COHERENCY, 0);
        mOverlay->resizeInput(mPreviewWidth, mPreviewHeight);
    }
        
    /* Set preview format */
    if(mCamMode == VT_MODE && (mPreviousAngle == 90 || mPreviousAngle == 270)) {
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = mPreviewHeight;
        format.fmt.pix.height = mPreviewWidth;
        format.fmt.pix.pixelformat = PIXEL_FORMAT_UYVY;
    } else {
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = mPreviewWidth;
        format.fmt.pix.height = mPreviewHeight;
        format.fmt.pix.pixelformat = PIXEL_FORMAT_UYVY;                 
    }
    HAL_PRINT("\nHAL_DBG: CameraConfigure (mPreviousAngle : %d)\n", mPreviousAngle);

    if (setPreviewFrameRate()) {
        LOGE("Error in setting Camera frame rate\n");
        return -1;
    }
    
    if ( ioctl(camera_device, VIDIOC_S_FMT, &format) < 0 ) {
        LOGE ("Failed to set VIDIOC_S_FMT.\n");
        return -1;
    }
    HAL_PRINT("CameraConfigure PreviewFormat: w=%d h=%d\n", format.fmt.pix.width, format.fmt.pix.height);

    LOG_FUNCTION_NAME_EXIT
        
    return NO_ERROR;
}

int CameraHal::CameraStart()
{
    struct v4l2_format format;
    struct v4l2_requestbuffers creqbuf;
    
    enum v4l2_buf_type type;

    int i;
    int err;
    int nSizeBytes;
    int mPreviewFrameSizeConvert;
#if OMAP_SCALE_CDD   
    int mCropSize; 
#endif

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    mVideoBufferCount = mOverlay->getBufferCount();
    HAL_PRINT("number of buffers = %d\n", mVideoBufferCount);

    //clearing heap
    mVideoConversionHeap.clear();
    mVideoConversionHeap = 0;

    for(i = 0; i < mVideoBufferCount; i++) {
        mVideoConversionBuffer[i].clear();
        mVideoConversionBuffer[i] = 0;            

        buffers_queued_to_dss[i] = 0;      
    }

    nCameraBuffersQueued = 0;  
    nOverlayBuffersQueued = 0;   

    mPassedFirstFrame = false;

    if (CameraConfigure() < 0) {
        LOGE("ERROR CameraConfigure()\n");                    
        goto fail_config;
    }       

    mPreviewFrameSize = mPreviewWidth*mPreviewHeight*2;
    if (mPreviewFrameSize & 0xfff) {
        mPreviewFrameSize = (mPreviewFrameSize & 0xfffff000) + 0x1000;
    }
    
#if OMAP_SCALE_CDD
    if((mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE)){           
        mCropSize = (mPreviewHeight*3/4)*mPreviewHeight*2;
        mVideoHeap_422 = new MemoryHeapBase(mVideoBufferCount * mPreviewFrameSize);        
        mVideoHeap_422_crop = new MemoryHeapBase(mVideoBufferCount * mCropSize);
    }
#endif

    // YUV420 conversion buffer
    mPreviewFrameSizeConvert = mPreviewWidth*mPreviewHeight*3/2;
    mVideoConversionHeap = new MemoryHeapBase(mVideoBufferCount * mPreviewFrameSizeConvert);
    for(i = 0; i < mOverlay->getBufferCount(); i++) { 
        mVideoConversionBuffer[i] = new MemoryBase(mVideoConversionHeap,
                                        mPreviewFrameSizeConvert *i, mPreviewFrameSizeConvert );
    }

    // YUV422 preview buffer 
    creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    creqbuf.memory = V4L2_MEMORY_USERPTR;
    creqbuf.count  = mVideoBufferCount ; 
    if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0) {
        LOGE ("VIDIOC_REQBUFS Failed. %s\n", strerror(errno));
        goto fail_reqbufs;
    }

    for (i = 0; i < (int)creqbuf.count; i++) {
        v4l2_cam_buffer[i].type = creqbuf.type;
        v4l2_cam_buffer[i].memory = creqbuf.memory;
        v4l2_cam_buffer[i].index = i;
        if (ioctl(camera_device, VIDIOC_QUERYBUF, &v4l2_cam_buffer[i]) < 0) {
            LOGE("VIDIOC_QUERYBUF Failed\n");
            goto fail_loop;
        }
        
        mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress((void*)i);
        if ( data == NULL ) 
        {
            LOGE(" getBufferAddress returned NULL\n");
            goto fail_loop;
        }

#if OMAP_SCALE_CDD
        if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {                       
            mCropSize = (mPreviewHeight*3/4)*mPreviewHeight*2;
            mVideoBuffer_422[i] = new MemoryBase(mVideoHeap_422, i * mPreviewFrameSize, mPreviewFrameSize);                
            mVideoBuffer_422_crop[i] = new MemoryBase(mVideoHeap_422_crop, i * mCropSize, mCropSize);          
                    
            //Map Overlay Buffers for Preview
            mVideoHeap_422_overlay[i]  = new MemoryHeapBase(data->fd, mPreviewFrameSize, 0, data->offset);
            mVideoBuffer_422_overlay[i] = new MemoryBase(mVideoHeap_422_overlay[i], 0, mPreviewFrameSize);

            //Assign Pointer        
            v4l2_cam_buffer[i].m.userptr = (long unsigned int)mVideoBuffer_422[i]->pointer();       
        } else
#endif
        v4l2_cam_buffer[i].m.userptr = (unsigned long) data->ptr;

        strcpy((char *)v4l2_cam_buffer[i].m.userptr, "hello");
        if (strcmp((char *)v4l2_cam_buffer[i].m.userptr, "hello")) 
        {
            LOGI("problem with buffer\n");
            goto fail_loop;
        }

        HAL_PRINT("User Buffer [%d].start = %p  length = %d\n", i,
             (void*)v4l2_cam_buffer[i].m.userptr, v4l2_cam_buffer[i].length);
        if (buffers_queued_to_dss[i] == 0) {
            nCameraBuffersQueued++;
            if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[i]) < 0) {
                LOGE("CameraStart VIDIOC_QBUF Failed: %s\n", strerror(errno) );
                goto fail_loop;
            }
        } else {
            LOGE("CameraStart::Could not queue buffer %d to Camera because it is being held by Overlay\n", i);
        }    

        if(!data->nQueueToOverlay) {
             LOGV("Buffer [%d] not with overlay Stats %d ",i,data->nQueueToOverlay);
        } else {
             LOGD("Buffer [%d] With overlay Stats %d", i,data->nQueueToOverlay);
             buffers_queued_to_dss[i] = 1;
             nOverlayBuffersQueued++;
        }
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    err = ioctl(camera_device, VIDIOC_STREAMON, &type);
    if ( err < 0) {
        LOGE("VIDIOC_STREAMON Failed\n");
        goto fail_loop;
    }

    LOG_FUNCTION_NAME_EXIT

    return 0;

fail_loop:
fail_reqbufs:
fail_config:

    return -1;
}

int CameraHal::CameraStop()
{   
    int err = NO_ERROR;
    int i = 0;
    
    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    mPreviewRunning = false;
    mPreviewNotReady = false;
    mRecordRunning = false;

    if(getPreviewFlashOn() == 1) {
        HAL_PRINT("flash mode off!!\n");
        setMovieFlash(FLASH_MOVIE_OFF);
    }

#if OMAP_SCALE_CDD      
    if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {        
        for(i = 0; i < VIDEO_FRAME_COUNT_MAX; i++) {     
            mVideoBuffer_422[i].clear();
            mVideoBuffer_422[i] = 0;

            mVideoBuffer_422_crop[i].clear();
            mVideoBuffer_422_crop[i] = 0;
            
            mVideoBuffer_422_overlay[i].clear();
            mVideoBuffer_422_overlay[i] = 0; 

            mVideoHeap_422_overlay[i].clear();
            mVideoHeap_422_overlay[i] = 0;   
        }
        
        mVideoHeap_422.clear();
        mVideoHeap_422 = 0;
        mVideoHeap_422_crop.clear();
        mVideoHeap_422_crop = 0;           
    }
#endif

    //clearing heap
    mVideoConversionHeap.clear();
    mVideoConversionHeap = 0;

    for(i = 0; i < VIDEO_FRAME_COUNT_MAX; i++) {
        mVideoConversionBuffer[i].clear();
        mVideoConversionBuffer[i] = 0;            

        buffers_queued_to_dss[i] = 0;      
    }

    if(mRecordOverlay) {
        HAL_PRINT("Clear the old memory \n");
        for(i = 0; i < VIDEO_FRAME_COUNT_MAX; i++) {
            mVideoBuffer[i].clear();
            mVideoBuffer[i] = 0;
            
            mVideoHeaps[i].clear();
            mVideoHeaps[i]  = 0;  
        }        
    }
    
    if (mOverlay != NULL && (mRecordOverlay == true || mCamMode == VT_MODE)) {
        mRecordOverlay = false;
        mOverlay->destroy();
        mOverlay = NULL;
    }     

    nCameraBuffersQueued = 0;  
    nOverlayBuffersQueued = 0;   

    //stream off
    struct v4l2_requestbuffers creqbuf;
    creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) < 0) {
        LOGE("VIDIOC_STREAMOFF Failed\n");
        err = -1;
    } 

    LOG_FUNCTION_NAME_EXIT
        
    return err;
}

void CameraHal::nextPreview()
{
    overlay_buffer_t overlaybuffer;// contains the index of the buffer dque

    int nBuffers_queued_to_dss;
    int buffer_count;

    mapping_data_t* data = NULL;

    bool queueBufferCheck = true;
    
#ifdef SKIP_FRAME
    // Normal Flow send the frame for display
    mCounterSkipFrame = mSkipFrameNumber;
#endif

    struct v4l2_buffer cfilledbuffer;
    cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    cfilledbuffer.memory = V4L2_MEMORY_USERPTR;

#if defined (SKT_FMC_DM) || defined (EAS_IT_POLICY)
    if(mCamMode == Trd_PART || mCamMode == VT_MODE) {
        if(!mSkipFrameCount) {
            mSkipFrameCount = mParameters.getPreviewFrameRate();
            LOGE("nextPreview() mSkipFrameCount = %d\n", mSkipFrameCount);

#ifdef SKT_FMC_DM
            if(getSecurityCheck()) 
                mCurSecurity = true;
            else
                mCurSecurity = false; 
#endif            
            
#ifdef EAS_IT_POLICY
            if(getSamsungSecurityCheck())
                mCurSecurity = true;
            else
                mCurSecurity = false;
#endif            
        } else if(mSkipFrameCount < 0) {
            mSkipFrameCount = 0;
            LOGE("nextPreview() mSkipFrameCount = %d\n", mSkipFrameCount);
        } else {
            if(mSkipFrameCount > 0) {
                mSkipFrameCount--;
            }
        }
    }
#endif

    /* De-queue the next avaliable buffer */
    if (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) {
        LOGE("VIDIOC_DQBUF Failed!!!\n");
        if(mPassedFirstFrame)
            mNotifyCb(CAMERA_MSG_ERROR,CAMERA_DEVICE_ERROR_FOR_RESTART,0,mCallbackCookie);
        else
            mNotifyCb(CAMERA_MSG_ERROR,CAMERA_DEVICE_ERROR_FOR_EXIT,0,mCallbackCookie);
        return;
    } else {
        if(nCameraBuffersQueued > 0) {
            nCameraBuffersQueued--;
        }
    }

#if OMAP_SCALE_CDD
    if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {        
        yuv_buffer = (unsigned char*) mVideoBuffer_422[cfilledbuffer.index]->pointer();
        crop_buffer = (unsigned char*) mVideoBuffer_422_crop[cfilledbuffer.index]->pointer();
        scale_buffer = (unsigned char*) mVideoBuffer_422_overlay[cfilledbuffer.index]->pointer();            

        // Crop and Upscale
        if(scale_process_preview(yuv_buffer, mPreviewWidth, mPreviewHeight, crop_buffer, (mPreviewHeight*3/4), mPreviewHeight, 0, PIX_YUV422I, 1)) {
            LOGE("scale_process() failed\n");
        }

        // Rotate
        neon_args->pIn    = (unsigned char*)yuv_buffer;
        neon_args->pOut   = (unsigned char*)scale_buffer;    
        neon_args->width  = mPreviewHeight;
        neon_args->height = mPreviewWidth;
        neon_args->rotate = NEON_ROT90;                 
        if(((*Neon_Rotate)(neon_args)) < 0) {
            LOGE("Error in Rotation 90\n");                
        }        
    }
#endif

    if((mCamMode == VT_MODE) && (mPreviousAngle == 90 || mPreviousAngle == 180 || mPreviousAngle == 270)) {
        setRotateYUV422((uint8_t *)cfilledbuffer.m.userptr, (uint8_t *)cfilledbuffer.m.userptr, mPreviewWidth, mPreviewHeight, mPreviousAngle); 
    }

    if((mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME)
#ifdef SKT_FMC_DM  
       && (!mCurSecurity)
#endif
    ) {
#if USE_NEON
        if(mCamMode == VT_MODE) {
            Neon_Convert_yuv422_to_YUV420P((unsigned char *)cfilledbuffer.m.userptr, (unsigned char *)mVideoConversionBuffer[cfilledbuffer.index]->pointer(), mPreviewWidth ,mPreviewHeight);            
        } else {
#if OMAP_SCALE_CDD      
            if(mCameraIndex == VGA_CAMERA && Neon_Rotate != NULL) {   
                Neon_Convert_yuv422_to_NV21((unsigned char *)scale_buffer, (unsigned char *)mVideoConversionBuffer[cfilledbuffer.index]->pointer(), mPreviewWidth, mPreviewHeight);                
            } else
#endif         
            Neon_Convert_yuv422_to_NV21((unsigned char *)cfilledbuffer.m.userptr,(unsigned char *)mVideoConversionBuffer[cfilledbuffer.index]->pointer(), mPreviewWidth, mPreviewHeight);            
        }
        mDataCb( CAMERA_MSG_PREVIEW_FRAME, mVideoConversionBuffer[cfilledbuffer.index], mCallbackCookie);
#else
        mDataCb( CAMERA_MSG_PREVIEW_FRAME, cfilledbuffer.m.userptr, mCallbackCookie);
#endif
    }

    if(mRecordRunning && (iOutStandingBuffersWithEncoder < NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE)
#ifdef SKT_FMC_DM
        && (!mCurSecurity)
#endif            
    ) {
        queueBufferCheck = false;
        mConsecutiveDropCount = 0;
        
        mRecordingLock.lock();
        iOutStandingBuffersWithEncoder++;        
        mRecordingLock.unlock();
        
        mCurrentTime = systemTime(); 
        mDataCbTimestamp(mCurrentTime, CAMERA_MSG_VIDEO_FRAME, mVideoBuffer[(int)cfilledbuffer.index], mCallbackCookie);
        
#ifdef CHECK_FRAMERATE
        debugRecordingFPS();
#endif       
    } else {        
        queueBufferCheck = true;

        // check drop frame
        if(mRecordRunning) {
            mConsecutiveDropCount++;
            LOGE("Consecutive recording frame drop count = %d\n", mConsecutiveDropCount);
        }
    }

#ifdef SKIP_FRAME
    if (!mCounterSkipFrame && mOverlay != NULL
#ifdef SKT_FMC_DM    
        && (!mCurSecurity)
#endif            
    ) {            
        // Notify overlay of a new frame.
        if(buffers_queued_to_dss[cfilledbuffer.index] == 0) {
            nBuffers_queued_to_dss = mOverlay->queueBuffer((void*)cfilledbuffer.index);
            if (nBuffers_queued_to_dss < 0) {
                LOGE("nextPreview(): mOverlay->queueBuffer() failed:[%d]\n",cfilledbuffer.index);
            } else {
                nOverlayBuffersQueued++;
                buffers_queued_to_dss[cfilledbuffer.index] = 1; //queued                
            }
        } else {
            LOGE("Buffer already with Overlay %d",cfilledbuffer.index);
        }

        if (nOverlayBuffersQueued >= NUM_BUFFERS_TO_BE_QUEUED_FOR_OPTIMAL_PERFORMANCE) {
            if(mOverlay->dequeueBuffer(&overlaybuffer) < 0) {
                // Overlay went from stream on to stream off
                // We need to reclaim all of these buffers since the video queue will be lost
                if(mDSSActive) {
                    unsigned int nDSS_ct = 0;
                    LOGD("Overlay went from STREAM ON to STREAM OFF");
                    buffer_count =  mOverlay->getBufferCount();
                    for(int i =0; i < buffer_count ; i++) {
                        LOGD("Buf Ct %d, DSS[%d] = %d, Ovl Cnt %d",buffer_count,i,
                                            buffers_queued_to_dss[i],nOverlayBuffersQueued);
                        if(buffers_queued_to_dss[i] == 1) {
                            // we need to find out if this buffer was queued after overlay called stream off
                            // if so, then we should not reclaim it
                            if((data = (mapping_data_t*) mOverlay->getBufferAddress((void*)i)) != NULL) {
                                if(!data->nQueueToOverlay) {
                                    if(nOverlayBuffersQueued > 0) { 
                                        nOverlayBuffersQueued--;
                                    }   
                                    buffers_queued_to_dss[i] = 0;                                    
                                    nDSS_ct++;
                                } else {
                                    LOGE("Trying to Reclaim[%d] to CameraHAL but is already queued to overlay", i);
                                }          
                            } else {
                                LOGE("Error getBufferAddress couldn't return any pointer.");
                            }
                        }
                    }

                    LOGD("Done reclaiming buffers, there are [%d] buffers queued to overlay actually %d", 
                                                                               nOverlayBuffersQueued,nDSS_ct);
                    mDSSActive = false;
                } else {
                   LOGE("nextPreview(): mOverlay->dequeueBuffer() failed Cnt %d\n",nOverlayBuffersQueued);
                }
            } else {
                if(nOverlayBuffersQueued > 0) { 
                    nOverlayBuffersQueued--;
                }
                buffers_queued_to_dss[(int)overlaybuffer] = 0;
                lastOverlayBufferDQ = (int)overlaybuffer;
                mDSSActive = true;
            }

#ifdef CHECK_FRAMERATE
            debugShowFPS();
#endif       
        }
    } else {
        // skip the frame for display
        if(mCounterSkipFrame > 0) {
            mCounterSkipFrame--;
        }       
    }
#endif

    if(!mPassedFirstFrame) {
        HAL_PRINT("Come to first frmae!\n ");
        mPassedFirstFrame = true;
    }

    if(queueBufferCheck) {
        if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[cfilledbuffer.index]) < 0) {
            LOGE("VIDIOC_QBUF Failed.\n");
        } else {
            nCameraBuffersQueued++;
        }
    }
}

#if JPEG_ENCODER 
int CameraHal::ICaptureCreate(void)
{
    LOG_FUNCTION_NAME

    isStart_JPEG = false;

    jpegEncoder = new JpegEncoder;

    if( NULL != jpegEncoder ) {
        isStart_JPEG = true;
    } else {
        LOGE("Jpeg encoder not loaded.\n");
        goto fail_icapture;
    }

    LOG_FUNCTION_NAME_EXIT
    
    return 0;  

fail_icapture:
exit:
    
    return -1;
}

int CameraHal::ICaptureDestroy(void)
{
    if( isStart_JPEG ) {
        delete jpegEncoder;
        jpegEncoder = NULL;
        isStart_JPEG = false;
    }
    
    return 0;
}
#endif

bool CameraHal::useOverlay()
{
    return true;
}

status_t CameraHal::setOverlay(const sp<Overlay> &overlay)
{
    LOG_FUNCTION_NAME

    LOGV("%s : overlay = %p", __func__, overlay->getHandleRef());
    
    // De-alloc any stale data
    if ( mOverlay != NULL ) {
        LOGE("current overlay not null. destroy it\n");
        mOverlay->destroy();
        mOverlay = NULL;        
    }

    mOverlay = overlay;

    if(mOverlay == NULL) {
        LOGV("ERR(%s) : overlay is null. retuning.\n", __func__);        
    } else {        
        if (mPreviewNotReady) {
            startPreview();          
        }
    }

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

status_t CameraHal::startPreview()
{
    int cnt = 0;
    
    LOG_FUNCTION_NAME

    if(mOverlay == NULL)
    {
        LOGD("overlay is null. returning from startPreview\n");
        mPreviewNotReady = true;
        mPreviewRunning = true;
        return NO_ERROR;
    } else {
    Message msg;        
        mPreviewNotReady = false;
        mPreviewRunning = false;
    msg.command = PREVIEW_START;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);        
        return msg.command == PREVIEW_ACK ? NO_ERROR : INVALID_OPERATION;
    }

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::stopPreview()
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_STOP;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    LOG_FUNCTION_NAME_EXIT
}

bool CameraHal::previewEnabled()
{
    return mPreviewRunning;
}

status_t CameraHal::startRecording( )
{
    int i, j;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    // Just for the same size case
    mRecordingFrameSize = mPreviewWidth * mPreviewHeight * 2;

    HAL_PRINT("Clear the old memory \n");
    for(i = 0; i < mVideoBufferCount; i++) {
        mVideoBuffer[i].clear();
        mVideoBuffer[i] = 0;
        
        mVideoHeaps[i].clear();
        mVideoHeaps[i]  = 0;  
    }

    for(i = 0; i < mVideoBufferCount; i++) {
        mapping_data_t* data = (mapping_data_t*) mOverlay->getBufferAddress((void*)i);
        if(data != NULL) {
            mVideoHeaps[i]    = new MemoryHeapBase(data->fd, mRecordingFrameSize, 0, data->offset);
            mVideoBuffer[i]   = new MemoryBase(mVideoHeaps[i], 0, mRecordingFrameSize);
        } else {
            for(j = 0; j < i+1; j++) {
                mVideoHeaps[j].clear();
                mVideoBuffer[j].clear();
            }
            LOGD("Error: data from overlay returned null");
            return UNKNOWN_ERROR;            
        }
    }

    if(mPreviousFlashMode == 2) {
        setMovieFlash(FLASH_MOVIE_ON);
    }

    // reset time
    mCurrentTime = 0;
    mRecordSkip = 0;

    // reset flag
    mRecordRunning = true;  
    mRecordOverlay = true;

    mConsecutiveDropCount = 0;
    iOutStandingBuffersWithEncoder = 0;

    LOG_FUNCTION_NAME_EXIT

    return NO_ERROR;
}

void CameraHal::stopRecording()
{
    int i;

    LOG_FUNCTION_NAME

    Mutex::Autolock lock(mLock);

    // reset flag
    mRecordRunning = false;

    // reset time
    mCurrentTime = 0;
    mRecordSkip = 0;

    if(mPreviousFlashMode == 2) {
        setMovieFlash(FLASH_MOVIE_OFF);
    }

    mConsecutiveDropCount = 0;
    iOutStandingBuffersWithEncoder = 0;

    LOG_FUNCTION_NAME_EXIT
}

bool CameraHal::recordingEnabled()
{
    LOG_FUNCTION_NAME
    return (mRecordRunning);

}

void CameraHal::releaseRecordingFrame(const sp<IMemory>& mem)
{
    LOG_FUNCTION_NAME
        
    int index = 0;
    bool isEqual = false;

    for(index = 0; index <mVideoBufferCount; index ++) {
        if(mem->pointer() == mVideoBuffer[index]->pointer()) {
            isEqual = true;
            break;
        }
    }

    if((isEqual == true) && (iOutStandingBuffersWithEncoder > 0)) {        
        mRecordingLock.lock();
        iOutStandingBuffersWithEncoder--;        
        mRecordingLock.unlock();
    }
    
    if (ioctl(camera_device, VIDIOC_QBUF, &v4l2_cam_buffer[index]) < 0) 
        LOGE("VIDIOC_QBUF Failed.\n");
    else
        nCameraBuffersQueued++;

    LOG_FUNCTION_NAME_EXIT
}

sp<IMemoryHeap>  CameraHal::getRawHeap() const
{
    return mPictureHeap;
}

status_t CameraHal::takePicture( )
{
    Message msg;

    LOG_FUNCTION_NAME

#ifdef SKT_FMC_DM
    if(mCurSecurity)
        return INVALID_OPERATION;
#endif

    msg.command = PREVIEW_CAPTURE;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    LOG_FUNCTION_NAME_EXIT
        
    return msg.command == CAPTURE_ACK ? NO_ERROR : INVALID_OPERATION;
}

status_t CameraHal::cancelPicture( )
{
    LOG_FUNCTION_NAME

    disableMsgType(CAMERA_MSG_RAW_IMAGE);
    disableMsgType(CAMERA_MSG_COMPRESSED_IMAGE);

    LOGE("Callbacks set to null\n");
    return -1;
}

int CameraHal::validateSize(int w, int h)
{
    if ((w < MIN_WIDTH) || (h < MIN_HEIGHT))
        return false;

    return true;
}

status_t CameraHal::setParameters(const CameraParameters &params)
{
    HAL_PRINT("setParameters\n");

    int w, h;
    int fps_min, fps_max;
    
    LOG_FUNCTION_NAME

    HAL_PRINT("PreviewFormat %s\n", params.getPreviewFormat());
    if (strcmp(params.getPreviewFormat(), "yuv422i-uyvy") != 0 && strcmp(params.getPreviewFormat(), "yuv420sp") != 0) {
        LOGE("Only yuv422i-uyvy preview is supported");
        return -1;
    }
        
    HAL_PRINT("PictureFormat %s\n", params.getPictureFormat());
    if (strcmp(params.getPictureFormat(), "jpeg") != 0) {
        LOGE("Only jpeg still pictures are supported\n");
        return -1;
    }

    params.getPreviewSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Preview size not supported\n");
        return UNKNOWN_ERROR;
    }
    HAL_PRINT("PreviewSize by App %d x %d\n", w, h);

    params.getPictureSize(&w, &h);
    if (!validateSize(w, h)) {
        LOGE("Picture size not supported\n");
        return UNKNOWN_ERROR;
    }
    HAL_PRINT("Picture Size by App %d x %d\n", w, h);

    mParameters = params;

    mParameters.getPictureSize(&w, &h);
    HAL_PRINT("Picture Size by CamHal %d x %d\n", w, h);
    
    mParameters.getPreviewSize(&w, &h);
    HAL_PRINT("Preview Size by CamHal %d x %d\n", w, h);

    if(mCameraIndex == MAIN_CAMERA) {
        if((w == QCIF_WIDTH && h == QCIF_HEIGHT)
            || (w == QCIF_HEIGHT && h == QCIF_WIDTH)
            || (w == QVGA_WIDTH && h == QVGA_HEIGHT)
            || (w == QVGA_HEIGHT && h == QVGA_WIDTH)
            || (w == CIF_WIDTH && h == CIF_HEIGHT)
            || (w == CIF_HEIGHT && h == CIF_WIDTH)
            || (w == VGA_WIDTH && h == VGA_HEIGHT)
            || (w == D1_WIDTH && h == D1_HEIGHT)
            || (w == WVGA_WIDTH && h == WVGA_HEIGHT)
            || (w == HD_WIDTH && h == HD_HEIGHT))
            HAL_PRINT("\ncorrect resolution for preview %d x %d\n", w, h);
        else {
            mParameters.setPreviewSize(VGA_WIDTH, VGA_HEIGHT);
            HAL_PRINT("\ndefault resolution for preview %d x %d\n", VGA_WIDTH, VGA_HEIGHT);
        }
    } else {
        if((w == QCIF_WIDTH && h == QCIF_HEIGHT)
            || (w == QCIF_HEIGHT && h == QCIF_WIDTH)
            || (w == QVGA_WIDTH && h == QVGA_HEIGHT)
            || (w == QVGA_HEIGHT && h == QVGA_WIDTH)
            || (w == CIF_WIDTH && h == CIF_HEIGHT)
            || (w == CIF_HEIGHT && h == CIF_WIDTH)
            || (w == VGA_WIDTH && h == VGA_HEIGHT))
            HAL_PRINT("\ncorrect resolution for preview %d x %d\n", w, h);
        else {
            mParameters.setPreviewSize(VGA_WIDTH, VGA_HEIGHT);
            HAL_PRINT("\ndefault resolution for preview %d x %d\n", VGA_WIDTH, VGA_HEIGHT);
        }    
    }

    mParameters.getPreviewFpsRange(&fps_min, &fps_max);
    if ((fps_min > fps_max) || (fps_min < 0) || (fps_max < 0)) {
        LOGE("WARN(%s): request for preview frame is not initial. \n",__func__);
        return UNKNOWN_ERROR;
    } else {
        mParameters.setPreviewFrameRate(fps_max/1000);
    }
    HAL_PRINT("FRAMERATE RANGE %d ~ %d\n", fps_min, fps_max);

    quality = params.getInt("jpeg-quality");
    if ( ( quality < 0 ) || (quality > 100) )
        quality = 100;

    mPreviewFrameSkipValue = getPreviewFrameSkipValue();

    if(setWB(getWBLighting()) < 0) {
        LOGE("(%s): Set WB fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setEffect(getEffect()) < 0) {
        LOGE("(%s): Set Effect fail!! \n",__func__);        
        return UNKNOWN_ERROR;
    }
    
    if(setAntiBanding(getAntiBanding()) < 0) {
        LOGE("(%s): Set Antibanding fail!! \n",__func__);        
        return UNKNOWN_ERROR;
    }
    if(setSceneMode(getSceneMode()) < 0) {
        LOGE("(%s): Set SceneMode fail!! \n",__func__); 
        return UNKNOWN_ERROR;
    }
    if(setFlashMode(getFlashMode()) < 0) {
        LOGE("(%s): Set FlashMode fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setBrightness(getBrightness()) < 0) {
        LOGE("(%s): Set Brightness fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setExposure(getExposure()) < 0) {
        LOGE("(%s): Set Exposure fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setZoom(getZoomValue()) < 0) {
        LOGE("(%s): Set ZoomValue fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setISO(getISO()) < 0) {
        LOGE("(%s): Set ISO fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setContrast(getContrast()) < 0) {
        LOGE("(%s): Set Contrast fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setSaturation(getSaturation()) < 0) {
        LOGE("(%s): Set Saturation fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setSharpness(getSharpness()) < 0) {
        LOGE("(%s): Set Sharpness fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setWDRMode(getWDRMode()) < 0) {
        LOGE("(%s): Set WDR fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setAntiShakeMode(getAntiShakeMode()) < 0) {
        LOGE("(%s): Set AntiShake fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setFocusMode(getFocusMode()) < 0) {
        LOGE("(%s): Set FocusMode fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setMetering(getMetering()) < 0) {
        LOGE("(%s): Set Metering fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setPrettyMode(getPrettyValue()) < 0) {
        LOGE("(%s): Set PrettyMode fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setRotate(getRotate()) < 0) {
        LOGE("(%s): Set Rotate fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(mCamMode != CAMCORDER_MODE && mCamMode != VT_MODE) {
        if(setJpegMainimageQuality(getJpegMainimageQuality()) < 0) {
            LOGE("(%s): Set Jpeg Quality fail!! \n",__func__);
            return UNKNOWN_ERROR;
        }
    }
    
    if(getPreviewFlashOn() == 1) {
        if(setMovieFlash(FLASH_MOVIE_ON) < 0) {
            LOGE("(%s): Set Movie Flash On fail!! \n",__func__);
            return UNKNOWN_ERROR;
        }
    } else {
        if(setMovieFlash(FLASH_MOVIE_OFF) < 0) {
            LOGE("(%s): Set Movie Flash Off fail!! \n",__func__);
            return UNKNOWN_ERROR;
        }
    }
    
    if(setGPSLatitude(getGPSLatitude()) < 0) {
        LOGE("(%s): Set GPS Latitude fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setGPSLongitude(getGPSLongitude()) < 0) {
        LOGE("(%s): Set GPS Longitude fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setGPSAltitude(getGPSAltitude()) < 0) {
        LOGE("(%s): Set GPS Altitude fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setGPSTimestamp(getGPSTimestamp()) < 0) {
        LOGE("(%s): Set GPS Timestamp fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setGPSProcessingMethod(getGPSProcessingMethod()) < 0) {
        LOGE("(%s): Set GPS Povessing Method fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }
    
    if(setJpegThumbnailSize(getJpegThumbnailSize()) < 0) {
        LOGE("(%s): Set Jpeg Thumbnail Size fail!! \n",__func__);
        return UNKNOWN_ERROR;
    }

    HAL_PRINT("exit setParameters\n");
    
    return NO_ERROR;
}

CameraParameters CameraHal::getParameters() const
{
    CameraParameters params;
    int iso, af, wb, exposure, scene, effects, compensation;
    int saturation, sharpness, contrast, brightness;

    LOG_FUNCTION_NAME
   
    params = mParameters;

    return params;
}

status_t  CameraHal::dump(int fd, const Vector<String16>& args) const
{
    return 0;
}

void CameraHal::dumpFrame(void *buffer, int size, char *path)
{
    FILE* fIn = NULL;

    fIn = fopen(path, "w");
    if ( fIn == NULL ) {
        LOGE("\n\n\n\nError: failed to open the file %s for writing\n\n\n\n", path);
        return;
    }
    
    fwrite((void *)buffer, 1, size, fIn);
    fclose(fIn);
}

int CameraHal::onSaveH3A(void *priv, void *buf, int size)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveH3A\n");
    camHal->SaveFile(NULL, (char*)"h3a", buf, size);

    return 0;
}

int CameraHal::onSaveLSC(void *priv, void *buf, int size)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveLSC\n");
    camHal->SaveFile(NULL, (char*)"lsc", buf, size);

    return 0;
}

int CameraHal::onSaveRAW(void *priv, void *buf, int size)
{
    CameraHal* camHal = reinterpret_cast<CameraHal*>(priv);

    LOGD("Observer onSaveRAW\n");
    camHal->SaveFile(NULL, (char*)"raw", buf, size);

    return 0;
}

int CameraHal::SaveFile(char *filename, char *ext, void *buffer, int size)
{
    LOG_FUNCTION_NAME

    //Store image
    char fn [512];

    if (filename) 
        strcpy(fn,filename);
    else 
    {
        if (ext==NULL)
            ext = (char*)"tmp";
        sprintf(fn, PHOTO_PATH, file_index, ext);
    }
    file_index++;
    LOGD("Writing to file: %s\n", fn);
    int fd = open(fn, O_RDWR | O_CREAT | O_SYNC);
    if (fd < 0) {
        LOGE("Cannot open file %s : %s\n", fn, strerror(errno));
        return -1;
    } else {  
        int written = 0;
        int page_block, page = 0;
        int cnt = 0;
        int nw;
        char *wr_buff = (char*)buffer;
        LOGD("Jpeg size %d\n", size);
        page_block = size / 20;
        while (written < size ) {
            nw = size - written;
            nw = (nw>512*1024)?8*1024:nw;

            nw = ::write(fd, wr_buff, nw);
            if (nw < 0) {
                LOGD("write fail nw=%d, %s\n", nw, strerror(errno));
                break;
            }
            wr_buff += nw;
            written += nw;
            cnt++;

            page += nw;
            if (page >= page_block) {
                page = 0;
                LOGD("Percent %6.2f, wn=%5d, total=%8d, jpeg_size=%8d\n", 
                    ((float)written)/size, nw, written, size);
            }
        }

        close(fd);

        return 0;
    }

    LOG_FUNCTION_NAME_EXIT
}


sp<IMemoryHeap> CameraHal::getPreviewHeap() const
{
    LOG_FUNCTION_NAME
        
    return mVideoConversionHeap;    
}

sp<CameraHardwareInterface> CameraHal::createInstance(int cameraId)
{
    LOG_FUNCTION_NAME

    if (singleton != 0) {
        sp<CameraHardwareInterface> hardware = singleton.promote();
        if (hardware != 0) 
            return hardware;
    }

    sp<CameraHardwareInterface> hardware(new CameraHal(cameraId));

    singleton = hardware;
    return hardware;
} 


void CameraHal::release()
{
    LOG_FUNCTION_NAME

    singleton.clear();
}


/*--------------------Eclair HAL---------------------------------------*/
void CameraHal::setCallbacks(notify_callback notify_cb,
                                      data_callback data_cb,
                                      data_callback_timestamp data_cb_timestamp,
                                      void* user)
{
    mNotifyCb = notify_cb;
    mDataCb = data_cb;
    mDataCbTimestamp = data_cb_timestamp;
    mCallbackCookie = user;
}

void CameraHal::enableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    mMsgEnabled |= msgType;
}

void CameraHal::disableMsgType(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    mMsgEnabled &= ~msgType;
}

bool CameraHal::msgTypeEnabled(int32_t msgType)
{
    Mutex::Autolock lock(mLock);
    return (mMsgEnabled & msgType);
}


status_t CameraHal::autoFocus()
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_AF_START;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    LOG_FUNCTION_NAME_EXIT
        
    return msg.command == PREVIEW_ACK ? NO_ERROR : INVALID_OPERATION;
}

status_t CameraHal::cancelAutoFocus()
{
    LOG_FUNCTION_NAME

    Message msg;
    msg.command = PREVIEW_AF_CANCEL;
    previewThreadCommandQ.put(&msg);
    previewThreadAckQ.get(&msg);

    LOG_FUNCTION_NAME_EXIT
        
    return msg.command == PREVIEW_ACK ? NO_ERROR : INVALID_OPERATION;
}

status_t CameraHal::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    return NO_ERROR;
}

status_t CameraHal::setWB(const char* wb)
{   
    if(camera_device && (mCameraIndex == MAIN_CAMERA  || mCamMode == VT_MODE)) {
        int wbValue =1;
        int i = 1;
    
        if(strcmp(wb, wb_value[mPreviousWB]) !=0) {
            HAL_PRINT("myungwoo mPreviousWB =%d wb=%s\n", mPreviousWB,wb);
            for (i = 1; i < MAX_WBLIGHTING_EFFECTS; i++) {
                if (! strcmp(wb_value[i], wb)) {
                    HAL_PRINT("In setWB : Match : %s : %d\n ", wb, i);
                    wbValue = i;
                    break;
                }
            }

            if(i == MAX_WBLIGHTING_EFFECTS) {
                LOGE("setWB : invalid value\n");
                return UNKNOWN_ERROR;           
            } else {
                struct v4l2_control vc;            
                CLEAR(vc);
                vc.id = V4L2_CID_WB;
                vc.value = wbValue;
        
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("setWB fail\n");
                    return UNKNOWN_ERROR;                 
                }

                mPreviousWB = i;
            }
        }
    }

    return NO_ERROR;
}

status_t CameraHal::setEffect(const char* effect)
{    
    if(camera_device && (mCameraIndex == MAIN_CAMERA  || mCamMode == VT_MODE)) {
        int effectValue =1;
        int i = 1;
        if(strcmp(effect, color_effects[mPreviousEffect]) !=0) {
            HAL_PRINT("myungwoo mPreviousEffect =%d effect=%s\n", mPreviousEffect,effect); 
            for(i = 1; i < MAX_COLOR_EFFECTS; i++) {
                if (! strcmp(color_effects[i], effect)) {
                    HAL_PRINT("In setEffect : Match : %s : %d \n", effect, i);
                    effectValue = i;
                    break;
                }
            }
            
            if(i == MAX_COLOR_EFFECTS) {
                LOGE("setEffect : invalid value\n");
                return UNKNOWN_ERROR;           
            } else {    
                struct v4l2_control vc;            
                CLEAR(vc);
                vc.id = V4L2_CID_EFFECT;
                vc.value = effectValue;
        
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("setEffect fail\n");
                    return UNKNOWN_ERROR;  
                }

                mPreviousEffect = i;
            }
        }
    }

    return NO_ERROR;
}

status_t CameraHal::setAntiBanding(const char* antibanding)
{
#ifndef NOWPLUS_MOD	//no antibandig on nowplus
    if(camera_device && mCameraIndex == MAIN_CAMERA) {
        int antibandingValue =1;
        int i = 0;
        if(strcmp(antibanding, anti_banding_values[mPreviousAntibanding]) !=0) {
            HAL_PRINT("myungwoo mPreviousAntibanding =%d antibanding=%s\n", mPreviousAntibanding,antibanding);
            for(i = 1; i < MAX_ANTI_BANDING_VALUES; i++) {
                if (! strcmp(anti_banding_values[i], antibanding)) {
                    HAL_PRINT("In setAntiBanding : Match : %s : %d \n", antibanding, i);
                    antibandingValue = i;
                    break;
                }
            }

            if(i == MAX_ANTI_BANDING_VALUES) {
                LOGE("setAntiBanding : invalid value\n");
                return UNKNOWN_ERROR;           
            } else {    
                struct v4l2_control vc;            
                CLEAR(vc);
                vc.id = V4L2_CID_ANTIBANDING;
                vc.value = antibandingValue;
        
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("setAntiBanding fail\n");
                    return UNKNOWN_ERROR;  
                }
                mPreviousAntibanding = i;
            }
        }
    }
#endif
    return NO_ERROR;
}

status_t CameraHal::setSceneMode(const char* scenemode)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA)
    {
        int sceneModeValue =1;
        int i = 1;
        
        if(strcmp(scenemode, scene_mode[mPreviousSceneMode]) !=0) {
            HAL_PRINT("myungwoo mPreviousSceneMode =%d scenemode=%s\n", mPreviousSceneMode,scenemode); 
            for(i = 1; i < MAX_SCENE_MODE; i++) {
                if (! strcmp(scene_mode[i], scenemode)) {
                    HAL_PRINT("In setSceneMode : Match : %s : %d \n", scenemode, i);
                    sceneModeValue = i;
                    break;
                }
            }

            if(i == MAX_SCENE_MODE) {
                LOGE("setSceneMode : invalid value\n");
                return UNKNOWN_ERROR;           
            } else {      
                struct v4l2_control vc;
            
                CLEAR(vc);
                vc.id = V4L2_CID_SCENE;
                vc.value = sceneModeValue;
        
                if(sceneModeValue == 2)
                    mASDMode = true;
                else
                    mASDMode = false;
                    
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("setSceneMode fail\n");
                    return UNKNOWN_ERROR; 
                }

                mPreviousSceneMode = i;
            }
        }
    }

    return NO_ERROR;
}

status_t CameraHal::setFlashMode(const char* flashmode)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA)
    {
        int flashModeValue =1;
        int i = 1;
        
        if(strcmp(flashmode, flash_mode[mPreviousFlashMode]) !=0) {
            HAL_PRINT("myungwoo mPreviousFlashMode =%d flashmode=%s\n", mPreviousFlashMode,flashmode); 
            for(i = 1; i < MAX_FLASH_MODE; i++) {
                if (! strcmp(flash_mode[i], flashmode)) {
                    HAL_PRINT("In setFlashMode : Match : %s : %d \n", flashmode, i);
                    flashModeValue = i;
                    break;
                }
            }

            if(i == MAX_FLASH_MODE) {
                LOGE("setFlashMode : invalid value\n");
                return UNKNOWN_ERROR;           
            } else {     
                struct v4l2_control vc;            
                CLEAR(vc);
                vc.id = V4L2_CID_FLASH_CAPTURE;
                vc.value = flashModeValue;
        
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("setFlashMode fail\n");
                    return UNKNOWN_ERROR; 
                }

                mPreviousFlashMode = i;
            }
        }
    }

    return NO_ERROR;
}

status_t CameraHal::setMovieFlash(int flag)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA) {
        if(flag != mPreviousFlag) {
            struct v4l2_control vc;

            CLEAR(vc);
            vc.id = V4L2_CID_FLASH_MOVIE;
            vc.value = flag;

            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setFlashMode fail\n");
                return UNKNOWN_ERROR; 
            }

            mPreviousFlag = flag;
        }
    }

    return NO_ERROR;
}

status_t CameraHal::setBrightness(int brightness)
{
    if(camera_device) {
        struct v4l2_control vc;
    
        if(brightness != mPreviousBrightness) {
            HAL_PRINT("myungwoo mPreviousBrightness =%d brightness=%d\n", mPreviousBrightness,brightness);
    
            CLEAR(vc);
            vc.id = V4L2_CID_BRIGHTNESS;
            vc.value = brightness;
    
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setBrightness fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousBrightness = brightness;
    }

    return NO_ERROR;    
}

status_t CameraHal::setExposure(int exposure)
{
    if(camera_device) {
        struct v4l2_control vc;
    
        if(exposure != mPreviousExposure) {
            HAL_PRINT("myungwoo mPreviousExposure =%d exposure=%d\n", mPreviousExposure,exposure);
    
            CLEAR(vc);
            vc.id = V4L2_CID_EXPOSURE;
            vc.value = exposure;
    
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setExposure fail\n");
                return UNKNOWN_ERROR;
            }
        }
    
        mPreviousExposure = exposure;
    }

    return NO_ERROR;    
}

status_t CameraHal::setZoom(int zoom)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA)
    {
        if(zoom < 0 || zoom > MAX_ZOOM) {
            if(zoom < 0) 
                mParameters.set(mParameters.KEY_ZOOM, 0);
            if(zoom > MAX_ZOOM) 
                mParameters.set(mParameters.KEY_ZOOM, MAX_ZOOM);
            return UNKNOWN_ERROR; 
        }
        
        struct v4l2_control vc;
    
        if(zoom != mPreviousZoom) {
            HAL_PRINT("myungwoo mPreviousZoom =%d zoom=%d\n", mPreviousZoom,zoom);
    
            CLEAR(vc);
            vc.id = V4L2_CID_ZOOM;
            vc.value = zoom + 1;
    
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setZoom fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousZoom = zoom;
    }

    return NO_ERROR;
}

status_t CameraHal::setISO(int iso)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA) {
        struct v4l2_control vc;
    
        if(iso != mPreviousIso) {
            HAL_PRINT("myungwoo mPreviousIso =%d iso=%d\n", mPreviousIso,iso);
    
            CLEAR(vc);
            vc.id = V4L2_CID_ISO;            
            switch(iso) {
            case 0:
                vc.value = ISO_AUTO;
                break;
            case 50:
                vc.value = ISO_50;
                break;
            case 100:
                vc.value = ISO_100;
                break;
            case 200:
                vc.value = ISO_200;
                break;
            case 400:
                vc.value = ISO_400;
                break;
            case 800:
                vc.value = ISO_800;
                break;
            }
            
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setISO fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousIso = iso;
    }

    return NO_ERROR;    
}

status_t CameraHal::setContrast(int contrast)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA) {
        struct v4l2_control vc;
    
        if(contrast != mPreviousContrast) {
            HAL_PRINT("myungwoo mPreviousContrast =%d contrast=%d\n", mPreviousContrast,contrast);
    
            CLEAR(vc);
            vc.id = V4L2_CID_CONTRAST;
            vc.value = contrast;    
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setContrast fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousContrast = contrast;
    }

    return NO_ERROR;    
}

status_t CameraHal::setSaturation(int saturation)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA) {
        struct v4l2_control vc;
    
        if(saturation != mPreviousSaturation) {
            HAL_PRINT("myungwoo mPreviousSaturation =%d saturation=%d\n", mPreviousSaturation,saturation);
    
            CLEAR(vc);
            vc.id = V4L2_CID_SATURATION;
            vc.value = saturation;    
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setSaturation fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousSaturation = saturation;
    }

    return NO_ERROR;    
}

status_t CameraHal::setSharpness(int sharpness)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA) {
        struct v4l2_control vc;
    
        if(sharpness != mPreviousSharpness) {
            HAL_PRINT("myungwoo mPreviousSharpness =%d sharpness=%d\n", mPreviousSharpness,sharpness);
    
            CLEAR(vc);
            vc.id = V4L2_CID_SHARPNESS;
            vc.value = sharpness;    
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setSharpness fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousSharpness = sharpness;
    }

    return NO_ERROR;    
}

status_t CameraHal::setWDRMode(int wdr)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA) {
        struct v4l2_control vc;
    
        if(wdr != mPreviousWdr) {
            HAL_PRINT("myungwoo mPreviousWdr =%d wdr=%d\n", mPreviousWdr,wdr);
    
            CLEAR(vc);
            vc.id = V4L2_CID_WDR;
            vc.value = wdr;    
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setWDRMode fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousWdr = wdr;
    }

    return NO_ERROR;    
}

status_t CameraHal::setAntiShakeMode(int antiShake)
{
#ifndef NOWPLUS_MOD
    if(camera_device && mCameraIndex == MAIN_CAMERA)
    {
        struct v4l2_control vc;
    
        if(antiShake != mPreviousAntiShake) {
            HAL_PRINT("myungwoo mPreviousAntiShake =%d antiShake=%d\n", mPreviousAntiShake,antiShake);
    
            CLEAR(vc);
            vc.id = V4L2_CID_ANTISHAKE;
            vc.value = antiShake;            
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setAntiShakeMode fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousAntiShake = antiShake;
    }
#endif
    return NO_ERROR;    
}

status_t CameraHal::setFocusMode(const char* focus)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA) {
        int focusValue =1;
        int i = 1;
        
        if(strcmp(focus, focus_mode[mPreviousFocus]) !=0) {
            HAL_PRINT("myungwoo mPreviousFocus =%d focus=%s\n", mPreviousFocus,focus); 
            for(i = 1; i < MAX_FOCUS_MODE; i++) {
                if (! strcmp(focus_mode[i], focus)) {
                    LOGI("In setFocusMode : Match : %s : %d \n", focus, i);
                    focusValue = i;
                    break;
                }
            }

            if(i == MAX_FOCUS_MODE) {
                LOGE("setFocusMode : invalid value\n");
                return UNKNOWN_ERROR;           
            } else {    
                struct v4l2_control vc;            
                CLEAR(vc);
                vc.id = V4L2_CID_FOCUS_MODE;
                vc.value = focusValue;        
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("setEffect fail\n");
                    return UNKNOWN_ERROR; 
                }

                mPreviousFocus = i;
            }
        }
    }

    return NO_ERROR;    
}

status_t CameraHal::setMetering(const char* metering)
{
    if(camera_device && mCameraIndex == MAIN_CAMERA)
    {
        int meteringValue =1;
        int i = 1;

        if(strcmp(metering, metering_mode[mPreviousMetering]) !=0) {
            HAL_PRINT("myungwoo mPreviousMetering =%d metering=%s\n", mPreviousMetering,metering); 
            for(i = 1; i < MAX_METERING; i++) {
                if (! strcmp(metering_mode[i], metering)) {
                    HAL_PRINT("In setMetering : Match : %s : %d \n", metering, i);
                    meteringValue = i;
                    break;
                }
            }

            if(i == MAX_METERING) {
                LOGE("setMetering : invalid value\n");
                return UNKNOWN_ERROR;           
            } else {    
                struct v4l2_control vc;     
                CLEAR(vc);
                vc.id = V4L2_CID_PHOTOMETRY;
                vc.value = meteringValue;        
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("setEffect fail\n");
                    return UNKNOWN_ERROR; 
                }

                mPreviousMetering = i;
            }
        }
    }

    return NO_ERROR;    
}

status_t CameraHal::setPrettyMode(int pretty)
{
    if(camera_device && mCameraIndex == VGA_CAMERA) {
        struct v4l2_control vc;
    
        if(pretty != mPreviousPretty) {
            HAL_PRINT("myungwoo mPreviousPretty =%d pretty=%d\n", mPreviousPretty,pretty);
    
            CLEAR(vc);
            vc.id = V4L2_CID_PRETTY;
            vc.value = pretty;            
            if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                LOGE("setPrettyMode fail\n");
                return UNKNOWN_ERROR; 
            }
        }
    
        mPreviousPretty = pretty;
    }

    return NO_ERROR;    
}

status_t CameraHal::setJpegMainimageQuality(int quality)
{
    if(camera_device) {
        if(quality != mPreviousQuality) {                   
            HAL_PRINT("myungwoo mPreviousQuality = %d quality = %d\n", mPreviousQuality,quality);
            
            if(mCamera_Mode == CAMERA_MODE_JPEG) {        
                struct v4l2_control vc;           
                        
                CLEAR(vc);
                vc.id = V4L2_CID_JPEG_QUALITY;
                
                if(quality < JPEG_NORMAL_limit)
                    vc.value = JPEG_ECONOMY;
                else if((JPEG_NORMAL_limit <= quality) && (quality < JPEG_FINE_limit))
                    vc.value = JPEG_NORMAL;
                else if((JPEG_FINE_limit <= quality) && (quality < JPEG_SUPERFINE_limit))
                    vc.value = JPEG_FINE;
                else
                    vc.value = JPEG_SUPERFINE;
                
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("setQuality fail\n");
                    return UNKNOWN_ERROR; 
                } 
            } else {
                if(quality < JPEG_NORMAL_limit)
                    mYcbcrQuality = YUV_ECONOMY;
                else if((JPEG_NORMAL_limit <= quality) && (quality < JPEG_FINE_limit))
                    mYcbcrQuality = YUV_NORMAL;
                else if((JPEG_FINE_limit <= quality) && (quality < JPEG_SUPERFINE_limit))
                    mYcbcrQuality = YUV_FINE;
                else
                    mYcbcrQuality = YUV_SUPERFINE;
            }
        }

        mPreviousQuality = quality;
    }

    return NO_ERROR;    
}

status_t CameraHal::setRotate(int angle)
{
    if(camera_device) {
        if(angle != mPreviousAngle) {
            HAL_PRINT("mPreviousAngle =%d angle=%d\n", mPreviousAngle, angle);
        }

        if(angle % 90 != 0)
            return UNKNOWN_ERROR; 
        else
            mPreviousAngle = angle;
    }

    return NO_ERROR;    
}

status_t CameraHal::setGPSLatitude(double gps_latitude)
{
    if(camera_device) {
        if(gps_latitude != mPreviousGPSLatitude) {
            HAL_PRINT("mPreviousGPSLatitude =%2.2f gps_latitude = %2.2f\n", mPreviousGPSLatitude, gps_latitude);
        }        
        mPreviousGPSLatitude = gps_latitude;
    }
    
    return NO_ERROR;    
}

status_t CameraHal::setGPSLongitude(double gps_longitude)
{
    if(camera_device) {
        if(gps_longitude != mPreviousGPSLongitude) {
            HAL_PRINT("mPreviousGPSLongitude =%2.2f gps_latitude = %2.2f\n", mPreviousGPSLongitude, gps_longitude);
        }
        mPreviousGPSLongitude = gps_longitude;
    } 

    return NO_ERROR;    
}

status_t CameraHal::setGPSAltitude(double gps_altitude)
{
    if(camera_device) {
        if(gps_altitude != mPreviousGPSAltitude) {
            HAL_PRINT("mPreviousGPSAltitude =%2.2f gps_latitude = %2.2f\n", mPreviousGPSAltitude, gps_altitude);
        }
        mPreviousGPSAltitude = gps_altitude;
    } 

    return NO_ERROR;    
}

status_t CameraHal::setGPSTimestamp(long gps_timestamp)
{
    if(camera_device) {
        if(gps_timestamp != mPreviousGPSTimestamp) {
            HAL_PRINT("mPreviousGPSTimestamp = %ld gps_timestamp = %ld\n", mPreviousGPSTimestamp, gps_timestamp);
        }
        mPreviousGPSTimestamp = gps_timestamp;
        m_gps_time = mPreviousGPSTimestamp;
        m_timeinfo = localtime(&m_gps_time);
        m_gpsHour = m_timeinfo->tm_hour;
        m_gpsMin  = m_timeinfo->tm_min;
        m_gpsSec  = m_timeinfo->tm_sec;
        strftime((char *)m_gps_date, 11, "%Y-%m-%d", m_timeinfo);
    }

    return NO_ERROR;    
}

status_t CameraHal::setGPSProcessingMethod(const char * gps_processingmethod)
{
    if(camera_device) {
        if(gps_processingmethod != NULL) {
            HAL_PRINT("gps_processingmethod = %s\n", gps_processingmethod);
            strcpy((char *)mPreviousGPSProcessingMethod, gps_processingmethod);
            
        } else {
            strcpy((char *)mPreviousGPSProcessingMethod, "GPS NETWORK HYBRID ARE ALL FINE.");
        }
    }

    return NO_ERROR;    
}

status_t CameraHal::setJpegThumbnailSize(imageInfo imgInfo)
{
    if(camera_device) {
        if(mThumbnailWidth != imgInfo.mImageWidth || mThumbnailHeight != imgInfo.mImageHeight) {
            HAL_PRINT("mThumbnailWidth = %d mThumbnailHeight = %d\n", imgInfo.mImageWidth, imgInfo.mImageHeight);
        }
        
        mThumbnailWidth = imgInfo.mImageWidth;
        mThumbnailHeight = imgInfo.mImageHeight;
    }

    return NO_ERROR;    
}

const char *CameraHal::getEffect() const
{
    return mParameters.get(mParameters.KEY_EFFECT);
}
const char *CameraHal::getWBLighting() const
{
    return mParameters.get(mParameters.KEY_WHITE_BALANCE);
}

const char *CameraHal::getAntiBanding() const
{
     return mParameters.get(mParameters.KEY_ANTIBANDING);
}

const char *CameraHal::getSceneMode() const
{
    return mParameters.get(mParameters.KEY_SCENE_MODE);
}

const char *CameraHal::getFlashMode() const
{
    return mParameters.get(mParameters.KEY_FLASH_MODE);
}

int CameraHal::getJpegMainimageQuality() const
{
     return atoi(mParameters.get(mParameters.KEY_JPEG_QUALITY));
}

int CameraHal::getBrightness() const
{
    int brightnessValue = 0;
    if( mParameters.get("brightness") ) {
        brightnessValue = atoi(mParameters.get("brightness"));
        if(brightnessValue != 0) {
            HAL_PRINT("in CameraParameters.cpp getbrightness not null int = %d \n", brightnessValue);
        }
        return brightnessValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp getbrightness null \n");
        return 0;
    }
}

int CameraHal::getExposure() const
{
    int exposureValue = 0;
    if( mParameters.get("exposure-compensation") ) {
        exposureValue = atoi(mParameters.get("exposure-compensation")) + 5;
        if(exposureValue != 0) {
            HAL_PRINT("in CameraParameters.cpp getExposure not null int = %d \n", exposureValue);
        }
        return exposureValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp getExposure null \n");
        return 0;
    }
}

int CameraHal::getZoomValue() const
{
    int zoomValue = 0;
    if( mParameters.get("zoom") ) {
        zoomValue = atoi(mParameters.get("zoom"));
        if( zoomValue != 0 ) {
            HAL_PRINT("CameraParameters.cpp :: ZOOM Value not null int = %d \n", zoomValue);
        }
        return zoomValue;
    } else {
        HAL_PRINT("CameraParameters.cpp :: ZOOM Value null \n");
        return 0;
    }
}

double CameraHal::getGPSLatitude() const
{
    double gpsLatitudeValue = 0;
    if( mParameters.get(mParameters.KEY_GPS_LATITUDE) ) {
        gpsLatitudeValue = atof(mParameters.get(mParameters.KEY_GPS_LATITUDE));
        if(gpsLatitudeValue != 0) {
            HAL_PRINT("CameraParameters.cpp :: gpsLatitudeValue = %2.2f \n", gpsLatitudeValue);
        }
        return gpsLatitudeValue;
    } else {
        HAL_PRINT("CameraParameters.cpp :: gpsLatitudeValue null \n");
        return 0;
    }
}

double CameraHal::getGPSLongitude() const
{
    double gpsLongitudeValue = 0;
    if( mParameters.get(mParameters.KEY_GPS_LONGITUDE) ) {
        gpsLongitudeValue = atof(mParameters.get(mParameters.KEY_GPS_LONGITUDE));
        if(gpsLongitudeValue != 0) {
            HAL_PRINT("CameraParameters.cpp :: gpsLongitudeValue = %2.2f \n", gpsLongitudeValue);
        }
        return gpsLongitudeValue;
    } else {
        HAL_PRINT("CameraParameters.cpp :: gpsLongitudeValue null \n");
        return 0;
    }
}

double CameraHal::getGPSAltitude() const
{
    double gpsAltitudeValue = 0;
    if( mParameters.get(mParameters.KEY_GPS_ALTITUDE) ) {
        gpsAltitudeValue = atof(mParameters.get(mParameters.KEY_GPS_ALTITUDE));
        if(gpsAltitudeValue != 0) {
            HAL_PRINT("CameraParameters.cpp :: gpsAltitudeValue = %2.2f \n", gpsAltitudeValue);
        }
        return gpsAltitudeValue;
    } else {
        HAL_PRINT("CameraParameters.cpp :: gpsAltitudeValue null \n");
        return 0;
    }
}

long CameraHal::getGPSTimestamp() const
{
    long gpsTimestampValue = 0;
    if( mParameters.get(mParameters.KEY_GPS_TIMESTAMP) ) {
        gpsTimestampValue = atol(mParameters.get(mParameters.KEY_GPS_TIMESTAMP));
        if(gpsTimestampValue != 0) {
            HAL_PRINT("CameraParameters.cpp :: gpsAltitudeValue = %ld \n", gpsTimestampValue);
        }
        return gpsTimestampValue;
    } else {
        HAL_PRINT("CameraParameters.cpp :: gpsAltitudeValue null \n");
        return 0;
    }
}

const char *CameraHal::getGPSProcessingMethod() const
{
    return mParameters.get(mParameters.KEY_GPS_PROCESSING_METHOD);
}

int CameraHal::getISO() const
{
    int isoValue = 0;
    if( mParameters.get("iso") ) {
        isoValue = atoi(mParameters.get("iso"));
        if(isoValue != 0) {
            HAL_PRINT("in CameraParameters.cpp iso not null int = %d \n", isoValue);
        }
        return isoValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp iso null \n");
        return 0;
    }
}

int CameraHal::getContrast() const
{
    int contrastValue = 0;
    if( mParameters.get("contrast") ) {
        contrastValue = atoi(mParameters.get("contrast"));
        if(contrastValue != 0) {
            HAL_PRINT("in CameraParameters.cpp contrastValue not null int = %d \n", contrastValue);
        }
        return contrastValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp contrast null \n");
        return 0;
    }
}

int CameraHal::getSaturation() const
{
    int saturationValue = 0;
    if( mParameters.get("saturation") ) {
        saturationValue = atoi(mParameters.get("saturation"));
        if(saturationValue != 0) {
            HAL_PRINT("in CameraParameters.cpp saturationValue not null int = %d \n", saturationValue);
        }
        return saturationValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp saturation null \n");
        return 0;
    }
}

int CameraHal::getSharpness() const
{
    int sharpnessValue = 0;
    if( mParameters.get("sharpness") ) {
        sharpnessValue = atoi(mParameters.get("sharpness"));
        if(sharpnessValue != 0) {
            HAL_PRINT("in CameraParameters.cpp sharpnessValue not null int = %d \n", sharpnessValue);
        }
        return sharpnessValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp sharpness null \n");
        return 0;
    }
}

int CameraHal::getWDRMode() const
{
    int wdrValue = 0;
    if( mParameters.get("wdr") ) {
        wdrValue = atoi(mParameters.get("wdr"));
        if(wdrValue != 0) {
            HAL_PRINT("in CameraParameters.cpp wdrValue not null int = %d \n", wdrValue);
        }
        return wdrValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp wdr null \n");
        return 0;
    }
}

int CameraHal::getAntiShakeMode() const
{
    int antiShakeValue = 0;
    if( mParameters.get("anti-shake") ) {
        antiShakeValue = atoi(mParameters.get("anti-shake"));
        if(antiShakeValue != 0) {
            HAL_PRINT("in CameraParameters.cpp antiShakeValue not null int = %d \n", antiShakeValue);
        }
        return antiShakeValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp anti-shake null \n");
        return 0;
    }
}

const char *CameraHal::getFocusMode() const
{
    return mParameters.get(mParameters.KEY_FOCUS_MODE);
}

const char *CameraHal::getMetering() const
{
    return mParameters.get("metering");
}

int CameraHal::getCameraSelect() const
{
    int CameraSelectValue = 0;
    if( mParameters.get("camera-id") ) {
        CameraSelectValue = atoi(mParameters.get("camera-id"));
        if(CameraSelectValue != 0) {
            HAL_PRINT("in CameraParameters.cpp getCameraSelect not null int = %d \n", CameraSelectValue);
        }
        return CameraSelectValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp camera-id null \n");
        return 0;
    }
}

int CameraHal::getCamcorderPreviewValue() const
{
    int camcorderPreviewValue = 0;
    if( mParameters.get("CamcorderPreviewMode") ) {
        camcorderPreviewValue = atoi(mParameters.get("CamcorderPreviewMode"));
        if(camcorderPreviewValue != 0) {
            HAL_PRINT("in CameraParameters.cpp getCamcorderPreviewValue not null int = %d \n", camcorderPreviewValue);
        }
        return camcorderPreviewValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp getCamcorderPreviewValue null \n");
        return 0;
    }
}

int CameraHal::getVTMode() const
{
    int vtModeValue = 0;
    if( mParameters.get("vtmode") ) {
        vtModeValue = atoi(mParameters.get("vtmode"));
        if(vtModeValue != 0) {
            HAL_PRINT("in CameraParameters.cpp getVTMode not null int = %d \n", vtModeValue);
        }
        return vtModeValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp getVTMode null \n");
        return 0;
    }
}

int CameraHal::getPrettyValue() const
{
    int prettyValue = 0;
    if( mParameters.get("pretty") ) {
        prettyValue = atoi(mParameters.get("pretty"));
        if(prettyValue != 0) {
            HAL_PRINT("CameraParameters.cpp :: pretty Value not null int = %d \n", prettyValue);
        }
        return prettyValue;
    } else {
        HAL_PRINT("CameraParameters.cpp :: pretty Value null \n");
        return 0;
    }
}

int CameraHal::getPreviewFrameSkipValue() const
{
    int previewFrameSkipValue = 0;
    if( mParameters.get("previewframeskip") ) {
        previewFrameSkipValue = atoi(mParameters.get("previewframeskip"));
        if(previewFrameSkipValue != 0) {
            HAL_PRINT("CameraParameters.cpp :: previewFrameSkip Value not null int = %d \n", previewFrameSkipValue);
        }
        return previewFrameSkipValue;
    } else {
        HAL_PRINT("CameraParameters.cpp :: previewFrameSkip Value null \n");
        return 0;
    }
}

void CameraHal::setDriverState(int state)
{
    if(camera_device) {
        struct v4l2_control vc;
    
        CLEAR(vc);
        vc.id = V4L2_CID_SELECT_STATE;
        vc.value = state;        
        if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
            LOGE("setDriverState fail\n");
            mNotifyCb(CAMERA_MSG_ERROR,CAMERA_DEVICE_ERROR_FOR_UNKNOWN,0,mCallbackCookie);
        }
    }
}

void CameraHal::mirrorForVTC(unsigned char * aSrcBufPtr, unsigned char * aDstBufPtr,int  aFramewidth,int  aFrameHeight)
{
    for (int i= 0; i< aFrameHeight; i++) {
        for (int j= 0; j< aFramewidth * 2; j += 2) {
            *(aDstBufPtr + j + (aFramewidth*2*i)) = *(aSrcBufPtr + (aFramewidth * 2 * i+aFramewidth * 2)-(j+2));
            *(aDstBufPtr + j + 1 + (aFramewidth*2*i)) = *(aSrcBufPtr + (aFramewidth * 2 * i+aFramewidth * 2)-(j+2)+1);
        }
    }
}

int CameraHal::getTwoSecondReviewMode() const
{
    int twoSecondReviewModeValue = 0;
    if( mParameters.get("twosecondreviewmode") ) {
        twoSecondReviewModeValue = atoi(mParameters.get("twosecondreviewmode"));
        if(twoSecondReviewModeValue!=0) {
            HAL_PRINT("in CameraParameters.cpp getTwoSecondReviewMode not null int = %d \n", twoSecondReviewModeValue);
        }
        return twoSecondReviewModeValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp getTwoSecondReviewMode null \n");
        return 0;
    }
}

int CameraHal::getPreviewFlashOn() const
{
    int previewFlashOn = 0;
    if( mParameters.get("previewflashon") ) {
        previewFlashOn = atoi(mParameters.get("previewflashon"));
        if(previewFlashOn!=0) {
            HAL_PRINT("in CameraParameters.cpp getPreviewFlashOn not null int = %d \n", previewFlashOn);
        }
        return previewFlashOn;
    } else {
        HAL_PRINT("in CameraParameters.cpp getPreviewFlashOn null \n");
        return 0;
    }
}

int CameraHal::getCropValue() const
{
    int cropValue = 0;
    if( mParameters.get("setcrop") ) {
        cropValue = atoi(mParameters.get("setcrop"));
        if(cropValue!=0) {
            HAL_PRINT("in CameraParameters.cpp crop not null int = %d \n", cropValue);
        }
        return cropValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp crop null \n");
        return 0;
    }
}

int CameraHal::checkFirmware()
{
    if(mCameraIndex == MAIN_CAMERA) {
        struct v4l2_control vc;
        int currentFirmware = 0, latestFirmware = 0;

        CLEAR(vc);
        vc.id = V4L2_CID_FW_VERSION;
        vc.value = 0;
        if (ioctl (camera_device, VIDIOC_G_CTRL, &vc) < 0) {
            LOGE("get fw version fail\n");
        }

        currentFirmware = vc.value;

        CLEAR(vc);
        vc.id = V4L2_CID_FW_LASTEST;
        vc.value = 0;
        if (ioctl (camera_device, VIDIOC_G_CTRL, &vc) < 0) {
            LOGE("get lastest fw version fail\n");
        }

        latestFirmware = vc.value;

        if(currentFirmware != latestFirmware)
            return -1;
    }
    
    return 0;

}

int CameraHal::getSamsungCameraValue() const
{
    int samsungCameraValue = 0;
    if( mParameters.get("samsungcamera") ) {
        samsungCameraValue = atoi(mParameters.get("samsungcamera"));
        if(samsungCameraValue!=0) {
            HAL_PRINT("in CameraParameters.cpp samsungcamera not null int = %d \n", samsungCameraValue);
        }
        return samsungCameraValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp samsungcamera null \n");
        return 0;
    }
}

void CameraHal::setRotateYUV420(uint8_t* pInputBuf, uint8_t* pOutputBuf, int w, int h, int angle)
{
    register int x, y, src_idx, dst_idx;
    int width, height, dst_pos;

    int ImgSize = w*h;
    uint8_t *pInBuf = pInputBuf;
    uint8_t *pInBuf_U = pInputBuf + ImgSize;
    uint8_t *pInBuf_V = pInBuf_U + (ImgSize>>2);
    uint8_t *pOutBuf = pOutputBuf;
    uint8_t *pOutBuf_U = pOutputBuf+ImgSize;
    uint8_t *pOutBuf_V = pOutBuf_U+(ImgSize>>2);

    LOG_FUNCTION_NAME

    width = w/2;
    height = h/2;

    angle = (360+angle)%360;

    if(angle == 90) {
        //Y
        for(y=0 ; y<w ; y++)
        {
            for(x=0 ; x<h ; x++)
            {
                pOutBuf[(x*w+(w-1-y))] = pInBuf[(y*h+x)];
            }
        }

        //UV
        for(y=0 ; y<width ; y++)
        {
            for(x=0 ; x<height ; x++)
            {
                pOutBuf_U[(x*width+(width-1-y))] = pInBuf_U[(y*height+x)];
                pOutBuf_V[(x*width+(width-1-y))] = pInBuf_V[(y*height+x)];                
            }
        }
    } else if(angle == 180) {
        //Y
        for(y=0 ; y<h ; y++) 
            for(x=0 ; x<w ; x++)
                pOutBuf[(x+y*w)] = pInBuf[((h-y-1)*w-x)];

        //UV
        for(y=0 ; y<width ; y++) {
            for(x=0 ; x<height ; x++) {
                pOutBuf_U[(x+y*width)] = pInBuf_U[((height-y-1)*width-x)];
                pOutBuf_V[(x+y*width)] = pInBuf_V[((height-y-1)*width-x)];                
            }
        }        
    } else if(angle == 270) {
        //Y
        for(y=0 ; y<w ; y++) 
            for(x=0 ; x<h ; x++)
                pOutBuf[(x*w+(w-1-y))] = pInBuf[((w-1-y)*h+(h-1-x))];

        //UV
        for(y=0 ; y<width ; y++) {
            for(x=0 ; x<height ; x++) {
                pOutBuf_U[(x*width+(width-1-y))] = pInBuf_U[((width-1-y)*height+(height-1-x))];
                pOutBuf_V[(x*width+(width-1-y))] = pInBuf_V[((width-1-y)*height+(height-1-x))];                
            }
        }        
    } else {
        //Y
        for(y=0 ; y<h ; y++) 
            for(x=0 ; x<w ; x++)
                pOutBuf[(x+y*w)] = pInBuf[(x+y*w)];

        //UV
        for(y=0 ; y<width ; y++) {
            for(x=0 ; x<height ; x++) {
                pOutBuf_U[(x+y*width)] = pInBuf_U[(x+y*width)];
                pOutBuf_V[(x+y*width)] = pInBuf_V[(x+y*width)];                
            }
        }        
    }    

    LOG_FUNCTION_NAME_EXIT
}

void CameraHal::setRotateYUV422(uint8_t* pInputBuf, uint8_t* pOutputBuf, int w, int h, int angle)
{
    typedef struct{
        uint8_t u;
        uint8_t y0;
        uint8_t v;
        uint8_t y1;
    } __attribute__ ((packed))UYVY;

    typedef struct{
        uint8_t u;
        uint8_t v;
        uint8_t y;
    } __attribute__ ((packed))UVY;

    int x, y, i, indexSrc, indexDest;

    UVY pUVYBuff444[w*h]; 
    UVY pUVYBuff444_Rotate[w*h];

    LOG_FUNCTION_NAME    

    for(i=0; i<w*h/2; i++) {
        pUVYBuff444[i*2].u    = pInputBuf[(i*4)];
        pUVYBuff444[i*2+1].u  = pInputBuf[(i*4)];
        pUVYBuff444[i*2].v    = pInputBuf[(i*4)+2];
        pUVYBuff444[i*2+1].v  = pInputBuf[(i*4)+2];
        pUVYBuff444[i*2].y    = pInputBuf[(i*4)+1];
        pUVYBuff444[i*2+1].y  = pInputBuf[(i*4)+3];
    }

    angle = (360+angle)%360;

    if(angle == 90) {
        for(y=0 ; y<w ; y++)
            for(x=0 ; x<h ; x++)
                pUVYBuff444_Rotate[(x*w+(w-1-y))] = pUVYBuff444[(y*h+x)];
    } else if(angle == 180) {
        for(y=0 ; y<h ; y++)
            for(x=0 ; x<w ; x++)
                pUVYBuff444_Rotate[(x+y*w)] = pUVYBuff444[((h-y-1)*w-x)];
    } else if(angle == 270){
        for(y=0 ; y<w ; y++)
            for(x=0 ; x<h ; x++)
                pUVYBuff444_Rotate[(x*w+(w-1-y))] = pUVYBuff444[((w-1-y)*h+(h-1-x))];
    } else {
        for(y=0 ; y<h ; y++)
            for(x=0 ; x<w ; x++)
                pUVYBuff444_Rotate[(x+y*w)] = pUVYBuff444[(x+y*w)];    
    }

    for(i=0 ; i<w*h/2 ; i++) {
        pOutputBuf[i*4]  = pUVYBuff444_Rotate[i*2].u;
        pOutputBuf[i*4+1] = pUVYBuff444_Rotate[i*2].y;
        pOutputBuf[i*4+2] = pUVYBuff444_Rotate[i*2].v;
        pOutputBuf[i*4+3] = pUVYBuff444_Rotate[i*2+1].y;
    }

    LOG_FUNCTION_NAME_EXIT
}

int CameraHal::getOrientation() const
{
    int rotationValue = 0;
    if( mParameters.get(mParameters.KEY_ROTATION) ) {
        rotationValue = atoi(mParameters.get(mParameters.KEY_ROTATION));
        if(rotationValue!=0) {
            HAL_PRINT("in CameraParameters.cpp getRotation not null int = %d \n", rotationValue);
        }
        return rotationValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp getRotation null \n");
        return 0;
    }
}

#ifdef SKT_FMC_DM
bool CameraHal::getSecurityCheck(void) const
{
    char CAM_PROPERTY[128] = {0,};

    property_get("persist.sys.camera_lock", CAM_PROPERTY, "Unknown");
    LOGE("getSecurityCheck() CAM_PROPERTY = %s\n", CAM_PROPERTY);

    if(strncmp(CAM_PROPERTY, "camera_lock.enabled", 19) == 0)
        return true;
    
    return false;
}
#endif

#ifdef EAS_IT_POLICY
bool CameraHal::getSamsungSecurityCheck(void) const
{
    char CAM_DPM_PROPERTY[10] = {0,};

    property_get("dpm.allowcamera", CAM_DPM_PROPERTY, "1");
    LOGE("getSamsungSecurityCheck() CAM_DPM_PROPERTY = %s\n", CAM_DPM_PROPERTY);

    if(strncmp(CAM_DPM_PROPERTY, "0", 1) == 0)
        return true;
    
    return false;
}
#endif

int CameraHal::getRotate() const
{
    int CameraRotationValue = 0;
    if( mParameters.get("rotation") ) {
        CameraRotationValue = atoi(mParameters.get("rotation"));
        if(CameraRotationValue >= 0 && CameraRotationValue <= 270) {
            HAL_PRINT("in CameraParameters.cpp getCameraRotation angle = %d \n", CameraRotationValue);
        }
        return CameraRotationValue;
    } else {
        HAL_PRINT("in CameraParameters.cpp rotation null \n");
        return 0;
    }
}

imageInfo CameraHal::getJpegThumbnailSize() const
{
    imageInfo ThumbImageInfo;
    if(mParameters.get("jpeg-thumbnail-width"))
        ThumbImageInfo.mImageWidth = atoi(mParameters.get(mParameters.KEY_JPEG_THUMBNAIL_WIDTH));
    
    if(mParameters.get("jpeg-thumbnail-height"))
        ThumbImageInfo.mImageHeight = atoi(mParameters.get(mParameters.KEY_JPEG_THUMBNAIL_HEIGHT));

    return ThumbImageInfo;
}


static CameraInfo sCameraInfo[] = {
    {
        CAMERA_FACING_BACK,
        90,  /* orientation */
    },
    {
        CAMERA_FACING_FRONT,
        270,  /* orientation */
    }
};


extern "C" int HAL_getNumberOfCameras()
{    
    return sizeof(sCameraInfo) / sizeof(sCameraInfo[0]);
}    

extern "C" void HAL_getCameraInfo(int cameraId, struct CameraInfo* cameraInfo)
{    
    memcpy(cameraInfo, &sCameraInfo[cameraId], sizeof(CameraInfo));
}    

extern "C" sp<CameraHardwareInterface> HAL_openCameraHardware(int cameraId)
{
    LOGD("opening TI camera hal, cameraID=%d",cameraId);
    return CameraHal::createInstance(cameraId);
}

}; // namespace android
