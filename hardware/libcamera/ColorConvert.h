#ifndef _COLORCONVERT_H_
#define _COLORCONVERT_H_

#include <stdint.h>


//#define LOGV printf
//#define LOGE printf

#define DBG printf("%s: line %d\n", __func__, __LINE__);
//#define DBG



#define RGB888_BPP       24
#define RGB888_BPP      24
#define UYV422_BPP      16
#define UYV444_BPP      24

#define BI_RGB          0   //for bmp header


#define INPLACE 1


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


enum {
    UNSUPPORTED,
// write imgage to raw after color conversion
    YUV444,
    UYV2,
    YUV2,
    NV21,
    RGB888
};

enum {
 //write image to bmp in rgb888
    BMP,
//dump yuv444 buffer
    RAW,
//dump input buffer
    SOURCE,
//dump output buffer
    OUTPUT
};

enum {
    ROTATE_90,
    ROTATE_180,
    ROTATE_270,
    ROTATE_360,
    ROTATE_RIGHT,
    ROTALTE_LEFT
};

enum {
    FLIP_HORIZONTAL,
    FLIP_VERTICAL
};



typedef struct{
    uint8_t y;
    uint8_t u;
    uint8_t v;
} __attribute__ ((packed))YUV;


//unpacked YUV444 image
class CYUVImage {
    public:
        CYUVImage(int width, int heigth):mHeigth(heigth),mWidth(width)
            { mpBuffer = (YUV*) new YUV[ mWidth*mHeigth];}
        virtual ~CYUVImage() {delete []mpBuffer;}
        int geHeight() const {return mHeigth;}
        int getWidth() const {return mWidth;}
        int getSize() const {return mWidth*mHeigth;}
        int getBufferSize() const {return mWidth*mHeigth*sizeof(YUV);}
        YUV *getBuffer() const {return mpBuffer;}
        YUV getPixel(int x, int y)
        {
            if(x<mWidth && y<mHeigth)
                return mpBuffer[(y * mWidth) + x];
            else
                return YuvFail;
        }
        void setPixel(int x, int y, YUV value)
        {
            if(x<mWidth && y<mHeigth)
                mpBuffer[(y * mWidth) + x] = value;
        }
        void dump(char* name)
        {
           printf("dump: %s\n", name);
           printf("     mWidth: %d\n", mWidth);
           printf("     mHeigth: %d\n", mHeigth);
           printf("     mpBuffer: %p\n", (void*)mpBuffer);
        }
        CYUVImage & operator=(const CYUVImage & rhs)
        {
            if(this==&rhs)
                return *this;

            mHeigth = rhs.geHeight();
            mWidth = rhs.getWidth();
            //make new buffer if neccesary
            if(rhs.getSize() != getSize())
            {
                delete []mpBuffer;
                mpBuffer = (YUV*) new YUV[ mWidth*mHeigth];
            }
            //copy picture data
            if(mpBuffer)
                memcpy( mpBuffer, rhs.getBuffer(), getSize()*sizeof(YUV));

            return *this;
        }
    private:
        int mHeigth;
        int mWidth;
        YUV *mpBuffer;
        YUV YuvFail;

};



class CColorConvert {
    public:

        CColorConvert(uint8_t *ptrIn,  int image_width, int image_height, int inColorformat);
        virtual ~CColorConvert();

// --- image processing
        // flipping, rotation
        void rotateImage( int angle, int direction = ROTALTE_LEFT);
        void flipImage(int flip);

// --- format conversion
        // create output buffer and return RGB888 image
        uint8_t *makeRGB888(size_t *buffersize);
         // create output buffer and return UYV2 image
        uint8_t* makeUYV2(size_t *buffersize, int inplace=0);

// ---  get / set
        int getWidth() {return pYuvImage->getWidth();}
        int geHeight() {return pYuvImage->geHeight();}

// --- file access
        int writeFile(char *filename, int format);


    private:

        //output buffer for format conversion yuv444->xxx
        uint8_t *getOutputBuffer(size_t bufferSize);

        int bufWriteBmp(FILE* fOut, int mRgbBpp);   //create RGB88 output buffer and write to bmp
        int dumpBuffer(FILE* fOut, uint8_t *pBuf, size_t size); //write desired buffer to file

        void rotateLeftImage(int angle);
        int fillYUVImage(int inputFormat);  // create main YUV444 buffer
        int getBitsPerPixel(int format);             // return bits per pixel for format

        uint8_t *mpInBuffer;             // pointer to input data
        uint8_t *mpOutBuffer;            // output data if conversion was done

        // holding full yuv444 image 24bit/pixel
        // all image processing is done in this
        CYUVImage* pYuvImage;

        size_t mInBufferSize;           // input buffer size in bytes
        size_t mOutBufferSize;          // output buffer size in bytes, 0 if not jet created
};



#endif