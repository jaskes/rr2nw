/*
 * File  : D:\GAME\OBASE\Spark\Spark.h
 * Autor :
 * Ver   1.0 
 */
#ifndef __SPARK_H__INCLUDED
#define __SPARK_H__INCLUDED

#include "storage/h/subject.h"
#include "storage/h/attr.h"
#include "kernel/h/active.h"
#include "SparkAttributeState.h"

#define LAST_H__VIEW
#include "game.h"
#include "..\DynObj\DynSpr.h"


#define sp_TIME_INCREMENT (0.04)

class CDC;

class Spark : public ct_Subject
{
 public:
    enum
    {
         MAX_PHASE = 15
    };

    AttributeSpark        *m_attr;
    CFVector3              m_position;
    int                    m_curPhase;
    double                 m_nextLifeTime;
    bool                   m_started;

    s_ViewDynamicSprite    m_viewDynSpr;

             Spark();
    virtual ~Spark();
    virtual int  receiveEvent( KR_Event &event );
    virtual void draw        ( CDC &gc );
    virtual void addNotify   ();
    virtual void removeNotify();
    virtual CFVector3 realPosition();
    virtual void render( CViewDynamicList &list, double ts );
    virtual void endRender( CViewScene *scene );
    void resetState();
    bool clean() const;
   
    virtual bool shouldDump () { return false; }
};

#endif // ifndef __SPARK_H__INCLUDED
/* End of file D:\GAME\OBASE\Spark\Spark.h */
