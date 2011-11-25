
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

#include "color_convert.h"
 
#define DUMP_PATH "/dump"



int dum_to_file(char *filename, uint8_t *start, int size)
{
    FILE* fIn = NULL;	
    char path[255];
    
    sprintf(path,"%s/%s",DUMP_PATH, filename);  
    fIn = fopen(path, "w");
    if ( fIn == NULL ) 	  
    { 		 
        LOGE("Error: failed to open the file for writing\n");
        return 1;            
    }	
    fwrite(start, 1, size, fIn);	
    fclose(fIn);
 
    return 0;
}


int write_to_bmp(char *filename, uint8_t *pRgbData, int image_width, int image_height, int rgbsize, int bpp)
{
    struct bmpfile_magic magic = { {'B', 'M'} };
    struct bmpfile_header file_header;
    struct bmpinfo_header info_header;
    FILE* fIn = NULL;	
    char path[255];
    int rgb_offset=sizeof(info_header)+sizeof(file_header)+sizeof(magic);
    int rgb_size = image_width * image_height * (bpp>>3);
    int filesize = rgb_size + rgb_offset; 
        
    sprintf(path,"%s/%s",DUMP_PATH, filename);  
    fIn = fopen(path, "w");
    if ( fIn == NULL ) 	  
    { 		 
        LOGE("Error: failed to open the file for writing\n");
        return 1;            
    }

    memset(&info_header, 0, sizeof(info_header));
    memset(&file_header, 0, sizeof(file_header));
 
    file_header.filesz=filesize;
    file_header.bmp_offset = rgb_offset;
    
    info_header.header_sz = sizeof(info_header);
    info_header.width = image_width;
    info_header.height = image_height;
    info_header.nplanes = 1;
    info_header.bitspp = bpp;
    info_header.compress_type = BI_RGB;
    info_header.bmp_bytesz = 0;
    info_header.hres = 0;
    info_header.vres = 0;
    info_header.ncolors = 0; 
    info_header.nimpcolors = 0;
     
    fwrite(&magic, 1, sizeof(magic), fIn);	
    fwrite(&file_header, 1, sizeof(file_header), fIn);	
    fwrite(&info_header, 1, sizeof(info_header), fIn);	
    fwrite(pRgbData, 1, rgbsize, fIn);	

    fclose(fIn);
 
    return 0;
}

static uint8_t clip(int val) {
	if(val < 0) return 0;
	if(val > 255) return 255;
	return val;
}

#if 0
/*


I'm doing RGB24 90 degrees rotation with algorithm:

if (pInType->subtype==MEDIASUBTYPE_RGB24)
        {
            int m_iFrameHeight3 = m_iFrameHeight*3;
                int j = 0;
                for (int y=m_iFrameHeight; y>0; y--)
                {
                    int y3 = y*3;
                    for (int x=0; x<m_iFrameWidth; x++)
                    {
                        memcpy(&pOutBuffer[x*m_iFrameHeight3+y3], &pInpBuffer[j], 3);
                        j+=3;
                    }
                }
        }


If resolution is 320x240 it wokrs fine, but then i'm changing to 640x480 it crachs. Why?
*/
//640x480
int rotate_rgb_left_90(uint8_t *ptrIn, uint8_t *ptrOut, int width, int height)
{
    long x;
    long y;
    long width = 100;
    long height = 100;
    char *ImageOriginal;
    char *ImageRotated;
    char temp;

    for (y = 0; y < height; y++)
    {
        for (x = 0; x < width; x++)
        {
            temp = GetOriginal (x, y);
            PutRotated (y, x, temp);
        }
    }

    char GetOriginal (long x, long y)
    {
        return ImageOriginal [(y * height) + x];
    }

    void PutRotated (long x, long y, char value)
    {
        ImageRotated [(y * height) + x] = value;
    }
}
#endif
// U,Y,V,Y: [ u0 y0 v0 y1 ][ u2 y2 v2 y3 ][ u4 y4 v4 y5 ]
int uyvy_to_rgb(uint8_t *ptrIn, uint8_t *ptrOut, int image_size )
{
    uint8_t *ptrEnd = ptrOut + image_size;
    
    while(ptrOut<ptrEnd)
    {
        int u0 = ptrIn[0];
        int y0 = ptrIn[1];
        int v0 = ptrIn[2];
        int y1 = ptrIn[3];
        ptrIn += 4;
        int c = y0 - 16;
        int d = u0 - 128;
        int e = v0 - 128;
        ptrOut[0] = clip(( 298 * c + 516 * d + 128) >> 8);              // blue
        ptrOut[1] = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);    // green
        ptrOut[2] = clip(( 298 * c + 409 * e + 128) >> 8);              // red
        c = y1 - 16;
        ptrOut[3] = clip(( 298 * c + 516 * d + 128) >> 8);              // blue
        ptrOut[4] = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);    // green
        ptrOut[5] = clip(( 298 * c + 409 * e + 128) >> 8);              // red
        ptrOut += 6;
    }
    return 0;
}
// U,Y,V,Y: [ u0 y0 v0 y1 ][ u2 y2 v2 y3 ][ u4 y4 v4 y5 ]
//         [ y0 u0 v0 ][ y1 u1 v1 ][ y2 u2 u2 ][ y3 u3 v3 ][ y4  u4 v4 ][  y5 u5 v5]
int uyvy_to_yuv444(uint8_t *ptrIn, uint8_t *ptrOut, int image_size )
{

    return 0;
}
/*
struct yuv_buffer {
    int   y_width;      
    int   y_height;     
    int   y_stride;     
    int   uv_width;     
    int   uv_height;    
    int   uv_stride;    
    unsigned char *y;   
    unsigned char *u;   
    unsigned char *v;   
} ;

int init_yuv422(uint8_t *ptrIn, int width, int heigth)
{
    struct yuv_buffer yuv422;
    struct yuv_buffer *pYuv=&yuv422;
    
    pYuv->y_width = width;
    pYuv->y_height = heigth;
    pYuv->y_stride = pYuv->y_width;
    
    pYuv->y_width = width;
    pYuv->y_height = heigth;
    pYuv->y_stride = pYuv->y_width;


}
*/
//YUY2: [ y0 u0 y1 v0 ][ y2 u2 y3 v2] {y4 u4 y5 v4] 
int yuv2_to_rgb(uint8_t *ptrIn, uint8_t *ptrOut, int image_size )
{
    uint8_t *ptrEnd = ptrOut + image_size;
    
    while(ptrOut<ptrEnd)
    {
        int y0 = ptrIn[0];
        int u0 = ptrIn[1];
        int y1 = ptrIn[2];
        int v0 = ptrIn[3];
        ptrIn += 4;
        int c = y0 - 16;
        int d = u0 - 128;
        int e = v0 - 128;
        ptrOut[0] = clip(( 298 * c + 516 * d + 128) >> 8); // blue
        ptrOut[1] = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8); // green
        ptrOut[2] = clip(( 298 * c + 409 * e + 128) >> 8); // red
        c = y1 - 16;
        ptrOut[3] = clip(( 298 * c + 516 * d + 128) >> 8); // blue
        ptrOut[4] = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8); // green
        ptrOut[5] = clip(( 298 * c + 409 * e + 128) >> 8); // red
        ptrOut += 6;
    }
    return 0;
}

