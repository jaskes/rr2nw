#include "graph.h"
#include "sd1_epal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

extern "C" {
unsigned char *_gr_pScreen = NULL;
unsigned char *_gr_pOrigin = NULL;
unsigned char **_gr_pYCache = NULL;
unsigned char *_gr_pHaze = NULL;
SGRColorDef *_gr_pTransparency = NULL;
int _gr_nTranspCount = 0;
unsigned char *_gr_pGouraud = NULL;
long _gr_pDiserTable[64*64] = {};

int _gr_nScreenWidth = 320;
int _gr_nScreenHeight = 200;
int _gr_nScreenOriginX = 0;
int _gr_nScreenOriginY = 0;
CRect2 _gr_clipRect;
SGRViewport *_Viewport = NULL;

float _gr_fFrontClip = 1.0f;
volatile int _gr_bRestoreSurf = 0;

HWND _gr_hWnd = NULL;
HDC _gr_hDC = NULL;
HPALETTE _gr_hPal = NULL;
GR_BITMAPINFO _gr_DIBInfo = {};
GR_LOGPALETTE _gr_logPal = {};
POINTS _gr_windowPos = {0, 0};

UGRVertex _gr_vertices[GR_MAX_VERTEX] = {};
SGRPolygon _gr_polygon = {};
SGRLight _gr_pLights[LIGHT_SOURCE_COUNT] = {};
}

TExtendedPalette _EPal;
TExtendedPalette _ETransparencyPal;
unsigned char _currPalette[768];
SDeviceList _dL = {NULL, -1, NULL, 0, 0, NULL};

int _rScale = 0;
int _rShift = 0;
int _gScale = 0;
int _gShift = 0;
int _bScale = 0;
int _bShift = 0;

float _kX = 0.01f;
float _kY = 0.01f;
float _kXX = 0.0001f;
float _kYY = 0.0001f;
float _kXY = 0.0001f;
float _ikX = 100.0f;
float _ikY = 100.0f;

int __HazeStartInt = 0;
float __HazeLen = 0.0f;

void (*_pGRSetClipRect)(void) = NULL;

namespace {

int DrawSoftwarePolygon()
{
    if( _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL ||
        _gr_nScreenWidth <= 0 || _gr_nScreenHeight <= 0 ||
        _gr_polygon.nVertices < 3 ||
        _gr_polygon.nVertices > GR_MAX_VERTEX ) return FALSE;

    const bool flat = _gr_polygon.dwFullType == GR_POLY_FLAT;
    const bool transparent =
        _gr_polygon.dwFullType == GR_POLY_TRANSPARENT;
    if( !flat && !transparent ) return FALSE;

    const unsigned char *transparentTable = NULL;
    if( transparent ) {
        const std::uintptr_t tableAddress = static_cast<std::uintptr_t>(
            static_cast<unsigned long>(_gr_polygon.dwColor.color));
        if( tableAddress == 0 ) return FALSE;
        transparentTable = reinterpret_cast<const unsigned char *>(
            tableAddress) + ((_gr_polygon.dwOpacity & 0xF0) << 4);
    }

    int clipLeft = static_cast<int>(_gr_clipRect.left);
    int clipTop = static_cast<int>(_gr_clipRect.top);
    int clipRight = static_cast<int>(_gr_clipRect.right);
    int clipBottom = static_cast<int>(_gr_clipRect.bottom);
    if( clipRight <= clipLeft || clipBottom <= clipTop ) {
        clipLeft = -_gr_nScreenOriginX;
        clipTop = -_gr_nScreenOriginY;
        clipRight = _gr_nScreenWidth-_gr_nScreenOriginX;
        clipBottom = _gr_nScreenHeight-_gr_nScreenOriginY;
    }
    clipLeft = (std::max)(clipLeft,-_gr_nScreenOriginX);
    clipTop = (std::max)(clipTop,-_gr_nScreenOriginY);
    clipRight = (std::min)(
        clipRight,_gr_nScreenWidth-_gr_nScreenOriginX);
    clipBottom = (std::min)(
        clipBottom,_gr_nScreenHeight-_gr_nScreenOriginY);
    if( clipRight <= clipLeft || clipBottom <= clipTop ) return FALSE;

    int minY = _gr_vertices[0].any.y;
    int maxY = minY;
    for( int i = 1; i < _gr_polygon.nVertices; ++i ) {
        minY = (std::min)(minY,_gr_vertices[i].any.y);
        maxY = (std::max)(maxY,_gr_vertices[i].any.y);
    }
    minY = (std::max)(minY,clipTop);
    maxY = (std::min)(maxY,clipBottom);

    double intersections[GR_MAX_VERTEX];
    for( int y = minY; y < maxY; ++y ) {
        const double scanY = static_cast<double>(y)+0.5;
        int count = 0;
        for( int i = 0; i < _gr_polygon.nVertices; ++i ) {
            const UGRVertex &first = _gr_vertices[i];
            const UGRVertex &second =
                _gr_vertices[(i+1)%_gr_polygon.nVertices];
            const double y0 = first.any.y;
            const double y1 = second.any.y;
            if( (y0 <= scanY && scanY < y1) ||
                (y1 <= scanY && scanY < y0) ) {
                intersections[count++] = first.any.x+
                    (scanY-y0)*(second.any.x-first.any.x)/(y1-y0);
            }
        }
        std::sort(intersections,intersections+count);
        for( int edge = 0; edge+1 < count; edge += 2 ) {
            int x0 = static_cast<int>(std::ceil(intersections[edge]-0.5));
            int x1 = static_cast<int>(
                std::ceil(intersections[edge+1]-0.5))-1;
            x0 = (std::max)(x0,clipLeft);
            x1 = (std::min)(x1,clipRight-1);
            if( x1 < x0 ) continue;

            unsigned char *pixel = _gr_pScreen+
                static_cast<std::size_t>(y+_gr_nScreenOriginY)*
                    _gr_nScreenWidth+
                x0+_gr_nScreenOriginX;
            if( flat ) {
                std::memset(pixel,
                    static_cast<unsigned char>(_gr_polygon.dwColor.color),
                    static_cast<std::size_t>(x1-x0+1));
            } else {
                for( int x = x0; x <= x1; ++x,++pixel )
                    *pixel = transparentTable[*pixel];
            }
        }
    }
    return TRUE;
}

}  // namespace

int (*_pGRDrawPolygonPCCW)(void) = DrawSoftwarePolygon;

void GRSetViewport(SGRViewport *pViewport)
{
    _Viewport = pViewport;

    if( pViewport == NULL ) return;

    _gr_nScreenOriginY = pViewport->y;
    _gr_nScreenOriginX = pViewport->x;
    _gr_clipRect = pViewport->clipRect;

    if( _dL.currDevice != NULL &&
        _dL.currDevice->swHw == GR_SOFTWARE ) {
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
    if( _dL.currDevice == NULL ) return 0;
    if( _dL.currDevice->swHw == GR_HARDWARE )
        return (((unsigned int)r>>_rScale)<<_rShift) |
               (((unsigned int)g>>_gScale)<<_gShift) |
               (((unsigned int)b>>_bScale)<<_bShift);

    return epal_Match(_EPal,RGB_i(r,g,b));
}

unsigned long GRCreateColor(int r,int g,int b)
{
    if( _dL.currDevice == NULL ) return 0;
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

int GRIsHardware()
{
    return _dL.currDevice != NULL &&
           _dL.currDevice->swHw == GR_HARDWARE;
}

void GRSetScale(float scaleX,float scaleY)
{
    if( scaleX == 0.0f || scaleY == 0.0f ) return;
    _kX = scaleX;
    _kY = scaleY;
    _kXY = scaleX*scaleY;
    _ikX = 1.0f/scaleX;
    _ikY = 1.0f/scaleY;
    _kXX = scaleX*scaleX;
    _kYY = scaleY*scaleY;
}

int GRSetHaze(int start,int length,SGRColorDef *definition)
{
    if( start <= 0 || length <= 0 || definition == NULL ||
        definition->pTable == NULL || GRIsHardware() ) return FALSE;

    __HazeStartInt = static_cast<int>(65536.0/start);
    __HazeLen = static_cast<float>(start+length);
    _gr_pHaze = definition->pTable;
    return TRUE;
}

void GRSetPaletteTables(SGRColorDef *pTransparency,int nTranspCount,
                        TCbyte *pGouraud)
{
    _gr_pTransparency = pTransparency;
    _gr_nTranspCount = pTransparency != NULL && nTranspCount > 0 ?
                       (std::min)(nTranspCount,256) : 0;
    _gr_pGouraud = const_cast<unsigned char *>(pGouraud);

    _ETransparencyPal.Clear();
    if( _gr_nTranspCount == 0 ) return;

    unsigned char palette[768] = {};
    for( int i = 0; i < _gr_nTranspCount; ++i ) {
        palette[i*3] = static_cast<unsigned char>(pTransparency[i].r);
        palette[i*3+1] = static_cast<unsigned char>(pTransparency[i].g);
        palette[i*3+2] = static_cast<unsigned char>(pTransparency[i].b);
    }
    epal_Load8BitPal(_ETransparencyPal,palette,_gr_nTranspCount);
}

void SetMixLightTable(unsigned char *table)
{
    // The recovered software graph has not initialized the legacy light-mix
    // allocation. The original implementation is also a no-op in that state.
    (void)table;
}

unsigned long GRTransparentColor(int r,int g,int b)
{
    if( _dL.currDevice == NULL ) return 0;
    if( _dL.currDevice->swHw == GR_HARDWARE )
        return (static_cast<unsigned long>(r)<<16) |
               (static_cast<unsigned long>(g)<<8) |
               static_cast<unsigned long>(b);
    if( _gr_pTransparency == NULL || _gr_nTranspCount <= 0 ) return 0;

    const int color = epal_Match(_ETransparencyPal,RGB_i(r,g,b));
    return static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(
        _gr_pTransparency[color].pTable));
}

int GRSetPalette(const unsigned char *pal8,int setScr)
{
    (void)setScr;
    if( _dL.currDevice == NULL || pal8 == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE ) return FALSE;

    std::memcpy(_currPalette,pal8,sizeof(_currPalette));
    _currPalette[0] = 0;
    _currPalette[1] = 0;
    _currPalette[2] = 0;
    for( int i = 3; i < 256*3; i += 3 )
        if( _currPalette[i] == 0 && _currPalette[i+1] == 0 &&
            _currPalette[i+2] == 0 ) _currPalette[i+2] = 2;

    _EPal.Clear();
    epal_Load8BitPal(_EPal,_currPalette,256);
    _gr_logPal.palVersion = 0x300;
    _gr_logPal.palNumEntries = 256;
    for( int i = 0; i < 256; ++i ) {
        const unsigned char r = _currPalette[i*3];
        const unsigned char g = _currPalette[i*3+1];
        const unsigned char b = _currPalette[i*3+2];
        _gr_logPal.palPalEntry[i].peRed = r;
        _gr_logPal.palPalEntry[i].peGreen = g;
        _gr_logPal.palPalEntry[i].peBlue = b;
        _gr_logPal.palPalEntry[i].peFlags = PC_NOCOLLAPSE;
        _gr_DIBInfo.bmiColors[i].rgbRed = r;
        _gr_DIBInfo.bmiColors[i].rgbGreen = g;
        _gr_DIBInfo.bmiColors[i].rgbBlue = b;
        _gr_DIBInfo.bmiColors[i].rgbReserved = 0;
        _gr_DIBInfo.bmiIndex[i] = static_cast<WORD>(i);
    }
    return TRUE;
}

int GRClearScreen(BOOL fClr,long fColor)
{
    if( _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE ) return FALSE;
    if( !fClr ) return TRUE;
    if( _gr_pScreen == NULL || _gr_nScreenWidth <= 0 ||
        _gr_nScreenHeight <= 0 ) return FALSE;

    int left = static_cast<int>(_gr_clipRect.left)+_gr_nScreenOriginX;
    int top = static_cast<int>(_gr_clipRect.top)+_gr_nScreenOriginY;
    int right = static_cast<int>(_gr_clipRect.right)+_gr_nScreenOriginX;
    int bottom = static_cast<int>(_gr_clipRect.bottom)+_gr_nScreenOriginY;
    if( right <= left || bottom <= top ) {
        left = 0;
        top = 0;
        right = _gr_nScreenWidth;
        bottom = _gr_nScreenHeight;
    }
    left = (std::max)(left,0);
    top = (std::max)(top,0);
    right = (std::min)(right,_gr_nScreenWidth);
    bottom = (std::min)(bottom,_gr_nScreenHeight);
    if( right <= left || bottom <= top ) return FALSE;

    unsigned char *row = _gr_pScreen+
        static_cast<std::size_t>(top)*_gr_nScreenWidth+left;
    for( int y = top; y < bottom; ++y,row += _gr_nScreenWidth )
        std::memset(row,static_cast<unsigned char>(fColor),
                    static_cast<std::size_t>(right-left));
    return TRUE;
}

void GRZBufferEnable(int enable)
{
    (void)enable;
}

int GREndScene()
{
    return _dL.currDevice != NULL &&
           _dL.currDevice->swHw == GR_SOFTWARE;
}

int GRStartScene()
{
    return _dL.currDevice != NULL &&
           _dL.currDevice->swHw == GR_SOFTWARE;
}

int GRDumpScreen()
{
    if( _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL )
        return FALSE;
    if( _gr_hDC == NULL ) return TRUE;

    return SetDIBitsToDevice(
        _gr_hDC,0,0,_gr_nScreenWidth,_gr_nScreenHeight,0,0,0,
        _gr_nScreenHeight,_gr_pScreen,
        reinterpret_cast<BITMAPINFO *>(&_gr_DIBInfo),DIB_RGB_COLORS) != 0;
}
