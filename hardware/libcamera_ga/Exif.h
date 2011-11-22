
#ifndef ANDROID_EXIF_H
#define ANDROID_EXIF_H

#define MAKER_NAME_LENGTH			20
#define MODEL_NAME_LENGTH			20
#define SOFTWARE_VERSION_LENGTH			5 
#define DATE_TIME_FIELD_LENGTH			50

typedef struct 
{
	unsigned int numerator;
	unsigned int denominator;
}Rational;

typedef struct 
{
	int numerator;
	int denominator;
}SRational;

typedef struct
{
	bool hasGps;
	bool hasThumbnail;
	
	unsigned char 		maker[MAKER_NAME_LENGTH];
	unsigned char		model[MODEL_NAME_LENGTH];
	unsigned char 		software[SOFTWARE_VERSION_LENGTH];

	unsigned int		imageWidth;
	unsigned int		imageHeight;
	
	unsigned int			pixelXDimension;		
	unsigned int			pixelYDimension;

	unsigned char			dateTimeOriginal[DATE_TIME_FIELD_LENGTH];
	unsigned char			dateTimeDigitized[DATE_TIME_FIELD_LENGTH];
	unsigned char			dateTime[DATE_TIME_FIELD_LENGTH];

	unsigned int			thumbImageWidth;
	unsigned int			thumbImageHeight;	
	unsigned char 			*thumbStream;    
	unsigned int 			thumbSize;    
	
	unsigned short			exposureProgram;
	unsigned short			meteringMode;	
	unsigned short			exposureMode;
	unsigned short			whiteBalance;
	unsigned short			saturation;
	unsigned short			sharpness;
	unsigned short			contrast;
	Rational			fNumber;
	Rational			maxAperture;
	Rational			focalLength;
	unsigned short			isoSpeedRating;	

	Rational			exposureTime;	
	SRational			brightness;
	SRational			shutterSpeed;

	unsigned short			iso;	
	unsigned short			flash;	

	int 				orientation;
	Rational			aperture;
	SRational			exposureBias;
	unsigned short			sceneCaptureType;
		    
	unsigned char		GPSLatitudeRef[2];
	Rational			GPSLatitude[3];	
	unsigned char		GPSLongitudeRef[2];
	Rational			GPSLongitude[3];	
	unsigned char		GPSAltitudeRef;
	Rational			GPSAltitude[1];    
	Rational			GPSTimestamp[3];
	unsigned char		GPSProcessingMethod[150];
	unsigned char		GPSDatestamp[11];
}ExifInfoStructure;
#endif
