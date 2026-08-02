#include "graph.h"
#include "sd1_epal.h"

#include <new>
#include <stdio.h>
#include <string.h>

int CFixedColorFont::Read(const char *filename)
{
    FILE *file = fopen(filename,"rb");
    if( file == NULL ) return 0;

    delete [] pOriginalFntSpr;
    delete [] pSwFntSpr;
    delete [] pHwFntSpr;
    pOriginalFntSpr = NULL;
    pSwFntSpr = NULL;
    pHwFntSpr = NULL;

    if( fread(&fontHeader,sizeof(fontHeader),1,file)!=1 ||
        _strnicmp(fontHeader.id,FONT_ID,sizeof(FONT_ID)-1)!=0 ||
        fontHeader.nFntSprSize<=0 ||
        fontHeader.nFntSprSize>16*1024*1024 ) {
        fclose(file);
        return 0;
    }

    pOriginalFntSpr =
        new (std::nothrow) unsigned char[fontHeader.nFntSprSize];
    const bool read = pOriginalFntSpr != NULL &&
        fread(pOriginalFntSpr,fontHeader.nFntSprSize,1,file)==1;
    const bool closed = fclose(file)==0;
    if( !read || !closed ) {
        delete [] pOriginalFntSpr;
        pOriginalFntSpr = NULL;
        return 0;
    }
    return RecreateFont();
}

extern int _rScale, _rShift;
extern int _gScale, _gShift;
extern int _bScale, _bShift;
extern TExtendedPalette _EPal;
extern SDeviceList _dL;

int CFixedColorFont::ReadFromMemory(void *buf)
{
    pOriginalFntSpr = NULL;
    pSwFntSpr = NULL;
    pHwFntSpr = NULL;
    if( buf == NULL ) return 0;

    memcpy(&fontHeader,buf,sizeof(fontHeader));
    if( _strnicmp(fontHeader.id,FONT_ID,sizeof(FONT_ID)-1) != 0 ||
        fontHeader.nFntSprSize <= 0 ) return 0;

    pOriginalFntSpr = new unsigned char [fontHeader.nFntSprSize];
    memcpy(pOriginalFntSpr,(char*)buf+sizeof(fontHeader),
           fontHeader.nFntSprSize);
    return RecreateFont();
}

int CFixedColorFont::RecreateFont()
{
    if( _dL.currDevice == NULL || pOriginalFntSpr == NULL ) return 0;

    if( _dL.currDevice->swHw == GR_HARDWARE ) {
        delete [] pSwFntSpr;
        pSwFntSpr = NULL;
        if( pHwFntSpr == NULL )
            pHwFntSpr = new unsigned short [fontHeader.nFntSprSize];

        unsigned short palette[256];
        for( int i = 0; i < 256; ++i )
            palette[i] = (unsigned short)(
                (((unsigned int)fontHeader.pPal[i*3]>>_rScale)<<_rShift) |
                (((unsigned int)fontHeader.pPal[i*3+1]>>_gScale)<<_gShift) |
                (((unsigned int)fontHeader.pPal[i*3+2]>>_bScale)<<_bShift));
        for( int i = 0; i < fontHeader.nFntSprSize; ++i )
            pHwFntSpr[i] = palette[pOriginalFntSpr[i]];
    } else {
        delete [] pHwFntSpr;
        pHwFntSpr = NULL;
        if( pSwFntSpr == NULL )
            pSwFntSpr = new unsigned char [fontHeader.nFntSprSize];

        unsigned char palette[256];
        for( int i = 0; i < 256; ++i )
            palette[i] = (unsigned char)epal_Match(
                _EPal,RGB_i(fontHeader.pPal[i*3],fontHeader.pPal[i*3+1],
                            fontHeader.pPal[i*3+2]));
        for( int i = 0; i < fontHeader.nFntSprSize; ++i )
            pSwFntSpr[i] = palette[pOriginalFntSpr[i]];
    }
    return 1;
}

int CFixedColorFont::PrintAt(long x,long y,const char *str)
{
    if( _dL.currDevice == NULL || _dL.currDevice->swHw != GR_SOFTWARE ||
        _gr_pScreen == NULL || pOriginalFntSpr == NULL || pSwFntSpr == NULL ||
        str == NULL || fontHeader.nFntWidth <= 0 ||
        fontHeader.nFntHeight <= 0 || fontHeader.nFntSprSize <= 0 ) return 0;

    long long destinationX = (long long)x+_gr_nScreenOriginX;
    long long destinationY = (long long)y+_gr_nScreenOriginY;
    while( *str != 0 ) {
        unsigned int character = (unsigned char)*str++;
        long glyphWidth = fontHeader.pSmFntTable[character*2];
        long glyphOffset = fontHeader.pSmFntTable[character*2+1];
        if( glyphWidth < 0 || glyphWidth > fontHeader.nFntWidth ||
            glyphOffset < 0 ) return 0;
        if( glyphWidth == 0 ) continue;

        long long last = (long long)glyphOffset+
            (long long)(fontHeader.nFntHeight-1)*fontHeader.nFntWidth+
            glyphWidth;
        if( last > fontHeader.nFntSprSize ) return 0;
        for( long row = 0; row < fontHeader.nFntHeight; ++row ) {
            long long screenY = destinationY+row;
            if( screenY < 0 || screenY >= _gr_nScreenHeight ) continue;
            const unsigned char *source = pSwFntSpr+glyphOffset+
                row*fontHeader.nFntWidth;
            for( long column = 0; column < glyphWidth; ++column ) {
                long long screenX = destinationX+column;
                if( source[column] != 0 && screenX >= 0 &&
                    screenX < _gr_nScreenWidth )
                    _gr_pScreen[(size_t)screenY*_gr_nScreenWidth+
                                (size_t)screenX] =
                        source[column];
            }
        }
        destinationX += glyphWidth;
    }
    return 1;
}

int CFixedColorFont::PrintColorAt(long x,long y,const char *str,
                                  unsigned long color)
{
    if( _dL.currDevice == NULL || _dL.currDevice->swHw != GR_SOFTWARE ||
        _gr_pScreen == NULL || pOriginalFntSpr == NULL || pSwFntSpr == NULL ||
        str == NULL || fontHeader.nFntWidth <= 0 ||
        fontHeader.nFntHeight <= 0 || fontHeader.nFntSprSize <= 0 ) return 0;

    long long destinationX = (long long)x+_gr_nScreenOriginX;
    const long long destinationY = (long long)y+_gr_nScreenOriginY;
    while( *str != 0 ) {
        const unsigned int character = (unsigned char)*str++;
        const long glyphWidth = fontHeader.pSmFntTable[character*2];
        const long glyphOffset = fontHeader.pSmFntTable[character*2+1];
        if( glyphWidth < 0 || glyphWidth > fontHeader.nFntWidth ||
            glyphOffset < 0 ) return 0;
        if( glyphWidth == 0 ) continue;
        const long long last = (long long)glyphOffset+
            (long long)(fontHeader.nFntHeight-1)*fontHeader.nFntWidth+
            glyphWidth;
        if( last > fontHeader.nFntSprSize ) return 0;

        for( long row = 0; row < fontHeader.nFntHeight; ++row ) {
            const long long screenY = destinationY+row;
            if( screenY < 0 || screenY >= _gr_nScreenHeight ) continue;
            const unsigned char *source = pSwFntSpr+glyphOffset+
                row*fontHeader.nFntWidth;
            for( long column = 0; column < glyphWidth; ++column ) {
                const long long screenX = destinationX+column;
                if( source[column] != 0 && screenX >= 0 &&
                    screenX < _gr_nScreenWidth )
                    _gr_pScreen[(size_t)screenY*_gr_nScreenWidth+
                                (size_t)screenX] =
                        static_cast<unsigned char>(color);
            }
        }
        destinationX += glyphWidth;
    }
    return 1;
}

int CFixedColorFont::PrintClipAt(long x,long y,const char *str)
{
    if( _dL.currDevice == NULL || _dL.currDevice->swHw != GR_SOFTWARE ||
        _gr_pScreen == NULL || pOriginalFntSpr == NULL || pSwFntSpr == NULL ||
        str == NULL || fontHeader.nFntWidth <= 0 ||
        fontHeader.nFntHeight <= 0 || fontHeader.nFntSprSize <= 0 ) return 0;

    const long clipLeft = _gr_clipRect.left;
    const long clipTop = _gr_clipRect.top;
    const long clipRight = _gr_clipRect.right;
    const long clipBottom = _gr_clipRect.bottom;
    long destinationX = x;
    while( *str != 0 ) {
        const unsigned int character = (unsigned char)*str++;
        const long glyphWidth = fontHeader.pSmFntTable[character*2];
        const long glyphOffset = fontHeader.pSmFntTable[character*2+1];
        if( glyphWidth < 0 || glyphWidth > fontHeader.nFntWidth ||
            glyphOffset < 0 ) return 0;
        if( glyphWidth == 0 ) continue;
        const long long last = (long long)glyphOffset+
            (long long)(fontHeader.nFntHeight-1)*fontHeader.nFntWidth+
            glyphWidth;
        if( last > fontHeader.nFntSprSize ) return 0;

        for( long row = 0; row < fontHeader.nFntHeight; ++row ) {
            const long screenY = y+row;
            if( screenY < clipTop || screenY >= clipBottom ) continue;
            const unsigned char *source = pSwFntSpr+glyphOffset+
                row*fontHeader.nFntWidth;
            for( long column = 0; column < glyphWidth; ++column ) {
                const long screenX = destinationX+column;
                if( source[column] != 0 && screenX >= clipLeft &&
                    screenX < clipRight )
                    _gr_pScreen[(size_t)(screenY+_gr_nScreenOriginY)*
                                    _gr_nScreenWidth+
                                (size_t)(screenX+_gr_nScreenOriginX)] =
                        source[column];
            }
        }
        destinationX += glyphWidth;
    }
    return 1;
}

int CFixedColorFont::PrintClipColorAt(long x,long y,const char *str,
                                      unsigned long color)
{
    if( _dL.currDevice == NULL || _dL.currDevice->swHw != GR_SOFTWARE ||
        _gr_pScreen == NULL || pOriginalFntSpr == NULL || pSwFntSpr == NULL ||
        str == NULL || fontHeader.nFntWidth <= 0 ||
        fontHeader.nFntHeight <= 0 || fontHeader.nFntSprSize <= 0 ) return 0;

    const long clipLeft = _gr_clipRect.left;
    const long clipTop = _gr_clipRect.top;
    const long clipRight = _gr_clipRect.right;
    const long clipBottom = _gr_clipRect.bottom;
    long destinationX = x;
    while( *str != 0 ) {
        const unsigned int character = (unsigned char)*str++;
        const long glyphWidth = fontHeader.pSmFntTable[character*2];
        const long glyphOffset = fontHeader.pSmFntTable[character*2+1];
        if( glyphWidth < 0 || glyphWidth > fontHeader.nFntWidth ||
            glyphOffset < 0 ) return 0;
        if( glyphWidth == 0 ) continue;
        const long long last = (long long)glyphOffset+
            (long long)(fontHeader.nFntHeight-1)*fontHeader.nFntWidth+
            glyphWidth;
        if( last > fontHeader.nFntSprSize ) return 0;

        for( long row = 0; row < fontHeader.nFntHeight; ++row ) {
            const long screenY = y+row;
            if( screenY < clipTop || screenY >= clipBottom ) continue;
            const unsigned char *source = pSwFntSpr+glyphOffset+
                row*fontHeader.nFntWidth;
            for( long column = 0; column < glyphWidth; ++column ) {
                const long screenX = destinationX+column;
                if( source[column] != 0 && screenX >= clipLeft &&
                    screenX < clipRight ) {
                    _gr_pScreen[(size_t)(screenY+_gr_nScreenOriginY)*
                                    _gr_nScreenWidth+
                                (size_t)(screenX+_gr_nScreenOriginX)] =
                        static_cast<unsigned char>(color);
                }
            }
        }
        destinationX += glyphWidth;
    }
    return 1;
}
