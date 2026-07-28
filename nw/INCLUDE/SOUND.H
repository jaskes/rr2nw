#ifndef __SOUND_H__
#define __SOUND_H__

#include <windows.h>
#include <windowsx.h>
#include <olectl.h>

#include <rsx.h>


extern IUnknown* lpRSX2Unk;
extern IRSX2* lpRSX2;

HRESULT InitializeRSX();
void DeInitializeRSX();

// Keeps the audible-zone radius usable when the legacy RSX device is not
// initialized. The update is transactional: invalid or overflowing values do
// not modify either public distance value.
bool SoundState_SetMaximumDistance(double maximumDistance);

extern int snd_engine ;
extern int snd_true3d ;
extern int snd_useDS  ;
extern double snd_distMax ;
extern double snd_distMax2;
extern double snd_engineIntensity;


#endif
