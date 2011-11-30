#define LOG_TAG "ExifCreator"
#include <utils/Log.h>

#include "ExifCreator.h"

namespace android {


ExifCreator::ExifCreator()
{
}

ExifCreator::~ExifCreator()
{
}


void* ExifCreator::ExifMemcpy(void *dest, void *src, unsigned int count)
{
	return memcpy(dest, src, count);
}

unsigned int ExifCreator::ExifCreate(unsigned char* pInput, ExifInfoStructure *pSetExifInfo)
{	
	unsigned int offset = 0;
	unsigned int appLength = 0;

	unsigned char app1Header[10] =
	{
		0xff,0xe1,0x00,0x08,0x45,
		0x78,0x69,0x66,0x00,0x00
	};


	ExifMemcpy (pInput, app1Header, 10);
	offset += 10;

	offset += __ExifCreate(pInput + offset, pSetExifInfo);
	if(offset == 10 ) {
		return  0;
	}

	//upate length
	appLength = offset-2;
	pInput[2] = appLength>>8;
	pInput[3] = appLength & 0xff;	

	return offset;
}

unsigned int ExifCreator::ExifCreate_wo_GPS(unsigned char* pInput, ExifInfoStructure *pSetExifInfo,int flag)
{

	unsigned int offset = 0;
	unsigned int appLength = 0;
	unsigned char app1Header[10]={0xff, 0xe1, 0x00, 0x08, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
	unsigned int app1HeaderSize = sizeof(app1Header);

		
	ExifMemcpy (pInput, app1Header, app1HeaderSize);
	offset += app1HeaderSize;

	offset += __ExifCreate_wo_GPS(pInput + offset, pSetExifInfo,flag);
	if(offset == app1HeaderSize ) {
		return  0;
	}
	
	if(flag)
	{
		//upate length
		appLength = offset-2;
		pInput[2] = appLength>>8;
		pInput[3] = appLength & 0xff;	
	}
	else
	{
		//clear lenght field, will be updated by TI OMX jpeg encoder
		memset(pInput, 0, 4);	//clear 1st 4 byte of app1 header
	}

	return offset;
}



unsigned int ExifCreator::__ExifCreate_wo_GPS(unsigned char* pInput, ExifInfoStructure *pSetExifInfo,int flag)
{
	unsigned char* pCurBuff = pInput;
	unsigned int offset = 0;
	unsigned int count = 0;
	unsigned int m_0th_num = NUM_0TH_IFD;
	unsigned int m_1th_num = NUM_1TH_IFD;

	if(pSetExifInfo->hasGps)
	{
		m_0th_num += 1; //add GPS
	}
	
	//Byte Order
	*pCurBuff++ = 0x49;
	*pCurBuff++ = 0x49;

	//42
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)42);
	pCurBuff +=2;

	offset +=8;

	//0th IFD Offset
	__ExifWriteLONG_LE(pCurBuff, offset);
	pCurBuff = pInput + offset;

	//0th IFD
	//num of Interoperablility
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short) m_0th_num);
	pCurBuff += NUMBER_SIZE;

	offset += (NUMBER_SIZE + ( m_0th_num * TAG_SIZE ) + OFFSET_SIZE ); 

	//1. make
	count = __ExifGetASCIILength(pSetExifInfo->maker);
	__ExifWriteASCIITag(MAKE_ID, pSetExifInfo->maker, count, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//2. Model
	count = __ExifGetASCIILength(pSetExifInfo->model);
	__ExifWriteASCIITag(MODEL_ID, pSetExifInfo->model, count, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//3. Orientation
	__ExifWriteSHORTTag(ORIENTATION_ID, pSetExifInfo->orientation, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//4. Software
	count = __ExifGetASCIILength(pSetExifInfo->software);
	__ExifWriteASCIITag(SOFTWARE_ID, pSetExifInfo->software, count, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//vinayan [[
	if(!flag)
	{
		unsigned char padd[ ]={0x00};
		ExifMemcpy(pInput + offset, padd, sizeof(padd));
		offset +=sizeof(padd);
	}
	//vinayan ]]

	//5. DateTime  / ½Å·Ú¼º spec
	count = __ExifGetASCIILength(pSetExifInfo->dateTime);
	__ExifWriteASCIITag(DATE_TIME_ID, pSetExifInfo->dateTime, count,  pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//6. YCbCr Positioning
	__ExifWriteSHORTTag(YCBCR_POSITIONING_ID, 1, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//7. X Resolution
	__ExifWriteRATIONALTag(X_RESOLUTION_ID, 72, 1, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//8. Y Resolution
	__ExifWriteRATIONALTag(Y_RESOLUTION_ID, 72, 1, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//9. Resolution unit
	__ExifWriteSHORTTag(RESOLUTION_UNIT_ID, 2, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//10. Exif IFD
	__ExifWriteIFDTag(EXIF_IFD_ID, pSetExifInfo, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//11. GPS IFD
	if(pSetExifInfo->hasGps)
	{
		__ExifWriteIFDTag(GPS_IFD_ID, pSetExifInfo, pInput, pCurBuff, &offset);
		pCurBuff += TAG_SIZE;
	}

	if(pSetExifInfo->hasThumbnail)
	{
		//Next IFD Offset
		__ExifWriteLONG_LE(pCurBuff, offset);
		pCurBuff = pInput + offset;
		
		//1st IFD
		//num of Interoperablility
		__ExifWriteSHORT_LE(pCurBuff, (unsigned short) m_1th_num);
		pCurBuff += NUMBER_SIZE;
		
		offset += (NUMBER_SIZE + ( m_1th_num * TAG_SIZE ) + OFFSET_SIZE );

		//1. ImageWidth  / ½Å·Ú¼º spec
		__ExifWriteLONGTag(IMAGE_WIDTH_ID, pSetExifInfo->thumbImageWidth, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;
		
		//2. ImageHeight  / ½Å·Ú¼º spec
		__ExifWriteLONGTag(IMAGE_LENGTH_ID, pSetExifInfo->thumbImageHeight, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//3. compression
		__ExifWriteSHORTTag(COMPRESSION_ID, 6, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//4. Orientation  / ½Å·Ú¼º spec
		__ExifWriteSHORTTag(ORIENTATION_ID, pSetExifInfo->orientation, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//5. X Resolution
		__ExifWriteRATIONALTag(X_RESOLUTION_ID, 72, 1, pInput, pCurBuff, &offset);
		pCurBuff += TAG_SIZE;

		//6. Y Resolution
		__ExifWriteRATIONALTag(Y_RESOLUTION_ID, 72, 1, pInput, pCurBuff, &offset);
		pCurBuff += TAG_SIZE;

		//7. Resolution unit
		__ExifWriteSHORTTag(RESOLUTION_UNIT_ID, 2, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//8. JPEGInterchangeFormat
		__ExifWriteLONGTag(JFIF_ID, offset, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;
		
		//9. JPEGInterchangeFormat Length
		__ExifWriteLONGTag(JFIF_LENGTH_ID, pSetExifInfo->thumbSize, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;
		
		//10. YCbCr Positioning
		__ExifWriteSHORTTag(YCBCR_POSITIONING_ID, 1, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//Next IFD Offset
		__ExifWriteLONG_LE(pCurBuff, 0);
		
		//make Thumbnail
		if(flag)
		{
			if(offset + pSetExifInfo->thumbSize < 0xffff) {
				ExifMemcpy(pInput + offset, pSetExifInfo->thumbStream, pSetExifInfo->thumbSize);
				offset += pSetExifInfo->thumbSize;
			}
			else {
				offset = 0;
			}
		}
	}
	else
	{
		//Next IFD Offset
		__ExifWriteLONG_LE(pCurBuff, 0);
	}
        
	return offset;
}



unsigned int ExifCreator::__ExifCreate(unsigned char* pInput, ExifInfoStructure *pSetExifInfo)
{
	unsigned char* pCurBuff = pInput;
	unsigned int offset = 0;
	unsigned int count = 0;
	unsigned int m_0th_num = NUM_0TH_IFD;
	unsigned int m_1th_num = NUM_1TH_IFD;

	if(pSetExifInfo->hasGps)
	{
		m_0th_num += 1; //add GPS
	}	
	
	//Byte Order
	*pCurBuff++ = 0x49;
	*pCurBuff++ = 0x49;

	//42
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)42);
	pCurBuff +=2;

	offset +=8;

	//0th IFD Offset
	__ExifWriteLONG_LE(pCurBuff, offset);
	pCurBuff = pInput + offset;

	//0th IFD
	//num of Interoperablility
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short) m_0th_num);
	pCurBuff += NUMBER_SIZE;

	offset += (NUMBER_SIZE + ( m_0th_num * TAG_SIZE ) + OFFSET_SIZE ); 

	//1. make
	count = __ExifGetASCIILength(pSetExifInfo->maker);
	__ExifWriteASCIITag(MAKE_ID, pSetExifInfo->maker, count, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//2. Model
	count = __ExifGetASCIILength(pSetExifInfo->model);
	__ExifWriteASCIITag(MODEL_ID, pSetExifInfo->model, count, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//3. Orientation
	__ExifWriteSHORTTag(ORIENTATION_ID, pSetExifInfo->orientation, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//4. Software
	count = __ExifGetASCIILength(pSetExifInfo->software);
	__ExifWriteASCIITag(SOFTWARE_ID, pSetExifInfo->software, count, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//5. DateTime  / ½Å·Ú¼º spec
	count = __ExifGetASCIILength(pSetExifInfo->dateTime);
	__ExifWriteASCIITag(DATE_TIME_ID, pSetExifInfo->dateTime, count,  pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//6. YCbCr Positioning
	__ExifWriteSHORTTag(YCBCR_POSITIONING_ID, 1, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//7. X Resolution
	__ExifWriteRATIONALTag(X_RESOLUTION_ID, 72, 1, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//8. Y Resolution
	__ExifWriteRATIONALTag(Y_RESOLUTION_ID, 72, 1, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

	//9. Resolution unit
	__ExifWriteSHORTTag(RESOLUTION_UNIT_ID, 2, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//10. Exif IFD
	__ExifWriteIFDTag(EXIF_IFD_ID, pSetExifInfo, pInput, pCurBuff, &offset);
	pCurBuff += TAG_SIZE;

   	//11. GPS IFD
	if(pSetExifInfo->hasGps)
	{
		__ExifWriteIFDTag(GPS_IFD_ID, pSetExifInfo, pInput, pCurBuff, &offset);
		pCurBuff += TAG_SIZE;
	}

	if(pSetExifInfo->hasThumbnail)
	{
		//Next IFD Offset
		__ExifWriteLONG_LE(pCurBuff, offset);
		pCurBuff =pInput + offset;
		
		//1st IFD
		//num of Interoperablility
		__ExifWriteSHORT_LE(pCurBuff, (unsigned short) m_1th_num);
		pCurBuff += NUMBER_SIZE;
		
		offset += (NUMBER_SIZE + ( m_1th_num * TAG_SIZE ) + OFFSET_SIZE );

		//1. ImageWidth  / ½Å·Ú¼º spec
		__ExifWriteLONGTag(IMAGE_WIDTH_ID, pSetExifInfo->thumbImageWidth, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;
		
		//2. ImageHeight  / ½Å·Ú¼º spec
		__ExifWriteLONGTag(IMAGE_LENGTH_ID, pSetExifInfo->thumbImageHeight, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//3. compression
		__ExifWriteSHORTTag(COMPRESSION_ID, 6, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//4. Orientation  / ½Å·Ú¼º spec
		__ExifWriteSHORTTag(ORIENTATION_ID, pSetExifInfo->orientation, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//5. X Resolution
		__ExifWriteRATIONALTag(X_RESOLUTION_ID, 72, 1, pInput, pCurBuff, &offset);
		pCurBuff += TAG_SIZE;

		//6. Y Resolution
		__ExifWriteRATIONALTag(Y_RESOLUTION_ID, 72, 1, pInput, pCurBuff, &offset);
		pCurBuff += TAG_SIZE;

		//7. Resolution unit
		__ExifWriteSHORTTag(RESOLUTION_UNIT_ID, 2, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;
	
		//8. JPEGInterchangeFormat
		__ExifWriteLONGTag(JFIF_ID, offset, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;
		
		//9. JPEGInterchangeFormat Length
		__ExifWriteLONGTag(JFIF_LENGTH_ID, pSetExifInfo->thumbSize, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;
		
		//10. YCbCr Positioning
		__ExifWriteSHORTTag(YCBCR_POSITIONING_ID, 1, pInput, pCurBuff);
		pCurBuff += TAG_SIZE;

		//Next IFD Offset
		__ExifWriteLONG_LE(pCurBuff, 0);
		
		//make Thumbnail
		if(offset + pSetExifInfo->thumbSize < 0xffff) {
			ExifMemcpy(pInput +offset, pSetExifInfo->thumbStream, pSetExifInfo->thumbSize);		
			offset += pSetExifInfo->thumbSize;
		}
		else {
			offset = 0;
		}
	}
	else
	{
		//Next IFD Offset
		__ExifWriteLONG_LE(pCurBuff, 0);
	}
	
	return offset;
}




void ExifCreator::__ExifWriteIFDTag(unsigned short tagID, ExifInfoStructure *pSetExifInfo, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	//Exif IFD pointer
	 __ExifWriteLONGTag(tagID, *pOffset, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;
	if(tagID == EXIF_IFD_ID)
		__ExifWriteExifIFD(pSetExifInfo, pInput, pCurBuff, pOffset);
	else if(tagID == GPS_IFD_ID)
		__ExifWriteGPDIFD(pSetExifInfo, pInput, pCurBuff, pOffset);
}


void ExifCreator::__ExifWriteGPDIFD(ExifInfoStructure *pSetExifInfo, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	unsigned int count = 0;

	unsigned char gpsVersion[4]		= { 0x2, 0x2, 0x0, 0x0 };
	unsigned char componentsConfig[4]	= { 0x01, 0x02, 0x03, 0x00 };
	unsigned char flashpixVersion[4]	= { 0x30, 0x31, 0x30, 0x30 };
	unsigned char processingMethodPrefix[8]	= {0x41, 0x53, 0x43, 0x49, 0x49, 0x00, 0x00, 0x00};

	pCurBuff = pInput + *pOffset;

	__ExifWriteSHORT_LE(pCurBuff, (unsigned short) NUM_GPS_IDF);
	pCurBuff += NUMBER_SIZE;

	*pOffset += (NUMBER_SIZE + ( NUM_GPS_IDF * TAG_SIZE ) + OFFSET_SIZE );

	//0. GPSVersionID
	__ExifWriteBYTESTag(0x0000, gpsVersion,  4, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//1. GPSLatitudeRef
	count = __ExifGetASCIILength(pSetExifInfo->GPSLatitudeRef);
	__ExifWriteASCIITag(0x0001, pSetExifInfo->GPSLatitudeRef, count, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//2. GPSLatitude
	__ExifWriteGPSInfoTag(0x0002, pSetExifInfo->GPSLatitude, 3, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//3. GPSLongitudeRef
	count = __ExifGetASCIILength(pSetExifInfo->GPSLongitudeRef);
	__ExifWriteASCIITag(0x0003, pSetExifInfo->GPSLongitudeRef, count, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//4. GPSLatitude
	__ExifWriteGPSInfoTag(0x0004, pSetExifInfo->GPSLongitude, 3,  pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//5. GPSAltitudeRef
	__ExifWriteBYTESTag(0x0005, &pSetExifInfo->GPSAltitudeRef, 1, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//6. GPSAltitude
	__ExifWriteGPSInfoTag(0x0006, pSetExifInfo->GPSAltitude, 1, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//7. GPSTimestamp
	__ExifWriteGPSInfoTag(0x0007, pSetExifInfo->GPSTimestamp, 3, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//8. GPSProcessingMethod
	count = __ExifGetASCIILength(pSetExifInfo->GPSProcessingMethod);
	memmove(pSetExifInfo->GPSProcessingMethod + sizeof(processingMethodPrefix), pSetExifInfo->GPSProcessingMethod, count);
	memcpy(pSetExifInfo->GPSProcessingMethod, processingMethodPrefix, sizeof(processingMethodPrefix));
	count += sizeof(processingMethodPrefix);
	__ExifWriteUNDEFINEDTag(0x001B, pSetExifInfo->GPSProcessingMethod, count, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//9. GPSDatestamp
	count = __ExifGetASCIILength(pSetExifInfo->GPSDatestamp);
	__ExifWriteASCIITag(0x001D, pSetExifInfo->GPSDatestamp, count, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;
	
}


void ExifCreator::__ExifWriteExifIFD(ExifInfoStructure *pSetExifInfo, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	unsigned int count = 0;

	unsigned char exifVersion[4]		= { 0x30, 0x32, 0x32, 0x30 };
	unsigned char componentsConfig[4]	= { 0x01, 0x02, 0x03, 0x00 };
	unsigned char flashpixVersion[4]	= { 0x30, 0x31, 0x30, 0x30 };

	//unsigned char setAuto[4]	= { 0x41, 0x75, 0x74, 0x6f };

	pCurBuff = pInput + *pOffset;

	__ExifWriteSHORT_LE(pCurBuff, (unsigned short) NUM_EXIF_IFD);
	pCurBuff += NUMBER_SIZE;

	*pOffset += ( NUMBER_SIZE + ( NUM_EXIF_IFD * TAG_SIZE ) + OFFSET_SIZE );

	//1. Exposure Time / 0x829A
	__ExifWriteRATIONALTag(EXPOSURE_TIME_ID, pSetExifInfo->exposureTime.numerator,  pSetExifInfo->exposureTime.denominator, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//2. F number / 0x829D
	__ExifWriteRATIONALTag(FNUMBER_ID, pSetExifInfo->fNumber.numerator, pSetExifInfo->fNumber.denominator, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//3. Exposure Program / 0x8822
	__ExifWriteSHORTTag(EXPOSURE_PROGRAM_ID, pSetExifInfo->exposureProgram, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//4. ISO speed rating / 0x8827
	__ExifWriteSHORTTag(ISO_SPEED_RATING_ID, pSetExifInfo->isoSpeedRating, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//5. ExifVersion / 0x9000
	__ExifWriteUNDEFINEDTag(EXIF_VERSION_ID, exifVersion, 4, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//6. DateTimeOrigianl / 0x9003
	count = __ExifGetASCIILength(pSetExifInfo->dateTimeOriginal);
	__ExifWriteASCIITag(DATE_TIME_ORG_ID, pSetExifInfo->dateTimeOriginal, count,  pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//7. DateTimeDigitized / 0x9004
	count = __ExifGetASCIILength(pSetExifInfo->dateTimeDigitized);
	__ExifWriteASCIITag(DATE_TIME_DIGITIZE_ID, pSetExifInfo->dateTimeDigitized,  count, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//8. Components Configuration / 0x9101
	__ExifWriteUNDEFINEDTag(COMPONENTS_CONIFG_ID, componentsConfig, 4, pInput, pCurBuff, pOffset);	
	pCurBuff += TAG_SIZE;

	//9. Shutter Speed Value / 0x9201
	__ExifWriteSRATIONALTag(SHUTTER_SPEED_ID, pSetExifInfo->shutterSpeed.numerator,  pSetExifInfo->shutterSpeed.denominator, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//10. Aperture Value / 0x9202
	__ExifWriteRATIONALTag(APERTURE_VALUE_ID, pSetExifInfo->aperture.numerator,  pSetExifInfo->aperture.denominator, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//11. Brightness Value / 0x9203
	__ExifWriteSRATIONALTag(BRIGHTNESS_VALUE_ID, pSetExifInfo->brightness.numerator,  pSetExifInfo->brightness.denominator, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//12. Exposure Bias Value / 0x9204
	__ExifWriteSRATIONALTag(EXPOSURE_BIAS_ID, pSetExifInfo->exposureBias.numerator, pSetExifInfo->exposureBias.denominator, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//13. Max Aperture Value / 0x9205
	__ExifWriteRATIONALTag(MAX_APERTURE_VALUE_ID, pSetExifInfo->maxAperture.numerator, pSetExifInfo->maxAperture.denominator, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

	//14. Exposure Metering(Metering Mode) / 0x9207
	__ExifWriteSHORTTag(METERING_MODE_ID, pSetExifInfo->meteringMode, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;	

	//15. Flash / 0x9209
	__ExifWriteSHORTTag(FLASH_ID, pSetExifInfo->flash, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//16. Focal length  / 0x920A
	__ExifWriteRATIONALTag(FOCAL_LENGTH_ID, pSetExifInfo->focalLength.numerator, pSetExifInfo->focalLength.denominator, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;

#if 0
	//17. flashpixVersion / 0xA000
	__ExifWriteUNDEFINEDTag(FLASH_PIX_VERSION_ID, flashpixVersion, 4, pInput, pCurBuff, pOffset);
	pCurBuff += TAG_SIZE;
#endif

	//18. Color Space / 0xA001
	__ExifWriteSHORTTag(COLOR_SPACE_ID, 1, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//19. Pixel X Dimension / 0xA002
	__ExifWriteLONGTag(PIXEL_X_DIMENSION_ID, pSetExifInfo->pixelXDimension, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//20. Pixel Y Dimension /0xA003
	__ExifWriteLONGTag(PIXEL_Y_DIMENSION_ID, pSetExifInfo->pixelYDimension, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

#if 0
	//21. Custom render / 0xA401
	//__ExifWriteSHORTTag(CUSTOM_RENDER_ID, 0, pInput, pCurBuff);
	//pCurBuff += TAG_SIZE;
#endif

	//22. Exposure mode / 0xA402
	__ExifWriteSHORTTag(EXPOSURE_MODE_ID, pSetExifInfo->exposureMode, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//23. White Balance / 0xa403
	__ExifWriteSHORTTag(WHITE_BALANCE_ID, pSetExifInfo->whiteBalance, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

#if 0
	//24. Digital zoom ratio / 0xA404
	//__ExifWriteRATIONALTag(DIGITAL_ZOOM_RATIO_ID, 0, 2, pInput, pCurBuff, pOffset);
	//pCurBuff += TAG_SIZE;
#endif

	//25. Scence Capture Type  / 0xA406
	__ExifWriteSHORTTag(SCENCE_CAPTURE_TYPE_ID, pSetExifInfo->sceneCaptureType, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//26. Contrast  / 0xA408
	__ExifWriteSHORTTag(CONTRAST_ID, pSetExifInfo->contrast, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//27. Saturation  / 0xA409
	__ExifWriteSHORTTag(SATURATION_ID, pSetExifInfo->saturation, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

	//28. Sharpness  / 0xA40A
	__ExifWriteSHORTTag(SHARPNESS_ID, pSetExifInfo->sharpness, pInput, pCurBuff);
	pCurBuff += TAG_SIZE;

#if 0
	//29. Subject Distance Range / 0xA40C
	//__ExifWriteSHORTTag(SUBJECT_DIST_RANGE_ID, 0, pInput, pCurBuff);
	//pCurBuff += TAG_SIZE;
#endif

	// 20090326 tube / enabled tags is 25 now
}


void ExifCreator::__ExifWriteGPSInfoTag(unsigned short tagID, Rational* pValue, unsigned int count, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	unsigned int numerator;
	unsigned int denominator=0;

	__ExifWriteSHORT_LE(pCurBuff, tagID);//tag
	pCurBuff +=2;
   __ExifWriteSHORT_LE(pCurBuff, (unsigned short)TYPE_RATIONAL);//type
	pCurBuff +=2;
   __ExifWriteLONG_LE(pCurBuff, (unsigned int)count);//count
	pCurBuff +=4;
   __ExifWriteLONG_LE(pCurBuff, *pOffset);//ValueOffset
	pCurBuff +=4;

	__ExifWriteRATIONAL(pInput + (*pOffset), pValue[0].numerator, pValue[0].denominator); //value
	if(count == 3)
	{
		numerator = (int)30;
		__ExifWriteRATIONAL(pInput + (*pOffset+8), pValue[1].numerator, pValue[1].denominator); //value
		numerator = (int)15;
		__ExifWriteRATIONAL(pInput + (*pOffset+16), pValue[2].numerator, pValue[2].denominator); //value
	}

   *pOffset += 8*count;
}

void ExifCreator::__ExifWriteRATIONALTag(unsigned short tagID, unsigned int numerator, unsigned int denominator, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	__ExifWriteSHORT_LE(pCurBuff, tagID);//tag
	pCurBuff +=2;
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)TYPE_RATIONAL);//type
	pCurBuff +=2;

	__ExifWriteRATIONAL(pInput + (*pOffset), numerator, denominator); //value

	__ExifWriteLONG_LE(pCurBuff, (unsigned int)1);//count
	pCurBuff +=4;
	__ExifWriteLONG_LE(pCurBuff, *pOffset);//ValueOffset
	pCurBuff +=4;
	
	*pOffset += 8;
}


void ExifCreator::__ExifWriteSRATIONALTag(unsigned short tagID, int numerator, int denominator, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	__ExifWriteSHORT_LE(pCurBuff, tagID);//tag
	pCurBuff +=2;
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)TYPE_SRATIONAL);//type
	pCurBuff +=2;

	__ExifWriteSRATIONAL(pInput + (*pOffset), numerator, denominator); //value

	__ExifWriteLONG_LE(pCurBuff, (unsigned int)1);//count
	pCurBuff +=4;
	__ExifWriteLONG_LE(pCurBuff, *pOffset);//ValueOffset
	pCurBuff +=4;
	
	*pOffset += 8;
}



void ExifCreator::__ExifWriteLONGTag(unsigned short tagID, unsigned int value, unsigned char* pInput, unsigned char* pCurBuff)
{
	__ExifWriteSHORT_LE(pCurBuff, tagID);//tag
	pCurBuff +=2;
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)TYPE_LONG);//type
	pCurBuff +=2;
	__ExifWriteLONG_LE(pCurBuff, (unsigned int)1);//count
	pCurBuff +=4;
	__ExifWriteLONG_LE(pCurBuff, value);//ValueOffset
	pCurBuff +=4;
}



void ExifCreator::__ExifWriteSHORTTag(unsigned short tagID, unsigned short value, unsigned char* pInput, unsigned char* pCurBuff )
{
	__ExifWriteSHORT_LE(pCurBuff, tagID);//tag
	pCurBuff +=2;
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)TYPE_SHORT);//type
	pCurBuff +=2;
	__ExifWriteLONG_LE(pCurBuff, (unsigned int)1);//count
	pCurBuff +=4;
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)value);//ValueOffset
	pCurBuff +=2;
	*pCurBuff++ = 0;
	*pCurBuff++ = 0;
}


void ExifCreator::__ExifWriteBYTESTag(unsigned short tagID, unsigned char *pValue, unsigned int count, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	__ExifWriteSHORT_LE(pCurBuff, tagID);//tag
	pCurBuff +=2;
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)TYPE_BYTE);//type
	pCurBuff +=2;
	__ExifWriteLONG_LE(pCurBuff, (unsigned int)count);//count
	pCurBuff +=4;
   if(count != 0) 
	{
		if(count <= 4) {
			__ExifWriteASCII(pCurBuff, pValue, count); //valueOffset(value)
			pCurBuff +=4;
		}
		else {					
			__ExifWriteLONG_LE(pCurBuff, *pOffset);//ValueOffset
			pCurBuff +=4;
			__ExifWriteASCII(pInput + (*pOffset), pValue, count); //value
			*pOffset +=count;
		}
	}
	else 
	{
		__ExifWriteLONG_LE(pCurBuff, 0);//ValueOffset
		pCurBuff +=4;
	}
}


void ExifCreator::__ExifWriteASCIITag(unsigned short tagID, unsigned char* pString, unsigned int count, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	__ExifWriteSHORT_LE(pCurBuff, tagID);//tag
	pCurBuff +=2;
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)TYPE_ASCII);//type
	pCurBuff +=2;

	if(pString != 0) 
	{
		if(count <= 4) {
			__ExifWriteLONG_LE(pCurBuff, count);//count
			pCurBuff +=4;
			
			__ExifWriteASCII(pCurBuff, pString, count); //valueOffset(value)
			pCurBuff +=4;
			
		}
		else {					
			__ExifWriteLONG_LE(pCurBuff, count);//count
			pCurBuff +=4;
			__ExifWriteLONG_LE(pCurBuff, *pOffset);//ValueOffset
			pCurBuff +=4;
			
			__ExifWriteASCII(pInput + (*pOffset), pString, count); //value

			*pOffset +=count;
		}
	}
	else 
	{
		count = 0;
		__ExifWriteLONG_LE(pCurBuff, count);//count
		pCurBuff +=4;
		__ExifWriteLONG_LE(pCurBuff, 0);//ValueOffset
		pCurBuff +=4;
	}
}


void ExifCreator::__ExifWriteUNDEFINEDTag(unsigned short tagID, unsigned char* pValue, unsigned int count, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset)
{
	__ExifWriteSHORT_LE(pCurBuff, tagID);//tag
	pCurBuff +=2;
	__ExifWriteSHORT_LE(pCurBuff, (unsigned short)TYPE_UNDEFINED);//type
	pCurBuff +=2;

	if(pValue != 0) 
	{
		if(count <= 4) {
			__ExifWriteLONG_LE(pCurBuff, count);//count
			pCurBuff +=4;
		
			__ExifWriteUNDEFINED(pCurBuff, pValue, count); //value offset
			pCurBuff +=4;
			
		}
		else {			
			__ExifWriteLONG_LE(pCurBuff, count);//count
			pCurBuff +=4;
			__ExifWriteLONG_LE(pCurBuff, *pOffset);//ValueOffset
			pCurBuff +=4;
			
			__ExifWriteUNDEFINED(pInput + (*pOffset), pValue, count); //value			
			*pOffset +=count;
		}
	}
	else 
	{
		count = 0;

		__ExifWriteLONG_LE(pCurBuff, count);//count
		pCurBuff +=4;
		__ExifWriteLONG_LE(pCurBuff, 0);//ValueOffset
		pCurBuff +=4;
	}
}



void ExifCreator::__ExifWriteRATIONAL(unsigned char* pBuff, unsigned int numerator, unsigned int denominator)
{
	__ExifWriteLONG_LE(pBuff, numerator);
	__ExifWriteLONG_LE(pBuff+4, denominator);
}


void ExifCreator::__ExifWriteSRATIONAL(unsigned char* pBuff, int numerator, int denominator)
{
	__ExifWriteSLONG_LE(pBuff, numerator);
	__ExifWriteSLONG_LE(pBuff+4, denominator);
}


void ExifCreator::__ExifWriteLONG_LE(unsigned char* pBuff, unsigned int value)
{
	*pBuff = (value & 0xff);
	value = value>>8;
	*(pBuff + 1) = (value & 0xff);
	value = value>>8;
	*(pBuff + 2) = (value & 0xff);
	value = value>>8;
	*(pBuff + 3) = (value & 0xff);
}


void ExifCreator::__ExifWriteSLONG_LE(unsigned char* pBuff, int value)
{
	*pBuff = (value & 0xff);
	value = value>>8;
	*(pBuff + 1) = (value & 0xff);
	value = value>>8;
	*(pBuff + 2) = (value & 0xff);
	value = value>>8;
	*(pBuff + 3) = (value & 0xff);
}


void ExifCreator::__ExifWriteSHORT_LE(unsigned char* pBuff, unsigned short value)
{
	*pBuff = (value & 0xff);
	value = value>>8;
	*(pBuff + 1) = (value & 0xff);
}


void ExifCreator::__ExifWriteASCII(unsigned char* pBuff, unsigned char* pString, unsigned int count)
{
	ExifMemcpy(pBuff, pString, count);
}



void ExifCreator::__ExifWriteUNDEFINED(unsigned char* pBuff, unsigned char* pValue, unsigned int count)
{
	ExifMemcpy(pBuff, pValue, count);
}




unsigned int ExifCreator::__ExifGetASCIILength(unsigned char* pString)
{
	unsigned int count = 0;
	if(pString) {
		while(pString[count] != 0) {
			count++;
		}
	}
	else {
		count = 0;
	}

	return count + 1;
}


}; // namespace android


