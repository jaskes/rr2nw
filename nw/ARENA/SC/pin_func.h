#include "i/commander.i"
#include "debugext.h"
#include "../arena/h/super.h"

 //===========================================================================
void s_NewProjectEx( TProcessContext *pc, void *a )
 {
    (void)a;
    int	 permanent = SC_PARI(0);
    int  node      = SC_PARI(1);
    const char *st = (char *)&(pc->m_codePtr[SC_PARI(2)]);
    
    projectTable.newProject( st, node , permanent);
 }



void s_SetHostileCommander( TProcessContext *pc, void *a )
 {    
    (void)a;

    KR_ObjectID commander (SC_PARI(3),SC_PARI(2));
    KR_ObjectID relativeCommander (SC_PARI(1),SC_PARI(0));

    //ct_Arena &storage = *((ct_Arena*)(a));

    ICommander *uobj=(ICommander*)(g_arena.getContext()->queryInterface(commander,ICommanderIID));
    VERIFYMSG(uobj,"Cannot find commander");
    uobj->setHostile(relativeCommander);

    uobj=(ICommander*)(g_arena.getContext()->queryInterface(relativeCommander,ICommanderIID));
    VERIFYMSG(uobj,"Cannot find commander");
    uobj->setHostile(commander);
}

void s_SetFriendlyCommander( TProcessContext *pc, void *a )
 {    
    (void)a;
                    
    KR_ObjectID commander (SC_PARI(3),SC_PARI(2));
    KR_ObjectID relativeCommander (SC_PARI(1),SC_PARI(0));

    //ct_Arena &storage = *((ct_Arena*)(a));

    ICommander *uobj=(ICommander*)(g_arena.getContext()->queryInterface(commander,ICommanderIID));
    VERIFYMSG(uobj,"Cannot find commander");
    uobj->setHostile(relativeCommander);

    uobj=(ICommander*)(g_arena.getContext()->queryInterface(relativeCommander,ICommanderIID));
    VERIFYMSG(uobj,"Cannot find commander");
    uobj->setHostile(commander);
}


void s_DeleteHowitzer( TProcessContext *pc, void *a )
{
   (void)a;
   const char *name   = SC_PARS(0);  

   g_super.m_level.AttachToHowitzerHolder(name, KR_ObjectID::NUL());
}