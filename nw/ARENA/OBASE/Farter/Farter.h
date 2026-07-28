/*
 * File  : C:\NW\ARENA\OBASE\Farter\Farter.h
 * Autor :
 * Ver   1.0 
 */
#ifndef __FARTER_H__INCLUDED
#define __FARTER_H__INCLUDED

#include "storage/h/subject.h"
#include "storage/h/attr.h"
#include "kernel/h/active.h"
#include "FarterAttributeState.h"

class Farter : public ct_Subject
{
    CFVector3              m_position;
    KR_ObjectID            m_snd;

    void resetState();
 public:
    AttributeFarter   *m_attr;

             Farter();
    virtual ~Farter();
    virtual int  receiveEvent( KR_Event &event );

    virtual void draw        ();
    virtual void addNotify   ();
    virtual void removeNotify();

    virtual CFVector3     realPosition();

    virtual void onEnterAudibleZone(double ts);
    virtual void onExitAudibleZone (double ts);

    const KR_ObjectID &soundObjectID() const { return m_snd; }
    bool hasSoundObject() const { return m_snd.getCachePos() != -1; }
    const CFVector3 &farterPosition() const { return m_position; }


   virtual bool shouldDump () { return false; } // we don't dump attributes

};

#endif // ifndef __FARTER_H__INCLUDED
/* End of file C:\NW\ARENA\OBASE\Farter\Farter.h */
