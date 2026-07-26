#ifndef RR2NW_SMOKE_TEXTURE_CACHE_INL
#define RR2NW_SMOKE_TEXTURE_CACHE_INL

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

cs_CacheSmoke g_cacheSmoke[10] = {};
int g_cacheSmokeCnt = 0;

GR_HTEXTURE g_loadSmoke(const char *fileName,GR_HTEXTURE texture,void *)
{
    if( fileName == NULL || fileName[0] == 0 ) return NULL;

    if( texture == NULL ) {
        const int cacheCount = (std::max)(0,(std::min)(10,g_cacheSmokeCnt));
        for( int i = 0; i < cacheCount; ++i )
            if( strcmpi(g_cacheSmoke[i].fname,fileName) == 0 )
                return g_cacheSmoke[i].hand;
    }

    long fileLength = 0;
    FILE *file = CFileResource::FOpenCurrent(fileName,&fileLength);
    if( file == NULL || fileLength < 5 ) {
        if( file != NULL ) fclose(file);
        return NULL;
    }

    unsigned char header[5];
    if( fread(header,1,sizeof(header),file) != sizeof(header) ) {
        fclose(file);
        return NULL;
    }
    const unsigned int width = header[0] | (header[1]<<8);
    const unsigned int height = header[2] | (header[3]<<8);
    const std::uint64_t pixelCount =
        static_cast<std::uint64_t>(width)*height;
    if( width == 0 || height == 0 || width > 4096 || height > 4096 ||
        pixelCount > 64ULL*1024ULL*1024ULL ||
        pixelCount+sizeof(header) > static_cast<std::uint64_t>(fileLength) ) {
        fclose(file);
        return NULL;
    }

    const std::size_t storageSize = static_cast<std::size_t>(pixelCount)+12;
    unsigned char *storage = new (std::nothrow) unsigned char[storageSize];
    if( storage == NULL ) {
        fclose(file);
        return NULL;
    }

    const dword flags = TEXTURE_ALPHA|TEXTURE_PAL_FORMAT;
    const dword storedHeight = height;
    const dword storedWidth = width;
    std::memcpy(storage,&flags,sizeof(flags));
    std::memcpy(storage+4,&storedHeight,sizeof(storedHeight));
    std::memcpy(storage+8,&storedWidth,sizeof(storedWidth));
    const std::size_t pixels = static_cast<std::size_t>(pixelCount);
    const bool readOK = fread(storage+12,1,pixels,file) == pixels;
    fclose(file);
    if( !readOK || _pGRLoadTextureToDB == NULL ) {
        delete [] storage;
        return NULL;
    }

    GR_HTEXTURE result = GRLoadTextureToDB(texture,NULL,0,storage+12);
    delete [] storage;
    if( result == NULL ) return NULL;

    GRSetTextureLoadFunc(result,fileName,g_loadSmoke);
    if( texture == NULL && g_cacheSmokeCnt >= 0 && g_cacheSmokeCnt < 10 ) {
        std::strncpy(g_cacheSmoke[g_cacheSmokeCnt].fname,fileName,
                     sizeof(g_cacheSmoke[g_cacheSmokeCnt].fname)-1);
        g_cacheSmoke[g_cacheSmokeCnt]
            .fname[sizeof(g_cacheSmoke[g_cacheSmokeCnt].fname)-1] = 0;
        g_cacheSmoke[g_cacheSmokeCnt].hand = result;
        ++g_cacheSmokeCnt;
    }
    return result;
}

#endif
