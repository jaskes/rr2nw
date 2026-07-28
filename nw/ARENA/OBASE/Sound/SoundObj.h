/*
 * File  : C:\NW\ARENA\OBASE\Sound\SoundObj.h
 * Autor :
 * Ver   1.0
 */
#ifndef __SOUNDOBJ_H__INCLUDED
#define __SOUNDOBJ_H__INCLUDED

#include "storage/h/subject.h"
#include "kernel/h/active.h"
#include "WAVObj.h"

#include "sound.h"

class SoundObj : public ct_Object
{

 IRSXCachedEmitter  * m_lpCE;        // Cached Emitter
 int                  m_emitterValid;
 int		      m_positionValid;
 int                  m_playing;
 int                  m_playCount;

    void releaseEmitter();
    void resetState();

 public:
    CFVector3  m_position;
    WAVObj    *m_wav;

             SoundObj();
    virtual ~SoundObj();
    virtual int  receiveEvent( KR_Event &event );
    virtual void addNotify   ();
    virtual void removeNotify();

    WAVObj    *getWAV();
    CFVector3  getPosition();
    void       onChangePos();
    void       startPlay(int count);
    void       endPlay();

    bool       hasWAV() const { return m_wav != 0; }
    bool       emitterValid() const { return m_emitterValid != 0; }
    bool       positionValid() const { return m_positionValid != 0; }
    bool       playing() const { return m_playing != 0; }
    int        playCount() const { return m_playCount; }

    virtual bool shouldDump () { return false; } // cannot be allocated dynamically
};

#endif // ifndef __SOUNDOBJ_H__INCLUDED
/* End of file C:\NW\ARENA\OBASE\Sound\SoundObj.h */
