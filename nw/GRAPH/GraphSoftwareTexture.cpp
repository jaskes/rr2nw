#include "graph.h"
#include "GraphSoftwareTextureInternal.h"
#include "sd1_epal.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

extern SDeviceList _dL;
extern TExtendedPalette _EPal;

namespace {

const long kMaxTextureDimension = 4096;
const std::uint64_t kMaxTexturePixels = 64ULL*1024ULL*1024ULL;

using rr2nw_software::kSoftwareTextureMagic;
using rr2nw_software::SoftwareTexture;
using rr2nw_software::TextureFromHandle;

bool ValidDimensions(long width,long height)
{
    if( width <= 0 || height <= 0 || width > kMaxTextureDimension ||
        height > kMaxTextureDimension ) return false;
    return static_cast<std::uint64_t>(width)*
               static_cast<std::uint64_t>(height) <= kMaxTexturePixels;
}

bool AllocateTextureStorage(SoftwareTexture *texture,long width,long height)
{
    const std::size_t pixelCount = static_cast<std::size_t>(width)*height;
    unsigned char *pixels = new (std::nothrow) unsigned char[pixelCount];
    const std::size_t cacheCount = static_cast<std::size_t>(height)*4+1;
    int *cacheBase = new (std::nothrow) int[cacheCount];
    if( pixels == NULL || cacheBase == NULL ) {
        delete [] pixels;
        delete [] cacheBase;
        return false;
    }

    int *cache = cacheBase+height*2;
    for( long row = -height*2; row <= height*2; ++row ) {
        if( row < -height ) cache[row] = (2-height)*width;
        else if( row > height ) cache[row] = (height-2)*width;
        else cache[row] = row*width;
    }

    if( texture->textureCache != NULL )
        delete [] (texture->textureCache-texture->h*2);
    delete [] texture->dataPtr;
    texture->textureCache = cache;
    texture->dataPtr = pixels;
    texture->w = width;
    texture->h = height;
    texture->size = static_cast<unsigned long>(pixelCount);
    return true;
}

void DestroySoftwareTexture(void *handle)
{
    SoftwareTexture *texture = TextureFromHandle(handle);
    if( texture == NULL ) return;
    if( texture->textureCache != NULL )
        delete [] (texture->textureCache-texture->h*2);
    delete [] texture->dataPtr;
    texture->magic = 0;
    delete texture;
}

void *LoadSoftwareTexture(void *handle,unsigned char *palette,int paletteCount,
                          unsigned char *source)
{
    if( source == NULL ) return NULL;

    const unsigned long flags = *TEXT_FLAG_PTR(source);
    const long height = static_cast<long>(*TEXT_HEIGHT_PTR(source));
    const long width = static_cast<long>(*TEXT_WIDTH_PTR(source));
    if( !ValidDimensions(width,height) ) return NULL;

    SoftwareTexture *texture = TextureFromHandle(handle);
    const bool created = handle == NULL;
    if( !created && texture == NULL ) return NULL;
    if( created ) {
        texture = new (std::nothrow) SoftwareTexture;
        if( texture == NULL ) return NULL;
        std::memset(texture,0,sizeof(*texture));
        texture->magic = kSoftwareTextureMagic;
    }

    if( texture->dataPtr == NULL || texture->w != width ||
        texture->h != height ) {
        if( !AllocateTextureStorage(texture,width,height) ) {
            if( created ) DestroySoftwareTexture(texture);
            return NULL;
        }
    }

    const std::size_t pixelCount = static_cast<std::size_t>(width)*height;
    bool supported = true;
    if( flags & TEXTURE_NOT_PRESENT ) {
        std::memset(texture->dataPtr,0,pixelCount);
    } else {
        switch( flags & TEXTURE_FORMAT_MASK ) {
        case TEXTURE_MEM_FORMAT:
            std::memcpy(texture->dataPtr,source,pixelCount);
            break;
        case TEXTURE_PAL_FORMAT:
            if( palette == NULL ) {
                if( flags & TEXTURE_ALPHA ) {
                    for( std::size_t i = 0; i < pixelCount; ++i )
                        texture->dataPtr[i] =
                            static_cast<unsigned char>(source[i]>>4);
                } else {
                    std::memcpy(texture->dataPtr,source,pixelCount);
                }
            } else if( paletteCount >= 256 ) {
                unsigned char colorMap[256];
                for( int i = 0; i < 256; ++i )
                    colorMap[i] = static_cast<unsigned char>(epal_Match(
                        _EPal,RGB_i(palette[i*3],palette[i*3+1],
                                    palette[i*3+2])));
                for( std::size_t i = 0; i < pixelCount; ++i )
                    texture->dataPtr[i] =
                        (flags & TEXTURE_SPRITE) && source[i] == 0 ?
                        0 : colorMap[source[i]];
            } else {
                supported = false;
            }
            break;
        case TEXTURE_TXR_FORMAT: {
            const std::uint16_t *indices =
                reinterpret_cast<const std::uint16_t *>(source);
            if( (flags & TEXTURE_ALPHA) &&
                (flags & TEXTURE_SPRITE) == 0 ) {
                for( std::size_t i = 0; i < pixelCount; ++i )
                    texture->dataPtr[i] =
                        static_cast<unsigned char>(indices[i]>>4);
                break;
            }
            if( palette == NULL || paletteCount <= 0 ||
                paletteCount > 4096 ) {
                supported = false;
                break;
            }
            unsigned char colorMap[4096];
            for( int i = 0; i < paletteCount; ++i ) {
                colorMap[i] = static_cast<unsigned char>(epal_Match(
                    _EPal,RGB_i(palette[i*3],palette[i*3+1],
                                palette[i*3+2])));
            }
            for( std::size_t i = 0; i < pixelCount; ++i ) {
                const std::uint16_t index = indices[i];
                if( index >= paletteCount ) {
                    supported = false;
                    break;
                }
                texture->dataPtr[i] =
                    (flags & TEXTURE_SPRITE) && index == 0 ?
                    0 : colorMap[index];
            }
            break;
        }
        default:
            supported = false;
            break;
        }
    }

    if( !supported ) {
        if( created ) DestroySoftwareTexture(texture);
        return NULL;
    }

    texture->flags = flags;
    texture->counter = 0;
    texture->filename[0] = 0;
    return texture;
}

void DrawSoftwareAlphaSprite(SGRAlphaSprite *sprite)
{
    if( sprite == NULL || _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL ||
        _gr_nScreenWidth <= 0 || _gr_nScreenHeight <= 0 ||
        sprite->x1 <= sprite->x0 || sprite->y1 <= sprite->y0 ) return;

    SoftwareTexture *texture = TextureFromHandle(sprite->hTexture);
    if( texture == NULL || texture->dataPtr == NULL || texture->w <= 0 ||
        texture->h <= 0 || sprite->color == 0 ) return;

    const std::uintptr_t tableAddress = static_cast<std::uintptr_t>(
        sprite->color);
    const unsigned char *blendTable =
        reinterpret_cast<const unsigned char *>(tableAddress);

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
    clipRight = (std::min)(clipRight,
                           _gr_nScreenWidth-_gr_nScreenOriginX);
    clipBottom = (std::min)(clipBottom,
                            _gr_nScreenHeight-_gr_nScreenOriginY);

    const int left = (std::max)(sprite->x0,clipLeft);
    const int top = (std::max)(sprite->y0,clipTop);
    const int right = (std::min)(sprite->x1,clipRight);
    const int bottom = (std::min)(sprite->y1,clipBottom);
    if( right <= left || bottom <= top ) return;

    const std::int64_t spanX = sprite->x1-sprite->x0;
    const std::int64_t spanY = sprite->y1-sprite->y0;
    const std::int64_t spanU =
        static_cast<std::int64_t>(sprite->u1)-sprite->u0;
    const std::int64_t spanV =
        static_cast<std::int64_t>(sprite->v1)-sprite->v0;
    const int opacity = (std::max)(0,(std::min)(15,sprite->opacity>>4));

    for( int y = top; y < bottom; ++y ) {
        const std::int64_t v = sprite->v0+
            static_cast<std::int64_t>(y-sprite->y0)*spanV/spanY;
        const long sourceY = (std::max)(0L,(std::min)(
            texture->h-1,static_cast<long>(v>>16)));
        unsigned char *destination = _gr_pScreen+
            static_cast<std::size_t>(y+_gr_nScreenOriginY)*
                _gr_nScreenWidth+left+_gr_nScreenOriginX;
        for( int x = left; x < right; ++x,++destination ) {
            const std::int64_t u = sprite->u0+
                static_cast<std::int64_t>(x-sprite->x0)*spanU/spanX;
            const long sourceX = (std::max)(0L,(std::min)(
                texture->w-1,static_cast<long>(u>>16)));
            int alpha = texture->dataPtr[
                static_cast<std::size_t>(sourceY)*texture->w+sourceX];
            alpha = (std::max)(0,(std::min)(15,alpha));
            if( opacity < 15 ) alpha = opacity*(alpha+1)>>4;
            *destination = blendTable[alpha*256+*destination];
        }
    }
}

void DrawSoftwareSprite(int x0,int y0,int x1,int y1,
                        int u0,int v0,int u1,int v1,int iz,void *handle)
{
    (void)iz;
    if( _dL.currDevice == NULL || _dL.currDevice->swHw != GR_SOFTWARE ||
        _gr_pScreen == NULL || x1 <= x0 || y1 <= y0 ) return;

    SoftwareTexture *texture = TextureFromHandle(handle);
    if( texture == NULL || texture->dataPtr == NULL || texture->w <= 0 ||
        texture->h <= 0 ) return;

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
    clipRight = (std::min)(clipRight,
                           _gr_nScreenWidth-_gr_nScreenOriginX);
    clipBottom = (std::min)(clipBottom,
                            _gr_nScreenHeight-_gr_nScreenOriginY);

    const int left = (std::max)(x0,clipLeft);
    const int top = (std::max)(y0,clipTop);
    const int right = (std::min)(x1,clipRight);
    const int bottom = (std::min)(y1,clipBottom);
    if( right <= left || bottom <= top ) return;

    const std::int64_t spanX = x1-x0;
    const std::int64_t spanY = y1-y0;
    const std::int64_t spanU = static_cast<std::int64_t>(u1)-u0;
    const std::int64_t spanV = static_cast<std::int64_t>(v1)-v0;
    const bool transparent = (texture->flags & TEXTURE_SPRITE) != 0;

    for( int y = top; y < bottom; ++y ) {
        const std::int64_t v = v0+
            static_cast<std::int64_t>(y-y0)*spanV/spanY;
        const long sourceY = (std::max)(0L,(std::min)(
            texture->h-1,static_cast<long>(v>>16)));
        unsigned char *destination = _gr_pScreen+
            static_cast<std::size_t>(y+_gr_nScreenOriginY)*
                _gr_nScreenWidth+left+_gr_nScreenOriginX;
        for( int x = left; x < right; ++x,++destination ) {
            const std::int64_t u = u0+
                static_cast<std::int64_t>(x-x0)*spanU/spanX;
            const long sourceX = (std::max)(0L,(std::min)(
                texture->w-1,static_cast<long>(u>>16)));
            const unsigned char color = texture->dataPtr[
                static_cast<std::size_t>(sourceY)*texture->w+sourceX];
            if( !transparent || color != 0 ) *destination = color;
        }
    }
}

void DrawSoftwareParticle(int x,int y,int size,int inverseZ,
                          unsigned long color)
{
    if( _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL ||
        _gr_nScreenWidth <= 0 || _gr_nScreenHeight <= 0 ||
        size <= 0 || inverseZ <= 0 ) return;

    // The preserved software particle assembler is not safe to enter from the
    // modern Win32 runtime. Retain its palette-index contract (the low byte of
    // GRCreateColor) and bounded circular footprint, with explicit clipping.
    const int diameter = (std::min)(size,4096);
    const int radius = (std::max)(1,(diameter+1)/2);
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
    clipRight = (std::min)(clipRight,
                           _gr_nScreenWidth-_gr_nScreenOriginX);
    clipBottom = (std::min)(clipBottom,
                            _gr_nScreenHeight-_gr_nScreenOriginY);

    const int left = (std::max)(x-radius,clipLeft);
    const int top = (std::max)(y-radius,clipTop);
    const int right = (std::min)(x+radius+1,clipRight);
    const int bottom = (std::min)(y+radius+1,clipBottom);
    if( right <= left || bottom <= top ) return;

    const int radiusSquared = radius*radius;
    const unsigned char paletteIndex =
        static_cast<unsigned char>(color&0xffUL);
    for( int py = top; py < bottom; ++py ) {
        unsigned char *destination = _gr_pScreen+
            static_cast<std::size_t>(py+_gr_nScreenOriginY)*
                _gr_nScreenWidth+left+_gr_nScreenOriginX;
        const int dy = py-y;
        for( int px = left; px < right; ++px,++destination ) {
            const int dx = px-x;
            if( dx*dx+dy*dy <= radiusSquared )
                *destination = paletteIndex;
        }
    }
}

void SetSoftwareBump(int, int) {}

}  // namespace

void *(*_pGRLoadTextureToDB)(void *,unsigned char *,int,unsigned char *) =
    LoadSoftwareTexture;
void (*_pGRDeleteTextureFromDB)(void *) = DestroySoftwareTexture;
void (*_pGRDrawAlphaSprite)(SGRAlphaSprite *) = DrawSoftwareAlphaSprite;
void (*_pGRDrawSprite)(int,int,int,int,int,int,int,int,int,void *) =
    DrawSoftwareSprite;
void (*_pGRDrawParticle)(int,int,int,int,unsigned long) =
    DrawSoftwareParticle;
void (*_pGRSetBump)(int,int) = SetSoftwareBump;

void GRSetTextureLoadFunc(GR_HTEXTURE handle,const char *fileName,
                          TTextureLoadFunc loadFunc,void *user)
{
    SoftwareTexture *texture = TextureFromHandle(handle);
    if( texture == NULL ) return;
    if( fileName == NULL ) fileName = "";
    std::strncpy(texture->filename,fileName,sizeof(texture->filename)-1);
    texture->filename[sizeof(texture->filename)-1] = 0;
    texture->loadFunc = loadFunc;
    texture->loadFuncUserPar = user;
}

void GRReInitTextureDB()
{
    // Software textures are individually owned and do not use the recovered
    // DirectDraw texture database.
}

void GRSetTextureLoadIntFunc(GR_HTEXTURE handle,TTextureLoadIntFunc loadFunc)
{
    SoftwareTexture *texture = TextureFromHandle(handle);
    if( texture != NULL ) texture->loadIntFunc = loadFunc;
}
