#include "_scene.h"

extern CViewObjectRef *pVesselObj;

double CVessel::m_fMaxShield = 12.5;

CVessel::CVessel()
    : CMovingObject(2),
      m_speed(0,0,0), m_position(0,2,20),
      m_shotSeq(100)
{
    m_fMaxShield = 12.5;
    m_pObj = NULL;
    m_pBase = NULL;
    m_bHighlightShots = TRUE;
    m_DisplayMessage = NULL;
    m_dynBase = m_dynBase1 = m_position;
}

void CVessel::_Draw()
{
    if( pVesselObj ) {
        m_fRadius = m_bump.fRadius = pVesselObj->Model()->Radius0();
        CFMatrix3x4 dt, dir;
        dt.LoadTransposed(m_dir);
        dir.LoadIdentity().TranslateL(-pVesselObj->Model()->Center());
        pVesselObj->GetDirModify().LoadMult(dt,dir).TranslateL(m_position);
        pVesselObj->LoadLights(m_dwLights);
        pVesselObj->Draw();
    }
}

void CVessel::SetUpBonusCharge()
{
    m_bonus.nBonus = 0;
    //if( m_shooter.ChargeRemaining() > 0 )
    //    m_bonus.nBonus = -m_shooter.ChargeRemaining();
}

bool CVessel::CollectBonus(SBonusDef &def)
{
    ASSERT(this);
    char szMsg[40];
    switch( def.eType ) {
        case SBonusDef::T_SHIELD:
            if( m_fShield >= m_fMaxShield*0.999 ) return FALSE;
            m_fShield = Min(m_fMaxShield,-def.nBonus/100.*m_fMaxShield+m_fShield);
            sprintf(szMsg,"\x87\x80\x99\x88\x92\x80 \x93\x82\x85\x8B\x88\x97\x85\x8D\x80 \x84\x8E %d\n",(int)(m_fShield/m_fMaxShield*100+0.001));
            break;
        case SBonusDef::T_LASER:
            if( m_shooter[0].ChargeRemaining() >= m_shooter[0].MaxCharge() ) return FALSE;
            m_shooter[0].LoadCharge(-def.nBonus);
            sprintf(szMsg,"%d \x8B\x80\x87\x85\x90\x8D\x9B\x95 \x87\x80\x90\x9F\x84\x8E\x82\n",-def.nBonus);
            break;
        case SBonusDef::T_PLASMAGUN:
            if( m_shooter[1].ChargeRemaining() >= m_shooter[1].MaxCharge() ) return FALSE;
            m_shooter[1].LoadCharge(-def.nBonus);
            sprintf(szMsg,"%d \x8F\x8B\x80\x87\x8C\x85\x8D\x8D\x9B\x95 \x87\x80\x90\x9F\x84\x8E\x82\n",-def.nBonus);
            break;
        case SBonusDef::T_MACHINEGUN:
            if( m_shooter[2].ChargeRemaining() >= m_shooter[2].MaxCharge() ) return FALSE;
            m_shooter[2].LoadCharge(-def.nBonus);
            sprintf(szMsg,"%d \x8F\x80\x92\x90\x8E\x8D\x8E\x82\n",-def.nBonus);
            break;
        case SBonusDef::T_SUPERPLASMAGUN:
            if( m_shooter[3].ChargeRemaining() >= m_shooter[3].MaxCharge() ) return FALSE;
            m_shooter[3].LoadCharge(-def.nBonus);
            sprintf(szMsg,"%d \x88\x8E\x8D\x8D\x9B\x95 \x87\x80\x90\x9F\x84\x8E\x82\n",-def.nBonus);
            break;
        case SBonusDef::T_ROCKET:
            if( m_shooter[4].ChargeRemaining() >= m_shooter[4].MaxCharge() ) return FALSE;
            m_shooter[4].LoadCharge(-def.nBonus);
            sprintf(szMsg,"%d \x90\x80\x8A\x85\x92\n",-def.nBonus);
            break;
        default: ASSERT(0);
    }
    def.nBonus = 0;
    DisplayMessage(szMsg);
    return TRUE;
}
