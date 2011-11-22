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
#include <cutils/tztime.h>
#include <math.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include <semaphore.h>
#include "binder/MemoryBase.h"
#include "binder/MemoryHeapBase.h"
#include <utils/threads.h>
#include <camera/CameraHardwareInterface.h>
#include "MessageQueue.h"
#include "Exif.h"
#include "ExifCreator.h"

extern "C" {
#include <stdio.h>
#include <sys/types.h>
}

#include "../../../../kernel/include/linux/videodev2.h"
#include "overlay_common.h"

#define CAMERA_MODE_JPEG    1
#define CAMERA_MODE_YUV     2

#define MAIN_CAMERA         0
#define VGA_CAMERA          1

#define JPEG_ENCODER        1

#define VPP_THREAD          1
#define VPP_CONVERSION      1

#define USE_NEON            1
#define OMAP_SCALE_CDD      1

#define CLEAR(x) memset (&(x), 0, sizeof (x))

#ifdef HARDWARE_OMX
#include "JpegEncoder.h"
#include "JpegDecoder.h"
#endif

//#define CAMERA_PANORAMA
//#define CAMERA_SMILE

/* colours in focus rectangle */
#define FOCUS_RECT_RED      0x10
#define FOCUS_RECT_GREEN    0x1F
#define FOCUS_RECT_WHITE    0xFF

#define VIDEO_DEVICE5       "/dev/video5"
#define VIDEO_DEVICE        "/dev/video0"
#define MIN_WIDTH           128
#define MIN_HEIGHT          96
#define CIF_WIDTH           352
#define CIF_HEIGHT          288
#define PICTURE_WIDTH       2560 
#define PICTURE_HEIGHT      1920
#define PREVIEW_WIDTH       640
#define PREVIEW_HEIGHT      480
#define THUMBNAIL_WIDTH     160
#define THUMBNAIL_HEIGHT    120

#define PIXEL_FORMAT_UYVY       V4L2_PIX_FMT_UYVY
#define PIXEL_FORMAT_JPEG       V4L2_PIX_FMT_JPEG

#define LOG_FUNCTION_NAME       LOGV("%d: %s() ENTER", __LINE__, __FUNCTION__);
#define LOG_FUNCTION_NAME_EXIT  LOGV("%d: %s() EXIT", __LINE__, __FUNCTION__);

#define VIDEO_FRAME_COUNT_MAX   NUM_OVERLAY_BUFFERS_REQUESTED
#define MAX_CAMERA_BUFFERS      NUM_OVERLAY_BUFFERS_REQUESTED

#define DEFAULT_CAMERA_WIDTH    640
#define DEFAULT_CAMERA_HEIGHT   480

#define ZEUS_CAMERA_WIDTH       640
#define ZEUS_CAMERA_HEIGHT      480

#define PIX_YUV422I 0
#define PIX_YUV420P 1

//#define HAL_DEBUGGING
//#define TIMECHECK
//#define CHECK_FRAMERATE

//define this macro to disable camera/camcorder preview
#undef PREVIEW_DISABLED

#ifdef HAL_DEBUGGING
#define HAL_PRINT(arg1,arg2...) LOGD(arg1,## arg2)
#else
#define HAL_PRINT(arg1,arg2...)
#endif

#define PPM(str){ \
    gettimeofday(&ppm, NULL); \
    ppm.tv_sec = ppm.tv_sec - ppm_start.tv_sec; \
    ppm.tv_sec = ppm.tv_sec * 1000000; \
    ppm.tv_sec = ppm.tv_sec + ppm.tv_usec - ppm_start.tv_usec; \
    LOGD("PPM: %s :%ld.%ld ms",str, ppm.tv_sec/1000, ppm.tv_sec%1000 ); \
}

namespace android {

    //icapture
    #define DTP_FILE_NAME    "/data/dyntunn.enc"
    #define EEPROM_FILE_NAME "/data/eeprom.hex"
    #define LIBICAPTURE_NAME "libicapture.so"

    #define CAM_EXIF_DEFAULT_EXIF_MAKER_INFO        "SAMSUNG"
    #define CAM_EXIF_DEFAULT_EXIF_MODEL_INFO        "ZOOM BOARD"
    #define CAM_EXIF_DEFAULT_EXIF_SOFTWARE_INFO     "Eclair"

    // 3A FW
    //#define LIB3AFW "libMMS3AFW.so"
    #define PHOTO_PATH "/sdcard/photo_%02d.%s"

    #define START_CMD     0x0
    #define STOP_CMD      0x1
    #define RUN_CMD       0x2
    #define TAKE_PICTURE  0x3
    #define NOOP          0x4

    #define CAMERA_MODE    1
    #define CAMCORDER_MODE 2
    #define VT_MODE        3
    #define Trd_PART       4

    #define NEON
    #define SKIP_FRAME
    #define SKT_FMC_DM //for SKT_FMC_DM
    #define EAS_IT_POLICY
    #define ROTATEANGLE   270
    #define VTROTATEANGLE 90

    #define CAMERA_DEVICE_ERROR_FOR_UNKNOWN    1
    #define CAMERA_DEVICE_ERROR_FOR_RESTART    -1
    #define CAMERA_DEVICE_ERROR_FOR_EXIT       -2
    #define CAMERA_DEVICE_FIRMWAREUPDATE       -100

    struct imageInfo{
        int mImageWidth;
        int mImageHeight;
    };

    struct RectInfo{
        int x1;
        int y1;
        int x2;
        int y2;
    };

    enum ArrowDirectionInfo{
      ARROW_RIGHT = 0x0001,                                        /**< Right moving direction */
      ARROW_LEFT  = 0x0002,                                        /**< Left moving direction */
      ARROW_UP    = 0x0004,                                        /**< Up moving direction */
      ARROW_DOWN  = 0x0008,                                        /**< Down moving direction */
      ARROW_AUTO_DIRECTION_HORIZONTAL = ARROW_RIGHT | ARROW_LEFT,        /**< Auto detect movement in horizontal direction */
      ARROW_AUTO_DIRECTION_VERTICAL = ARROW_UP | ARROW_DOWN,             /**< Auto detect movement in vertical direction */
      ARROW_AUTO_DIRECTION = ARROW_UP | ARROW_DOWN | ARROW_RIGHT | ARROW_LEFT  /**< Auto detect movement in horizontal and vertical directions */
    };    

    enum COMMAND_DEFINE
    {
#ifdef CAMERA_SMILE    
      CAMERA_SMILE_SHOT_DETECTION_START = 1017, 
      CAMERA_SMILE_SHOT_DETECTION_STOP,
      CAMERA_SMILE_SHOT_FACE_RECT_POSITION,
      CAMERA_SMILE_SHOT_MOUSE_RECT_POSITION,
#endif
#ifdef CAMERA_PANORAMA
      CAMERA_PANORAMA_SHOT_START,
      CAMERA_PANORAMA_SHOT_STOP,
      CAMERA_PANORAMA_SHOT_FINALIZE,
      CAMERA_PANORAMA_SHOT_CANCEL,
      CAMERA_PANORAMA_SHOT_RACT_POSITION,
      CAMERA_PANORAMA_SHOT_ARROW_DIRECTION,
#endif      
      CAMERA_CHECK_DATALINE = 1106
    };

#ifdef USE_NEON
    typedef enum TI_NEON_ROTATION_VAL
    {
        NEON_ROTNONE = 0,
        NEON_ROT90,
        NEON_ROT180,
        NEON_ROT270
    } TI_NEON_ROTATION_VAL;
    
    typedef struct NEON_FUNCTION_ARGS
    {
        unsigned char* pIn;
        unsigned char* pOut;
        unsigned short width;
        unsigned short height;
        unsigned char rotate;
    }NEON_FUNCTION_ARGS;
                                    
    typedef int (*NEON_fpo)(NEON_FUNCTION_ARGS *);
#endif

    class CameraHal : public CameraHardwareInterface {
    public:
        virtual sp<IMemoryHeap> getRawHeap() const;
        virtual void stopRecording();

        virtual status_t startRecording();
        virtual bool recordingEnabled();
        virtual void releaseRecordingFrame(const sp<IMemory>& mem);
        virtual sp<IMemoryHeap> getPreviewHeap() const ;

        virtual status_t startPreview();
        virtual bool useOverlay();
        virtual status_t setOverlay(const sp<Overlay> &overlay);
        virtual void stopPreview();
        virtual bool previewEnabled();

        virtual status_t autoFocus();


        virtual status_t takePicture();

        virtual status_t cancelPicture();
        virtual status_t cancelAutoFocus();

        virtual status_t dump(int fd, const Vector<String16>& args) const;
        void dumpFrame(void *buffer, int size, char *path);
        virtual status_t setParameters(const CameraParameters& params);
        virtual CameraParameters getParameters() const;
        virtual void release();
        void initDefaultParameters();

        /*--------------------Eclair HAL---------------------------------------*/
        virtual void  setCallbacks(notify_callback notify_cb,
                                    data_callback data_cb,
                                    data_callback_timestamp data_cb_timestamp,
                                    void* user);

        virtual void enableMsgType(int32_t msgType);
        virtual void disableMsgType(int32_t msgType);
        virtual bool msgTypeEnabled(int32_t msgType);
        /*--------------------Eclair HAL---------------------------------------*/
        static sp<CameraHardwareInterface> createInstance(int cameraId);

        virtual status_t sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);

  private:

        class PreviewThread : public Thread {
            CameraHal* mHardware;
            public:
                PreviewThread(CameraHal* hw)
                    : Thread(false), mHardware(hw) { }

                virtual bool threadLoop() {
                    mHardware->previewThread();

                    return false;
                }
        };
             
        static int onSaveH3A(void *priv, void *buf, int size);
        static int onSaveLSC(void *priv, void *buf, int size);
        static int onSaveRAW(void *priv, void *buf, int size);

        CameraHal(int cameraId);
        virtual ~CameraHal();
        void previewThread();
        int validateSize(int w, int h);
        void drawRect(uint8_t *input, uint8_t color, int x1, int y1, int x2, int y2, int width, int height);
        void drawLineMixed(uint8_t *frame, int row, int col, int size, int width, int height, uint8_t cbValue, uint8_t crValue, int fraction);
#ifdef CAMERA_PANORAMA
        void drawArrows(uint8_t *frame, ArrowDirectionInfo arrow_dir, int width, int height);
#endif
        int zoomPerform(int zoom);

        void nextPreview();
#if JPEG_ENCODER        
        int ICaptureCreate(void);
        int ICaptureDestroy(void);
#endif        
        int startCapture();

        void createExif(unsigned char* pInThumbnailData,int Inthumbsize,unsigned char* pOutExifBuf,int& OutExifSize,int flag);
        bool createJpegWithExif(unsigned char* pInJpegData, int InJpegSize,unsigned char* pInExifBuf,
                                   int InExifSize,unsigned char* pOutJpegData, int& OutJpegSize);
        int getJpegImageSize();
        int getJpegCaptureWidth();
        int getJpegCaptureHeight();
        int getThumbNailDataSize();
        int getThumbNailOffset();
        int getYUVOffset();
        void convertFromDecimalToGPSFormat(double,int&,int&,double&);
        void getExifInfoFromDriver(v4l2_exif* );
        int convertToExifLMH(int, int);

        int CameraCreate();
        int CameraDestroy();
        int CameraConfigure();
        int setCaptureFrameRate();
        int setPreviewFrameRate();
        int CameraStart();
        int CameraStop();
        int SaveFile(char *filename, char *ext, void *buffer, int jpeg_size);

        status_t setWB(const char* wb);
        status_t setEffect(const char* effect);
        status_t setAntiBanding(const char* antibanding);
        status_t setSceneMode(const char* scenemode);
        status_t setFlashMode(const char* flashmode);
        status_t setMovieFlash(int flag);
        status_t setBrightness(int brightness);
        status_t setExposure(int exposure);
        status_t setZoom(int zoom);
        status_t setISO(int iso);
        status_t setContrast(int contrast);
        status_t setSaturation(int saturation);
        status_t setSharpness(int sharpness);
        status_t setWDRMode(int wdr);
        status_t setAntiShakeMode(int antiShake);
        status_t setFocusMode(const char* focus);
        status_t setMetering(const char* metering);
        status_t setPrettyMode(int pretty);
        status_t setJpegMainimageQuality(int quality);
        status_t setRotate(int angle);
        status_t setGPSLatitude(double gps_latitude);
        status_t setGPSLongitude(double gps_longitude);
        status_t setGPSAltitude(double gps_altitude);
        status_t setGPSTimestamp(long gps_timestamp);
        status_t setGPSProcessingMethod(const char *gps_processingmethod);
        status_t setJpegThumbnailSize(imageInfo imgInfo);

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

        int getISO() const;
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
        void setRotateYUV420(unsigned char *pInputBuf, unsigned char *pOutputBuf, int w, int h, int angle);
        void setRotateYUV422(unsigned char *pInputBuf, unsigned char *pOutputBuf, int w, int h, int angle);
        int getOrientation() const;
        int getRotate() const;
#ifdef SKT_FMC_DM
        bool getSecurityCheck() const;
#endif
#ifdef EAS_IT_POLICY
        bool getSamsungSecurityCheck(void) const;
#endif
        imageInfo getJpegThumbnailSize() const;

#ifdef CAMERA_PANORAMA
        void setPanoramaMode(bool mode);
        void setPanoramaRect(int rect);
        void setPanoramaArrow(int arrow_dir);
#endif
#ifdef CAMERA_SMILE
        void setSmileMode(bool mode);
        void setFaceRect(int rect, int num);
        void setMouseRect(int rect, int num);
#endif
        mutable Mutex mLock;
        CameraParameters mParameters;
        sp<MemoryHeapBase> mPictureHeap;
        sp<MemoryHeapBase> mJPEGPictureHeap;
        int mPictureOffset, mJPEGOffset, mJPEGLength, mPictureLength;
        void *mYuvBuffer, *mJPEGBuffer;
        sp<MemoryHeapBase> mFinalPictureHeap;

        sp<MemoryHeapBase> mYUVPictureHeap;
        sp<MemoryBase> mYUVPictureBuffer;
        sp<IMemoryHeap> mYUVNewheap;
        sp<MemoryHeapBase> mVGAYUVPictureHeap;
        sp<MemoryBase> mVGAYUVPictureBuffer;
        sp<IMemoryHeap> mVGANewheap;
        sp<IMemoryHeap> mVGARotateheap;

        int iOutStandingBuffersWithEncoder;

        int  mPreviewFrameSize;
        sp<Overlay>  mOverlay;
        sp<PreviewThread>  mPreviewThread;
        bool mPreviewRunning;
        bool mPreviewNotReady;
        bool mUseOverlay;
        int mRecordingFrameSize;
        int mVideoBufferCount;
        Mutex mRecordingLock;
        int mConsecutiveDropCount;
        int mRecordSkip;

        sp<MemoryHeapBase>  mVideoHeap;
        sp<MemoryHeapBase>  mVideoHeaps[VIDEO_FRAME_COUNT_MAX];
        sp<MemoryBase>      mVideoBuffer[VIDEO_FRAME_COUNT_MAX];
        sp<MemoryHeapBase>  mVideoConversionHeap;
        sp<MemoryBase>      mVideoConversionBuffer[VIDEO_FRAME_COUNT_MAX];
#ifdef OMAP_SCALE_CDD
        sp<MemoryHeapBase>  mVideoHeap_422;
        sp<MemoryBase>      mVideoBuffer_422[VIDEO_FRAME_COUNT_MAX];    
        sp<MemoryHeapBase>  mVideoHeap_422_crop;
        sp<MemoryBase>      mVideoBuffer_422_crop[VIDEO_FRAME_COUNT_MAX];
        sp<MemoryHeapBase>  mVideoHeap_422_overlay[VIDEO_FRAME_COUNT_MAX];
        sp<MemoryBase>      mVideoBuffer_422_overlay[VIDEO_FRAME_COUNT_MAX];                 
#endif
        v4l2_buffer         mfilledbuffer[VIDEO_FRAME_COUNT_MAX];
        unsigned long       mVideoBufferPtr[VIDEO_FRAME_COUNT_MAX];
        int                 mVideoBufferUsing[VIDEO_FRAME_COUNT_MAX];
        int                 mRecordingFrameCount;
        void*               mPreviewBlocks[VIDEO_FRAME_COUNT_MAX];

        int nOverlayBuffersQueued;
        int nCameraBuffersQueued;
        struct v4l2_buffer v4l2_cam_buffer[MAX_CAMERA_BUFFERS];
        int buffers_queued_to_dss[MAX_CAMERA_BUFFERS];
        bool mDSSActive;
        int nBuffToStartDQ;
        int mfirstTime;
        static wp<CameraHardwareInterface> singleton;
        static int camera_device;
        struct timeval ppm;
        struct timeval ppm_start;
#if VPP_THREAD          
        pthread_mutex_t lock;
        pthread_mutexattr_t attr;
#endif
        int mippMode;
        int pictureNumber;
        int rotation;
        struct timeval take_before, take_after;
        struct timeval focus_before, focus_after;
        struct timeval ppm_before, ppm_after;
        struct timeval ipp_before, ipp_after;
        int lastOverlayIndex;
        int lastOverlayBufferDQ;

        notify_callback mNotifyCb;
        data_callback   mDataCb;
        data_callback_timestamp mDataCbTimestamp;
        void                   *mCallbackCookie;

        int32_t             mMsgEnabled;
        bool                mRecordRunning;
        nsecs_t             mCurrentTime;

        bool mFalsePreview;
        
        bool mRecordOverlay;

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

        bool mAFState;
        bool mCaptureFlag;

        int mCamera_Mode;
        int mCameraIndex;
        int mPreviousPretty;
        int mPreviousQuality;
        int mPreviousAngle;
        int mYcbcrQuality;
        bool mASDMode;
        int mPreviewFrameSkipValue;

        int mCamMode;
        int mCounterSkipFrame;
        int mSkipFrameNumber;
        bool mPassedFirstFrame;
        int mPreviousFlag;
        int mPreviewWidth;
        int mPreviewHeight;

#ifdef HARDWARE_OMX
#if JPEG_ENCODER
        JpegEncoder*    jpegEncoder;
        int isStart_JPEG;
#endif
#endif

#ifdef OMAP_SCALE_CDD
        int isStart_Scale;
        int isStart_Rotate;
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
#ifdef SKT_FMC_DM
        int mSkipFrameCount;
        bool mCurSecurity; //Security Area Check
#endif
#ifdef CAMERA_PANORAMA
        bool mPanoramaMode;
        RectInfo mPanoramaRect;
        ArrowDirectionInfo mPanoramaArrowDir;
#endif
#ifdef CAMERA_SMILE
        bool mSmileMode;
        int mFaceNum;
        int mMouseNum;
        RectInfo mFaceRect[3];
        RectInfo mMouseRect[3];
#endif

#ifdef USE_NEON
        NEON_fpo Neon_Rotate;
        NEON_FUNCTION_ARGS* neon_args;
        void* pTIrtn;
#endif
        enum PreviewThreadCommands {
            // Comands
            PREVIEW_NONE = 0,
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

        MessageQueue    previewThreadCommandQ;
        MessageQueue    previewThreadAckQ;
        MessageQueue    processingThreadCommandQ;
        MessageQueue    processingThreadAckQ;

        mutable Mutex takephoto_lock;
        uint8_t *yuv_buffer, *crop_buffer, *scale_buffer, *jpeg_buffer, *ancillary_buffer;

#if VPP_THREAD
#if VPP_CONVERSION
		int vppPipe[2];
        int isStart_convert;
#endif
#endif
        
        int capture_len, yuv_len, jpeg_len, ancillary_len;

        FILE *foutYUV;
        FILE *foutJPEG;

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
}; // namespace android

extern "C" {
    int scale_init(int inWidth, int inHeight, int outWidth, int outHeight, int inFmt, int outFmt);
    int scale_deinit(void);
    int scale_process_preview(void* inBuffer, int inWidth, int inHeight, void* outBuffer, int outWidth, int outHeight, int rotation, int fmt, float zoom);
    int scale_process_capture(void* inBuffer, int inWidth, int inHeight, void* outBuffer, int outWidth, int outHeight, int rotation, int fmt, float zoom);    
}

extern "C" {
    int ColorConvert_Init(int width, int height, int infmt, int outyuvfmt, int outrgbfmt, int rotateAngle);
    int ColorConvert_Process(char *inbuffer,char *outbuffer);
    int ColorConvert_Deinit(void);
    
    void Neon_Convert_yuv422_to_NV21(unsigned char * aSrcBufPtr, unsigned char * aDstBufPtr, unsigned int  aFramewidth, unsigned int  aFrameHeight);
    void Neon_Convert_yuv422_to_NV12(unsigned char * aSrcBufPtr, unsigned char * aDstBufPtr, unsigned int  aFramewidth, unsigned int  aFrameHeight);
    void Neon_Convert_yuv422_to_YUV420P(unsigned char * aSrcBufPtr, unsigned char * aDstBufPtr, unsigned int  aFramewidth, unsigned int  aFrameHeight);
}
#endif
