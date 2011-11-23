#ifndef _COLORCONVERT_H_
#define _COLORCONVERT_H_

#define BI_RGB 0

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

int yuv2_to_rgb(uint8_t *ptrIn, uint8_t *ptrOut, int image_size );

#endif