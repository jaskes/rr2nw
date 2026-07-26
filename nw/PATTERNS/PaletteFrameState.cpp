#define LAST_H__VIEW
#include "game.h"

CPaletteTranslator paletteTranslator;

SRGB CPaletteTranslator::m_palette[256] = {};
byte CPaletteTranslator::m_palette64[768] = {};
byte CPaletteTranslator::m_gouraud[16 * 256] = {};
byte* CPaletteTranslator::m_pTransparencies = 0;
byte* CPaletteTranslator::m_pLights = 0;
SHazeDef CPaletteTranslator::m_haze[CPaletteTranslator::NUM_HAZES] = {};
SGRColorDef* CPaletteTranslator::m_pTranspDefs = 0;
int CPaletteTranslator::m_nLightColors = 0;
int CPaletteTranslator::m_nTransparentColors = 0;
