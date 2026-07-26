#include "graph.h"
#include "sd1_epal.h"

#include <limits.h>
#include <map>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern TExtendedPalette _EPal;
extern SDeviceList _dL;

// This owner intentionally stops before CGRPanel::Draw. It recovers the
// complete software-panel archive and lifecycle path while the polygon/ASM
// drawing backend remains a separately measured frontier.

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
    if( data == NULL || size < kFontHeaderSize ||
        memcmp(data,FONT_ID,sizeof(FONT_ID)-1) != 0 ) return false;
    int32_t spriteSize = 0;
    memcpy(&spriteSize,data+kFontHeaderSize-sizeof(spriteSize),
           sizeof(spriteSize));
    return spriteSize > 0 && spriteSize <= size-kFontHeaderSize;
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
            while( cursor+2 <= end ) {
                unsigned short flag = 0;
                memcpy(&flag,cursor,sizeof(flag));
                cursor += 2;
                if( flag == 0 ) { terminated = true; break; }
                unsigned int runLength = flag&0x7FFF;
                decodedPixels += runLength;
                if( decodedPixels >
                    (unsigned long long)filePanel.width*filePanel.height )
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
