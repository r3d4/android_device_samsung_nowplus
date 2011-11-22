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
* @file CameraHal_Utils.cpp
*
* This file maps the Camera Hardware Interface to V4L2.
*
*/

#define LOG_TAG "CameraHalUtils"

#include "CameraHal.h"
#include <cutils/properties.h> // for property_get for the voice recognition mode switch
#include <pthread.h>

namespace android {

    #define SLOWMOTION4           60
    #define SLOWMOTION8           120
    #define SLOWMOTION4SKIP       3
    #define SLOWMOTION8SKIP       7

    #define CAMERA_FLIP_NONE             1
    #define CAMERA_FLIP_MIRROR           2
    #define CAMERA_FLIP_WATER            3
    #define CAMERA_FLIP_WATER_MIRROR     4

    #define FIXED2INT(x) (((x) + 128) >> 8)

    template <typename tn> inline tn max(tn a, tn b) { return (a>b) ? a:b; };
    template <typename tn> inline tn min(tn a, tn b) { return (a>b) ? b:a; };
    template <typename tn> inline tn abs(tn a) { return (a>=0) ? a:(-a); };
    template <typename tn> inline tn clamp(tn x, tn a, tn b) { return min(max(x, a), b); };    

    int CameraHal::setCaptureFrameRate()
    {
        struct v4l2_streamparm parm;
        int fps_min, fps_max;
        int framerate = 30;

        LOG_FUNCTION_NAME   

        if(mCameraIndex == MAIN_CAMERA)
            framerate = 30;
        else
            framerate = 15;

        HAL_PRINT("setCaptureFrameRate: framerate=%d\n",framerate);

        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.capturemode = mCamMode;
        parm.parm.capture.currentstate = V4L2_MODE_CAPTURE;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = framerate;
        if(ioctl(camera_device, VIDIOC_S_PARM, &parm) < 0) {
            LOGE("ERROR VIDIOC_S_PARM\n");
            goto s_fmt_fail;
        }

        HAL_PRINT("setCaptureFrameRate Capture fps:%d/%d\n",parm.parm.capture.timeperframe.denominator,parm.parm.capture.timeperframe.numerator);

        LOG_FUNCTION_NAME_EXIT
        return 0;

s_fmt_fail:
        return -1;

    }

    int CameraHal::setPreviewFrameRate()
    {       
        struct v4l2_streamparm parm;

        int fps_min, fps_max;
        int framerate = 30;

        LOG_FUNCTION_NAME   
       
        framerate = mParameters.getPreviewFrameRate();
        HAL_PRINT("setPreviewFrameRate: framerate=%d\n",framerate);

        if(mCameraIndex == MAIN_CAMERA) {
            if(mCamMode == VT_MODE) {
                switch(framerate) {
                case 7:
                case 10:
                case 15:
                    break;
                
                default:
                    framerate = 7;
                    break;
                }                
            } else  {
                int w, h;
               
                mParameters.getPreviewSize(&w, &h);

                if(w > 320 && h > 240) { // limit QVGA ~ WVGA
                    if(framerate > 30)
                        framerate = 30;
                } else {  //limit slowmotion
                    if(framerate > 120)
                        framerate = 30;
                }
            }
        } else  {
            switch(framerate) {
            case 7:
            case 10:
            case 15:
                break;
            
            default:
                framerate = 15;
                break;
            }
        }    
        
        HAL_PRINT("CameraSetFrameRate: framerate=%d\n",framerate);
        
#ifdef SKIP_ENABLED
        if(framerate == SLOWMOTION8)
            mSkipFrameNumber = SLOWMOTION8SKIP;
        else if(framerate == SLOWMOTION4)
            mSkipFrameNumber = SLOWMOTION4SKIP;
        else
            mSkipFrameNumber = 0;
#endif
        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.capturemode = mCamMode;
        parm.parm.capture.currentstate = V4L2_MODE_PREVIEW;
        parm.parm.capture.timeperframe.numerator = 1;
        parm.parm.capture.timeperframe.denominator = framerate;
        if(ioctl(camera_device, VIDIOC_S_PARM, &parm) < 0) {
            LOGE("ERROR VIDIOC_S_PARM\n");
            goto s_fmt_fail;
        }

        HAL_PRINT("setPreviewFrameRate Preview fps:%d/%d\n",parm.parm.capture.timeperframe.denominator,parm.parm.capture.timeperframe.numerator);

        LOG_FUNCTION_NAME_EXIT
        return 0;

s_fmt_fail:
        return -1;
    }

    /***************/

    void CameraHal::drawRect(uint8_t *input, uint8_t color, int x1, int y1, int x2, int y2, int width, int height)
    {
        int i, start_offset, end_offset;
        int mod = 0;
        int cb,cr;

        switch (color) {
        case FOCUS_RECT_WHITE:
            cb = 255;
            cr = 255;
            break;
        case FOCUS_RECT_GREEN:
            cb = 0;
            cr = 0;
            break;
        case FOCUS_RECT_RED:
            cb = 128;
            cr = 255;
            break;
        default:
            cb = 255;
            cr = 255;
            break;
        }

        if((x1 < 0 && x2 < 0) || (x1 > width && x2 > width)
        || (y1 < 0 && y2 < 0) || (y1 > height && y2 > height))
            return;

        if( x1 < 0 ) 
            x1 = 0;
        if( x1 > width ) 
            x1 = width;
        if( y1 < 0 ) 
            y1 = 0;
        if( y1 > height) 
            y1 = height;        
        if( x2 < 0 ) 
            x2 = 0;
        if( x2 > width ) 
            x2 = width;
        if( y2 < 0 ) 
            y2 = 0;
        if( y2 > height) 
            y2 = height;        

        if(y1 > 0 && y1 < height) {
            //Top Line
            start_offset = ((width * y1) + x1) * 2;
            end_offset =  start_offset + (x2 - x1) * 2;
            mod = 0;
            for ( i = start_offset; i < end_offset; i += 2) {
                if (mod % 2)
                    *( input + i ) = cb;
                else
                    *( input + i ) = cr;
                mod++;
            }

            start_offset = ((width * (y1 + 1)) + x1) * 2;
            end_offset =  start_offset + (x2 - x1) * 2;
            mod = 0;
            for ( i = start_offset; i < end_offset; i += 2) {
                if (mod % 2)
                    *( input + i ) = cb;
                else
                    *( input + i ) = cr;
                mod++;
            }
        }
        
        if(x1 > 0 && x1 < width) {
            //Left Line
            start_offset = ((width * y1) + x1) * 2;
            end_offset = ((width * y2) + x1) * 2;
            mod = 0;
            for ( i = start_offset; i < end_offset; i += 2 * width) {
                if (mod % 2)
                    *( input + i ) = cb;
                else
                    *( input + i ) = cr;
                mod++;
            }

            start_offset = ((width * y1) + (x1 + 1)) * 2;
            end_offset = ((width * y2) + (x1 + 1)) * 2;
            mod = 0;
            for ( i = start_offset; i < end_offset; i += 2 * width) {
                if (mod % 2)
                    *( input + i ) = cb;
                else
                    *( input + i ) = cr;
                mod++;
            }            
        }
        
        mod = 0;

        if(y2 > 0 && y2 < height) {
            //Botom Line
            start_offset = ((width * y2) + x1) * 2;
            end_offset = start_offset + (x2 - x1) * 2;
            mod = 0;
            for( i = start_offset; i < end_offset; i += 2) {
                if (mod % 2)
                    *( input + i ) = cb;
                else
                    *( input + i ) = cr;
                mod++;
            }
            
            start_offset = ((width * (y2 - 1)) + x1) * 2;
            end_offset = start_offset + (x2 - x1) * 2;
            mod = 0;
            for( i = start_offset; i < end_offset; i += 2) {
                if (mod % 2)
                    *( input + i ) = cb;
                else
                    *( input + i ) = cr;
                mod++;
            }        
        }
        
        if(x2 > 0 && x2 < width) {
            //Right Line
            start_offset = ((width * y1) + x1 + (x2 - x1)) * 2;
            end_offset = ((width * y2) + x1 + (x2 - x1)) * 2;
            mod = 0;
            for ( i = start_offset; i < end_offset; i += 2 * width) {
                if (mod % 2)
                    *( input + i ) = cb;
                else
                    *( input + i ) = cr;
                mod++;
            }

            start_offset = ((width * y1) + (x1 - 1) + (x2 - x1)) * 2;
            end_offset = ((width * y2) + (x1 - 1) + (x2 - x1)) * 2;
            mod = 0;
            for ( i = start_offset; i < end_offset; i += 2 * width) {
                if (mod % 2)
                    *( input + i ) = cb;
                else
                    *( input + i ) = cr;
                mod++;
            }        
        }
    }
    
    void CameraHal::drawLineMixed(uint8_t *frame, 
                                   int row, 
                                   int col, 
                                   int size, 
                                   int width,
                                   int height,
                                   uint8_t cbValue,
                                   uint8_t crValue,
                                   int fraction)
    {
        int weight1 = fraction;
        int weight2 = 255 - weight1;

        int i = 0;
    
        // Update Cb and Cr values (assuming line-packed).
        uint8_t* pCbCr = (uint8_t*)frame + ((row * width) + col) * 2;

        for (i = 0; i < size; i += 2) {
            if(i % 2)
                *(pCbCr + i) = clamp(FIXED2INT(cbValue * weight1 + pCbCr[i] * weight2), 0, 255);
            else
                *(pCbCr + i) = clamp(FIXED2INT(crValue * weight1 + pCbCr[i] * weight2), 0, 255);
        }
    }

#ifdef CAMERA_PANORAMA
    void CameraHal::drawArrows(uint8_t *frame, ArrowDirectionInfo arrow_dir, int width, int height)
    {
        static size_t fading_idx = 0;
        static const int fractions[] = {255, 230, 200, 120, 40, 120, 200, 230, 255};
        static const int size = 50;
        static const int size_sqrt2 = 35; 
        static const int margin = 25;

        int color = (255 + 256 * 255);
        
        uint8_t r = (uint8_t)(255 & (color >> 16));
        uint8_t g = (uint8_t)(255 & (color >> 8));
        uint8_t b = (uint8_t)(255 & (color));

        uint8_t yValue  = (uint8_t)(      + 0.299    * r + 0.587    * g + 0.114    * b);
        uint8_t cbValue = (uint8_t)(128   - 0.168736 * r - 0.331264 * g + 0.5      * b);
        uint8_t crValue = (uint8_t)(128   + 0.5      * r - 0.418688 * g - 0.081312 * b);

    
        int fraction = fractions[fading_idx++];
        if (fading_idx >= sizeof(fractions)/sizeof(fractions[0])) {
            fading_idx = 0;
        }
    
        switch (arrow_dir) {
        case (ARROW_UP | ARROW_LEFT):
            int base_size = size_sqrt2 * 2;
            int row = margin;
            int col = margin; 
            int length = base_size / 2;
            if (row % 2 == 1) 
                row -= 1;
            if (col % 2 == 1) 
                col -= 1;
            for (int i = 0; i < length; i++)
                drawLineMixed(frame, row + i, col, base_size - i * 2, width, height, cbValue, crValue, fraction);
            break;

        case (ARROW_UP | ARROW_RIGHT):
            int base_size = size_sqrt2 * 2;
            int row = margin;
            int col = width - margin - base_size; 
            int length = base_size / 2;
            if (row % 2 == 1) 
                row -= 1; 
            if (col % 2 == 1)
                col -= 1;
            for (int i = 0; i < length; i++)
                drawLineMixed(frame, row + i, col + i, base_size - i * 2, width, height, cbValue, crValue, fraction);
            break;

        case (ARROW_DOWN | ARROW_LEFT):
            int base_size = size_sqrt2 * 2;
            int row = height - margin;
            int col = margin; 
            int length = base_size / 2;
            if (row % 2 == 1) 
                row -= 1;
            if (col % 2 == 1)
                col -= 1;
            for (int i = 0; i < length; i++)
                drawLineMixed(frame, row - i, col, base_size - i * 2, width, height, cbValue, crValue, fraction);
            break;

        case (ARROW_DOWN | ARROW_RIGHT):
            int base_size = size_sqrt2 * 2;
            int row = height - margin;
            int col = width - margin - base_size; 
            int length = base_size / 2;
            if (row % 2 == 1)
                row -= 1; 
            if (col % 2 == 1) 
                col -= 1;
            for (int i = 0; i < length; i++)
                drawLineMixed(frame, row - i, col + i, base_size - i * 2, width, height, cbValue, crValue, fraction);
            break;

        default:
            if (arrow_dir & ARROW_DOWN) {
                int base_size = size;
                int row = height - margin;
                int col = width / 2;
                int shift = base_size / 2;
                int length = base_size / 2;
                row -= shift;
                col -= shift;
                if (row % 2 == 1) 
                    row -= 1;
                if (col % 2 == 1) 
                    col -= 1;
                for (int i = 0; i < length; i++) {
                    drawLineMixed(frame, row + i, col + i, base_size - i * 2, width, height, cbValue, crValue, fraction);
                    drawLineMixed(frame, row + i, col + shift, base_size - i * 2, width, height, cbValue, crValue, fraction);
                }
            }
    
            if (arrow_dir & ARROW_UP) {
                int base_size = size;
                int row = margin;
                int col = width / 2;
                int shift = base_size / 2;
                int length = base_size / 2;
                row += shift;
                col -= shift;
                if (row % 2 == 1) 
                    row -= 1; 
                if (col % 2 == 1) 
                    col -= 1;
                for (int i = 0; i < length; i++) {
                    drawLineMixed(frame, row - i, col + i, base_size - i * 2, width, height, cbValue, crValue, fraction);                
                    drawLineMixed(frame, row - i, col + shift, base_size - i * 2, width, height, cbValue, crValue, fraction);
                }
            }
    
            if (arrow_dir & ARROW_LEFT) {
                int base_size = size;
                int row = height / 2;
                int col = margin; 
                int length = base_size / 2;
                if (row % 2 == 1) 
                    row -= 1; 
                if (col % 2 == 1) 
                    col -= 1;
                for (int i = 0; i < length; i++) {
                    drawLineMixed(frame, row + i, col + i, base_size - i * 2, width, height, cbValue, crValue, fraction);
                    drawLineMixed(frame, row - i - 1, col + i, base_size - i * 2, width, height, cbValue, crValue, fraction);                        
                }
            }
    
            if (arrow_dir & ARROW_RIGHT) {
                int base_size = size;
                int row = height / 2;
                int col = width - margin;
                int shift = base_size / 2;
                int length = base_size / 2;
                col -= shift;
                if (row % 2 == 1) 
                    row -= 1;
                if (col % 2 == 1) 
                    col -= 1;
                for (int i = 0; i < length; i++) {
                    drawLineMixed(frame, row + i, col, base_size - i * 2, width, height, cbValue, crValue, fraction);
                    drawLineMixed(frame, row - i - 1, col, base_size - i * 2, width, height, cbValue, crValue, fraction);
                }
            }
            break;
        }
    }
#endif
    int CameraHal::zoomPerform(int zoom)
    {
        struct v4l2_crop crop;
        int fwidth, fheight;
        int zoom_left,zoom_top,zoom_width, zoom_height,w,h;
        int ret;

        LOG_FUNCTION_NAME
            
        HAL_PRINT("In zoomPerform %d\n",zoom);

        if (zoom < 1) 
           zoom = 1;
        if (zoom > 7)
           zoom = 7;

        mParameters.getPreviewSize(&w, &h);
        fwidth = w/zoom;
        fheight = h/zoom;
        zoom_width = ((int) fwidth) & (~3);
        zoom_height = ((int) fheight) & (~3);
        zoom_left = w/2 - (zoom_width/2);
        zoom_top = h/2 - (zoom_height/2);

        HAL_PRINT("Perform ZOOM: zoom_width = %d, zoom_height = %d, zoom_left = %d, zoom_top = %d\n", zoom_width, zoom_height, zoom_left, zoom_top); 

        memset(&crop, 0, sizeof(crop));
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c.left   = zoom_left; 
        crop.c.top    = zoom_top;
        crop.c.width  = zoom_width;
        crop.c.height = zoom_height;

        ret = ioctl(camera_device, VIDIOC_S_CROP, &crop);
        if (ret < 0) {
            LOGE("[%s]: ERROR VIDIOC_S_CROP failed\n", strerror(errno));
            return -1;
        }

        return 0;
    }


    /************/
    int CameraHal::startCapture()
    {
        struct v4l2_buffer buffer; // for VIDIOC_QUERYBUF and VIDIOC_QBUF
        struct v4l2_buffer cfilledbuffer; // for VIDIOC_DQBUF
        struct v4l2_requestbuffers creqbuf; // for VIDIOC_REQBUFS and VIDIOC_STREAMON and VIDIOC_STREAMOFF
        struct v4l2_format format;
        
        sp<MemoryBase>      mPictureBuffer;
        sp<MemoryBase>      mFinalPictureBuffer;
        sp<MemoryBase>      mJPEGPictureMemBase;
        sp<MemoryBase>      mPictureRotateBuffer;
        sp<MemoryHeapBase>  mJPEGPictureHeap;
        sp<MemoryHeapBase>  mThumbnailPictureHeap;
        sp<MemoryHeapBase>  mPictureRotateHeap;

        ssize_t newoffset;
        size_t newsize;

        int image_width, image_height;
                
        int jpegFormat  = PIX_YUV422I; 
        int jpeg_width  = getJpegCaptureWidth();
        int jpeg_height = getJpegCaptureHeight();

        int EXIF_Data_Size = 0;
        int ThumbNail_Data_Size = 0;
        
        unsigned char* pExifBuf = new unsigned char[65536];//64*1024

        int twoSecondReviewMode = getTwoSecondReviewMode();
        int orientation = getOrientation();

        int err = NO_ERROR;

        unsigned long base, offset;

        void * outBuffer = NULL;      
        
        LOG_FUNCTION_NAME
               
        if (setCaptureFrameRate()) {
            LOGE("Error in setting Camera frame rate\n");
            goto exit_capture;
        }

        mParameters.getPictureSize(&image_width, &image_height);
        HAL_PRINT("Picture Size: Width = %d, Height = %d\n", image_width, image_height);

#if VPP_THREAD
#if VPP_CONVERSION
        if(!isStart_convert) {
            if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {
                if( ColorConvert_Init(image_height, image_width, 2, 1, 0, ROTATEANGLE) != 0) {
                    LOGE("ColorConvert_Init() fail!!\n");
                    return -1;
                } else {
                    LOGV("ColorConvert_Init() success!!\n");
                    isStart_convert = true;
                }
            } 
        }
#endif
#endif

        if(mCamera_Mode == CAMERA_MODE_JPEG)
            capture_len = jpeg_width * jpeg_height * 2;
        else
            capture_len = image_width * image_height * 2;

        if (capture_len & 0xfff)
            capture_len = (capture_len & 0xfffff000) + 0x1000;
        HAL_PRINT("pictureFrameSize = 0x%x = %d\n", capture_len, capture_len);
        
        mPictureHeap = new MemoryHeapBase(capture_len);
        
        base = (unsigned long)mPictureHeap->getBase();
        base = (base + 0xfff) & 0xfffff000;
        offset = base - (unsigned long)mPictureHeap->getBase();

        HAL_PRINT("VIDIOC_S_FMT Start\n");        
        /* set size & format of the video image */
        format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        format.fmt.pix.width = image_width;
        format.fmt.pix.height = image_height;
        if(mCamera_Mode == CAMERA_MODE_JPEG)
            format.fmt.pix.pixelformat = PIXEL_FORMAT_JPEG;
        else
            format.fmt.pix.pixelformat = PIXEL_FORMAT_UYVY;
        if (ioctl(camera_device, VIDIOC_S_FMT, &format) < 0) {
            LOGE ("VIDIOC_S_FMT Failed. errno = %d\n", errno);
            goto exit_capture;
        }
        HAL_PRINT("VIDIOC_S_FMT End\n");

#if OMAP_SCALE_CDD
        if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {
            if(orientation == 0 || orientation == 180) {
                struct v4l2_control vc;            
                CLEAR(vc);
                vc.id = V4L2_CID_FLIP;                
                vc.value = CAMERA_FLIP_WATER_MIRROR;
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("V4L2_CID_FLIP fail!\n");
                    return UNKNOWN_ERROR;  
                }
            }
        }
#endif        

        /* Shutter CB */
        if(mMsgEnabled & CAMERA_MSG_SHUTTER) {
            mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0 ,mCallbackCookie);
        }

        /* Check if the camera driver can accept 1 buffer */
        creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        creqbuf.memory = V4L2_MEMORY_USERPTR;
        creqbuf.count  = 1;
        if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0) {
            LOGE ("VIDIOC_REQBUFS Failed. errno = %d\n", errno);
            goto exit_capture;
        }

        buffer.type = creqbuf.type;
        buffer.memory = creqbuf.memory;
        buffer.index = 0;
        if (ioctl(camera_device, VIDIOC_QUERYBUF, &buffer) < 0) {
            LOGE ("VIDIOC_QUERYBUF Failed. errno = %d\n", errno);
            goto exit_capture;
        }

        buffer.m.userptr = base;
        mPictureBuffer = new MemoryBase(mPictureHeap, offset, buffer.length);
        HAL_PRINT("Picture Buffer: Base = %p Offset = 0x%x\n", (void *)base, (unsigned int)offset);

        if (ioctl(camera_device, VIDIOC_QBUF, &buffer) < 0) {
            LOGE ("VIDIOC_QBUF Failed. errno = %d\n", errno);
            mPictureBuffer.clear();
            goto exit_capture;
        }

        /* turn on streaming */
        if (ioctl(camera_device, VIDIOC_STREAMON, &creqbuf.type) < 0) {
            LOGE ("VIDIOC_STREAMON Failed. errno = %d\n", errno);
            goto exit_capture;
        }

        /* De-queue the next avaliable buffer */
        if (ioctl(camera_device, VIDIOC_DQBUF, &buffer) < 0) {
            LOGE ("VIDIOC_DQBUF Failed. errno = %d\n", errno);
            mPictureBuffer.clear();
            goto exit_capture; 
        }

        /* turn off streaming */
        if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) < 0) {
            LOGE ("VIDIOC_STREAMOFF Failed. errno = %d\n", errno);
            goto exit_capture;
        }

#if OMAP_SCALE_CDD
        if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {
            if(orientation == 0 || orientation == 180) {
                struct v4l2_control vc;            
                CLEAR(vc);
                vc.id = V4L2_CID_FLIP;                
                vc.value = CAMERA_FLIP_NONE;
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("V4L2_CID_FLIP fail!\n");
                    return UNKNOWN_ERROR;  
                }
            }
        }
#endif        

        if(mCamera_Mode == CAMERA_MODE_JPEG) {
            int JPEG_Image_Size = getJpegImageSize();
            int thumbNailOffset = getThumbNailOffset();
            int yuvOffset = getYUVOffset();
            ThumbNail_Data_Size = getThumbNailDataSize();
            sp<IMemoryHeap> heap = mPictureBuffer->getMemory(&newoffset, &newsize);
            uint8_t* pInJPEGDataBUuf = (uint8_t *)heap->base() + newoffset ;
            uint8_t* pInThumbNailDataBuf = (uint8_t *)heap->base() + thumbNailOffset;
            uint8_t* pYUVDataBuf = (uint8_t *)heap->base() + yuvOffset;

            createExif(pInThumbNailDataBuf,ThumbNail_Data_Size,pExifBuf,EXIF_Data_Size,1);

            //create a new binder object 
            mFinalPictureHeap = new MemoryHeapBase(EXIF_Data_Size+JPEG_Image_Size);
            mFinalPictureBuffer = new MemoryBase(mFinalPictureHeap,0,EXIF_Data_Size+JPEG_Image_Size);
            heap = mFinalPictureBuffer->getMemory(&newoffset, &newsize);
            uint8_t* pOutFinalJpegDataBuf = (uint8_t *)heap->base();

            //create a new binder obj to send yuv data
            if(yuvOffset) {
                int mFrameSizeConvert = (mPreviewWidth*mPreviewHeight*3/2) ;
                mYUVPictureHeap = new MemoryHeapBase(mFrameSizeConvert);
                mYUVPictureBuffer = new MemoryBase(mYUVPictureHeap,0,mFrameSizeConvert);
                mYUVNewheap = mYUVPictureBuffer->getMemory(&newoffset, &newsize);
#if USE_NEON                
                Neon_Convert_yuv422_to_NV21((unsigned char *)pYUVDataBuf,(unsigned char *)mYUVNewheap->base(), mPreviewWidth, mPreviewHeight);

                if(mMsgEnabled & CAMERA_MSG_RAW_IMAGE)
                    mDataCb(CAMERA_MSG_RAW_IMAGE, mYUVPictureBuffer, mCallbackCookie);
#else
                if(mMsgEnabled & CAMERA_MSG_RAW_IMAGE)
                    mDataCb(CAMERA_MSG_RAW_IMAGE, pYUVDataBuf, mCallbackCookie);
#endif                
            }
            
            //create final JPEG with EXIF into that
            int OutJpegSize = 0;
            if(createJpegWithExif(pInJPEGDataBUuf,JPEG_Image_Size,pExifBuf,EXIF_Data_Size,pOutFinalJpegDataBuf,OutJpegSize) != true) {
                LOGE("createJpegWithExif fail!!\n");
                goto exit_capture;
            }

            if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)
                mDataCb(CAMERA_MSG_COMPRESSED_IMAGE,mFinalPictureBuffer,mCallbackCookie);
        }

        //When a snapshot is taken, Surface Flinger indirectly calls streamoff on overlay
        //which implies that all the overlay buffers will be dequeued.
        //So, update our status accordingly
        if(mCamera_Mode == CAMERA_MODE_YUV) {
#ifdef HARDWARE_OMX
            int mFrameSizeConvert = (image_width*image_height*3/2) ;            
            mYUVPictureHeap = new MemoryHeapBase(mFrameSizeConvert);
            mYUVPictureBuffer = new MemoryBase(mYUVPictureHeap,0,mFrameSizeConvert);
            mYUVNewheap = mYUVPictureBuffer->getMemory(&newoffset, &newsize);            
            yuv_buffer = (uint8_t*)buffer.m.userptr;

#if OMAP_SCALE_CDD
            int mCropSize = (image_height*3/4)*image_height*2; 
            sp<MemoryHeapBase> mCropHeap = new MemoryHeapBase(mCropSize);
            sp<MemoryBase> mCropBuffer = new MemoryBase(mCropHeap,0,mCropSize);
            if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {
                if(scale_process_capture(yuv_buffer, image_width, image_height, mCropBuffer->pointer(), (image_height*3/4), image_height, 0, PIX_YUV422I, 1)) {
                    LOGE("scale_process() failed\n");
                }
            }     
            mCropBuffer.clear();
            mCropHeap.clear();
#endif

#if VPP_THREAD                
#if VPP_CONVERSION               
            pthread_mutex_lock(&lock);
            if( ColorConvert_Process((char *)yuv_buffer,(char *)mYUVNewheap->base()) < 0 ) {
                LOGE("ColorConvert_Process() failed\n");
                pthread_mutex_unlock(&lock);
                goto exit_capture;
            }
            pthread_mutex_unlock(&lock);

#endif
#else
#if USE_NEON              
            Neon_Convert_yuv422_to_YUV420P((unsigned char *)yuv_buffer,(unsigned char *)mYUVNewheap->base(), image_width, image_height);
#endif
#endif

            if(mMsgEnabled & CAMERA_MSG_RAW_IMAGE)
                mDataCb(CAMERA_MSG_RAW_IMAGE, mYUVPictureBuffer, mCallbackCookie);
#endif

            if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE) {
#ifdef HARDWARE_OMX  
#if JPEG_ENCODER
                int jpegSize = image_width*image_height*2;
                createExif(NULL, 0, pExifBuf, EXIF_Data_Size, 0);
                mJPEGPictureHeap = new MemoryHeapBase(jpegSize + EXIF_Data_Size + 256);
                outBuffer = (void *)((unsigned long)(mJPEGPictureHeap->getBase()) + 128);
#ifdef TIMECHECK
                PPM("BEFORE JPEG Encode Image\n");
#endif
                if(isStart_JPEG) {
                    int ret;
                    LOGD("JPEG ENCODE START\n");
                    jpegFormat  = PIX_YUV420P;
                    capture_len = image_width*image_height*3/2 ;
                    ret = jpegEncoder->encodeImage(outBuffer, 
                                jpegSize, 
                                (uint8_t*)mYUVNewheap->base(), 
                                capture_len, 
                                pExifBuf,
                                EXIF_Data_Size,
                                image_width,
                                image_height,                                    
                                mThumbnailWidth,
                                mThumbnailHeight,                                    
                                mYcbcrQuality,
                                jpegFormat);
                    LOGD("JPEG ENCODE END\n");
                    
                    if(ret != true) {
                        LOGE("Jpeg encode failed!!\n");
                        goto exit_capture;
                    } else {
                        LOGD("Jpeg encode success!!\n");
                    }
                }
#ifdef TIMECHECK
                PPM("AFTER JPEG Encode Image\n");
#endif
                mJPEGPictureMemBase = new MemoryBase(mJPEGPictureHeap, 128, jpegEncoder->jpegSize);

                if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)
                    mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, mJPEGPictureMemBase ,mCallbackCookie);
#endif
#endif
            }
        }

        mCaptureFlag = 0;

exit_capture:
    
        mPictureBuffer.clear();
        mPictureHeap.clear();

        if(mCamera_Mode == CAMERA_MODE_JPEG) {
            mFinalPictureBuffer.clear();
            mFinalPictureHeap.clear();
            mYUVPictureBuffer.clear();
            mYUVPictureHeap.clear();
        }
        
        if(mCamera_Mode == CAMERA_MODE_YUV) {
            mJPEGPictureMemBase.clear();
            mJPEGPictureHeap.clear();
            mYUVPictureBuffer.clear();
            mYUVPictureHeap.clear();    
        }

        delete []pExifBuf;

        if(mCaptureFlag) {
            LOGE("startCapture() fail!!\n");
            err = -1;
        }

#if VPP_THREAD
#if VPP_CONVERSION
        if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {
            if(isStart_convert) {
                if( ColorConvert_Deinit() != 0 ) {
                    LOGE("ColorConvert_Deinit() failed\n");
                } else { 
                    LOGV("ColorConvert_Deinit() OK\n");
                }
                isStart_convert = false;
            }
        }
#endif
#endif

        mCaptureFlag = 0;
        
        LOG_FUNCTION_NAME_EXIT

        return err;

    }

    void CameraHal::createExif(unsigned char* pInThumbnailData,int Inthumbsize,unsigned char* pOutExifBuf,int& OutExifSize,int flag)
    {
        int orientationValue = getOrientation();
        ExifCreator* mExifCreator = new ExifCreator();
        unsigned int ExifSize = 0;
        ExifInfoStructure ExifInfo;
        unsigned short tempISO = 0;
        struct v4l2_exif exifobj;
        double arg0 = 0, arg3 = 0;
        int arg1 = 0, arg2 = 0;
        unsigned int i = 0;

        /* standard values */
        unsigned short iso_std_values[] = { 0, 10, 12, 16, 20, 25, 32, 40, 50, 64, 80,
            100, 125, 160, 200, 250, 320, 400, 500, 640, 800,
            1000, 1250, 1600, 2000, 2500, 3200, 4000, 5000, 6400, 8000};
        /* quantization table */
        double iso_qtable[] = { 8.909, 11.22, 14.14, 17.82, 22.45, 28.28, 35.64, 44.90, 56.57, 71.27, 89.09,
            112.2, 141.4, 178.2, 224.5, 282.8, 356.4, 449.0, 565.7, 712.7, 890.9,
            1122, 1414, 1782, 2245, 2828, 3564, 4490, 5657, 7127, 8909};

        LOG_FUNCTION_NAME

        if(mCameraIndex != VGA_CAMERA)
            getExifInfoFromDriver(&exifobj);

        memset( &ExifInfo, 0x00, sizeof(ExifInfoStructure));

        strcpy( (char *)&ExifInfo.maker, "SAMSUNG");
        strcpy( (char *)&ExifInfo.model, "SHW-M100S");
        property_get("ro.build.version.incremental", (char *)&ExifInfo.software, "YMDD");

        mParameters.getPictureSize((int*)&ExifInfo.imageWidth , (int*)&ExifInfo.imageHeight);
        mParameters.getPictureSize((int*)&ExifInfo.pixelXDimension, (int*)&ExifInfo.pixelYDimension);

        struct tm *t = NULL;
        time_t nTime;
        time(&nTime);
        t = localtime(&nTime);

        if(t != NULL) {
            sprintf((char *)&ExifInfo.dateTimeOriginal, "%4d:%02d:%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
            sprintf((char *)&ExifInfo.dateTimeDigitized, "%4d:%02d:%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
            sprintf((char *)&ExifInfo.dateTime, "%4d:%02d:%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
        }

        if(mCameraIndex != VGA_CAMERA) {
            if(mThumbnailWidth > 0 && mThumbnailHeight > 0) {
                //thumb nail data added here 
                ExifInfo.thumbStream                = pInThumbnailData;
                ExifInfo.thumbSize                  = Inthumbsize;
                ExifInfo.thumbImageWidth            = mThumbnailWidth;
                ExifInfo.thumbImageHeight           = mThumbnailHeight;
                ExifInfo.hasThumbnail = true;
            } else {
                ExifInfo.hasThumbnail = false;
            }
            ExifInfo.exposureProgram            = 3;
            ExifInfo.exposureMode               = 0;
            ExifInfo.contrast                   = convertToExifLMH(getContrast(), 4);
            ExifInfo.fNumber.numerator          = 26;
            ExifInfo.fNumber.denominator        = 10;
            ExifInfo.aperture.numerator         = 26;
            ExifInfo.aperture.denominator       = 10;
            ExifInfo.maxAperture.numerator      = 26;
            ExifInfo.maxAperture.denominator    = 10;
            ExifInfo.focalLength.numerator      = 3546;
            ExifInfo.focalLength.denominator    = 1000;
            ExifInfo.shutterSpeed.numerator     = (unsigned int)((double)exifobj.TV_Value/100.0);
            ExifInfo.shutterSpeed.denominator   = 1;
            ExifInfo.exposureTime.numerator     = 1;
            ExifInfo.exposureTime.denominator   = (unsigned int)pow(2.0, ((double)exifobj.TV_Value/100.0));            
            ExifInfo.brightness.numerator       = 5;
            ExifInfo.brightness.denominator     = 9;
            ExifInfo.iso                        = 1;
            switch(orientationValue) {
            case 0:
                ExifInfo.orientation = 1;
                break;
            case 90:
                ExifInfo.orientation = 6;
                break;
            case 180:
                ExifInfo.orientation = 3;
                break;
            case 270:
                ExifInfo.orientation = 8;
                break;
            default :
                ExifInfo.orientation = 1;
                break;
            }

            double calIsoValue = 0;
            calIsoValue = pow(2.0,((double)exifobj.SV_Value/100.0))/pow(2.0,(-7.0/4.0));
        	for (i = 0; i < sizeof(iso_qtable); i++) {
        		if (calIsoValue <= iso_qtable[i]) {
        			tempISO = iso_std_values[i];
        			break;
        		}
        	}            

            if(mPreviousSceneMode <= 1) {
                ExifInfo.meteringMode = mPreviousMetering;
                if(mPreviousWB <= 1)
                    ExifInfo.whiteBalance = 0;
                else
                    ExifInfo.whiteBalance = 1;
                ExifInfo.saturation = convertToExifLMH(getSaturation(), 4);
                ExifInfo.sharpness  = convertToExifLMH(getSharpness(), 4);
                switch(mPreviousIso) {
                case 50:
                    ExifInfo.isoSpeedRating = 50;
                    break;
                case 100:
                    ExifInfo.isoSpeedRating = 100;
                    break;
                case 200:
                    ExifInfo.isoSpeedRating = 200;
                    break;
                case 400:
                    ExifInfo.isoSpeedRating = 400;
                    break;
                case 800:
                    ExifInfo.isoSpeedRating = 800;
                    break;
                default:
                    ExifInfo.isoSpeedRating = tempISO;
                    break;
                }
                
                if(mPreviousFlashMode <= 1)
                    ExifInfo.flash                          = 0;
                else
                    ExifInfo.flash                          = 1;
                switch(getBrightness()) {
                case 1:
                    ExifInfo.exposureBias.numerator = -20;
                    break;
                case 2:
                    ExifInfo.exposureBias.numerator = -15;
                    break;
                case 3:
                    ExifInfo.exposureBias.numerator = -10;
                    break;
                case 4:
                    ExifInfo.exposureBias.numerator =  -5;
                    break;
                case 5:
                    ExifInfo.exposureBias.numerator =   0;
                    break;
                case 6:
                    ExifInfo.exposureBias.numerator =   5;
                    break;
                case 7:
                    ExifInfo.exposureBias.numerator =  10;
                    break;
                case 8:
                    ExifInfo.exposureBias.numerator =  15;
                    break;
                case 9:
                    ExifInfo.exposureBias.numerator =  20;
                    break;
                default:
                    ExifInfo.exposureBias.numerator = 0;
                    break;
                }
                ExifInfo.exposureBias.denominator       = 10;
                ExifInfo.sceneCaptureType               = 0;
            } else {
                switch(mPreviousSceneMode) {
                case 3://sunset
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 1;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = tempISO;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                case 4://dawn
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 1;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = tempISO;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                case 5://candlelight
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 1;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = tempISO;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                case 6://beach & snow
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 2;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = 50;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 10;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 1;
                    break;
                case 7://againstlight
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = tempISO;
                    if(mPreviousFlashMode <= 1) {
                        ExifInfo.flash                      = 0;
                        ExifInfo.meteringMode               = 3;
                    } else {
                        ExifInfo.flash                      = 1;
                        ExifInfo.meteringMode               = 2;
                    }
                    ExifInfo.exposureBias.numerator 	= 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                case 8://text
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 2;
                    ExifInfo.isoSpeedRating             = tempISO;
                    if(mPreviousFlashMode <= 1)
                        ExifInfo.flash                      = 0;
                    else
                        ExifInfo.flash                      = 1;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                case 9://night
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = tempISO;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 3;
                    break;	
                case 10://landscape
                    ExifInfo.meteringMode               = 1;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 2;
                    ExifInfo.sharpness                  = 2;
                    ExifInfo.isoSpeedRating             = tempISO;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 1;
                    break;
                case 11://fireworks
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = 50;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                case 12://portrait
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 1;
                    ExifInfo.isoSpeedRating             = tempISO;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 2;
                    break;
                case 13://fallcolor
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 2;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = tempISO;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                case 14://indoors
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 2;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = 200;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                case 15://sports
                    ExifInfo.meteringMode               = 2;
                    ExifInfo.whiteBalance               = 0;
                    ExifInfo.saturation                 = 0;
                    ExifInfo.sharpness                  = 0;
                    ExifInfo.isoSpeedRating             = tempISO;
                    ExifInfo.flash                      = 0;
                    ExifInfo.exposureBias.numerator     = 0;
                    ExifInfo.exposureBias.denominator   = 10;
                    ExifInfo.sceneCaptureType           = 4;
                    break;
                }
            }
        } else {
            if(mThumbnailWidth > 0 && mThumbnailHeight > 0) {
                //thumb nail data added here 
                ExifInfo.thumbStream                = pInThumbnailData;
                ExifInfo.thumbSize                  = Inthumbsize;
                ExifInfo.thumbImageWidth            = mThumbnailWidth;
                ExifInfo.thumbImageHeight           = mThumbnailHeight;
                ExifInfo.hasThumbnail               = true;
            } else {
                ExifInfo.hasThumbnail = false;
            }
            ExifInfo.exposureProgram            = 3;
            ExifInfo.exposureMode               = 0;
            ExifInfo.contrast                   = 0;
            ExifInfo.fNumber.numerator          = 28;
            ExifInfo.fNumber.denominator        = 10;
            ExifInfo.aperture.numerator         = 26;
            ExifInfo.aperture.denominator       = 10;
            ExifInfo.maxAperture.numerator      = 26;
            ExifInfo.maxAperture.denominator    = 10;
            ExifInfo.focalLength.numerator      = 1300;
            ExifInfo.focalLength.denominator    = 1000;
            ExifInfo.shutterSpeed.numerator     = 16;
            ExifInfo.shutterSpeed.denominator   = 1;
            ExifInfo.brightness.numerator       = 5;
            ExifInfo.brightness.denominator     = 9;
            ExifInfo.iso                        = 1;
            
            switch(orientationValue) {
            case 0:
                ExifInfo.orientation        = 1;
                break;
            case 90:
                ExifInfo.orientation        = 6;
                break;
            case 180:
                ExifInfo.orientation        = 3;
                break;
            case 270:
                ExifInfo.orientation        = 8;
                break;
            default :
                ExifInfo.orientation        = 1;
                break;
            }
            
            ExifInfo.meteringMode               = mPreviousMetering;
            ExifInfo.whiteBalance               = 0;
            ExifInfo.saturation                 = 0;
            ExifInfo.sharpness                  = 0;
            ExifInfo.isoSpeedRating             = 100;
            ExifInfo.exposureTime.numerator     = 1;
            ExifInfo.exposureTime.denominator   = 16;
            ExifInfo.flash = 0;
            switch(getBrightness()) {
                case 1:
                    ExifInfo.exposureBias.numerator = -20;
                    break;
                case 2:
                    ExifInfo.exposureBias.numerator = -15;
                    break;
                case 3:
                    ExifInfo.exposureBias.numerator = -10;
                    break;
                case 4:
                    ExifInfo.exposureBias.numerator =  -5;
                    break;
                case 5:
                    ExifInfo.exposureBias.numerator =   0;
                    break;
                case 6:
                    ExifInfo.exposureBias.numerator =   5;
                    break;
                case 7:
                    ExifInfo.exposureBias.numerator =  10;
                    break;
                case 8:
                    ExifInfo.exposureBias.numerator =  15;
                    break;
                case 9:
                    ExifInfo.exposureBias.numerator =  20;
                    break;
                default:
                    ExifInfo.exposureBias.numerator = 0;
                    break;
            }
            ExifInfo.exposureBias.denominator       = 10;
            ExifInfo.sceneCaptureType               = 0;
        }

        if (mPreviousGPSLatitude != 0 && mPreviousGPSLongitude != 0) {
            arg0 = arg1 = arg2 = arg3 = 0;
            arg0 = getGPSLatitude();
            
            if (arg0 > 0)
                ExifInfo.GPSLatitudeRef[0] = 'N'; 
            else
                ExifInfo.GPSLatitudeRef[0] = 'S';

            convertFromDecimalToGPSFormat(fabs(arg0),arg1,arg2,arg3);

            ExifInfo.GPSLatitude[0].numerator= arg1;
            ExifInfo.GPSLatitude[0].denominator= 1;
            ExifInfo.GPSLatitude[1].numerator= arg2; 
            ExifInfo.GPSLatitude[1].denominator= 1;
            ExifInfo.GPSLatitude[2].numerator= arg3; 
            ExifInfo.GPSLatitude[2].denominator= 1;

            arg0 = arg1 = arg2 = arg3 = 0;
            arg0 = getGPSLongitude();
            
            if (arg0 > 0)
                ExifInfo.GPSLongitudeRef[0] = 'E';
            else
                ExifInfo.GPSLongitudeRef[0] = 'W';
            
            convertFromDecimalToGPSFormat(fabs(arg0),arg1,arg2,arg3);

            ExifInfo.GPSLongitude[0].numerator= arg1; 
            ExifInfo.GPSLongitude[0].denominator= 1;
            ExifInfo.GPSLongitude[1].numerator= arg2; 
            ExifInfo.GPSLongitude[1].denominator= 1;
            ExifInfo.GPSLongitude[2].numerator= arg3; 
            ExifInfo.GPSLongitude[2].denominator= 1;

            arg0 = arg1 = arg2 = arg3 = 0;
            arg0 = getGPSAltitude();
            
            if (arg0 > 0)
                ExifInfo.GPSAltitudeRef = 0;
            else
                ExifInfo.GPSAltitudeRef = 1;
            
            ExifInfo.GPSAltitude[0].numerator= fabs(arg0) ; 
            ExifInfo.GPSAltitude[0].denominator= 1;

            //GPS_Time_Stamp
            ExifInfo.GPSTimestamp[0].numerator = (uint32_t)m_gpsHour;
            ExifInfo.GPSTimestamp[0].denominator = 1; 
            ExifInfo.GPSTimestamp[1].numerator = (uint32_t)m_gpsMin;
            ExifInfo.GPSTimestamp[1].denominator = 1;               
            ExifInfo.GPSTimestamp[2].numerator = (uint32_t)m_gpsSec;
            ExifInfo.GPSTimestamp[2].denominator = 1;

            //GPS_ProcessingMethod
            strcpy((char *)ExifInfo.GPSProcessingMethod, mPreviousGPSProcessingMethod);

            //GPS_Date_Stamp
            strcpy((char *)ExifInfo.GPSDatestamp, m_gps_date);
            
            ExifInfo.hasGps = true;
        } else {
            ExifInfo.hasGps = false;
        }
        
        ExifSize = mExifCreator->ExifCreate_wo_GPS( (unsigned char *)pOutExifBuf, &ExifInfo ,flag);
        OutExifSize = ExifSize;
        delete mExifCreator; 
    }

    bool CameraHal::createJpegWithExif(unsigned char* pInJpegData, int InJpegSize,unsigned char* pInExifBuf,int InExifSize,
        unsigned char* pOutJpegData, int& OutJpegSize)
    {
        int offset = 0;

        LOG_FUNCTION_NAME

        if( pInJpegData == NULL || InJpegSize == 0 || pInExifBuf == 0 || InExifSize == 0 || pOutJpegData == NULL ) {
            LOGE("Jpeg data is Null!!\n");
            return false;
        }

        memcpy( pOutJpegData, pInJpegData, 2 );

        offset += 2;

        if( InExifSize != 0 ) {
            memcpy( &(pOutJpegData[offset]), pInExifBuf, InExifSize );
            offset += InExifSize;
        }

        memcpy( &(pOutJpegData[offset]), &(pInJpegData[2]), InJpegSize - 2);

        offset +=  InJpegSize -2;

        OutJpegSize = offset;

        return true;
    }

    int CameraHal::getJpegImageSize()
    {
        struct v4l2_control vc;
        vc.id=V4L2_CID_JPEG_SIZE;
        vc.value=0;
        if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0) {
            LOGE ("Failed to get VIDIOC_G_CTRL.\n");
            return -1;
        }
        return (vc.value);
    }

    int CameraHal::getThumbNailDataSize()
    {
        struct v4l2_control vc;
        vc.id=V4L2_CID_THUMBNAIL_SIZE;
        vc.value=0;
        if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0) {
            LOGE ("Failed to get VIDIOC_G_CTRL.\n");
            return -1;
        }
        HAL_PRINT("Thumbnail data size = %d\n", vc.value);
        return(vc.value);
    }

    int CameraHal::getThumbNailOffset()
    {
        struct v4l2_control vc;

        LOG_FUNCTION_NAME
            
        vc.id=V4L2_CID_FW_THUMBNAIL_OFFSET;
        vc.value=0;
        if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0) {
            LOGE ("Failed to get VIDIOC_G_CTRL.\n");
            return -1;
        }
        HAL_PRINT("Thumbnail data offset = 0x%x\n", vc.value);
        return(vc.value);
    }

    int CameraHal::getYUVOffset()
    {
        struct v4l2_control vc;

        LOG_FUNCTION_NAME
            
        vc.id=V4L2_CID_FW_YUV_OFFSET;
        vc.value=0;
        if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0) {
            LOGE ("Failed to get VIDIOC_G_CTRL.\n");
            return -1;
        }
        HAL_PRINT("YUV data offset = 0x%x\n", vc.value);
        return(vc.value);
    }

    int CameraHal::getJpegCaptureWidth()
    {
        struct v4l2_control vc;

        LOG_FUNCTION_NAME
            
        vc.id=V4L2_CID_JPEG_CAPTURE_WIDTH;
        vc.value=0;
        if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0) {
            LOGE ("Failed to get VIDIOC_G_CTRL.\n");
            return -1;
        }
        HAL_PRINT("JPEG capture width = %d\n", vc.value);
        return(vc.value);
    }

    int CameraHal::getJpegCaptureHeight()
    {
        struct v4l2_control vc;

        LOG_FUNCTION_NAME
            
        vc.id=V4L2_CID_JPEG_CAPTURE_HEIGHT;
        vc.value=0;
        if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0) {
            LOGE ("Failed to get VIDIOC_G_CTRL.\n");
            return -1;
        }
        HAL_PRINT("JPEG capture height = %d\n", vc.value);
        return(vc.value);
    }

    void CameraHal::convertFromDecimalToGPSFormat(double arg1,int& arg2,int& arg3,double& arg4)
    {
        double temp1=0,temp2=0,temp3=0;

        LOG_FUNCTION_NAME
            
        arg2  = (int)arg1;
        temp1 = arg1-arg2;
        temp2 = temp1*60 ;
        arg3  = (int)temp2;
        temp3 = temp2-arg3;
        arg4  = temp3*60;
    }

    void CameraHal::getExifInfoFromDriver(v4l2_exif* exifobj)
    {
        LOG_FUNCTION_NAME
            
        if(ioctl(camera_device,VIDIOC_G_EXIF,exifobj) < 0)
            LOGE ("Failed to get vidioc_g_exif.\n"); 
    }

    int CameraHal::convertToExifLMH(int value, int key)
    {
        const int NORMAL = 0;
        const int LOW    = 1;
        const int HIGH   = 2;

        LOG_FUNCTION_NAME

        value -= key;

        if(value == 0) return NORMAL;
        if(value < 0) return LOW;
        else return HIGH;
    }
};
