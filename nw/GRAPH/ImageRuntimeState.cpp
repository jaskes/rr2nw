#include "graph.h"
#include "sd1_epal.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

extern int _rScale, _rShift;
extern int _gScale, _gShift;
extern int _bScale, _bShift;
extern TExtendedPalette _EPal;
extern SDeviceList _dL;

CGRImage::CGRImage(int w,int h)
{
    image = NULL;
    usePalette = 0;
    SetWidthHeight(w,h);
}

void CGRImage::SetWidthHeight(int w,int h)
{
    if( image != NULL || _dL.currDevice == NULL || w <= 0 || h <= 0 ||
        w > INT_MAX/h ) return;

    imageW = w;
    imageH = h;

    if( _dL.currDevice->swHw == GR_HARDWARE ) {
        image = new unsigned short [w*h];
        memset(image,0,w*h*2);
    } else {
        image = new unsigned char [w*h];
        memset(image,0,w*h);
    }
}

void CGRImage::SetPalette(unsigned char *pal8)
{
    if( _dL.currDevice == NULL ) return;
    if( pal8 == NULL ) {
        usePalette = 0;
        return;
    }

    usePalette = 1;
    if( _dL.currDevice->swHw == GR_HARDWARE ) {
        for( int i = 0; i < 256; ++i ) {
            int r = pal8[i*3];
            int g = pal8[i*3+1];
            int b = pal8[i*3+2];
            palToScr16[i] = (unsigned short)(
                (((unsigned int)r>>_rScale)<<_rShift) |
                (((unsigned int)g>>_gScale)<<_gShift) |
                (((unsigned int)b>>_bScale)<<_bShift));
        }
        palToScr16[0] = 0;
    } else {
        for( int i = 0; i < 256; ++i )
            palToScr8[i] = (unsigned char)epal_Match(
                _EPal,RGB_i(pal8[i*3],pal8[i*3+1],pal8[i*3+2]));
        palToScr8[0] = 0;
    }
}

void CGRImage::LoadPalImage(unsigned char *mem,int x,int y,
                            int x0,int y0,int x1,int y1,int iW)
{
    if( image == NULL || mem == NULL || x >= imageW || y >= imageH ||
        x+(x1-x0) < 0 || y+(y1-y0) < 0 || usePalette == 0 ) return;

    if( x < 0 ) { x0 -= x; x = 0; }
    if( y < 0 ) { y0 -= y; y = 0; }
    if( x+(x1-x0) >= imageW ) x1 -= x+(x1-x0)-imageW+1;
    if( y+(y1-y0) >= imageH ) y1 -= y+(y1-y0)-imageH+1;

    if( _dL.currDevice->swHw == GR_HARDWARE ) {
        unsigned short *dst = (unsigned short*)image+x+y*imageW;
        unsigned char *src = mem+x0+y0*iW;
        for( int j = y0; j <= y1; ++j,dst += imageW,src += iW ) {
            unsigned short *out = dst;
            unsigned char *in = src;
            for( int i = x0; i <= x1; ++i ) *out++ = palToScr16[*in++];
        }
    } else {
        unsigned char *dst = (unsigned char*)image+x+y*imageW;
        unsigned char *src = mem+x0+y0*iW;
        for( int j = y0; j <= y1; ++j,dst += imageW,src += iW ) {
            unsigned char *out = dst;
            unsigned char *in = src;
            for( int i = x0; i <= x1; ++i ) *out++ = palToScr8[*in++];
        }
    }
}

int CGRImage::LoadFromBMPFile(const char *bmpName,int x,int y)
{
    if( _dL.currDevice == NULL || bmpName == NULL ) return 0;

    FILE *fp = fopen(bmpName,"rb");
    if( fp == NULL ) return 0;

    BITMAPFILEHEADER fileHeader = {};
    BITMAPINFOHEADER imageHeader = {};
    bool valid = fread(&fileHeader,sizeof(fileHeader),1,fp) == 1 &&
                 fileHeader.bfType == 0x4D42 &&
                 fread(&imageHeader,sizeof(imageHeader),1,fp) == 1 &&
                 imageHeader.biSize == sizeof(BITMAPINFOHEADER) &&
                 imageHeader.biPlanes == 1 &&
                 imageHeader.biCompression == BI_RGB &&
                 imageHeader.biBitCount == 8 &&
                 imageHeader.biWidth > 0 && imageHeader.biHeight != 0 &&
                 imageHeader.biHeight != LONG_MIN &&
                 imageHeader.biClrUsed <= 256;
    if( !valid ) { fclose(fp); return 0; }

    RGBQUAD palette[256];
    unsigned char palette8[768];
    memset(palette,0,sizeof(palette));
    unsigned int paletteCount = imageHeader.biClrUsed;
    if( paletteCount == 0 ) paletteCount = 256;
    if( fread(palette,sizeof(RGBQUAD),paletteCount,fp) != paletteCount ) {
        fclose(fp);
        return 0;
    }
    for( int i = 0; i < 256; ++i ) {
        palette8[i*3] = palette[i].rgbRed;
        palette8[i*3+1] = palette[i].rgbGreen;
        palette8[i*3+2] = palette[i].rgbBlue;
    }

    int width = imageHeader.biWidth;
    int height = imageHeader.biHeight > 0 ? imageHeader.biHeight :
                                               -imageHeader.biHeight;
    int padding = (4-(width&3))&3;
    long current = ftell(fp);
    bool sizeValid = current >= 0 && fseek(fp,0,SEEK_END) == 0;
    long fileSize = sizeValid ? ftell(fp) : -1;
    unsigned long long required =
        ((unsigned long long)width+(unsigned int)padding)*
        (unsigned long long)height;
    if( height <= 0 || width > INT_MAX/height || fileSize < 0 || current < 0 ||
        fileHeader.bfOffBits < (unsigned long)current ||
        fileHeader.bfOffBits > (unsigned long)fileSize ||
        required > (unsigned long long)fileSize-fileHeader.bfOffBits ||
        fseek(fp,(long)fileHeader.bfOffBits,SEEK_SET) != 0 ) {
        fclose(fp);
        return 0;
    }
    if( image == NULL ) SetWidthHeight(width,height);
    if( image == NULL ) { fclose(fp); return 0; }

    unsigned char *bitmap = new unsigned char [width*height];
    bool readOk = true;
    for( int row = 0; row < height; ++row ) {
        int target = imageHeader.biHeight > 0 ? height-row-1 : row;
        if( fread(bitmap+target*width,1,width,fp) != (size_t)width ||
            fseek(fp,padding,SEEK_CUR) != 0 ) {
            readOk = false;
            break;
        }
    }
    fclose(fp);
    if( !readOk ) { delete [] bitmap; return 0; }

    SetPalette(palette8);
    LoadPalImage(bitmap,x,y,0,0,width-1,height-1,width);
    delete [] bitmap;
    return 1;
}

int CGRImage::DrawSprite(int xs,int ys) const
{
    return DrawSprite(xs,ys,0,0,imageW-1,imageH-1);
}

int CGRImage::DrawSprite(int xs,int ys,int x0,int y0,int x1,int y1) const
{
    if( image == NULL || _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL ||
        imageW <= 0 || imageH <= 0 || _gr_nScreenWidth <= 0 ||
        _gr_nScreenHeight <= 0 ) return 0;

    long long sourceLeft = x0;
    long long sourceTop = y0;
    long long sourceRight = x1;
    long long sourceBottom = y1;
    long long destinationLeft = xs;
    long long destinationTop = ys;

    if( sourceLeft > sourceRight || sourceTop > sourceBottom ) return 0;
    if( sourceLeft < 0 ) {
        destinationLeft -= sourceLeft;
        sourceLeft = 0;
    }
    if( sourceTop < 0 ) {
        destinationTop -= sourceTop;
        sourceTop = 0;
    }
    if( sourceRight >= imageW ) sourceRight = imageW-1;
    if( sourceBottom >= imageH ) sourceBottom = imageH-1;
    if( sourceLeft > sourceRight || sourceTop > sourceBottom ) return 0;

    if( destinationLeft < 0 ) {
        sourceLeft -= destinationLeft;
        destinationLeft = 0;
    }
    if( destinationTop < 0 ) {
        sourceTop -= destinationTop;
        destinationTop = 0;
    }
    long long width = sourceRight-sourceLeft+1;
    long long height = sourceBottom-sourceTop+1;
    if( destinationLeft >= _gr_nScreenWidth ||
        destinationTop >= _gr_nScreenHeight ) return 0;
    if( destinationLeft+width > _gr_nScreenWidth )
        sourceRight -= destinationLeft+width-_gr_nScreenWidth;
    if( destinationTop+height > _gr_nScreenHeight )
        sourceBottom -= destinationTop+height-_gr_nScreenHeight;
    if( sourceLeft > sourceRight || sourceTop > sourceBottom ) return 0;

    const unsigned char *source = (const unsigned char*)image;
    for( int ySource = (int)sourceTop, yDestination = (int)destinationTop;
         ySource <= (int)sourceBottom; ++ySource,++yDestination ) {
        const unsigned char *input = source+ySource*imageW+(int)sourceLeft;
        unsigned char *output = _gr_pScreen+
            (size_t)yDestination*_gr_nScreenWidth+(int)destinationLeft;
        for( int xSource = (int)sourceLeft;
             xSource <= (int)sourceRight; ++xSource,++input,++output )
            if( *input != 0 ) *output = *input;
    }
    return 1;
}
