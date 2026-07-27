#define LAST_H__VIEW
#include "game.h"

#include <new>

#include "ViewFigureLibraryState.h"

namespace {

bool g_figureLibraryReady = false;

}  // namespace

dword CViewTexture::m_dwDefaultInfoFlags = 0;

CViewTexture::~CViewTexture() { Release(); }

void CViewTexture::Release() {
  if (m_hImage != nullptr) GRDeleteTextureFromDB(m_hImage);
  m_hImage = nullptr;
  m_dwFlags = 0;
  m_nWidth = 0;
  m_nHeight = 0;
  m_nTransparentColor = 0;
}

CViewLibTexturedFigure::SLibTexture*
    CViewLibTexturedFigure::m_pLibTextures = nullptr;
int CViewLibTexturedFigure::m_nLibTextures = 0;
int CViewLibTexturedFigure::m_nMaxLibTextures = 0;

void CViewLibTexturedFigure::LoadLib(TCchar* name) {
  if (name != nullptr) return;
  ClearLib();
  m_pLibTextures = new (std::nothrow) SLibTexture[100];
  m_nMaxLibTextures = m_pLibTextures != nullptr ? 100 : 0;
  g_figureLibraryReady = m_pLibTextures != nullptr;
}

void CViewLibTexturedFigure::ClearLib() {
  delete[] m_pLibTextures;
  m_pLibTextures = nullptr;
  m_nLibTextures = 0;
  m_nMaxLibTextures = 0;
}

bool ViewFigureLibrary_Initialize() {
  CViewLibTexturedFigure::LoadLib(nullptr);
  return g_figureLibraryReady;
}

void ViewFigureLibrary_Release() {
  CViewLibTexturedFigure::ClearLib();
  g_figureLibraryReady = false;
}

bool ViewFigureLibrary_IsReady() { return g_figureLibraryReady; }
