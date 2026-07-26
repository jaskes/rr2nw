#include "graph.h"
#include "sd1_epal.h"

#include <cmath>
#include <limits.h>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern TExtendedPalette _EPal;
extern SDeviceList _dL;

// This owner recovers the complete 8-bit software-panel archive, lifecycle and
// draw path. The legacy Direct3D payload remains a separately measured
// frontier and is never represented by a no-op hardware implementation.

namespace {

const char kPanelFileId[] = "PNL";
const int kFontHeaderSize = 2832;

struct PanelFileHeader {
    char id[3];
    unsigned char version;
    int resolCount;
    unsigned char palette[768];
};

struct PanelFileResolution {
    int width;
    int height;
    int originX;
    int originY;
    CRect2 clip;
    int swPanelSize;
    int hwPanelSize;
    int hwPanelTexturesSize;
};

struct PanelViewportMetadata {
    int originX;
    int originY;
    CRect2 clip;
    int swPanelSize;
};

struct PanelRuntimeMetadata {
    PanelViewportMetadata resolution[MAX_PANEL_RESOLUTIONS];
};

std::map<CGRPanel*,PanelRuntimeMetadata> panelMetadata;

bool ReadExact(FILE *fp,void *buffer,size_t size)
{
    return size == 0 || fread(buffer,1,size,fp) == size;
}

bool SkipExact(FILE *fp,int size)
{
    if( size < 0 ) return false;
    long current = ftell(fp);
    if( current < 0 || fseek(fp,0,SEEK_END) != 0 ) return false;
    long end = ftell(fp);
    if( end < current || size > end-current ) return false;
    return fseek(fp,current+size,SEEK_SET) == 0;
}

bool HasRemaining(FILE *fp,int size)
{
    if( size < 0 ) return false;
    long current = ftell(fp);
    if( current < 0 || fseek(fp,0,SEEK_END) != 0 ) return false;
    long end = ftell(fp);
    bool valid = end >= current && size <= end-current;
    return fseek(fp,current,SEEK_SET) == 0 && valid;
}

void ConvertPanelColor(unsigned long *color,bool createColor)
{
    int r = (int)(*color>>16);
    int g = (int)((*color&0xFF00)>>8);
    int b = (int)(*color&0xFF);
    *color = createColor ? GRCreateColor(r,g,b) : GRFillColor(r,g,b);
}

bool ValidateFont(const unsigned char *data,int size)
{
    struct FontFileHeader {
        char id[4];
        int32_t width;
        int32_t height;
        unsigned char palette[768];
        int32_t glyphTable[256*2];
        int32_t spriteSize;
    };
    static_assert(sizeof(FontFileHeader) == kFontHeaderSize,
                  "legacy fixed-font header ABI changed");

    if( data == NULL || size < kFontHeaderSize ) return false;
    FontFileHeader header;
    memcpy(&header,data,sizeof(header));
    if( memcmp(header.id,FONT_ID,sizeof(FONT_ID)-1) != 0 ||
        header.width <= 0 || header.height <= 0 || header.spriteSize <= 0 ||
        header.spriteSize > size-kFontHeaderSize ) return false;
    for( int i = 0; i < 256; ++i ) {
        int32_t width = header.glyphTable[i*2];
        int32_t offset = header.glyphTable[i*2+1];
        if( width < 0 || width > header.width || offset < 0 ) return false;
        if( width > 0 && (int64_t)offset+
            (int64_t)(header.height-1)*header.width+width >
            header.spriteSize ) return false;
    }
    return true;
}

void PutPixel(int x,int y,unsigned char color)
{
    if( x >= 0 && y >= 0 && x < _gr_nScreenWidth &&
        y < _gr_nScreenHeight )
        _gr_pScreen[(size_t)y*_gr_nScreenWidth+x] = color;
}

void DrawLine(int x0,int y0,int x1,int y1,unsigned char color)
{
    long long rise = (long long)y1-y0;
    int stepY = rise < 0 ? -1 : 1;
    if( rise < 0 ) rise = -rise;
    int diagonalY = stepY;

    long long run = (long long)x1-x0;
    int stepX = run < 0 ? -1 : 1;
    if( run < 0 ) run = -run;
    int diagonalX = stepX;
    if( run+rise >
        ((long long)_gr_nScreenWidth+_gr_nScreenHeight)*8 ) return;

    if( run < rise ) {
        long long swap = run;
        stepX = 0;
        run = rise;
        rise = swap;
    } else {
        stepY = 0;
    }
    long long increment = rise*2;
    long long decision = increment-run;
    long long diagonalIncrement = decision-run;
    for( ; run >= 0; --run ) {
        PutPixel(x0,y0,color);
        if( run == 0 ) break;
        if( decision < 0 ) {
            x0 += stepX;
            y0 += stepY;
            decision += increment;
        } else {
            x0 += diagonalX;
            y0 += diagonalY;
            decision += diagonalIncrement;
        }
    }
}

double Edge(int ax,int ay,int bx,int by,int px,int py)
{
    return ((double)px-ax)*((double)by-ay)-
           ((double)py-ay)*((double)bx-ax);
}

void DrawTriangle(int x0,int y0,int x1,int y1,int x2,int y2,
                  unsigned char color)
{
    int left = x0;
    int right = x0;
    int top = y0;
    int bottom = y0;
    if( x1 < left ) left = x1;
    if( x2 < left ) left = x2;
    if( x1 > right ) right = x1;
    if( x2 > right ) right = x2;
    if( y1 < top ) top = y1;
    if( y2 < top ) top = y2;
    if( y1 > bottom ) bottom = y1;
    if( y2 > bottom ) bottom = y2;
    if( left < 0 ) left = 0;
    if( top < 0 ) top = 0;
    if( right >= _gr_nScreenWidth ) right = _gr_nScreenWidth-1;
    if( bottom >= _gr_nScreenHeight ) bottom = _gr_nScreenHeight-1;

    for( int y = top; y <= bottom; ++y )
        for( int x = left; x <= right; ++x ) {
            double edge0 = Edge(x0,y0,x1,y1,x,y);
            double edge1 = Edge(x1,y1,x2,y2,x,y);
            double edge2 = Edge(x2,y2,x0,y0,x,y);
            if( (edge0 >= 0 && edge1 >= 0 && edge2 >= 0) ||
                (edge0 <= 0 && edge1 <= 0 && edge2 <= 0) )
                PutPixel(x,y,color);
        }
}

bool DrawPanelRuns(const unsigned char *block,int blockSize)
{
    const int dataOffset = sizeof(int)*2;
    if( block == NULL || blockSize < dataOffset+2 ) return false;
    int xStart = 0;
    int yStart = 0;
    memcpy(&xStart,block,sizeof(xStart));
    memcpy(&yStart,block+sizeof(xStart),sizeof(yStart));
    if( xStart < 0 || yStart < 0 || xStart >= _gr_nScreenWidth ||
        yStart >= _gr_nScreenHeight ) return false;

    const unsigned char *cursor = block+dataOffset;
    const unsigned char *end = block+blockSize;
    size_t output = (size_t)yStart*_gr_nScreenWidth+xStart;
    size_t screenSize = (size_t)_gr_nScreenWidth*_gr_nScreenHeight;
    while( cursor+2 <= end ) {
        unsigned short flag = 0;
        memcpy(&flag,cursor,sizeof(flag));
        cursor += 2;
        if( flag == 0 ) return true;
        size_t count = flag&0x7FFF;
        if( count > screenSize-output ) return false;
        if( flag&0x8000 ) {
            if( count > (size_t)(end-cursor) ) return false;
            memcpy(_gr_pScreen+output,cursor,count);
            cursor += count;
        }
        output += count;
    }
    return false;
}

bool ToInt(double value,int *result)
{
    if( result == NULL || !std::isfinite(value) || value < INT_MIN ||
        value > INT_MAX ) return false;
    *result = (int)value;
    return true;
}

} // namespace

CGRPanel::CGRPanel(const char *filename)
{
    static_assert(sizeof(PanelFileHeader) == 776,
                  "legacy panel header ABI changed");
    static_assert(sizeof(PanelFileResolution) == 44,
                  "legacy panel resolution ABI changed");
    static_assert(sizeof(SControl) == 100,
                  "legacy panel control ABI changed");

    resolCount = 0;
    currentPanel = NULL;
    drawCrosshair = 1;
    drawPanel = 1;
    memset(panel,0,sizeof(panel));

    if( filename == NULL || _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE ) return;

    FILE *fp = fopen(filename,"rb");
    if( fp == NULL ) return;

    auto Cleanup = [&](int count) {
        for( int i = 0; i < count; ++i ) {
            GRReleaseViewport(panel[i].panelViewport);
            panel[i].panelViewport = NULL;
            delete [] panel[i].pSWPanel;
            panel[i].pSWPanel = NULL;
            delete [] panel[i].pHWPanel;
            panel[i].pHWPanel = NULL;
            for( int j = 0; j < panel[i].controlCount; ++j ) {
                SControl *control = &panel[i].control[j];
                if( control->type == PANEL_CTRL_MOVE_SPRITE )
                    delete control->moveSprite.sprite;
                else if( control->type == PANEL_CTRL_FILL_SPRITE )
                    delete control->fillSprite.sprite;
                else if( control->type == PANEL_CTRL_DIGITS )
                    delete control->digits.font;
            }
            panel[i].controlCount = 0;
        }
        resolCount = 0;
        currentPanel = NULL;
    };

    PanelFileHeader header;
    if( !ReadExact(fp,&header,sizeof(header)) ||
        memcmp(header.id,kPanelFileId,3) != 0 || header.version > 0 ||
        header.resolCount < 0 || header.resolCount > MAX_PANEL_RESOLUTIONS ) {
        fclose(fp);
        return;
    }

    PanelRuntimeMetadata metadata = {};

    int paletteRelocation[256];
    for( int i = 0; i < 256; ++i )
        paletteRelocation[i] = epal_Match(
            _EPal,RGB_i(header.palette[i*3],header.palette[i*3+1],
                        header.palette[i*3+2]));

    for( int i = 0; i < header.resolCount; ++i ) {
        PanelFileResolution filePanel;
        if( !ReadExact(fp,&filePanel,sizeof(filePanel)) ||
            filePanel.width <= 0 || filePanel.height <= 0 ||
            filePanel.width > INT_MAX/filePanel.height ||
            filePanel.swPanelSize < 0 || filePanel.hwPanelSize < 0 ||
            filePanel.hwPanelTexturesSize < 0 || filePanel.originX < 0 ||
            filePanel.originX > filePanel.width || filePanel.originY < 0 ||
            filePanel.originY > filePanel.height ||
            filePanel.clip.left < 0 || filePanel.clip.top < 0 ||
            filePanel.clip.right < filePanel.clip.left ||
            filePanel.clip.bottom < filePanel.clip.top ||
            filePanel.clip.right > filePanel.width ||
            filePanel.clip.bottom > filePanel.height ) {
            Cleanup(i+1);
            fclose(fp);
            return;
        }

        SPanel *loaded = &panel[i];
        loaded->width = filePanel.width;
        loaded->height = filePanel.height;
        loaded->open = 0;
        metadata.resolution[i].originX = filePanel.originX;
        metadata.resolution[i].originY = filePanel.originY;
        metadata.resolution[i].clip = filePanel.clip;
        metadata.resolution[i].swPanelSize = filePanel.swPanelSize;

        if( filePanel.swPanelSize > 0 ) {
            const int dataOffset = (int)offsetof(SSwPanel,pPanel);
            if( filePanel.swPanelSize < dataOffset+2 ) {
                Cleanup(i+1);
                fclose(fp);
                return;
            }
            if( !HasRemaining(fp,filePanel.swPanelSize) ) {
                Cleanup(i+1);
                fclose(fp);
                return;
            }
            loaded->pSWPanel =
                (SSwPanel*)new char [filePanel.swPanelSize];
            if( !ReadExact(fp,loaded->pSWPanel,filePanel.swPanelSize) ) {
                Cleanup(i+1);
                fclose(fp);
                return;
            }

            unsigned char *cursor = loaded->pSWPanel->pPanel;
            unsigned char *end =
                (unsigned char*)loaded->pSWPanel+filePanel.swPanelSize;
            bool terminated = false;
            unsigned long long decodedPixels = 0;
            unsigned long long panelPixels =
                (unsigned long long)filePanel.width*filePanel.height;
            if( loaded->pSWPanel->xStart < 0 ||
                loaded->pSWPanel->yStart < 0 ||
                loaded->pSWPanel->xStart >= filePanel.width ||
                loaded->pSWPanel->yStart >= filePanel.height ) {
                Cleanup(i+1);
                fclose(fp);
                return;
            }
            unsigned long long startPixel =
                (unsigned long long)loaded->pSWPanel->yStart*filePanel.width+
                loaded->pSWPanel->xStart;
            while( cursor+2 <= end ) {
                unsigned short flag = 0;
                memcpy(&flag,cursor,sizeof(flag));
                cursor += 2;
                if( flag == 0 ) { terminated = true; break; }
                unsigned int runLength = flag&0x7FFF;
                decodedPixels += runLength;
                if( decodedPixels > panelPixels-startPixel )
                    break;
                if( flag&0x8000 ) {
                    int count = (int)runLength;
                    if( count > end-cursor ) break;
                    for( int j = 0; j < count; ++j )
                        cursor[j] =
                            (unsigned char)paletteRelocation[cursor[j]];
                    cursor += count;
                }
            }
            if( !terminated ) {
                Cleanup(i+1);
                fclose(fp);
                return;
            }
        } else {
            metadata.resolution[i].originX = filePanel.width/2;
            metadata.resolution[i].originY = filePanel.height/2;
            metadata.resolution[i].clip =
                CRect2(0,0,filePanel.width,filePanel.height);
        }

        if( !SkipExact(fp,filePanel.hwPanelSize) ||
            !SkipExact(fp,filePanel.hwPanelTexturesSize) ||
            !ReadExact(fp,&loaded->controlCount,sizeof(int)) ||
            loaded->controlCount < 0 ||
            loaded->controlCount > MAX_PANEL_CONTROLS ) {
            loaded->controlCount = 0;
            Cleanup(i+1);
            fclose(fp);
            return;
        }

        int controlCount = loaded->controlCount;
        loaded->controlCount = 0;
        for( int j = 0; j < controlCount; ++j ) {
            SControl *control = &loaded->control[j];
            if( !ReadExact(fp,control,sizeof(*control)) ) {
                Cleanup(i+1);
                fclose(fp);
                return;
            }
            loaded->controlCount = j+1;
            if( memchr(control->name,0,sizeof(control->name)) == NULL ) {
                Cleanup(i+1);
                fclose(fp);
                return;
            }

            switch( control->type ) {
                case PANEL_CTRL_INDICATOR:
                    ConvertPanelColor(&control->indicator.color0,true);
                    ConvertPanelColor(&control->indicator.color1,true);
                    control->controlData.fValue = control->indicator.alpha0;
                break;
                case PANEL_CTRL_ARROW:
                    ConvertPanelColor(&control->arrow.color,false);
                    control->controlData.fValue = control->arrow.alpha0;
                break;
                case PANEL_CTRL_MOVE_SPRITE:
                case PANEL_CTRL_FILL_SPRITE: {
                    if( control->type == PANEL_CTRL_MOVE_SPRITE )
                        control->moveSprite.sprite = NULL;
                    else
                        control->fillSprite.sprite = NULL;
                    int width = 0,height = 0;
                    if( !ReadExact(fp,&width,sizeof(width)) ||
                        !ReadExact(fp,&height,sizeof(height)) || width <= 0 ||
                        height <= 0 || width > INT_MAX/height ) {
                        Cleanup(i+1);
                        fclose(fp);
                        return;
                    }
                    int imageSize = width*height;
                    if( !HasRemaining(fp,imageSize) ) {
                        Cleanup(i+1);
                        fclose(fp);
                        return;
                    }
                    unsigned char *data = new unsigned char [imageSize];
                    if( !ReadExact(fp,data,imageSize) ) {
                        delete [] data;
                        Cleanup(i+1);
                        fclose(fp);
                        return;
                    }
                    CGRImage *image = new CGRImage(width,height);
                    image->SetPalette(header.palette);
                    image->LoadPalImage(data,0,0,0,0,width-1,height-1,width);
                    delete [] data;
                    if( control->type == PANEL_CTRL_MOVE_SPRITE ) {
                        control->moveSprite.sprite = image;
                        control->controlData.fValue =
                            control->moveSprite.type == CTRL_MOVE_SPRITE_ARC ?
                            control->moveSprite.alpha0 : 0.0f;
                    } else {
                        control->fillSprite.sprite = image;
                        control->controlData.fValue = 0.0f;
                    }
                }
                break;
                case PANEL_CTRL_DIGITS: {
                    control->digits.font = NULL;
                    int size = 0;
                    if( control->digits.digitsCount < 0 ||
                        control->digits.digitsCount > CTRL_DIGITS_MAX_COUNT ||
                        !ReadExact(fp,&size,sizeof(size)) || size <= 0 ) {
                        Cleanup(i+1);
                        fclose(fp);
                        return;
                    }
                    if( !HasRemaining(fp,size) ) {
                        Cleanup(i+1);
                        fclose(fp);
                        return;
                    }
                    unsigned char *data = new unsigned char [size];
                    if( !ReadExact(fp,data,size) || !ValidateFont(data,size) ) {
                        delete [] data;
                        Cleanup(i+1);
                        fclose(fp);
                        return;
                    }
                    control->digits.font = new CFixedColorFont();
                    if( !control->digits.font->ReadFromMemory(data) ) {
                        delete [] data;
                        Cleanup(i+1);
                        fclose(fp);
                        return;
                    }
                    delete [] data;
                    strcpy(control->controlData.sValue,"000");
                }
                break;
            }
        }
        resolCount = i+1;
    }
    fclose(fp);
    panelMetadata[this] = metadata;

    const char *dot = strrchr(filename,'.');
    if( dot != NULL && dot-filename < 252 ) {
        char crosshairName[256];
        size_t prefix = (size_t)(dot-filename+1);
        memcpy(crosshairName,filename,prefix);
        memcpy(crosshairName+prefix,"crh",4);
        FILE *crosshairFile = fopen(crosshairName,"rb");
        if( crosshairFile != NULL ) {
            fclose(crosshairFile);
            crosshair.LoadFromBMPFile(crosshairName,0,0);
        }
    }
}

CGRPanel::~CGRPanel()
{
    if( currentPanel != NULL && currentPanel->open ) Close();
    for( int i = 0; i < resolCount; ++i ) {
        GRReleaseViewport(panel[i].panelViewport);
        delete [] panel[i].pSWPanel;
        delete [] panel[i].pHWPanel;
        for( int j = 0; j < panel[i].controlCount; ++j ) {
            SControl *control = &panel[i].control[j];
            if( control->type == PANEL_CTRL_MOVE_SPRITE )
                delete control->moveSprite.sprite;
            else if( control->type == PANEL_CTRL_FILL_SPRITE )
                delete control->fillSprite.sprite;
            else if( control->type == PANEL_CTRL_DIGITS )
                delete control->digits.font;
        }
    }
    resolCount = 0;
    currentPanel = NULL;
    panelMetadata.erase(this);
}

void CGRPanel::SetResolution(int width,int height)
{
    if( resolCount == 0 ) return;
    if( currentPanel != NULL ) {
        if( currentPanel->open ) Close();
        currentPanel = NULL;
    }
    for( int i = 0; i < resolCount; ++i ) {
        GRReleaseViewport(panel[i].panelViewport);
        panel[i].panelViewport = NULL;
    }

    std::map<CGRPanel*,PanelRuntimeMetadata>::iterator metadata =
        panelMetadata.find(this);
    if( metadata == panelMetadata.end() || _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE ||
        width != _gr_nScreenWidth || height != _gr_nScreenHeight ||
        _gr_pScreen == NULL ) return;
    for( int i = 0; i < resolCount; ++i )
        if( panel[i].width == width && panel[i].height == height ) {
            currentPanel = &panel[i];
            PanelViewportMetadata &viewport = metadata->second.resolution[i];
            currentPanel->panelViewport = GRCreateViewport(
                viewport.originX,viewport.originY,viewport.clip);
            break;
        }
}

SGRViewport *CGRPanel::Open()
{
    if( resolCount == 0 || currentPanel == NULL ||
        currentPanel->panelViewport == NULL ) return NULL;
    currentPanel->open = 1;
    return currentPanel->panelViewport;
}

void CGRPanel::Close()
{
    if( resolCount == 0 || currentPanel == NULL || !currentPanel->open ) return;
    currentPanel->open = 0;
}

int CGRPanel::IsDigitControl(const char *name)
{
    if( name == NULL || resolCount == 0 || currentPanel == NULL ) return 0;
    for( int i = 0; i < currentPanel->controlCount; ++i )
        if( strcmp(name,currentPanel->control[i].name) == 0 )
            return currentPanel->control[i].type == PANEL_CTRL_DIGITS;
    return 0;
}

void CGRPanel::SetControlValue(char *name,unsigned long value)
{
    if( name == NULL || resolCount == 0 || currentPanel == NULL ) return;
    float input = 0.0f;
    memcpy(&input,&value,sizeof(input));

    for( int i = 0; i < currentPanel->controlCount; ++i ) {
        SControl *control = &currentPanel->control[i];
        if( strcmp(name,control->name) != 0 ) continue;
        switch( control->type ) {
            case PANEL_CTRL_INDICATOR:
                control->controlData.fValue = control->indicator.alpha0+
                    (control->indicator.alpha1-control->indicator.alpha0)*input;
            break;
            case PANEL_CTRL_ARROW:
                control->controlData.fValue = control->arrow.alpha0+
                    (control->arrow.alpha1-control->arrow.alpha0)*input;
            break;
            case PANEL_CTRL_MOVE_SPRITE:
                if( control->moveSprite.type == CTRL_MOVE_SPRITE_ARC )
                    control->controlData.fValue = control->moveSprite.alpha0+
                        (control->moveSprite.alpha1-
                         control->moveSprite.alpha0)*input;
                else if( control->moveSprite.type == CTRL_MOVE_SPRITE_LINE )
                    control->controlData.fValue = input;
            break;
            case PANEL_CTRL_FILL_SPRITE:
                control->controlData.fValue = input;
            break;
            case PANEL_CTRL_DIGITS:
                snprintf(control->controlData.sValue,
                         sizeof(control->controlData.sValue),"%0*d",
                         control->digits.digitsCount,(int)value);
                control->controlData.sValue[
                    sizeof(control->controlData.sValue)-1] = 0;
            break;
        }
    }
}

void CGRPanel::DrawSector(SControl *control,float angle0,float angle1,
                          unsigned long color)
{
    const double sectorStep = 20.0*3.14159265358979323846/180.0;
    double difference = (double)angle1-angle0;
    if( control == NULL || !std::isfinite(difference) ||
        !std::isfinite(control->indicator.r) || control->indicator.r <= 0 )
        return;

    double segmentValue = std::fabs(difference)/sectorStep;
    if( segmentValue > 64.0 ) return;
    int segmentCount = (int)segmentValue;
    if( segmentCount == 0 ) segmentCount = 1;
    double denominator = std::cos(difference/(segmentCount*2.0));
    if( !std::isfinite(denominator) || std::fabs(denominator) < 0.000001 )
        return;
    double radius = control->indicator.r/denominator;
    if( !std::isfinite(radius) ) return;

    int centerX = control->indicator.x;
    int centerY = control->indicator.y;
    double previousAngle = angle1;
    int previousX = 0;
    int previousY = 0;
    if( !ToInt(centerX+radius*std::cos(previousAngle),&previousX) ||
        !ToInt(centerY-radius*std::sin(previousAngle),&previousY) ) return;
    for( int i = 1; i <= segmentCount; ++i ) {
        double angle = angle1-difference*i/segmentCount;
        int nextX = 0;
        int nextY = 0;
        if( !ToInt(centerX+radius*std::cos(angle),&nextX) ||
            !ToInt(centerY-radius*std::sin(angle),&nextY) ) return;
        DrawTriangle(centerX,centerY,previousX,previousY,nextX,nextY,
                     (unsigned char)color);
        previousX = nextX;
        previousY = nextY;
    }
}

void CGRPanel::DrawControlsFirst()
{
    if( currentPanel == NULL ) return;
    for( int i = 0; i < currentPanel->controlCount; ++i ) {
        SControl *control = &currentPanel->control[i];
        if( control->type != PANEL_CTRL_INDICATOR ) continue;
        DrawSector(control,control->indicator.alpha0,
                   control->indicator.alpha1,control->indicator.color1);
        DrawSector(control,control->indicator.alpha0,
                   control->controlData.fValue,control->indicator.color0);
    }
}

void CGRPanel::DrawControlsLast()
{
    if( currentPanel == NULL ) return;
    for( int i = 0; i < currentPanel->controlCount; ++i ) {
        SControl *control = &currentPanel->control[i];
        switch( control->type ) {
            case PANEL_CTRL_ARROW: {
                double value = control->controlData.fValue;
                if( !std::isfinite(value) ||
                    !std::isfinite(control->arrow.r) ||
                    !std::isfinite(control->arrow.rTrsp) ) break;
                double sine = -std::sin(value);
                double cosine = std::cos(value);
                int x0 = 0,y0 = 0,x1 = 0,y1 = 0;
                if( ToInt(control->arrow.x+cosine*control->arrow.rTrsp,&x0) &&
                    ToInt(control->arrow.y+sine*control->arrow.rTrsp,&y0) &&
                    ToInt(control->arrow.x+cosine*control->arrow.r,&x1) &&
                    ToInt(control->arrow.y+sine*control->arrow.r,&y1) )
                    DrawLine(x0,y0,x1,y1,(unsigned char)control->arrow.color);
            }
            break;

            case PANEL_CTRL_MOVE_SPRITE: {
                CGRImage *sprite = control->moveSprite.sprite;
                double value = control->controlData.fValue;
                if( sprite == NULL || !std::isfinite(value) ) break;
                int x = 0,y = 0;
                if( control->moveSprite.type == CTRL_MOVE_SPRITE_ARC ) {
                    if( !std::isfinite(control->moveSprite.r) ||
                        !ToInt(control->moveSprite.x+
                               control->moveSprite.r*std::cos(value),&x) ||
                        !ToInt(control->moveSprite.y-
                               control->moveSprite.r*std::sin(value),&y) )
                        break;
                    int destinationX = 0,destinationY = 0;
                    if( ToInt((double)x-sprite->Width()/2,&destinationX) &&
                        ToInt((double)y-sprite->Height()/2,&destinationY) )
                        sprite->DrawSprite(destinationX,destinationY);
                } else if( control->moveSprite.type ==
                           CTRL_MOVE_SPRITE_LINE ) {
                    if( !ToInt(control->moveSprite.x+value*
                               (control->moveSprite.x1-
                                (double)control->moveSprite.x),&x) ||
                        !ToInt(control->moveSprite.y+value*
                               (control->moveSprite.y1-
                                (double)control->moveSprite.y),&y) ) break;
                    int destinationX = 0,destinationY = 0;
                    if( ToInt((double)x-sprite->Width()/2,&destinationX) &&
                        ToInt((double)y-sprite->Height()/2,&destinationY) )
                        sprite->DrawSprite(destinationX,destinationY);
                } else if( control->moveSprite.type ==
                           CTRL_MOVE_SPRITE_BOX ) {
                    int sourceX = 0,sourceY = 0;
                    if( !ToInt(control->moveSprite.u+value*
                               (control->moveSprite.u1-
                                (double)control->moveSprite.u),&sourceX) ||
                        !ToInt(control->moveSprite.v+value*
                               (control->moveSprite.v1-
                                (double)control->moveSprite.v),&sourceY) )
                        break;
                    int sourceRight = 0,sourceBottom = 0;
                    if( !ToInt((double)sourceX+control->moveSprite.x1,
                               &sourceRight) ||
                        !ToInt((double)sourceY+control->moveSprite.y1,
                               &sourceBottom) ) break;
                    sprite->DrawSprite(control->moveSprite.x,
                        control->moveSprite.y,sourceX,sourceY,
                        sourceRight,sourceBottom);
                }
            }
            break;

            case PANEL_CTRL_FILL_SPRITE: {
                CGRImage *sprite = control->fillSprite.sprite;
                double value = control->controlData.fValue;
                if( sprite == NULL || !std::isfinite(value) ) break;
                int destinationX = control->fillSprite.x;
                int destinationY = control->fillSprite.y;
                int x0 = 0,y0 = 0,x1 = 0,y1 = 0;
                bool valid = true;
                switch( control->fillSprite.type ) {
                    case CTRL_FILL_SPRITE_LR:
                        valid = ToInt((sprite->Width()-1)*value,&x1);
                        y1 = sprite->Height()-1;
                    break;
                    case CTRL_FILL_SPRITE_RL:
                        valid = ToInt((sprite->Width()-1)*(1.0-value),&x0);
                        x1 = sprite->Width()-1;
                        y1 = sprite->Height()-1;
                        valid = valid && ToInt((double)destinationX+x0,
                                               &destinationX);
                    break;
                    case CTRL_FILL_SPRITE_TB:
                        x1 = sprite->Width()-1;
                        valid = ToInt((sprite->Height()-1)*value,&y1);
                    break;
                    case CTRL_FILL_SPRITE_BT:
                        valid = ToInt((sprite->Height()-1)*(1.0-value),&y0);
                        x1 = sprite->Width()-1;
                        y1 = sprite->Height()-1;
                        valid = valid && ToInt((double)destinationY+y0,
                                               &destinationY);
                    break;
                    default:
                        valid = false;
                    break;
                }
                if( valid ) sprite->DrawSprite(destinationX,destinationY,
                                               x0,y0,x1,y1);
            }
            break;

            case PANEL_CTRL_DIGITS:
                if( control->digits.font != NULL ) {
                    char digit[2] = {0,0};
                    for( int j = 0; j < control->digits.digitsCount; ++j ) {
                        digit[0] = control->controlData.sValue[j];
                        control->digits.font->LUPrintAt(control->digits.x[j],
                                                       control->digits.y[j],
                                                       digit);
                    }
                }
            break;
        }
    }
}

void CGRPanel::Draw()
{
    if( _dL.currDevice == NULL ||
        _dL.currDevice->swHw != GR_SOFTWARE || _gr_pScreen == NULL ||
        _gr_nScreenWidth <= 0 || _gr_nScreenHeight <= 0 ) return;

    if( drawPanel && currentPanel != NULL && currentPanel->open &&
        currentPanel->width == _gr_nScreenWidth &&
        currentPanel->height == _gr_nScreenHeight ) {
        DrawControlsFirst();
        if( currentPanel->pSWPanel != NULL ) {
            std::map<CGRPanel*,PanelRuntimeMetadata>::iterator metadata =
                panelMetadata.find(this);
            int index = (int)(currentPanel-panel);
            if( metadata != panelMetadata.end() && index >= 0 &&
                index < resolCount )
                DrawPanelRuns((const unsigned char*)currentPanel->pSWPanel,
                    metadata->second.resolution[index].swPanelSize);
        }
        DrawControlsLast();
    }

    if( drawCrosshair )
        crosshair.DrawSprite(_gr_nScreenOriginX-crosshair.Width()/2,
                             _gr_nScreenOriginY-crosshair.Height()/2);
}
