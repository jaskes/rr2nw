#include "graph.h"
#include "sd1_epal.h"

#include <string.h>

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
