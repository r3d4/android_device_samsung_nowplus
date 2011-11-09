#ifndef ANDROID_EXIFCREATOR_H
#define ANDROID_EXIFCREATOR_H

#include <utils/Errors.h>

#include "Exif.h"

namespace android {

//exif Type
#define	TYPE_BYTE		1
#define	TYPE_ASCII		2
#define	TYPE_SHORT		3
#define TYPE_LONG		4
#define TYPE_RATIONAL		5
#define TYPE_UNDEFINED		7
#define TYPE_SLONG		9
#define TYPE_SRATIONAL		10

//TIFF IFD
#define	MAKE_ID				0x010f
#define	MODEL_ID			0x0110
#define	ORIENTATION_ID			0x0112
#define	SOFTWARE_ID			0x0131
#define	DATE_TIME_ID			0x0132
#define	YCBCR_POSITIONING_ID		0x0213

#define	IMAGE_WIDTH_ID			0x0100
#define	IMAGE_LENGTH_ID			0x0101
#define COMPRESSION_SCHEME_ID		0x0102
#define COMPRESSION_ID			0x0103
#define	X_RESOLUTION_ID			0x011A
#define	Y_RESOLUTION_ID			0x011B
#define	RESOLUTION_UNIT_ID		0x0128
#define JFIF_ID					0x0201
#define JFIF_LENGTH_ID			0x0202


//EXIF IFD
#define	EXPOSURE_TIME_ID		0x829A
#define	FNUMBER_ID			0x829D 
#define EXPOSURE_PROGRAM_ID		0x8822		
#define	ISO_SPEED_RATING_ID		0x8827 
#define EXIF_VERSION_ID			0x9000
#define DATE_TIME_ORG_ID		0x9003
#define DATE_TIME_DIGITIZE_ID		0x9004
#define	SHUTTER_SPEED_ID		0x9201 
#define	APERTURE_VALUE_ID		0x9202	
#define	BRIGHTNESS_VALUE_ID		0x9203
#define	EXPOSURE_BIAS_ID		0x9204
#define	MAX_APERTURE_VALUE_ID		0x9205	
#define	METERING_MODE_ID		0x9207 
#define FLASH_ID			0x9209
#define FOCAL_LENGTH_ID			0x920A 
#define USER_COMMENT_ID			0x9286
#define COLOR_SPACE_ID			0xA001
#define PIXEL_X_DIMENSION_ID		0xA002
#define PIXEL_Y_DIMENSION_ID		0xA003
#define EXPOSURE_MODE_ID		0xA402
#define WHITE_BALANCE_ID		0xA403
#define SCENCE_CAPTURE_TYPE_ID		0xA406

#define	EXIF_IFD_ID			0x8769
#define	GPS_IFD_ID			0x8825
#define COMPONENTS_CONIFG_ID		0x9101
#define FLASH_PIX_VERSION_ID		0xA000
#define CUSTOM_RENDER_ID		0xA401
#define DIGITAL_ZOOM_RATIO_ID		0xA404
#define CONTRAST_ID			0xA408
#define SATURATION_ID			0xA409
#define SHARPNESS_ID			0xA40A
#define SUBJECT_DIST_RANGE_ID		0xA40C

#define	TAG_SIZE			12
#define	NUMBER_SIZE			2
#define	OFFSET_SIZE			4

#define	NUM_0TH_IFD			10 // -1 by GPS
#define	NUM_0TH_IFD_WO_GPS		11 // +1 by GPS

#define  NUM_GPS_IDF       		10 


#define	NUM_EXIF_IFD			25 // 22 // 12 // 11 // 14 // 20

#define	NUM_1TH_IFD			10 // 7


class ExifCreator
{
public:
	ExifCreator();
	~ExifCreator();

        unsigned int
        ExifCreate(unsigned char* dest, ExifInfoStructure *pSetExifInfo);

        unsigned int
        ExifCreate_wo_GPS(unsigned char* dest, ExifInfoStructure *pSetExifInfo,int flag);

        void *
        ExifMemcpy(void *dest, void *src, unsigned int count);

        unsigned int
        __ExifCreate(unsigned char* pInput, ExifInfoStructure *pSetExifInfo);

        unsigned int
        __ExifCreate_wo_GPS(unsigned char* pInput, ExifInfoStructure *pSetExifInfo,int flag);

        void
        __ExifWriteGPSInfoTag(unsigned short tagID, Rational* pValue, unsigned int count, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        void
        __ExifWriteRATIONALTag(unsigned short tagID, unsigned int numerator, unsigned int denominator, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        void
        __ExifWriteSRATIONALTag(unsigned short tagID, int numerator, int denominator, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        void
        __ExifWriteLONGTag(unsigned short tagID, unsigned int value, unsigned char* pInput, unsigned char* pCurBuff);

        void
        __ExifWriteSHORTTag(unsigned short tagID, unsigned short value, unsigned char* pInput, unsigned char* pCurBuff );

        void
        __ExifWriteBYTESTag(unsigned short tagID, unsigned char *pValue, unsigned int count, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        void
        __ExifWriteASCIITag(unsigned short tagID, unsigned char* pString, unsigned int count, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        void
        __ExifWriteUNDEFINEDTag(unsigned short tagID, unsigned char* pValue, unsigned int count, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        void
        __ExifWriteIFDTag(unsigned short tagID, ExifInfoStructure *pSetExifInfo, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        void
        __ExifWriteLONG_LE(unsigned char* pBuff, unsigned int value);

        void
        __ExifWriteSLONG_LE(unsigned char* pBuff, int value);

        void
        __ExifWriteSHORT_LE(unsigned char* pBuff, unsigned short value);

        void
        __ExifWriteASCII(unsigned char* pBuff, unsigned char* pString, unsigned int count);

        void
        __ExifWriteUNDEFINED(unsigned char* pBuff, unsigned char* pValue, unsigned int count);

        void
        __ExifWriteRATIONAL(unsigned char* pBuff, unsigned int numerator, unsigned int denominator);

        void
        __ExifWriteSRATIONAL(unsigned char* pBuff, int numerator, int denominator);

        void
        __ExifWriteExifIFD(ExifInfoStructure *pSetExifInfo, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        void
        __ExifWriteGPDIFD(ExifInfoStructure *pSetExifInfo, unsigned char* pInput, unsigned char* pCurBuff, unsigned int *pOffset);

        unsigned int
        __ExifGetASCIILength(unsigned char* pString);

	
};

};// namespace android
#endif
