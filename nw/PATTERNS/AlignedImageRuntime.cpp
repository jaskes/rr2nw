#define LAST_H__VIEW
#include "game.h"

#include <cstddef>
#include <limits>

byte *PTNCreate2AlignedImage(int w,int h,int bytesPixel,dword flags)
{
    if( w <= 0 || h <= 0 || bytesPixel <= 0 || bytesPixel > 4 )
        return NULL;

    const size_t width = static_cast<size_t>(w);
    const size_t height = static_cast<size_t>(h);
    const size_t bytes = static_cast<size_t>(bytesPixel);
    if( width > 4096 || height > 4096 ||
        width > (std::numeric_limits<size_t>::max)()/height ||
        width*height >
            ((std::numeric_limits<size_t>::max)()-
             sizeof(PTNS2AlignedImageHeader))/bytes )
        return NULL;

    byte *base = new byte[
        sizeof(PTNS2AlignedImageHeader)+width*height*bytes];

    PTNS2AlignedImageHeader *header =
        reinterpret_cast<PTNS2AlignedImageHeader *>(base);
    header->pCache = NULL;
    header->flags = flags;
    header->height = h;
    header->width = w;
    return base+sizeof(PTNS2AlignedImageHeader);
}

byte *PTNUnload2AlignedImage(byte *p)
{
    if( p ) {
        PTNS2AlignedImageHeader *header = PTNGet2AlignedImageHeader(p);
        if( header->pCache )
            delete [] (header->pCache-header->height);
        delete [] reinterpret_cast<byte *>(header);
    }
    return NULL;
}
