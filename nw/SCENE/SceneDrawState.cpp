#include "_scene.h"

void CPhasedMovie::Draw(double fTime,TCCFVector3 &pt,double fZoom)
{
    (void)fTime;
    (void)pt;
    (void)fZoom;
    // The recovered software-renderer body is disabled by #if 0 in PMOVIE.CPP.
}

CPhasedMovie CMovingObject::m_boomMovie;

CPhasedMovie SBonusDef::m_bonusMovies[SBonusDef::T_SUP];
double SBonusDef::m_afBonusScale[SBonusDef::T_SUP] = {
    0.04, 0.04, 0.04, 0.04, 0.04, 0.04
};

CPhasedMovie CShot::m_staticHitMovie;
CPhasedMovie CShot::m_dynamicHitMovie;
CPhasedMovie CShot::m_chargeBallMovie;
CPhasedMovie CShot::m_hiChargeBallMovie;
CViewObjectModel *CShot::m_pRocketBase = NULL;
CViewObjectRef *CShot::m_pRocketRef = NULL;
int CShot::m_nColors[CShot::CLR_SUP];

void CMovingObject::Init(double fRadius)
{
    CViewSphericDynamic::Init();
    m_bump.fRadius = m_fRadius = fRadius;
    m_bump.fMass = 1.;
    m_fShield = 1.;
    m_nExistFlags = 0;
    m_bangShots.Clear();
}

void CShot::DrawLaserShot()
{
    // The recovered software-renderer body is disabled by #if 0 in SHOTS.CPP.
}

void CShot::DrawChargeBall()
{
    CPhasedMovie &movie = m_chargeBallMovie;
    movie.Draw(movie.Time(movie.Count()-1)-m_fLifeTime,m_dynBase,0.04);
}

void CShot::DrawHiChargeBall()
{
    CPhasedMovie &movie = m_hiChargeBallMovie;
    movie.Draw(movie.Time(movie.Count()-1)-m_fLifeTime,m_dynBase,0.04);
}

void CShot::DrawRocket()
{
    m_pRocketRef->GetDirModify().LoadIdentity().RotateOxL(
        atan2(m_dynVel.y,hypot(m_dynVel.x,m_dynVel.z))-M_PI/2).
        RotateOyL(atan2(-m_dynVel.x,-m_dynVel.z)).TranslateL(m_dynBase);
    m_pRocketRef->Draw();
}

void CShot::Draw()
{
    if( m_nStatus == S_OFF || m_nStatus == S_INIT ) return;
    if( m_nStatus == S_BANG || m_nStatus == S_JUSTBANG ) {
        CPhasedMovie &movie = m_pBangDynamic ? m_dynamicHitMovie :
                                                     m_staticHitMovie;
        movie.Draw(movie.Time(movie.Count()-1)-m_fLifeTime,m_dynBase,
                   m_pBangDynamic ? 0.02 : 0.04);
        if( m_nStatus == S_BANG ) return;
    }

    switch( m_eType ) {
        case T_LASER: DrawLaserShot(); break;
        case T_BULLET: break;
        case T_CHARGEBALL: DrawChargeBall(); break;
        case T_HICHARGEBALL: DrawHiChargeBall(); break;
        case T_ROCKET: DrawRocket(); break;
        default: ASSERT(0);
    }
}

void CMovingObject::Draw()
{
    if( m_bangShots.First() ) {
        CFVector3 viewDir(
            m_dynBase.x - CViewObject::m_viewPointInvMx.m[0][3],
            m_dynBase.y - CViewObject::m_viewPointInvMx.m[1][3],
            m_dynBase.z - CViewObject::m_viewPointInvMx.m[2][3] );
        for( CShot *ps = (CShot*)m_bangShots.First(), *ps1; ps != NULL;
             ps = ps1 ) {
            ps1 = (CShot*)ps->Next();
            if( !ps->IsOn() ) continue;
            if( ps->m_bangOffset*viewDir > 0 ) {
                ps->m_dynBase = m_dynBase+ps->m_bangOffset;
                ps->Draw();
            }
        }
        if( !m_bIsMain ) {
            if( m_nExistFlags & EF_DYNAMIC ) _Draw();
            if( m_nExistFlags & EF_BONUS )
                m_bump.pBonus->Draw(m_dynBase,m_fLifeTime);
            if( m_nExistFlags & EF_BOOM )
                m_boomMovie.Draw(
                    m_boomMovie.Time(m_boomMovie.Count()-1)-m_fLifeTime,
                    m_dynBase,m_fRadius/40 );
        }
        for( CShot *ps = (CShot*)m_bangShots.First(); ps != NULL;
             ps = (CShot*)ps->Next() ) {
            if( ps->m_bangOffset*viewDir <= 0 && ps->IsOn() ) {
                ps->m_dynBase = m_dynBase+ps->m_bangOffset;
                ps->Draw();
            }
        }
    } else if( !m_bIsMain ) {
        if( m_nExistFlags & EF_DYNAMIC ) _Draw();
        if( m_nExistFlags & EF_BONUS )
            m_bump.pBonus->Draw(m_dynBase,m_fLifeTime);
        if( m_nExistFlags & EF_BOOM )
            m_boomMovie.Draw(
                m_boomMovie.Time(m_boomMovie.Count()-1)-m_fLifeTime,
                m_dynBase,m_fRadius/40 );
    }
}
