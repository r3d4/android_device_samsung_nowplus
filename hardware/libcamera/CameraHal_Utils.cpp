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
//[ 2010 05 02 03
#include <cutils/properties.h> // for property_get for the voice recognition mode switch
//]
namespace android {

#define SLOWMOTION4           60
#define SLOWMOTION8           120
#define SLOWMOTION4SKIP       3
#define SLOWMOTION8SKIP       7

#if OMAP_SCALE
#define CAMERA_FLIP_NONE			 1
#define CAMERA_FLIP_MIRROR			 2
#define CAMERA_FLIP_WATER			 3
#define CAMERA_FLIP_WATER_MIRROR	 4
#endif

	bool convert_date_to_version(const char * date, char *version)
	{
		const int start_year = 1981;
		unsigned char cal_month[1][13][4] = {{ "   ", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" }};
		char m_alpha[13] = {' ','A','B','C','D','E','F','G','H','I','J','K','L' };
		int pow[4] = {1,10,100,1000};
		int year=0,month=0,j, i;
		bool check=false;

		memset(version,0,4); //prevent

		for (i= 0 ; i < 4 ; i++)
		{
			year = year + (date[10-i]-'0') * pow[i];
		}

		// year
		year = year - start_year;
		year = year % 26;
		version[0] = 'A' + year;

		// month
		for( month=1; month<13; month++ )
		{
			check = true;

			for( j=0; j<3; j++ )
			{
				if( date[j] != cal_month[0][month][j] )
					check = false;
			}

			if(check)
				break;
		}

		if (month >=13)
		{
			return false;
		}

		version[1] = m_alpha[month];

		// day
		version[2] = ( date[4] == ' ' ? '0' : date[4] );
		version[3] = date[5];

		return true;
	}	

	int CameraHal::CameraSetFrameRate()
	{
		struct v4l2_streamparm parm;
		int framerate = 30;

		LOG_FUNCTION_NAME   

		framerate = mParameters.getPreviewFrameRate();
		HAL_PRINT("CameraSetFrameRate: framerate=%d\n",framerate);

		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		parm.parm.capture.capturemode = mCamMode;
		parm.parm.capture.currentstate = V4L2_MODE_CAPTURE;
		parm.parm.capture.timeperframe.numerator = 1;
		parm.parm.capture.timeperframe.denominator = framerate;
		if(ioctl(camera_device, VIDIOC_S_PARM, &parm) < 0) 
		{
			LOGE("ERROR VIDIOC_S_PARM\n");
			goto s_fmt_fail;
		}

		HAL_PRINT("CameraSetFrameRate Preview fps:%d/%d\n",parm.parm.capture.timeperframe.denominator,parm.parm.capture.timeperframe.numerator);

		LOG_FUNCTION_NAME_EXIT
		return 0;

s_fmt_fail:
		return -1;

	}

	int CameraHal::initCameraSetFrameRate()
	{       
		struct v4l2_streamparm parm;
		int framerate = 30;

		LOG_FUNCTION_NAME   

		framerate = mParameters.getPreviewFrameRate();

		if(mCameraIndex == MAIN_CAMERA)
		{
			if(mCamMode == VT_MODE)
			{		
				switch(framerate)
				{
					case 7:			
					case 10:
					case 15:
						break;

					default:
						framerate = 15;
						break;
				}
			}
			else
			{
				int w, h;

				mParameters.getPreviewSize(&w, &h);

				if(w > 320 && h > 240) // limit QVGA ~ WVGA
				{
					if(framerate > 30)
					{
						framerate = 30;
					}
				}
				else if (w == 176 && h == 144) // MMS recording
				{
					framerate = 15;
				}
				else
				{
					if(framerate > 120) //limit slowmotion
					{
						framerate = 30;
					}
				}
			}
		}
		else
		{
			switch(framerate)
			{
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

		if(framerate == SLOWMOTION8)
		{
			mSkipFrameNumber = SLOWMOTION8SKIP;
		}
		else if(framerate == SLOWMOTION4)
		{
			mSkipFrameNumber = SLOWMOTION4SKIP;
		}
		else
		{
			mSkipFrameNumber = 0;
		}

		parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		parm.parm.capture.capturemode = mCamMode;
		parm.parm.capture.currentstate = V4L2_MODE_PREVIEW;
		parm.parm.capture.timeperframe.numerator = 1;
		parm.parm.capture.timeperframe.denominator = framerate;
		if(ioctl(camera_device, VIDIOC_S_PARM, &parm) < 0) 
		{
			LOGE("ERROR VIDIOC_S_PARM\n");
			goto s_fmt_fail;
		}

		HAL_PRINT("initCameraSetFrameRate Preview fps:%d/%d\n",parm.parm.capture.timeperframe.denominator,parm.parm.capture.timeperframe.numerator);

		LOG_FUNCTION_NAME_EXIT

		return 0;

s_fmt_fail:
		return -1;
	}

	/***************/

	void CameraHal::drawRect(uint8_t *input, uint8_t color, int x1, int y1, int x2, int y2, int width, int height)
	{
		int i, start_offset, end_offset;
		int offset = 0;
		int offset_1 = 1;;
		int mod = 0;
		int cb,cr;

		switch (color)
		{
			case FOCUS_RECT_WHITE:
				cb = 255;
				cr = 255;
				offset = 1;
				offset_1 = 1;
				break;
			case FOCUS_RECT_GREEN:
				cb = 0;
				cr = 0;
				offset = 0;
				offset_1 = 2;
				break;
			case FOCUS_RECT_RED:
				cb = 128;
				cr = 255;
				offset = 0;
				offset_1 = 2;
				break;
			default:
				cb = 255;
				cr = 255;
				offset = 1;
				offset_1 = 1;
				break;
		}

		//Top Line
		start_offset = ((width*y1) + x1)*2 + offset;
		end_offset =  start_offset + (x2 - x1)*2 + offset;
		for ( i = start_offset; i < end_offset; i += 2)
		{
			if (mod % 2)
				*( input + i ) = cr;
			else
				*( input + i ) = cb;
			mod++;
		}

		//Left Line
		start_offset = ((width*y1) + x1)*2 + offset_1;
		end_offset = ((width*y2) + x1)*2 + offset_1;
		for ( i = start_offset; i < end_offset; i += 2*width)
		{
			*( input + i ) = cr;
		}

		mod = 0;

		//Botom Line
		start_offset = ((width*y2) + x1)*2 + offset;
		end_offset = start_offset + (x2 - x1)*2 + offset;
		for( i = start_offset; i < end_offset; i += 2)
		{
			if (mod % 2)
				*( input + i ) = cr;
			else
				*( input + i ) = cb;
			mod++;
		}

		//Right Line
		start_offset = ((width*y1) + x1 + (x2 - x1))*2 + offset_1;
		end_offset = ((width*y2) + x1 + (x2 - x1))*2 + offset_1;
		for ( i = start_offset; i < end_offset; i += 2*width)
		{
			*( input + i ) = cr;
		}
	}

	int CameraHal::ZoomPerform(int zoom)
	{
		struct v4l2_crop crop;
		int fwidth, fheight;
		int zoom_left,zoom_top,zoom_width, zoom_height,w,h;
		int ret;
		LOGD("In ZoomPerform %d",zoom);

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

		LOGE("Perform ZOOM: zoom_width = %d, zoom_height = %d, zoom_left = %d, zoom_top = %d\n", zoom_width, zoom_height, zoom_left, zoom_top); 

		memset(&crop, 0, sizeof(crop));
		crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		crop.c.left   = zoom_left; 
		crop.c.top    = zoom_top;
		crop.c.width  = zoom_width;
		crop.c.height = zoom_height;

		ret = ioctl(camera_device, VIDIOC_S_CROP, &crop);
		if (ret < 0) 
		{
			LOGE("[%s]: ERROR VIDIOC_S_CROP failed\n", strerror(errno));
			return -1;
		}

		return 0;
	}


	int CameraHal::CapturePicture()
	{
		int image_width, image_height, preview_width, preview_height;
		unsigned long base, offset;
		
		struct v4l2_buffer buffer; // for VIDIOC_QUERYBUF and VIDIOC_QBUF
		struct v4l2_format format;
		struct v4l2_buffer cfilledbuffer; // for VIDIOC_DQBUF
		struct v4l2_requestbuffers creqbuf; // for VIDIOC_REQBUFS and VIDIOC_STREAMON and VIDIOC_STREAMOFF

		sp<MemoryBase> 		mPictureBuffer;
		sp<MemoryBase> 		mFinalPictureBuffer;
		sp<MemoryHeapBase>  mJPEGPictureHeap;
		sp<MemoryBase>		mJPEGPictureMemBase;
#if OMAP_SCALE
		sp<MemoryHeapBase> 	TempHeapBase;
		sp<MemoryBase>	 	TempBase;
		sp<IMemoryHeap> 	TempHeap;
#endif

		ssize_t newoffset;
		size_t newsize;

		mCaptureFlag = true;
		int jpegSize;
		void* outBuffer;
		int err, i;
		int err_cnt = 0;

		int EXIF_Data_Size = 0;
		int ThumbNail_Data_Size = 0;
		unsigned char* pExifBuf = new unsigned char[65536];	//64*1024

		int twoSecondReviewMode = getTwoSecondReviewMode();
		int orientation = getOrientation();

		LOG_FUNCTION_NAME

		if (CameraSetFrameRate())
		{
			LOGE("Error in setting Camera frame rate\n");
			return -1;
		}

		HAL_PRINT("\n\n\n PICTURE NUMBER =%d\n\n\n",++pictureNumber);

		mParameters.getPictureSize(&image_width, &image_height);
		mParameters.getPreviewSize(&preview_width, &preview_height);	
		HAL_PRINT("Picture Size: Width = %d \t Height = %d\n", image_width, image_height);
		HAL_PRINT("Preview Size: Width = %d \t Height = %d\n", preview_width, preview_height);

#if OPEN_CLOSE_WORKAROUND
		close(camera_device);
		camera_device = open(VIDEO_DEVICE, O_RDWR);
		if (camera_device < 0) 
		{
			LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!\n",
					strerror(errno) );
		}
#endif

		if(mCamera_Mode == CAMERA_MODE_JPEG)
		{
			int jpeg_width = GetJPEG_Capture_Width();
			int jpeg_height = GetJPEG_Capture_Height();
			capture_len = jpeg_width * jpeg_height * 2;
		}
		else
		{
			capture_len = image_width * image_height * 2;
		}

		if (capture_len & 0xfff)
		{
			capture_len = (capture_len & 0xfffff000) + 0x1000;
		}

		HAL_PRINT("pictureFrameSize = 0x%x = %d\n", capture_len, capture_len);

		mPictureHeap = new MemoryHeapBase(capture_len);

		base = (unsigned long)mPictureHeap->getBase();
		base = (base + 0xfff) & 0xfffff000;
		offset = base - (unsigned long)mPictureHeap->getBase();

		/* set size & format of the video image */
		format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		format.fmt.pix.width = image_width;
		format.fmt.pix.height = image_height;
		
		if(mCamera_Mode == CAMERA_MODE_JPEG)
			format.fmt.pix.pixelformat = PIXEL_FORMAT_JPEG;
		else
			format.fmt.pix.pixelformat = PIXEL_FORMAT;

		if (ioctl(camera_device, VIDIOC_S_FMT, &format) < 0)
		{
			LOGE ("Failed to set VIDIOC_S_FMT.\n");
			return -1;
		}
		
#if OMAP_SCALE
        if(mCameraIndex == VGA_CAMERA && mCamMode != VT_MODE) {
            if(orientation == 0 || orientation == 180) {
                struct v4l2_control vc;            
                CLEAR(vc);
                vc.id = V4L2_CID_FLIP;                
                vc.value = CAMERA_FLIP_MIRROR;
                if (ioctl (camera_device, VIDIOC_S_CTRL, &vc) < 0) {
                    LOGE("V4L2_CID_FLIP fail!\n");
                    return UNKNOWN_ERROR;  
                }
            }
        }
#endif  

		/* Shutter CallBack */
		if(mMsgEnabled & CAMERA_MSG_SHUTTER)
		{
			mNotifyCb(CAMERA_MSG_SHUTTER,0,0,mCallbackCookie);
		} 

		/* Check if the camera driver can accept 1 buffer */
		creqbuf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		creqbuf.memory = V4L2_MEMORY_USERPTR;
		creqbuf.count  = 1;
		if (ioctl(camera_device, VIDIOC_REQBUFS, &creqbuf) < 0)
		{
			LOGE ("VIDIOC_REQBUFS Failed. errno = %d\n", errno);
			return -1;
		}

		buffer.type = creqbuf.type;
		buffer.memory = creqbuf.memory;
		buffer.index = 0;
		if (ioctl(camera_device, VIDIOC_QUERYBUF, &buffer) < 0) {
			LOGE("VIDIOC_QUERYBUF Failed");
			return -1;
		}

		buffer.m.userptr = base;
		mPictureBuffer = new MemoryBase(mPictureHeap, offset, buffer.length);
		LOGD("Picture Buffer: Base = %p Offset = 0x%x\n", (void *)base, (unsigned int)offset);

		if (ioctl(camera_device, VIDIOC_QBUF, &buffer) < 0) {
			LOGE("CAMERA VIDIOC_QBUF Failed");
			return -1;
		}

		/* turn on streaming */
		creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(camera_device, VIDIOC_STREAMON, &creqbuf.type) < 0)
		{
			LOGE("VIDIOC_STREAMON Failed\n");
			return -1;
		}

		HAL_PRINT("De-queue the next avaliable buffer\n");

		/* De-queue the next avaliable buffer */
		cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		cfilledbuffer.memory = creqbuf.memory;

		while (ioctl(camera_device, VIDIOC_DQBUF, &cfilledbuffer) < 0) 
		{
			LOGE("VIDIOC_DQBUF Failed cnt = %d\n", err_cnt);
			if(err_cnt++ > 10)
			{
				mNotifyCb(CAMERA_MSG_ERROR,CAMERA_DEVICE_ERROR_FOR_RESTART,0,mCallbackCookie);

				mPictureBuffer.clear();
				mPictureHeap.clear();

				return NO_ERROR;           
			}
		}
#if TIMECHECK
		PPM("AFTER CAPTURE YUV IMAGE\n");
#endif
		/* turn off streaming */
		creqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		if (ioctl(camera_device, VIDIOC_STREAMOFF, &creqbuf.type) < 0) 
		{
			LOGE("VIDIOC_STREAMON Failed\n");
			return -1;
		}

#if OMAP_SCALE
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

		if(mCamera_Mode == CAMERA_MODE_JPEG)
		{
			int JPEG_Image_Size = GetJpegImageSize();
			int thumbNailOffset = GetThumbNailOffset();
			int yuvOffset = GetYUVOffset();
			ThumbNail_Data_Size = GetThumbNailDataSize();
			sp<IMemoryHeap> heap = mPictureBuffer->getMemory(&newoffset, &newsize);
			uint8_t* pInJPEGDataBUuf = (uint8_t *)heap->base() + newoffset ;
			uint8_t* pInThumbNailDataBuf = (uint8_t *)heap->base() + thumbNailOffset;
			uint8_t* pYUVDataBuf = (uint8_t *)heap->base() + yuvOffset;

			CreateExif(pInThumbNailDataBuf,ThumbNail_Data_Size,pExifBuf,EXIF_Data_Size,1);

			//create a new binder object 
			mFinalPictureHeap = new MemoryHeapBase(EXIF_Data_Size+JPEG_Image_Size);
			mFinalPictureBuffer = new MemoryBase(mFinalPictureHeap,0,EXIF_Data_Size+JPEG_Image_Size);
			heap = mFinalPictureBuffer->getMemory(&newoffset, &newsize);
			uint8_t* pOutFinalJpegDataBuf = (uint8_t *)heap->base();

			//create a new binder obj to send yuv data
			if(yuvOffset)
			{
				int mFrameSizeConvert = (preview_width*preview_height*3/2) ;

				mYUVPictureHeap = new MemoryHeapBase(mFrameSizeConvert);
				mYUVPictureBuffer = new MemoryBase(mYUVPictureHeap,0,mFrameSizeConvert);
				sp<IMemoryHeap> newheap = mYUVPictureBuffer->getMemory(&newoffset, &newsize);
#if TIMECHECK
				PPM("YUV COLOR CONVERSION STARTED\n");
#endif
				Neon_Convert_yuv422_to_NV21((uint8_t *)pYUVDataBuf, (uint8_t *)newheap->base(), mPreviewWidth, mPreviewHeight);
#if TIMECHECK
				PPM("YUV COLOR CONVERSION ENDED\n");
#endif
			}
			//create final JPEG with EXIF into that
			int OutJpegSize = 0;
			err = CreateJpegWithExif(pInJPEGDataBUuf,JPEG_Image_Size,pExifBuf,EXIF_Data_Size,pOutFinalJpegDataBuf,OutJpegSize);
			if(err==false) return -1;
			
			if(yuvOffset)
			{
#ifdef HARDWARE_OMX
				if(twoSecondReviewMode == 1)
				{
					DrawOverlay(pYUVDataBuf, true);
				}
#endif //HARDWARE_OMX
				if(mMsgEnabled & CAMERA_MSG_RAW_IMAGE)
				{
					mDataCb(CAMERA_MSG_RAW_IMAGE, mYUVPictureBuffer, mCallbackCookie);
				}	
			}

			if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)
			{
				mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, mFinalPictureBuffer, mCallbackCookie);
			}

#if OPEN_CLOSE_WORKAROUND
			close(camera_device);
			camera_device = open(VIDEO_DEVICE, O_RDWR);
			if (camera_device < 0) 
			{
				LOGE ("!!!!!!!!!FATAL Error: Could not open the camera device: %s!!!!!!!!!\n", strerror(errno) );
			}
#endif
		}

		if(mCamera_Mode == CAMERA_MODE_YUV)
		{
#ifdef HARDWARE_OMX
			int mFrameSizeConvert = (image_width*image_height*2) ;
			mVGAYUVPictureHeap = new MemoryHeapBase(mFrameSizeConvert);
			mVGAYUVPictureBuffer = new MemoryBase(mVGAYUVPictureHeap,0,mFrameSizeConvert);
			mVGANewheap = mVGAYUVPictureBuffer->getMemory(&newoffset, &newsize);

			sp<IMemoryHeap> heap = mPictureBuffer->getMemory(&newoffset, &newsize);
			uint8_t* pYUVDataBuf = (uint8_t *)heap->base() + newoffset ;
			LOGD("PictureThread: generated a picture, yuv_buffer=%p yuv_len=%d\n",pYUVDataBuf,capture_len);
#if OMAP_SCALE
			TempHeapBase = new MemoryHeapBase(mFrameSizeConvert);
			TempBase = new MemoryBase(TempHeapBase,0,mFrameSizeConvert);
			TempHeap = TempBase->getMemory(&newoffset, &newsize);
			if(scale_process((void*)pYUVDataBuf, mPreviewWidth, mPreviewHeight,(void*)TempHeap->base(), mPreviewHeight, mPreviewWidth, 0, PIX_YUV422I, 1))
			{
				LOGE("scale_process() failed\n");
			}
#endif

#if TIMECHECK
			PPM("YUV COLOR ROTATION STARTED\n");
#endif                   

#if 0	//YUV dump code for testing
			FILE* fIn = NULL;	
			fIn = fopen("/data/output.yuv", "w");
			if ( fIn == NULL ) 	  
			{ 		 
				LOGE("Error: failed to open the file for writing\n");		 
			}		
			fwrite((uint8_t*)mVGANewheap->base(), 1, mPreviewWidth*mPreviewHeight*2, fIn);	   
			fclose(fIn);
#endif

			/*
			   	for VGA capture case
				pYUVDataBuf : Input buffer from Camera Driver YUV422.
				mVGANewheap->base() : 90 or 270 degree rotated YUV422 format.
			 */
			{
				int error = 0;
#if OMAP_SCALE
				neon_args->pIn 		= (uint8_t*)TempHeap->base();
#else
				neon_args->pIn 		= (uint8_t*)pYUVDataBuf;
#endif
				neon_args->pOut 	= (uint8_t*)mVGANewheap->base();
				neon_args->height 	= mPreviewWidth; 
				neon_args->width 	= mPreviewHeight;
#if OMAP_SCALE
				neon_args->rotate 	= NEON_ROT90;
#else
				neon_args->rotate 	= NEON_ROT270;
#endif
				if (Neon_Rotate != NULL)
					error = (*Neon_Rotate)(neon_args);
				else
					LOGE("Rotate Fucntion pointer Null");

				if (error < 0) {
					LOGE("Error in Rotation 90");

				}							    					
			}
#if OMAP_SCALE
			TempHeapBase.clear();
			TempBase.clear();
			TempHeap.clear();
#endif
#if TIMECHECK
			PPM("YUV COLOR ROTATION Done\n");
#endif
#endif //HARDWARE_OMX

			if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)
			{
#ifdef HARDWARE_OMX  
				int jpegSize = image_width * image_height*2;
				capture_len = (image_width*image_height*2) ;
				CreateExif(NULL, 0, pExifBuf, EXIF_Data_Size, 0);
				HAL_PRINT("VGA EXIF size : %d\n", EXIF_Data_Size);
				mJPEGPictureHeap = new MemoryHeapBase(jpegSize + 256);
				outBuffer = (void *)((unsigned long)(mJPEGPictureHeap->getBase()) + 128);
#if TIMECHECK
				PPM("BEFORE JPEG Encode Image\n");
#endif
				HAL_PRINT("VGA capture : outbuffer = 0x%x, jpegSize = %d, yuv_buffer = 0x%x, yuv_len = %d, image_width = %d, image_height = %d, quality = %d, mippMode =%d\n", 
							outBuffer, jpegSize, yuv_buffer, capture_len, image_width, image_height, mYcbcrQuality, mippMode); 

				if(isStart_JPEG)
				{
					int jpegFormat = PIX_YUV422I;
#ifdef OMAP_ENHANCEMENT
#if OMAP_SCALE

					err = jpegEncoder->encodeImage(outBuffer, 
											jpegSize, 
											(uint8_t*)mVGANewheap->base(), 
											capture_len, 
											pExifBuf,
											EXIF_Data_Size,
											image_width,	//
											image_height,	//
											mThumbnailWidth,
											mThumbnailHeight,
											mYcbcrQuality,
											jpegFormat);
#else
					err = jpegEncoder->encodeImage(outBuffer, 
											jpegSize, 
											(uint8_t*)mVGANewheap->base(), 
											capture_len, 
											pExifBuf,
											EXIF_Data_Size,
											image_height,	//
											image_width,	//
											mThumbnailWidth,
											mThumbnailHeight,
											mYcbcrQuality,
											jpegFormat);
#endif        	
					LOGD("JPEG ENCODE END\n");

					if(err != true) {
						LOGE("Jpeg encode failed!!\n");
						return -1;
					} else {
						LOGD("Jpeg encode success!!\n");
					}
#endif
				}
#if TIMECHECK
				PPM("AFTER JPEG Encode Image\n");
#endif
#ifdef OMAP_ENHANCEMENT
				mJPEGPictureMemBase = new MemoryBase(mJPEGPictureHeap, 128, jpegEncoder->jpegSize);
#endif
				if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)
				{
					mDataCb(CAMERA_MSG_COMPRESSED_IMAGE,mJPEGPictureMemBase,mCallbackCookie);
				}

				mJPEGPictureMemBase.clear();
				mJPEGPictureHeap.clear();
#endif //HARDWARE_OMX
			}//END of CAMERA_MSG_COMPRESSED_IMAGE

			yuv_buffer = (uint8_t*)cfilledbuffer.m.userptr;		           

			if(twoSecondReviewMode == 1)
			{ 	
				DrawOverlay(yuv_buffer, true);
			}  
			//yuv_buffer: [Reused]Output buffer with YUV 420P 270 degree rotated.             
			if(mMsgEnabled & CAMERA_MSG_RAW_IMAGE)
			{
				Neon_Convert_yuv422_to_YUV420P((uint8_t *)mVGANewheap->base(), (uint8_t *)yuv_buffer, mPreviewHeight, mPreviewWidth);         	
				mDataCb(CAMERA_MSG_RAW_IMAGE, mPictureBuffer, mCallbackCookie);
			}        
		}
		//END of CAMERA_MODE_YUV

		mPictureBuffer.clear();
		mPictureHeap.clear();
		if(mCamera_Mode == CAMERA_MODE_JPEG)
		{
			mFinalPictureBuffer.clear();
			mFinalPictureHeap.clear();
			mYUVPictureBuffer.clear();
			mYUVPictureHeap.clear();
		}

		if(mCamera_Mode == CAMERA_MODE_YUV)
		{
			if(mCameraIndex == VGA_CAMERA)
			{
				mVGAYUVPictureBuffer.clear();
				mVGAYUVPictureHeap.clear();
			}
		}

		delete []pExifBuf;
		mCaptureFlag = false;
		LOG_FUNCTION_NAME_EXIT

		return NO_ERROR;

	}

	//[20091123 exif Ratnesh
	void CameraHal::CreateExif(unsigned char* pInThumbnailData,int Inthumbsize,unsigned char* pOutExifBuf,int& OutExifSize,int flag)
	{
		int w =0, h = 0;

		int orientationValue = getOrientation();
		HAL_PRINT("CreateExif orientationValue = %d \n", orientationValue);				
		
		ExifCreator* mExifCreator = new ExifCreator();
		unsigned int ExifSize = 0;
		ExifInfoStructure ExifInfo;
		char ver_date[5] = {NULL,};
		unsigned short tempISO = 0;
		struct v4l2_exif exifobj;
		
		// To read values from driver
		if(mCameraIndex==MAIN_CAMERA)
		{
			getExifInfoFromDriver(&exifobj);
		}
   
		memset(&ExifInfo, NULL, sizeof(ExifInfoStructure));

		strcpy( (char *)&ExifInfo.maker, "SAMSUNG");
		strcpy( (char *)&ExifInfo.model, "GT-I9003");

		mParameters.getPreviewSize(&w, &h);

		mParameters.getPictureSize((int*)&ExifInfo.imageWidth , (int*)&ExifInfo.imageHeight);
		mParameters.getPictureSize((int*)&ExifInfo.pixelXDimension, (int*)&ExifInfo.pixelYDimension);

		struct tm *t = NULL;
		time_t nTime;
		time(&nTime);
		t = localtime(&nTime);

		if(t != NULL)
		{
			sprintf((char *)&ExifInfo.dateTimeOriginal, "%4d:%02d:%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
			sprintf((char *)&ExifInfo.dateTimeDigitized, "%4d:%02d:%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);						
			sprintf((char *)&ExifInfo.dateTime, "%4d:%02d:%02d %02d:%02d:%02d", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec); 					
		}

		if(mCameraIndex==MAIN_CAMERA)
		{
			int cam_ver = GetCamera_version();

			ExifInfo.Camversion[0] = (cam_ver & 0xFF);
			ExifInfo.Camversion[1] = ((cam_ver >> 8) & 0xFF);
			ExifInfo.Camversion[2] = ((cam_ver >> 16) & 0xFF);
			ExifInfo.Camversion[3] = ((cam_ver >> 24) & 0xFF);
			HAL_PRINT("CreateExif GetCamera_version =[%x][%x][%x][%x]\n", ExifInfo.Camversion[2],ExifInfo.Camversion[3],ExifInfo.Camversion[0],ExifInfo.Camversion[1]);	

			sprintf((char *)&ExifInfo.software, "fw %02d.%02d prm %02d.%02d", ExifInfo.Camversion[2],ExifInfo.Camversion[3],ExifInfo.Camversion[0],ExifInfo.Camversion[1]); 	
			if(mThumbnailWidth > 0 && mThumbnailHeight > 0)
			{
				ExifInfo.hasThumbnail = true;
				ExifInfo.thumbStream			= pInThumbnailData;
				ExifInfo.thumbSize				= Inthumbsize;
				ExifInfo.thumbImageWidth		= mThumbnailWidth;
				ExifInfo.thumbImageHeight		= mThumbnailHeight;
			}
			else
			{
				ExifInfo.hasThumbnail = false;
			}

			ExifInfo.exposureProgram            = 3;
			ExifInfo.exposureMode               = 0;
			ExifInfo.contrast                   = convertToExifLMH(getContrast(), 2);
			ExifInfo.fNumber.numerator          = 26;
			ExifInfo.fNumber.denominator        = 10;
			ExifInfo.aperture.numerator         = 26;
			ExifInfo.aperture.denominator       = 10;
			ExifInfo.maxAperture.numerator      = 26;
			ExifInfo.maxAperture.denominator    = 10;
			ExifInfo.focalLength.numerator      = 3430;
			ExifInfo.focalLength.denominator    = 1000;
			//[ 2010 05 01 exif
			ExifInfo.shutterSpeed.numerator 	= exifobj.TV_Value;
			ExifInfo.shutterSpeed.denominator   = 100;
			ExifInfo.exposureTime.numerator     = 1;
			ExifInfo.exposureTime.denominator   = (unsigned int)pow(2.0, ((double)exifobj.TV_Value/100.0));
			//]
			ExifInfo.brightness.numerator       = 5;
			ExifInfo.brightness.denominator     = 9;
			ExifInfo.iso                        = 1;
			ExifInfo.flash                     	= 0;	// default value

			// Flash
			// bit 0    -whether the flash fired
			// bit 1,2 -status of returned light
			// bit 3,4 - indicating the camera's flash mode
			// bit 5    -presence of a flash function
			// bit 6    - red-eye mode

			// refer to flash_mode[] at CameraHal.cpp
			// off = 1
			// on = 2
			// auto = 3


			// Todo : Need to implement how HAL can recognize existance of flash
			//		if( ! isFlashExist )	// pseudo code
			//			ExifInfo.flash = 32;		// bit 5 - No flash function.
			//		else
			{
				LOGD("createExif - flashmode = %d flash result = %d", mPreviousFlashMode, ExifInfo.flash);

				// bit 0
				ExifInfo.flash = ExifInfo.flash | exifobj.flash;
				// bit 3,4
				if(mPreviousFlashMode == 3)	// Flashmode auto
					ExifInfo.flash = ExifInfo.flash |24;
				// bit 6
				// Todo : Need to implement about red-eye			
				//			if(mPreviousFlashMode == ??)	// Flashmode red-eye
				//				ExifInfo.flash = ExifInfo.flash | 64;						
			}

			HAL_PRINT("Main Orientation = %d\n",orientationValue);
			switch(orientationValue)
			{            		
				case 0:
					ExifInfo.orientation                = 1 ;
					break;
				case 90:
					ExifInfo.orientation                = 6 ;
					break;
				case 180:
					ExifInfo.orientation                = 3 ;
					break;
				case 270:
					ExifInfo.orientation                = 8 ;
					break;
				default:
					ExifInfo.orientation                = 1 ;
					break;
			}
			//[ 2010 05 01 exif
			double calIsoValue = 0;
			calIsoValue = pow(2.0,((double)exifobj.SV_Value/100.0))*3.125;
			//]
			if(calIsoValue < 8.909)
			{
				tempISO = 0;
			}
			else if(calIsoValue >=8.909 && calIsoValue < 11.22)
			{
				tempISO = 10;
			}
			else if(calIsoValue >=11.22 && calIsoValue < 14.14)
			{
				tempISO = 12;
			}
			else if(calIsoValue >=14.14 && calIsoValue < 17.82)
			{
				tempISO = 16;
			}
			else if(calIsoValue >=17.82 && calIsoValue < 22.45)
			{
				tempISO = 20;
			}
			else if(calIsoValue >=22.45 && calIsoValue < 28.28)
			{
				tempISO = 25;
			}
			else if(calIsoValue >=28.28 && calIsoValue < 35.64)
			{
				tempISO = 32;
			}
			else if(calIsoValue >=35.64 && calIsoValue < 44.90)
			{
				tempISO = 40;
			}
			else if(calIsoValue >=44.90 && calIsoValue < 56.57)
			{
				tempISO = 50;
			}
			else if(calIsoValue >=56.57 && calIsoValue < 71.27)
			{
				tempISO = 64;
			}
			else if(calIsoValue >=71.27 && calIsoValue < 89.09)
			{
				tempISO = 80;
			}
			else if(calIsoValue >=89.09 && calIsoValue < 112.2)
			{
				tempISO = 100;
			}
			else if(calIsoValue >=112.2 && calIsoValue < 141.4)
			{
				tempISO = 125;
			}
			else if(calIsoValue >=141.4 && calIsoValue < 178.2)
			{
				tempISO = 160;
			}
			else if(calIsoValue >=178.2 && calIsoValue < 224.5)
			{
				tempISO = 200;
			}
			else if(calIsoValue >=224.5 && calIsoValue < 282.8)
			{
				tempISO = 250;
			}
			else if(calIsoValue >=282.8 && calIsoValue < 356.4)
			{
				tempISO = 320;
			}
			else if(calIsoValue >=356.4 && calIsoValue < 449.0)
			{
				tempISO = 400;
			}
			else if(calIsoValue >=449.0 && calIsoValue < 565.7)
			{
				tempISO = 500;
			}
			else if(calIsoValue >=565.7 && calIsoValue < 712.7)
			{
				tempISO = 640;
			}
			else if(calIsoValue >=712.7 && calIsoValue < 890.9)
			{
				tempISO = 800;
			}
			else if(calIsoValue >=890.9 && calIsoValue < 1122)
			{
				tempISO = 1000;
			}
			else if(calIsoValue >=1122 && calIsoValue < 1414)
			{
				tempISO = 1250;
			}
			else if(calIsoValue >=1414 && calIsoValue < 1782)
			{
				tempISO = 160;
			}
			else if(calIsoValue >=1782 && calIsoValue < 2245)
			{
				tempISO = 2000;
			}
			else if(calIsoValue >=2245 && calIsoValue < 2828)
			{
				tempISO = 2500;
			}
			else if(calIsoValue >=2828 && calIsoValue < 3564)
			{
				tempISO = 3200;
			}
			else if(calIsoValue >=3564 && calIsoValue < 4490)
			{
				tempISO = 4000;
			}
			else if(calIsoValue >=4490 && calIsoValue < 5657)
			{
				tempISO = 5000;
			}
			else if(calIsoValue >=5657 && calIsoValue < 7127)
			{
				tempISO = 6400;
			}
			else
			{
				tempISO = 8000;
			}

			if(mPreviousSceneMode <= 1)
			{
				ExifInfo.meteringMode               = mPreviousMetering;
				if(mPreviousWB <= 1)
				{
					ExifInfo.whiteBalance               = 0;
				}
				else
				{
					ExifInfo.whiteBalance               = 1;
				}
				ExifInfo.saturation                 = convertToExifLMH(getSaturation(), 2);
				ExifInfo.sharpness                  = convertToExifLMH(getSharpness(), 2);
				switch(mPreviousISO)
				{
					case 2:
						ExifInfo.isoSpeedRating             = 50;
						break;
					case 3:
						ExifInfo.isoSpeedRating             = 100;
						break;
					case 4:
						ExifInfo.isoSpeedRating             = 200;
						break;
					case 5:
						ExifInfo.isoSpeedRating             = 400;
						break;
					case 6:
						ExifInfo.isoSpeedRating             = 800;
						break;
					default:
						ExifInfo.isoSpeedRating             = tempISO;
						break;
				}                

				switch(getBrightness())
				{
					case 0:
						ExifInfo.exposureBias.numerator = -20;
						break;
					case 1:
						ExifInfo.exposureBias.numerator = -15;
						break;
					case 2:
						ExifInfo.exposureBias.numerator = -10;
						break;
					case 3:
						ExifInfo.exposureBias.numerator =  -5;
						break;
					case 4:
						ExifInfo.exposureBias.numerator =   0;
						break;
					case 5:
						ExifInfo.exposureBias.numerator =   5;
						break;
					case 6:
						ExifInfo.exposureBias.numerator =  10;
						break;
					case 7:
						ExifInfo.exposureBias.numerator =  15;
						break;
					case 8:
						ExifInfo.exposureBias.numerator =  20;
						break;
					default:
						ExifInfo.exposureBias.numerator = 0;
						break;
				}
				ExifInfo.exposureBias.denominator       = 10;
				ExifInfo.sceneCaptureType               = 0;
			}
			else
			{
				switch(mPreviousSceneMode)
				{
					case 3://sunset
						ExifInfo.meteringMode               = 2;
						ExifInfo.whiteBalance               = 1;
						ExifInfo.saturation                 = 0;
						ExifInfo.sharpness                  = 0;
						ExifInfo.isoSpeedRating             = tempISO;
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
						ExifInfo.exposureBias.numerator     = 10;
						ExifInfo.exposureBias.denominator   = 10;
						ExifInfo.sceneCaptureType           = 1;
						break;
					case 7://againstlight
						ExifInfo.whiteBalance               = 0;
						ExifInfo.saturation                 = 0;
						ExifInfo.sharpness                  = 0;
						ExifInfo.isoSpeedRating             = tempISO;
						if(mPreviousFlashMode <= 1)
						{
							ExifInfo.meteringMode               = 3;
						}
						else
						{
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
						ExifInfo.exposureBias.numerator     = 0;
						ExifInfo.exposureBias.denominator   = 10;
						ExifInfo.sceneCaptureType           = 3;
						break;	
					case 10://landscape
						ExifInfo.meteringMode               = 5;
						ExifInfo.whiteBalance               = 0;
						ExifInfo.saturation                 = 2;
						ExifInfo.sharpness                  = 2;
						ExifInfo.isoSpeedRating             = tempISO;
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
						ExifInfo.exposureBias.numerator     = 0;
						ExifInfo.exposureBias.denominator   = 10;
						ExifInfo.sceneCaptureType           = 4;
						break;
				}
			}
		}
		else // VGA Camera
		{
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
			ExifInfo.maxAperture.denominator	= 10;
			ExifInfo.focalLength.numerator      = 900;
			ExifInfo.focalLength.denominator	= 1000;
			ExifInfo.shutterSpeed.numerator 	= 16;
			ExifInfo.shutterSpeed.denominator	= 1;
			ExifInfo.brightness.numerator 	    = 5;
			ExifInfo.brightness.denominator     = 9;
			ExifInfo.iso                        = 1;

			HAL_PRINT("VGA Orientation = %d\n",orientationValue);
			switch(orientationValue)
			{
				case 0:
					ExifInfo.orientation                = 1;
					break;
				case 90:
					ExifInfo.orientation                = 6;
					break;
				case 180:
					ExifInfo.orientation                = 3;
					break;
				case 270:
					ExifInfo.orientation                = 8;
					break;
				default:
					ExifInfo.orientation                = 1 ;
					break;
			}

			ExifInfo.meteringMode               = mPreviousMetering;
			ExifInfo.whiteBalance               = 0;
			ExifInfo.saturation                 = 0;
			ExifInfo.sharpness                  = 0;
			ExifInfo.isoSpeedRating             = 100;
			ExifInfo.exposureTime.numerator     = 1;
			ExifInfo.exposureTime.denominator   = 16;
			ExifInfo.flash 						= 0;
			switch(getBrightness())
			{
				case 0:
					ExifInfo.exposureBias.numerator = -20;
					break;
				case 1:
					ExifInfo.exposureBias.numerator = -15;
					break;
				case 2:
					ExifInfo.exposureBias.numerator = -10;
					break;
				case 3:
					ExifInfo.exposureBias.numerator =  -5;
					break;
				case 4:
					ExifInfo.exposureBias.numerator =   0;
					break;
				case 5:
					ExifInfo.exposureBias.numerator =   5;
					break;
				case 6:
					ExifInfo.exposureBias.numerator =  10;
					break;
				case 7:
					ExifInfo.exposureBias.numerator =  15;
					break;
				case 8:
					ExifInfo.exposureBias.numerator =  20;
					break;
				default:
					ExifInfo.exposureBias.numerator = 0;
					break;
			}
			ExifInfo.exposureBias.denominator       = 10;
			ExifInfo.sceneCaptureType               = 0;
		}

		double arg0,arg3;
		int arg1,arg2;

		if (mPreviousGPSLatitude != 0 && mPreviousGPSLongitude != 0)
		{		
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
		}
		else
		{
			ExifInfo.hasGps = false;
		}		


		ExifSize = mExifCreator->ExifCreate_wo_GPS( (unsigned char *)pOutExifBuf, &ExifInfo, flag);
		OutExifSize = ExifSize;
		delete mExifCreator; 
	}

	//[20091123 exif Ratnesh
	bool CameraHal::CreateJpegWithExif(unsigned char* pInJpegData, int InJpegSize,unsigned char* pInExifBuf,int InExifSize,
			unsigned char* pOutJpegData, int& OutJpegSize)
	{
		int offset = 0;

		if( pInJpegData == NULL || InJpegSize == 0 || pInExifBuf == 0 || InExifSize == 0 || pOutJpegData == NULL )
		{
			return false;
		}

		memcpy( pOutJpegData, pInJpegData, 2 );

		offset += 2;

		if( InExifSize != 0 )
		{
			memcpy( &(pOutJpegData[offset]), pInExifBuf, InExifSize );
			offset += InExifSize;		
		}

		memcpy( &(pOutJpegData[offset]), &(pInJpegData[2]), InJpegSize - 2);

		offset +=  InJpegSize -2;

		OutJpegSize = offset;

		return true;
	}

	//[20091123 exif Ratnesh
	int CameraHal::GetJpegImageSize()
	{
		struct v4l2_control vc;
		vc.id=V4L2_CID_JPEG_SIZE;
		vc.value=0;
		if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0) 
		{
			LOGE ("Failed to get VIDIOC_G_CTRL.\n");
			return -1;
		}
		return (vc.value);
	}

	//[20091123 exif Ratnesh
	int CameraHal::GetThumbNailDataSize()
	{
		struct v4l2_control vc;
		vc.id=V4L2_CID_THUMBNAIL_SIZE;
		vc.value=0;
		if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0)
		{
			LOGE ("Failed to get VIDIOC_G_CTRL.");
			return -1;
		}
		return(vc.value);
	}

	int CameraHal::GetThumbNailOffset()
	{
		struct v4l2_control vc;
		vc.id=V4L2_CID_FW_THUMBNAIL_OFFSET;
		vc.value=0;
		if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0)
		{
			LOGE ("Failed to get VIDIOC_G_CTRL.\n");
			return -1;
		}
		return(vc.value);
	}

	int CameraHal::GetYUVOffset()
	{
		struct v4l2_control vc;
		vc.id=V4L2_CID_FW_YUV_OFFSET;
		vc.value=0;
		if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0)
		{
			LOGE ("Failed to get VIDIOC_G_CTRL.");
			return -1;
		}
		return(vc.value);
	}

	int CameraHal::GetJPEG_Capture_Width()
	{
		struct v4l2_control vc;
		vc.id=V4L2_CID_JPEG_CAPTURE_WIDTH;
		vc.value=0;
		if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0)
		{
			LOGE ("Failed to get VIDIOC_G_CTRL.\n");
			return -1;
		}
		return(vc.value);
	}

	int CameraHal::GetJPEG_Capture_Height()
	{
		struct v4l2_control vc;
		vc.id=V4L2_CID_JPEG_CAPTURE_HEIGHT;
		vc.value=0;
		if(ioctl(camera_device,VIDIOC_G_CTRL,&vc) < 0)
		{
			LOGE ("Failed to get VIDIOC_G_CTRL.\n");
			return -1;
		}
		return(vc.value);
	}

	int CameraHal::GetCamera_version()
	{
		struct v4l2_control vc;

		CLEAR(vc);
		vc.id = V4L2_CID_FW_VERSION;
		vc.value = 0;
		if (ioctl (camera_device, VIDIOC_G_CTRL, &vc) < 0)
		{
			LOGE("Failed to get V4L2_CID_FW_VERSION.\n");
		}

		return (vc.value);
	}

	//[20100122 exif Ratnesh
	void CameraHal::convertFromDecimalToGPSFormat(double arg1,int& arg2,int& arg3,double& arg4)
	{
		double temp1=0,temp2=0,temp3=0;
		arg2  = (int)arg1;
		temp1 = arg1-arg2;
		temp2 = temp1*60 ;
		arg3  = (int)temp2;
		temp3 = temp2-arg3;
		arg4  = temp3*60;
	}

	//[20100122 exif Ratnesh
	void CameraHal::getExifInfoFromDriver(v4l2_exif* exifobj)
	{
		if(ioctl(camera_device,VIDIOC_G_EXIF,exifobj) < 0)
		{
			LOGE ("Failed to get vidioc_g_exif.\n"); 
		}
	}

	int CameraHal::convertToExifLMH(int value, int key)
	{
		const int NORMAL = 0;
		const int LOW    = 1;
		const int HIGH   = 2;

		value -= key;

		if(value == 0) return NORMAL;
		if(value < 0) return LOW;
		else return HIGH;
	}
};
