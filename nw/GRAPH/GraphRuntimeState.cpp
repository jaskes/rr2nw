#include "graph.h"
#include "sd1_epal.h"

extern "C" {
unsigned char *_gr_pScreen = NULL;
unsigned char *_gr_pOrigin = NULL;
unsigned char **_gr_pYCache = NULL;

int _gr_nScreenWidth = 320;
int _gr_nScreenHeight = 200;
int _gr_nScreenOriginX = 0;
int _gr_nScreenOriginY = 0;
CRect2 _gr_clipRect;
SGRViewport *_Viewport = NULL;

HWND _gr_hWnd = NULL;
}

TExtendedPalette _EPal;
unsigned char _currPalette[768];
SDeviceList _dL = {NULL, -1, NULL, 0, 0, NULL};

int _rScale = 0;
int _rShift = 0;
int _gScale = 0;
int _gShift = 0;
int _bScale = 0;
int _bShift = 0;

void (*_pGRSetClipRect)(void) = NULL;

void GRSetViewport(SGRViewport *pViewport)
{
    _Viewport = pViewport;

    if( pViewport == NULL ) return;

    _gr_nScreenOriginY = pViewport->y;
    _gr_nScreenOriginX = pViewport->x;
    _gr_clipRect = pViewport->clipRect;

    if( _dL.currDevice->swHw == GR_SOFTWARE ) {
        _gr_pOrigin = pViewport->pOrigin;
        _gr_pYCache = pViewport->pCache;
    }

    GRSetClipRect();
}

SGRViewport *GRGetViewport()
{
    return _Viewport;
}

SGRViewport *GRCreateViewport(int originX,int originY,TCSRect2 &clipRect)
{
    if( _dL.currDevice == NULL || originX < 0 || originY < 0 ||
        originX > _gr_nScreenWidth || originY > _gr_nScreenHeight ||
        (_dL.currDevice->swHw == GR_SOFTWARE && _gr_pScreen == NULL) )
        return NULL;

    SGRViewport *pViewport = new SGRViewport;
    int clipH = _gr_nScreenHeight;

    pViewport->x = originX;
    pViewport->y = originY;
    pViewport->clipRect.left = clipRect.left-originX;
    pViewport->clipRect.right = clipRect.right-originX;
    pViewport->clipRect.top = clipRect.top-originY;
    pViewport->clipRect.bottom = clipRect.bottom-originY;

    if( _dL.currDevice->swHw == GR_SOFTWARE ) {
        pViewport->pCache0 = new byte *[clipH];
        pViewport->pCache = pViewport->pCache0+originY;
        pViewport->pOrigin = _gr_pScreen+originY*_gr_nScreenWidth+originX;
        for( int i = -originY; i < _gr_nScreenHeight-originY; ++i )
            pViewport->pCache[i] = pViewport->pOrigin+i*_gr_nScreenWidth;
    } else {
        pViewport->pCache0 = NULL;
    }

    return pViewport;
}

void GRReleaseViewport(SGRViewport *pViewport)
{
    if( !pViewport ) return;
    if( pViewport->pCache0 != NULL ) {
        if( _gr_pYCache == pViewport->pCache ) _gr_pYCache = NULL;
        if( _gr_pOrigin == pViewport->pOrigin ) _gr_pOrigin = NULL;
        delete [] pViewport->pCache0;
    }
    if( _Viewport == pViewport ) _Viewport = NULL;

    delete pViewport;
}

unsigned long GRFillColor(int r,int g,int b)
{
    if( _dL.currDevice->swHw == GR_HARDWARE )
        return (((unsigned int)r>>_rScale)<<_rShift) |
               (((unsigned int)g>>_gScale)<<_gShift) |
               (((unsigned int)b>>_bScale)<<_bShift);

    return epal_Match(_EPal,RGB_i(r,g,b));
}

unsigned long GRCreateColor(int r,int g,int b)
{
    STextureFormat *tf =
        &_dL.currDevice->textureFormat[NORMAL_TEXTURE_INDEX];

    if( tf->rgbBitCount == 8 ) {
        unsigned int col = epal_Match(_EPal,RGB_i(r,g,b));
        unsigned char *pal = &_currPalette[col*3];

        return (((unsigned int)pal[0])<<24) |
               (((unsigned int)pal[1])<<16) |
               (((unsigned int)pal[2])<<8) | col;
    }

    return (((unsigned int)r)<<24) |
           (((unsigned int)g)<<16) |
           (((unsigned int)b)<<8) |
           epal_Match(_EPal,RGB_i(r,g,b));
}
