#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "graph.h"
#include "h/cachesmoke.h"

extern SDeviceList _dL;

namespace {

bool Expect(bool condition,const char *message)
{
    if( condition ) return true;
    std::cerr << "legacy-menu-texture-smoke: " << message << '\n';
    return false;
}

bool WriteFixture(const std::string &path,unsigned short width,
                  unsigned short height,
                  const std::vector<unsigned char> &pixels)
{
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    const unsigned char header[] = {
        static_cast<unsigned char>(width&0xff),
        static_cast<unsigned char>(width>>8),
        static_cast<unsigned char>(height&0xff),
        static_cast<unsigned char>(height>>8),0};
    output.write(reinterpret_cast<const char *>(header),sizeof(header));
    if( !pixels.empty() ) output.write(
        reinterpret_cast<const char *>(pixels.data()),pixels.size());
    return output.good();
}

}  // namespace

int main(int argc,char **argv)
{
    static_assert(sizeof(void *) == sizeof(unsigned long),
                  "legacy alpha-table handles require the Win32 ABI");
    if( argc < 2 ) {
        std::cerr << "legacy-menu-texture-smoke: expected fixture path\n";
        return EXIT_FAILURE;
    }

    const std::string fixture = argv[1];
    const std::string truncated = fixture+".truncated";
    const std::string oversized = fixture+".oversized";
    std::remove(fixture.c_str());
    std::remove(truncated.c_str());
    std::remove(oversized.c_str());

    const std::vector<unsigned char> source = {
        0,16,128,255,
        255,128,16,0};
    if( !WriteFixture(fixture,4,2,source) ||
        !WriteFixture(truncated,4,2,{0,16}) ||
        !WriteFixture(oversized,65535,65535,{}) ) {
        return EXIT_FAILURE;
    }

    SDeviceDescr device = {};
    device.swHw = GR_SOFTWARE;
    _dL.currDevice = &device;
    _gr_nScreenWidth = 6;
    _gr_nScreenHeight = 4;
    _gr_nScreenOriginX = 0;
    _gr_nScreenOriginY = 0;
    _gr_clipRect.left = 1;
    _gr_clipRect.top = 0;
    _gr_clipRect.right = 4;
    _gr_clipRect.bottom = 2;

    std::vector<unsigned char> screen(24,7);
    _gr_pScreen = screen.data();
    unsigned char blendTable[16*256];
    for( int alpha = 0; alpha < 16; ++alpha )
        for( int color = 0; color < 256; ++color )
            blendTable[alpha*256+color] = alpha == 0 ?
                static_cast<unsigned char>(color) :
                static_cast<unsigned char>(40+alpha);

    g_cacheSmokeCnt = 0;
    void *texture = g_loadSmoke(fixture.c_str(),NULL);
    SGRAlphaSprite sprite = {};
    sprite.x0 = 0;
    sprite.y0 = 0;
    sprite.x1 = 4;
    sprite.y1 = 2;
    sprite.u0 = 0;
    sprite.v0 = 0;
    sprite.u1 = 4<<16;
    sprite.v1 = 2<<16;
    sprite.color = reinterpret_cast<unsigned long>(blendTable);
    sprite.opacity = 255;
    sprite.iz = 65535;
    sprite.hTexture = texture;

    if( !Expect(texture != NULL,"valid corona fixture was rejected") ||
        !Expect(g_loadSmoke(fixture.c_str(),NULL) == texture,
                "corona cache did not retain its texture handle") ||
        !Expect(g_loadSmoke(truncated.c_str(),NULL) == NULL,
                "truncated corona fixture was accepted") ||
        !Expect(g_loadSmoke(oversized.c_str(),NULL) == NULL,
                "oversized corona fixture was accepted") ) {
        return EXIT_FAILURE;
    }

    GRDrawAlphaSprite(&sprite);
    if( !Expect(screen[0] == 7 && screen[1] == 41 &&
                   screen[2] == 48 && screen[3] == 55 && screen[4] == 7,
               "full-opacity alpha row or horizontal clipping changed") ||
        !Expect(screen[6] == 7 && screen[7] == 48 &&
                   screen[8] == 41 && screen[9] == 7 && screen[10] == 7,
               "second alpha row or zero-alpha blending changed") ) {
        return EXIT_FAILURE;
    }

    std::fill(screen.begin(),screen.end(),static_cast<unsigned char>(7));
    _gr_clipRect.left = 0;
    _gr_clipRect.right = 4;
    sprite.y1 = 1;
    sprite.v1 = 1<<16;
    sprite.opacity = 128;
    GRDrawAlphaSprite(&sprite);
    if( !Expect(screen[0] == 7 && screen[1] == 41 &&
                   screen[2] == 44 && screen[3] == 48,
               "recovered 16x16 opacity multiplication changed") ) {
        return EXIT_FAILURE;
    }

    const std::vector<unsigned char> reloaded(8,255);
    if( !WriteFixture(fixture,4,2,reloaded) ||
        !Expect(g_loadSmoke(fixture.c_str(),texture) == texture,
                "in-place corona reload changed the handle") ) {
        return EXIT_FAILURE;
    }
    std::fill(screen.begin(),screen.end(),static_cast<unsigned char>(7));
    sprite.opacity = 255;
    GRDrawAlphaSprite(&sprite);
    if( !Expect(screen[0] == 55 && screen[1] == 55 &&
                   screen[2] == 55 && screen[3] == 55,
               "reloaded alpha pixels were not published") ) {
        return EXIT_FAILURE;
    }

    for( int i = 2; i < argc; ++i ) {
        g_cacheSmokeCnt = 0;
        void *retailTexture = g_loadSmoke(argv[i],NULL);
        if( !Expect(retailTexture != NULL,
                    "installed retail corona texture was rejected") )
            return EXIT_FAILURE;
        GRDeleteTextureFromDB(retailTexture);
    }

    const std::vector<unsigned char> screenBeforeMissingFramebuffer = screen;
    _gr_pScreen = NULL;
    GRDrawAlphaSprite(&sprite);
    if( !Expect(screen == screenBeforeMissingFramebuffer,
                "missing framebuffer changed the backing pixels") )
        return EXIT_FAILURE;

    g_cacheSmokeCnt = 0;
    GRDeleteTextureFromDB(texture);
    GRDeleteTextureFromDB(NULL);
    _dL.currDevice = NULL;
    _gr_pScreen = NULL;
    std::remove(fixture.c_str());
    std::remove(truncated.c_str());
    std::remove(oversized.c_str());
    std::cout << "legacy-menu-texture-smoke: OK\n";
    return EXIT_SUCCESS;
}
