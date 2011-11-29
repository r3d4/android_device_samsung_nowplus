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


#ifndef ANDROID_HARDWARE_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_CAMERA_HARDWARE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include "binder/MemoryBase.h"
#include "binder/MemoryHeapBase.h"
#include <utils/threads.h>
#include <ui/Overlay.h>
#include <camera/CameraHardwareInterface.h>
#include "MessageQueue.h"

//[for EXIF
#include <cutils/tztime.h>
#include <math.h>

#include "Exif.h"
#include "ExifCreator.h"

extern "C" {
	#include <stdio.h>
	#include <sys/types.h>
}
//]

#include "RotationInterface.h"
#include "../include/videodev2.h"
#include "../liboverlay/overlay_common.h"
#include "../liboverlay/v4l2_utils.h"

//[Debugging Options
#define HAL_DEBUGGING		1
#define TIMECHECK			0
#define CHECK_FRAMERATE		0

#if HAL_DEBUGGING
#define HAL_PRINT(arg1,arg2...) LOGD(arg1,## arg2)
#else
#define HAL_PRINT(arg1,arg2...)
#endif

#define LOG_FUNCTION_NAME      		LOGD("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT 		LOGD("%d: %s() EXIT", __LINE__, __FUNCTION__);
//]

#define EVE_CAM //NCB-TI
//#define VT_BACKGROUND_SOLUTION	// Latona TD/Heron : It's for VT of CMCC projects
#define CAMERA_MODE_JPEG    1
#define CAMERA_MODE_YUV     2
#define MAIN_CAMERA         0
#define VGA_CAMERA          1
#define RESIZER             1
#define JPEG                1

//#define MAIN_CAM_CAPTURE_YUV    // use YUV,OMX jpeg encoder instead of camera ISP

#define OMAP_SCALE			0	// RealCAM Preview & Capture landscpae view option for GB

#define CLEAR(x) memset (&(x), 0, sizeof (x))


#ifdef OMAP_ENHANCEMENT

#ifdef HARDWARE_OMX
#include "JpegEncoder.h"
#include "JpegDecoder.h"
#endif

#endif

#define FOCUS_RECT          		0
/* colours in focus rectangle */
#define FOCUS_RECT_RED      		0x10
#define FOCUS_RECT_GREEN    		0x1F
#define FOCUS_RECT_WHITE    		0xFF

#define VIDEO_DEVICE5       		"/dev/video5"
#define VIDEO_DEVICE        		"/dev/video0"

#define MIN_WIDTH           		128
#define MIN_HEIGHT          		96
#define CIF_WIDTH           		352
#define CIF_HEIGHT          		288

#define PICTURE_WIDTH       		2560 /* 5mp - 2560. 8mp - 3280 */ /* Make sure it is a multiple of 16. */
#define PICTURE_HEIGHT      		1920 /* 5mp - 2048. 8mp - 2464 */ /* Make sure it is a multiple of 16. */
#define PREVIEW_WIDTH       		640
#define PREVIEW_HEIGHT      		480
#define JPEG_THUMBNAIL_WIDTH		160
#define JPEG_THUMBNAIL_HEIGHT		120

#define PIXEL_FORMAT           		V4L2_PIX_FMT_UYVY
#define PIXEL_FORMAT_JPEG      		V4L2_PIX_FMT_JPEG

#define VIDEO_FRAME_COUNT_MAX		NUM_OVERLAY_BUFFERS_REQUESTED
#define MAX_CAMERA_BUFFERS    		NUM_OVERLAY_BUFFERS_REQUESTED

#define DEFAULT_CAMERA_WIDTH    	640 // Eclair Camera for zoom2
#define DEFAULT_CAMERA_HEIGHT   	480 // Eclair Camera for zoom2

#define ZEUS_CAMERA_WIDTH       	640 // Eclair Camera for zoom2
#define ZEUS_CAMERA_HEIGHT      	480 // Eclair Camera for zoom2

#define OPEN_CLOSE_WORKAROUND	  	0

#define PIX_YUV422I 				0  //(aka "YUY2") - YUV 4:2:2 (16bpp), interleaved format, Y-U-Y-V (Y/Cb/Y/Cr) order 
#define PIX_YUV420P 				1  //(aka "I420") - YUV 4:2:0 (12bpp), basic planar format, Y-U-V (Y/Cb/Cr) order 

#define UYV_BYTES_PER_PIXEL         2
#define JPG_BYTES_PER_PIXEL         2
#define PIX_YUV422I_BYTES_PER_PIXEL 2
#define PIX_YUV420P_BYTES_PER_PIXEL 3/2


#ifndef max
#define max(a,b) ({typeof(a) _a = (a); typeof(b) _b = (b); _a > _b ? _a : _b; })
#define min(a,b) ({typeof(a) _a = (a); typeof(b) _b = (b); _a < _b ? _a : _b; })
#endif

#if TIMECHECK
#define PPM(str){ \
	gettimeofday(&ppm, NULL); \
	ppm.tv_sec = ppm.tv_sec - ppm_start.tv_sec; \
	ppm.tv_sec = ppm.tv_sec * 1000000; \
	ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_start.tv_usec; \
	LOGD("PPM: %s :%ld.%ld ms",str, ppm.tv_sec/1000, ppm.tv_sec%1000 ); \
}
#else
#define PPM(str)
#endif

#define HALO_ISO

namespace android {

#define DTP_FILE_NAME    						"/data/dyntunn.enc"
#define EEPROM_FILE_NAME 						"/data/eeprom.hex"
#define LIBICAPTURE_NAME 						"libicapture.so"

#define	CAM_EXIF_DEFAULT_EXIF_MAKER_INFO		"SAMSUNG"
#define	CAM_EXIF_DEFAULT_EXIF_MODEL_INFO		"NOWPLUS BOARD"
#define	CAM_EXIF_DEFAULT_EXIF_SOFTWARE_INFO		"Gingerbread"

//#define LIB3AFW "libMMS3AFW.so"	// 3A FW

#define PHOTO_PATH 								"/sdcard/photo_%02d.%s"

#define START_CMD     							0x0
#define STOP_CMD      							0x1
#define RUN_CMD       							0x2
#define TAKE_PICTURE  							0x3
#define NOOP          							0x4

#define CAMERA_MODE    							1
#define CAMCORDER_MODE 							2
#define VT_MODE        							3
#define Trd_PART       							4    

#define NEON

#define ROTATEANGLE   							270
#define VTROTATEANGLE 							90

#define CAMERA_DEVICE_ERROR_FOR_UNKNOWN    		1
#define CAMERA_DEVICE_ERROR_FOR_RESTART    		-1000
#define CAMERA_DEVICE_ERROR_FOR_EXIT       		-1001
#define CAMERA_DEVICE_FIRMWAREUPDATE       		-100
#define CAMERA_DEVICE_RESET_RESTART_COUNT  		-200

#define CAMERA_AF_FAIL    						0
#define CAMERA_AF_SUCCESS 						1
#define CAMERA_AF_CANCEL  						2

	struct imageInfo{
		int mImageWidth;
		int mImageHeight;
	};

	class CameraHal : public CameraHardwareInterface {
		public:

			//[ In "CameraHardwareInterface.h" functions
			virtual sp<IMemoryHeap> getPreviewHeap() const ;
			virtual sp<IMemoryHeap> getRawHeap() const;

			virtual void  setCallbacks(notify_callback notify_cb,
					data_callback data_cb,
					data_callback_timestamp data_cb_timestamp,
					void* user);

			virtual void enableMsgType(int32_t msgType);
			virtual void disableMsgType(int32_t msgType);
			virtual bool msgTypeEnabled(int32_t msgType);

			virtual status_t startPreview();

			virtual bool useOverlay() { return true; }
			virtual status_t setOverlay(const sp<Overlay> &overlay);
   
			virtual void stopPreview();
			virtual bool previewEnabled();

			virtual status_t startRecording();  
			virtual void stopRecording();
			virtual bool recordingEnabled();
			virtual void releaseRecordingFrame(const sp<IMemory>& mem);

        	virtual status_t autoFocus(); 
			virtual status_t cancelAutoFocus();

			virtual status_t takePicture();		
			virtual status_t cancelPicture();	

			virtual status_t setParameters(const CameraParameters& params);
			virtual CameraParameters getParameters() const;

			virtual status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);
			virtual void release();
			virtual status_t dump(int fd, const Vector<String16>& args) const;
			//] In "CameraHardwareInterface.h" functions

			static int beginAutoFocusThread(void *cookie);
			virtual void autoFocusThread();
			static int beginCancelAutoFocusThread(void *cookie);
			virtual void cancelAutoFocusThread();
			void dumpFrame(void *buffer, int size, char *path);
			void initDefaultParameters(int cameraId);
			static sp<CameraHardwareInterface> createInstance(int cameraId);

			virtual void DrawHorizontalLineForOverlay(int yAxis, int xLeft, int xRight, uint8_t yValue, uint8_t cbValue, uint8_t crValue, int previewWidth, int previewHeight);
			virtual void DrawVerticalLineForOverlay(int xAxis, int yTop, int yBottom, uint8_t yValue, uint8_t cbValue, uint8_t crValue, int previewWidth, int previewHeight);
			virtual void DrawHorizontalLineMixedForOverlay(int row, int col, int size, uint8_t yValue, uint8_t cbValue, uint8_t crValue, int fraction, int previewWidth, int previewHeight);

		private:

			class PreviewThread : public Thread {
				CameraHal* mHardware;
				int camId;
				public:
				PreviewThread(CameraHal* hw, int cameraId)
					: Thread(false), mHardware(hw), camId(cameraId) { }

				virtual bool threadLoop() {
					mHardware->previewThread(camId);
					return false;
				}
			};

			class ProcThread : public Thread {
				CameraHal *mHardware;
				public:
				ProcThread(CameraHal *hw) : Thread(false), mHardware(hw) {}
				virtual bool threadLoop(){
					mHardware->procThread();
					return false;			
				}
			};

			class FacetrackingThread : public Thread{
				CameraHal *mHardware;
				public:
				FacetrackingThread(CameraHal *hw) : Thread(false), mHardware(hw) {}
				virtual bool threadLoop(){
					mHardware->facetrackingThread();
					return false;
				}
			};

			static int onSaveH3A(void *priv, void *buf, int size);
			static int onSaveLSC(void *priv, void *buf, int size);
			static int onSaveRAW(void *priv, void *buf, int size);

			CameraHal(int cameraId);
			virtual ~CameraHal();
			void previewThread(int cameraId);
			static int beginPictureThread(void *cookie);

			int validateSize(int w, int h);	
			void drawRect(uint8_t *input, uint8_t color, int x1, int y1, int x2, int y2, int width, int height);
			void procThread();
			void facetrackingThread();

			//Eclair Camera L25.12
			int ZoomPerform(int zoom); //Eclair Camera L25.14, &L25.12
			//Eclair Camera L25.12

			void nextPreview();
			int ICapturePerform();
			int ICaptureCreate(void);
			int ICaptureDestroy(void);
			int CapturePicture();
			
			//[for EXIF
			void CreateExif(unsigned char* pInThumbnailData,int Inthumbsize,unsigned char* pOutExifBuf,int& OutExifSize,int flag);
			bool CreateJpegWithExif(unsigned char* pInJpegData, int InJpegSize,unsigned char* pInExifBuf,
					int InExifSize,unsigned char* pOutJpegData, int& OutJpegSize);	
			int GetJpegImageSize();
			int GetThumbNailDataSize();
			int GetThumbNailOffset();
			int GetYUVOffset();
			int GetJPEG_Capture_Width();
			int GetJPEG_Capture_Height();
			int GetCamera_version();		
			void convertFromDecimalToGPSFormat(double,int&,int&,double&);
			void getExifInfoFromDriver(v4l2_exif* );
			int convertToExifLMH(int, int);
			//]
            void setRotateYUV420(uint8_t* pInputBuf, uint8_t* pOutputBuf, int w, int h, int angle);
            void setRotateYUV422(uint8_t* pInputBuf, uint8_t* pOutputBuf, int w, int h, int angle);

			
			int CameraCreate(int );
			int CameraDestroy();
			int CameraConfigure();	
			int CameraSetFrameRate();
			int initCameraSetFrameRate();
			int CameraStart();
			int CameraStop();
			int SaveFile(char *filename, char *ext, void *buffer, int jpeg_size);

			int isStart_VPP;
			int isStart_Scale;

			status_t setWB(const char* wb);
			status_t setEffect(const char* effect);
			status_t setAntiBanding(const char* antibanding);
			status_t setSceneMode(const char* scenemode);
			status_t setFlashMode(const char* flashmode);
			status_t setMovieFlash(int flag);
			status_t setBrightness(int brightness);
			status_t setExposure(int exposure);
			status_t setZoom(int zoom);

#ifdef HALO_ISO
			status_t setISO(const char* iso);
#else
			void setISO(int iso);
#endif
			status_t setContrast(int contrast);
			status_t setSaturation(int saturation);
			status_t setSharpness(int sharpness);
			status_t setWDRMode(int wdr);
			status_t setAntiShakeMode(int antiShake);
			status_t setFocusMode(const char* focus);
			status_t setMetering(const char* metering);
			status_t setPrettyMode(int pretty);
			status_t setJpegMainimageQuality(int quality);
			status_t setGPSLatitude(double gps_latitude);
			status_t setGPSLongitude(double gps_longitude);
			status_t setGPSAltitude(double gps_altitude);
			status_t setGPSTimestamp(long gps_timestamp);
			status_t setGPSProcessingMethod(const char *gps_processingmethod);
			status_t setJpegThumbnailSize(imageInfo imgInfo);

			enum AE_AWB_LOCK_UNLOCK
			{
                AE_LOCK_AWB_LOCK = 1,
                AE_LOCK_AWB_UNLOCK,
                AE_UNLOCK_AWB_LOCK, 
                AE_UNLOCK_AWB_UNLOCK
			};

			int setAEAWBLockUnlock(int, int );
			int setObjectPosition(int, int);
			int setFaceDetectLockUnlock(int );
			int setObjectTrackingStartStop(int );
			int setTouchAFStartStop(int );
			int setDefultIMEI(int );	
            int setFlip(int );            
			int setCAFStart(int );	
			int setCAFStop(int );		

			int m_touch_af_start_stop;
			int m_focus_mode;
			int m_iso;
			int m_default_imei;

			void setDatalineCheckStart();
			void setDatalineCheckStop();
			void setCameraMode(int32_t mode);	
			void setSamsungCamera();			

			const char *getEffect() const;

			/* White Balance Lighting Conditions */
			const char *getWBLighting() const;

			/* Anti Banding */
			const char *getAntiBanding() const;
			const char *getSceneMode() const;
			const char *getFlashMode() const;

			/* Main image quality */
			int getJpegMainimageQuality() const;
			
			/* Brightness control */
			int getBrightness() const;
			int getExposure() const;

			/* Digital Zoom control */
			int getZoomValue() const;
			
			double getGPSLatitude() const;
			double getGPSLongitude() const;
			double getGPSAltitude() const;
			long getGPSTimestamp() const;
			const char *getGPSProcessingMethod() const;		
			
#ifdef HALO_ISO    
			const char *getISO() const;
#else
			int getISO() const;
#endif
			int getContrast() const;
			int getSaturation() const;
			int getSharpness() const;
			int getWDRMode() const;
			int getAntiShakeMode() const;
			const char *getFocusMode() const;
			const char *getMetering() const;

			int getCameraSelect() const;
			int getCamcorderPreviewValue() const;
			int getVTMode() const;
			int getPrettyValue() const;
			int getPreviewFrameSkipValue() const;

			void setDriverState(int state);
			void mirrorForVTC(unsigned char * aSrcBufPtr, unsigned char * aDstBufPtr,int  aFramewidth,int  aFrameHeight);
			int getTwoSecondReviewMode() const;
			int getPreviewFlashOn() const;
			int getCropValue() const;
			int checkFirmware();
			int getSamsungCameraValue() const;
			void rotate90_out(unsigned char *pInputBuf, unsigned char *pOutputBuf,int w, int h);
			int getOrientation() const;
			imageInfo getJpegThumbnailSize() const;
#ifdef EVE_CAM		
			void DrawOverlay(uint8_t *pBuffer, bool bCopy);
			void PreviewConversion(uint8_t *pInBuffer, uint8_t *pOutBuffer);
#endif
			mutable Mutex mLock;
			CameraParameters mParameters;
			sp<MemoryHeapBase> mPictureHeap;
			sp<MemoryHeapBase> mJPEGPictureHeap;
			int mPictureOffset, mJPEGOffset, mJPEGLength, mPictureLength;
			void *mYuvBuffer, *mJPEGBuffer;

			//[20091216 Ratnesh NEC
			sp<MemoryHeapBase> mFinalPictureHeap;
			sp<MemoryHeapBase> mYUVPictureHeap;
			sp<MemoryBase> mYUVPictureBuffer;
            sp<IMemoryHeap> mYUVNewheap;
			//]
			sp<MemoryHeapBase> mVGAYUVPictureHeap;
			sp<MemoryBase> mVGAYUVPictureBuffer;
			sp<IMemoryHeap> mVGANewheap;

			int iOutStandingBuffersWithEncoder;
			int iConsecutiveVideoFrameDropCount;

			int  mPreviewFrameSize;
			sp<Overlay>  mOverlay;
			sp<PreviewThread>  mPreviewThread;
			bool mPreviewRunning;
			Mutex               mRecordingLock;
			int mRecordingFrameSize;
			// Video Frame Begin
			int                 mVideoBufferCount;
			sp<MemoryHeapBase>  mVideoHeap;
#ifdef EVE_CAM		
			sp<MemoryHeapBase>  mVideoHeap_422;
			sp<MemoryHeapBase>  mVideoHeaps_422;
			sp<MemoryBase>		mVideoBuffer_422[VIDEO_FRAME_COUNT_MAX];
#endif		
			sp<MemoryHeapBase>  mVideoHeaps[VIDEO_FRAME_COUNT_MAX];
			sp<MemoryBase>      mVideoBuffer[VIDEO_FRAME_COUNT_MAX];
#ifdef VT_BACKGROUND_SOLUTION
			sp<MemoryHeapBase>  mVTHeaps[VIDEO_FRAME_COUNT_MAX];
			sp<MemoryBase>      mVTBuffer[VIDEO_FRAME_COUNT_MAX];
#endif
			sp<MemoryHeapBase>  mVideoConversionHeap;
			sp<MemoryBase>      mVideoConversionBuffer[VIDEO_FRAME_COUNT_MAX];
			sp<MemoryHeapBase>  mHeapForRotation;
			sp<MemoryBase>      mBufferForRotation;
			v4l2_buffer         mfilledbuffer[VIDEO_FRAME_COUNT_MAX];
			unsigned long       mVideoBufferPtr[VIDEO_FRAME_COUNT_MAX];
			int                 mVideoBufferUsing[VIDEO_FRAME_COUNT_MAX];
			int                 mRecordingFrameCount;
			void*               mPreviewBlocks[VIDEO_FRAME_COUNT_MAX];
		
			int nOverlayBuffersQueued;
			int nCameraBuffersQueued;
			struct v4l2_buffer v4l2_cam_buffer[MAX_CAMERA_BUFFERS];
			int buffers_queued_to_dss[MAX_CAMERA_BUFFERS];
			int buffers_queued_to_camera_driver[MAX_CAMERA_BUFFERS];   // Added for CSR - OMAPS00242402

			bool mDSSActive;	// OVL_PATCH
			
			int nBuffToStartDQ;
			int mfirstTime;
			static wp<CameraHardwareInterface> singleton;
			static int camera_device;
			struct timeval ppm;
			struct timeval ppm_start;

			int mippMode;
			int pictureNumber;
			int rotation; 
			struct timeval take_before, take_after;
			struct timeval focus_before, focus_after;
			struct timeval ppm_before, ppm_after;
			struct timeval ipp_before, ipp_after;
			struct v4l2_buffer mCfilledbuffer;
			int lastOverlayIndex;
			int lastOverlayBufferDQ;
#ifdef EVE_CAM
			int mBufferCount_422;				
#endif 	

			/*--------------Eclair Camera HAL---------------------*/	
			notify_callback 			mNotifyCb;
			data_callback   			mDataCb;
			data_callback_timestamp 	mDataCbTimestamp;
			void                 		*mCallbackCookie;
			int32_t	mMsgEnabled;
			bool    mRecordEnabled; 
			nsecs_t mCurrentTime;   
			bool mFalsePreview;
			bool mRecorded;
			bool mAutoFocusRunning;	
			int mPreviousWB;
			int mPreviousEffect;
			int mPreviousAntibanding;
			int mPreviousSceneMode;
			int mPreviousFlashMode;
			int mPreviousBrightness;
			int mPreviousExposure;
			int mPreviousZoom;
			int mPreviousIso;
			int mPreviousContrast;
			int mPreviousSaturation;
			int mPreviousSharpness;
			int mPreviousWdr;
			int mPreviousAntiShake;
			int mPreviousFocus;
			int mPreviousMetering;
			int m_chk_dataline;
			bool m_chk_dataline_end;
			bool mAFState;
			bool mCaptureFlag;
			int mCamera_Mode;
			int mCameraIndex;
			int mPreviousPretty;
			int mPreviousQuality;
			int mYcbcrQuality;
			bool mASDMode;
			int mPreviewFrameSkipValue;

			int32_t mCameraMode;
			bool mSamsungCamera;
            bool mAutoFocusUsed; //was AF used        
			int mCamMode;		
			int mCounterSkipFrame;
			int mSkipFrameNumber;
			unsigned int mPassedFirstFrame;
			unsigned int mOldResetCount;
			int mPreviousFlag;
			int dequeue_from_dss_failed;	
#ifdef HALO_ISO
			int mPreviousISO;
#endif
			int mPreviewWidth;
			int mPreviewHeight;


#ifdef OMAP_ENHANCEMENT	  
#ifdef HARDWARE_OMX
			JpegEncoder*    jpegEncoder;
#ifdef jpeg_decoder
			JpegDecoder*    jpegDecoder;
#endif    
#endif    
#endif
			int file_index;
			int cmd;
			int quality;
			unsigned int sensor_width;
			unsigned int sensor_height;
			unsigned int zoom_width;
			unsigned int zoom_height;
			int mflash;
			int mred_eye;
			int mcapture_mode;
			int mzoom;
			int mcaf;
			int j;
			int myuv;
			int mMMSApp;

			NEON_fpo Neon_Rotate;
			NEON_FUNCTION_ARGS* neon_args;
			void* pTIrtn;

			enum PreviewThreadCommands {

				// Comands       
				PREVIEW_START,
				PREVIEW_STOP,
				PREVIEW_AF_START,
				PREVIEW_AF_CANCEL,
				PREVIEW_AF_STOP,
				PREVIEW_CAPTURE,
				PREVIEW_CAPTURE_CANCEL,
				PREVIEW_KILL,
				PREVIEW_CAF_START,
				PREVIEW_CAF_STOP,
				PREVIEW_FPS,
				ZOOM_UPDATE,           
				// ACKs        
				PREVIEW_ACK,
				PREVIEW_NACK,
				CAPTURE_ACK,
				CAPTURE_NACK,
			};

			enum ProcessingThreadCommands {

				// Comands        
				PROCESSING_PROCESS,
				PROCESSING_CANCEL,
				PROCESSING_KILL,

				// ACKs        
				PROCESSING_ACK,
				PROCESSING_NACK,
			};    

			enum COMMAND_DEFINE
			{
				COMMAND_AE_AWB_LOCK_UNLOCK 			= 1101,
				COMMAND_FACE_DETECT_LOCK_UNLOCK 	= 1102,
				COMMAND_OBJECT_POSITION 			= 1103,
				COMMAND_OBJECT_TRACKING_STARTSTOP 	= 1104,
				COMMAND_TOUCH_AF_STARTSTOP 			= 1105,
				COMMAND_CHECK_DATALINE 				= 1106,
				COMMAND_DEFAULT_IMEI 				= 1107,
			};

			MessageQueue    previewThreadCommandQ;
			MessageQueue    previewThreadAckQ;    
			MessageQueue    processingThreadCommandQ;
			MessageQueue    processingThreadAckQ;

			mutable Mutex takephoto_lock;
			uint8_t *yuv_buffer, *jpeg_buffer, *vpp_buffer, *ancillary_buffer;
			int yuv_len, jpeg_len, ancillary_len;
#ifdef FOCUS_RECT
			/* Focus rectangle  flags */
			int focus_rect_set;
			int focus_rect_color;
#endif
			double mPreviousGPSLatitude;
			double mPreviousGPSLongitude;
			double mPreviousGPSAltitude;
			long mPreviousGPSTimestamp;
			struct tm *m_timeinfo;
			char m_gps_date[11];
			time_t m_gps_time;
			int m_gpsHour;
			int m_gpsMin;
			int m_gpsSec;
			char mPreviousGPSProcessingMethod[150];
			int mThumbnailWidth;
			int mThumbnailHeight;
	};
}; // end of namespace android

extern "C" {
	int scale_init(int inWidth, int inHeight, int outWidth, int outHeight, int inFmt, int outFmt);
	int scale_deinit();
	int scale_process(void* inBuffer, int inWidth, int inHeight, void* outBuffer, int outWidth, int outHeight, int rotation, int fmt, float zoom);
}

extern "C" {
	int ColorConvert_Init(int , int , int);
	int ColorConvert_Deinit();
	int ColorConvert_Process(char *, char *);

	void Neon_Convert_yuv422_to_NV21(unsigned char * aSrcBufPtr, unsigned char * aDstBufPtr,unsigned int  aFramewidth,unsigned int  aFrameHeight);
	void Neon_Convert_yuv422_to_NV12(unsigned char * aSrcBufPtr, unsigned char * aDstBufPtr,unsigned int  aFramewidth,unsigned int  aFrameHeight);
	void Neon_Convert_yuv422_to_YUV420P(unsigned char * aSrcBufPtr, unsigned char * aDstBufPtr,unsigned int  aFramewidth,unsigned int  aFrameHeight);
}
#endif
