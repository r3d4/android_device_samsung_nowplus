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


#include "ColorConvert.h"





CColorConvert::CColorConvert(uint8_t *ptrIn,  int image_width, int image_height, int inColorformat):
    mpOutBuffer(NULL),
    pYuvImage(NULL),
    mOutBufferSize(0)
{
DBG
    int mInBpp;

    mInBpp = getBitsPerPixel(inColorformat);

     if(!mInBpp)
     {
            inColorformat=UNSUPPORTED;
            mInBpp = 0;
            LOGE("unsupported colorformat: %d\n", inColorformat);
            return;
    }

    mpInBuffer = ptrIn;
    mInBufferSize= image_width * image_height * (mInBpp>>3);

    pYuvImage = new CYUVImage(image_width, image_height) ;
    fillYUVImage(inColorformat);
}


CColorConvert::~CColorConvert()
{
    if(pYuvImage)
        delete pYuvImage;

    if (mpOutBuffer)
        delete [] mpOutBuffer;
}

int CColorConvert::getBitsPerPixel(int format)
{
    switch(format)
    {
        case YUV444:
        case RGB888:
            return 24;
        case UYV2:
        case YUV2:
            return 16;
        case NV21:
            return 12;
        default:
            return 16;
    }
}

uint8_t *CColorConvert::getOutputBuffer(size_t bufferSize)
{
    // if same ssize reuse allocated memory
    if(mpOutBuffer)
    {
        if(bufferSize == mOutBufferSize)
            return mpOutBuffer;
        else
            delete [] mpOutBuffer;
    }

    mpOutBuffer = (uint8_t*) new uint8_t[bufferSize];
    if(mpOutBuffer == NULL)
    {
        LOGV("getOutputBuffer: cant get memory");
        mOutBufferSize = 0;
        return NULL;
    }
    //memset(mpOutBuffer, 0, sizeof(uint8_t)*bufferSize);
    mOutBufferSize = bufferSize;
    return mpOutBuffer;
}


void CColorConvert::rotateImage( int angle, int direction)
{
    if(direction == ROTATE_RIGHT)
    {
        if(angle==ROTATE_90)
            angle=ROTATE_270;
        else
            if(angle==ROTATE_270) 
                angle=ROTATE_90;
    }
    
    rotateLeftImage(angle);
}

void CColorConvert::rotateLeftImage(int angle)
{
    int x, y;
    int w = pYuvImage->getWidth();
    int h = pYuvImage->geHeight();
    CYUVImage *pUVYRotate;


    if(angle==ROTATE_180)
        pUVYRotate = new CYUVImage( w, h) ;
    else
        pUVYRotate = new CYUVImage( h, w) ;

    if(pUVYRotate==NULL)
    {
        LOGV("rotateLeftImage: cant get memory!");
        return;
    }

    switch (angle)
    {
        case ROTATE_90:
            for(x=0 ; x<w ; x++)
                for(y=0 ; y<h ; y++)
                    pUVYRotate->setPixel(y, x , pYuvImage->getPixel(w-x-1, y));
        break;
        case ROTATE_180:
            for(x=0 ; x<w ; x++)
                for(y=0 ; y<h ; y++)
                    pUVYRotate->setPixel(x, y , pYuvImage->getPixel(w-x-1, h-y-1));
        break;
        case ROTATE_270:
            for(x=0 ; x<w ; x++)
                for(y=0 ; y<h ; y++)
                    pUVYRotate->setPixel(y, x , pYuvImage->getPixel(x, h-y-1));
        break;

    }


    //replace image with rotatet one
    CYUVImage* pOldImage = pYuvImage;
    pYuvImage = pUVYRotate;
    delete pOldImage;
}

 void CColorConvert::flipImage(int flip)
{
    int x, y;
    int w = pYuvImage->getWidth();
    int h = pYuvImage->geHeight();
    CYUVImage *pUVYFlip = new CYUVImage( w, h) ;

    if(pUVYFlip==NULL)
    {
        LOGV("rotateLeftImage: cant get memory!");
        return;
    }

    if(flip == FLIP_HORIZONTAL)
    {
        for(x=0 ; x<w ; x++)
            for(y=0 ; y<h ; y++)
                pUVYFlip->setPixel(w-x-1, y, pYuvImage->getPixel(x, y));
    }
    else
    {
        for(x=0 ; x<w ; x++)
            for(y=0 ; y<h ; y++)
                pUVYFlip->setPixel(x, h-y-1, pYuvImage->getPixel(x, y));
    }

    //replace image with flipped one
    CYUVImage* pOldImage = pYuvImage;
    pYuvImage = pUVYFlip;
    delete pOldImage;
}

int CColorConvert::fillYUVImage(int inputFormat)
{
    if(!pYuvImage)
        return 1;

    YUV *pBuffer = pYuvImage->getBuffer();
    int w = pYuvImage->getWidth();
    int h = pYuvImage->geHeight();
    switch(inputFormat)
    {
        case UYV2:
        {
            for(int i=0; i<w*h/2; i++)
            {
                pBuffer[i*2].u    = mpInBuffer[(i*4)];
                pBuffer[i*2+1].u  = mpInBuffer[(i*4)];
                pBuffer[i*2].v    = mpInBuffer[(i*4)+2];
                pBuffer[i*2+1].v  = mpInBuffer[(i*4)+2];
                pBuffer[i*2].y    = mpInBuffer[(i*4)+1];
                pBuffer[i*2+1].y  = mpInBuffer[(i*4)+3];
            }
        }
        break;
        case YUV2:
        {
            for(int i=0; i<w*h/2; i++)
            {
                pBuffer[i*2].y    = mpInBuffer[(i*4)];
                pBuffer[i*2+1].y  = mpInBuffer[(i*4)];
                pBuffer[i*2].u    = mpInBuffer[(i*4)+2];
                pBuffer[i*2+1].u  = mpInBuffer[(i*4)+2];
                pBuffer[i*2].v    = mpInBuffer[(i*4)+1];
                pBuffer[i*2+1].v  = mpInBuffer[(i*4)+3];
            }
        }
        break;
        case NV21:
        {   // img 6x4:
            // |-----------------------
            // | Y0  Y1  Y2  Y3  Y4  Y5
            // | Y6  Y7  Y8  Y9  Y10 Y11
            // | Y12 Y13 Y14 Y15 Y16 Y17
            // | Y18 Y19 Y20 Y21 Y22 Y23
            // |-----------------
            // | U0  V0  U1  V1  U2  V2
            // | U3  V3  U4  V4  U5  V5
            // |-----------------
            //get full res luma byte (y)
            int i=0;
            int j=0;
            for(; i<w*h; i++)
                pBuffer[i].y =   mpInBuffer[i];

            // get interleaved chroma
            // U0 V0 belogns to Y0, Y1, Y6, Y7

            while(j<w*h/2)
            {
                int u = mpInBuffer[i];
                int v = mpInBuffer[i+1];

                pBuffer[j].u      = u;
                pBuffer[j+1].u    = u;
                pBuffer[j+w].u    = u;
                pBuffer[j+w+1].u  = u;

                pBuffer[j].v      = v;
                pBuffer[j+1].v    = v;
                pBuffer[j+w].v    = v;
                pBuffer[j+w+1].v  = v;

                i+=2;
                j+=2;
                if(j%w==0) j+=w;    //skip every 2nd line
            }
        }
        break;
    }

return 0;
}


int CColorConvert::dumpBuffer(FILE* fOut, uint8_t *pBuf, size_t size)
{

    if(fwrite(pBuf, 1, size, fOut) != size)
        return 1;
    else
        return 0;
}


int CColorConvert::bufWriteBmp(FILE* fOut, int mRgbBpp )
{
    uint8_t* pRgbData = mpOutBuffer;
    int rgbSize = mOutBufferSize;
    struct bmpfile_magic magic = { {'B', 'M'} };
    struct bmpfile_header file_header;
    struct bmpinfo_header info_header;

    int rgb_offset=sizeof(info_header)+sizeof(file_header)+sizeof(magic);
    int filesize = rgbSize + rgb_offset;


    // create bmp header
    memset(&info_header, 0, sizeof(info_header));
    memset(&file_header, 0, sizeof(file_header));

    file_header.filesz=filesize;
    file_header.bmp_offset = rgb_offset;

    info_header.header_sz = sizeof(info_header);
    info_header.width = pYuvImage->getWidth();
    info_header.height = -(pYuvImage->geHeight());  // store first line top
    info_header.nplanes = 1;
    info_header.bitspp = mRgbBpp;
    info_header.compress_type = BI_RGB;
    info_header.bmp_bytesz = 0;
    info_header.hres = 0;
    info_header.vres = 0;
    info_header.ncolors = 0;
    info_header.nimpcolors = 0;

    // write header
    fwrite(&magic, 1, sizeof(magic), fOut);
    fwrite(&file_header, 1, sizeof(file_header), fOut);
    fwrite(&info_header, 1, sizeof(info_header), fOut);

    // write rgb data
    fwrite(pRgbData, 1, rgbSize, fOut);

    return 0;
}

int CColorConvert::writeFile(char *filename, int format)
{
    FILE* fOut = NULL;
    size_t tmpSize; //not really used here, size store in class

    fOut = fopen(filename, "w");
    if ( fOut == NULL )
    {
        LOGE("Error: failed to open the file for writing\n");
        return 1;
    }

    switch (format)
    {
        case BMP:
            if(makeRGB888(&tmpSize))
                bufWriteBmp(fOut, RGB888_BPP);
        break;
        case SOURCE:
            dumpBuffer(fOut, mpInBuffer, mInBufferSize);
            break;

        case RAW:
            dumpBuffer(fOut, (uint8_t*) pYuvImage->getBuffer(), pYuvImage->getBufferSize());
            break;

        case OUTPUT:
            if(mOutBufferSize)
                dumpBuffer(fOut, mpOutBuffer, mOutBufferSize);
            break;

    }

    fclose(fOut);

    return 0;
}

int clip(int val)
 {
    if(val<0) return 0;
    if(val>255) return 255;
    return val;
}

uint8_t* CColorConvert::makeUYV2(size_t *buffersize, int inplace)
{
    uint8_t *ptrOut;
    uint8_t *ptrEnd;
    size_t outSize;
    YUV *ptrYuv  = pYuvImage->getBuffer();

    if(inplace) //use input buffer for conversion
    {
        ptrOut = mpInBuffer;
        outSize = mInBufferSize;
    }
    else
    {
        outSize = pYuvImage->getSize() * (UYV422_BPP>>3);
        if(!getOutputBuffer(outSize))
            return NULL;
        ptrOut = mpOutBuffer;
    }

    ptrEnd = ptrOut + outSize;

    while(ptrOut<ptrEnd)
    {
        // averaging ov u and v
        int v= clip((int)(ptrYuv[0].v + ptrYuv[0].v)>>1);
        int u= clip((int)(ptrYuv[0].u + ptrYuv[1].u)>>1);

        ptrOut[0]  = u;
        ptrOut[1]  = ptrYuv[0].y;
        ptrOut[2]  = v;
        ptrOut[3]  = ptrYuv[1].y;

        ptrYuv+=2;
        ptrOut+=4;

    }
DBG
   if(buffersize)
    *buffersize = outSize;

   return ptrOut;
}


//in = 24bit out=24bit
uint8_t* CColorConvert::makeRGB888(size_t *buffersize)
{
    size_t RgbSize = pYuvImage->getSize() * (RGB888_BPP>>3);
    if(!getOutputBuffer(RgbSize))
        return NULL;

    uint8_t *ptrRgb = mpOutBuffer;
    uint8_t *ptrEnd = ptrRgb + RgbSize;

    YUV *ptrYuv  = pYuvImage->getBuffer();

    while(ptrRgb<ptrEnd)
    {
        int y = ptrYuv->y;
        int u = ptrYuv->u;
        int v = ptrYuv->v;

        int c = y - 16;
        int d = u - 128;
        int e = v - 128;
        //we us BGR insetad of RGB for storing BMP file
        ptrRgb[2] = clip(( 298 * c           + 409 * e + 128) >> 8);        // r
        ptrRgb[1] = clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);        // g
        ptrRgb[0] = clip(( 298 * c + 516 * d           + 128) >> 8);        // b

        ptrYuv++;
        ptrRgb+=3;
    }
    if(buffersize)
        *buffersize = RgbSize;

    return ptrRgb;
}
