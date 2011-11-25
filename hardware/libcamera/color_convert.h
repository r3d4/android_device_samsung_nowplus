#ifndef _COLORCONVERT_H_
#define _COLORCONVERT_H_

#define RGB_BIT_PER_PIXEL     24
#define UYVY_BIT_PER_PIXEL    16

#define BI_RGB 0

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

struct bmpfile_magic {
  unsigned char magic[2];
};


struct bmpfile_header {
  uint32_t filesz;
  uint16_t creator1;
  uint16_t creator2;
  uint32_t bmp_offset;
};

struct bmpinfo_header{
  uint32_t header_sz;
  int32_t width;
  int32_t height;
  uint16_t nplanes;
  uint16_t bitspp;
  uint32_t compress_type;
  uint32_t bmp_bytesz;
  int32_t hres;
  int32_t vres;
  uint32_t ncolors;
  uint32_t nimpcolors;
};


int write_to_bmp(char *filename, uint8_t *pRgbData, int image_width, int image_height, int rgbsize, int bpp);
int dum_to_file(char *filename, uint8_t *start, int size);
int uyvy_to_rgb(uint8_t *ptrIn, uint8_t *ptrOut, int image_size );
int yuv2_to_rgb(uint8_t *ptrIn, uint8_t *ptrOut, int image_size );





#endif