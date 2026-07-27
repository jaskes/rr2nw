#ifndef	__SCENE_CPP__
#include"_scene.h"
#endif


#include "d3d.h"
#include "hardware.h"

#include <memory>
#include <stdexcept>

typedef	CViewOrder	CBumpShotOrder;
typedef	CViewObjectRef	CBumpShotObjectRef;

/*class	CBumpShotOrder : public CViewOrder {
public:
	void PromoteDynamic();
};
void CBumpShotOrder::PromoteDynamic()
{
	CViewOrder::PromoteDynamic();
	for( CViewDynamic *p = m_dynamicList.First() ; p ; p = p->Next() )
		p->SetPromoteNode(this);
}*/

static	CLandObjectMap2  *pMap2 = NULL;

CViewSceneLandPiece::CViewSceneLandPiece(CTaggedFile &f,
		CViewOrdered *(*ReadOrder)(CTaggedFile &f),
						int nSizeShift,double fCellSize,double fHRatio)
{
	CViewScene *scene = CViewScene::DecodeCurrent();
	if( !scene ) throw std::logic_error("land piece decoded without a scene");
	f.Descend("LNDP",FALSE,0);
		f.Descend("LDPH",TRUE,0);
			m_rect.Read(f);
			f.SetRTEFatal(FALSE);
				m_bBumpable = TRUE;
				f.ReadBool(m_bBumpable);
			f.SetOK(TRUE);
		f.Ascend();
		m_land.Read(f,m_rect,ReadOrder);
	f.Ascend();
	m_land.Setup((byte*)
			scene->GetTerrain()->GetHeightMap()
			,nSizeShift,fCellSize,fHRatio);
	//	(void)nSizeShift; (void)fCellSize; (void)fHRatio;
	scene->Attach(m_land.Map());
	scene->m_nLandPieces++;
	pMap2 = &m_land.Map();
}

bool __bDrawLand = TRUE;

extern int __bViewHide;

void CViewSceneLandPiece::Draw()
{
	ASSERT( !m_afterList.First() );
	//if(!__bDrawLand) return;
	//CViewDynamic *pDyn = m_dynamicList.First();
	//if( pDyn  ) WRITELOG1("Dynamic in land rect: %d,%d - %d,%d\n",m_rect);
	if( m_dynamicList.First() && __bViewHide ) {
		m_dynamicList.Draw();
		return;
	}
	//CViewScene::Current()->Update(m_land.Map());
	CViewScene::Current()->SetupLandDynamics(m_dynamicList,m_rect);
	//if( pDyn ) WRITELOG1("Dynamic cell: %d,%d\n",pDyn->Cell());
	//#if _KHAMIDULIN
    //	ShowLandFragment(m_rect,m_land.Map());
	//#else
		//dword dwOld = GetDebugFlags();
		//SetDebugFlags(dwOld & ~DBGF_LOG_MSG);
		CViewScene::Current()->GetTerrain()->Draw(m_rect,m_land.Map());
		//SetDebugFlags(dwOld);
		m_dynamicList.ZSortDrawNDrop();	// afterlist for current
	//#endif
}

double CViewSceneLandPiece::ComputeBSPError()
{
	return 1e10;
}
double CViewSceneLandPiece::ComputeBSPError(TCCFVector3 &pt,TCCFVector3 &n,double d)
{
	return	m_land.ComputeBSPError(pt,n,d);
}

#if 1
void CViewSceneLandPiece::PromoteDynamic()
{
	for( CViewDynamic *pd = m_dynamicList.First() ; pd != NULL ; pd = pd->Next() ) {
		ASSERT(pd->IsBumpDynamic());
		((CViewBumpDynamic*)pd)->SetPromoteNode(this);
	}
	//m_map.PromoteDynamic(m_dynamicList,m_shotList);
	//CViewScene::Current()->SetupLandDynamics(m_dynamicList);
}
#endif

void __AdjustPlane(CViewOrdered *p)
{
	if( p->Type() == CViewOrdered::VOT_ORDER ) {
		CViewOrder	*pOrder = (CViewOrder*)p;
		CFVector3 pt = pOrder->m_normal.pt, n = pOrder->m_normal.n;
		double	d = pOrder->m_normal.d;
		CFVector3 n1 = -n;
		if( pOrder->Ordered(0).Type() == CViewOrdered::VOT_LANDPIECE )
			((CViewSceneLandPiece*)&pOrder->Ordered(0))->AdjustPlane(pt,n1,d);
		if( pOrder->Ordered(1).Type() == CViewOrdered::VOT_LANDPIECE )
			((CViewSceneLandPiece*)&pOrder->Ordered(1))->AdjustPlane(pt,n,d);
		pOrder->SetPlane(pt,n,d);
	}
}

CViewScene *CViewScene::m_pCurrent = NULL;
CViewScene *CViewScene::m_pBuilding = NULL;
CViewOrdered	*CViewScene::ReadOrder(CTaggedFile &f)
{
	CViewScene *scene = m_pBuilding;
	if( !scene ) throw std::logic_error("scene order decoded without owner");
	f.Descend("ORDR",FALSE,1);
		f.Descend("ORDH",TRUE,1);
			int  type;
			f.ReadInt(type);
			CNameDef name(f);
			std::unique_ptr<CViewOrdered> pOrdered;
			switch(type) {
				case CViewOrdered::VOT_ORDER: {
					name.AssertEmpty();
					std::unique_ptr<CViewOrder> pOrder(new CBumpShotOrder());
					CFVector3 pt; int nNormIdx; CFVector3 n; double d;
					pt.Read(f); f.ReadInt(nNormIdx); n.Read(f); f.ReadDouble(d);
					pOrder->SetPlane(pt,n,d);
					f.Ascend();	// ORDH
					pOrder->SetOrdered(0,ReadOrder(f));
					pOrder->SetOrdered(1,ReadOrder(f));
					pOrdered.reset(pOrder.release());
				} break;
				case CViewOrdered::VOT_OBJECT: {
					int  nBase;
					f.ReadInt(nBase);
					if( nBase < 0 || nBase >= scene->m_nBases )
						throw std::runtime_error("bad scene object base index");
					std::unique_ptr<CViewObjectRef> pRef(new CBumpShotObjectRef(
											&scene->m_pBases[nBase]));
					pRef->GetDirModify().Read(f);
					pRef->GetDir().CheckOrtho();
					pRef->ClaimModelUsage();
					f.Ascend();	// ORDH
					name.Resolve(pRef.get(),scene->m_objRefNames);
					pOrdered.reset(pRef.release());
				} break; // VOT_OBJECT
				case CViewOrdered::VOT_LANDPIECE:
					name.AssertEmpty();
					f.Ascend(); // ORDH
					pOrdered.reset(new CViewSceneLandPiece(f,ReadOrder,
						scene->m_landParms.nSizeShift,
						scene->m_landParms.fCellSize,
						scene->m_landParms.fHRatio));
					break;
				case CViewOrdered::VOT_SHELTERORDER: {
					name.AssertEmpty();
					int  nBase;
					f.ReadInt(nBase);
					if( nBase < 0 || nBase >= scene->m_nBases )
						throw std::runtime_error("bad shelter base index");
					std::unique_ptr<CShelterOrder> pRef(new CShelterOrder(
						&scene->m_pBases[nBase]));
					pRef->GetDirModify().Read(f);
					pRef->ClaimModelUsage();
					f.Ascend();	// ORDH
					CViewOrdered  *pOldTop = CViewOrdered::SetCurrentTop(pRef.get());
					try {
						pRef->SetInner(ReadOrder(f));
					} catch(...) {
						CViewOrdered::SetCurrentTop(pOldTop);
						throw;
					}
					CViewOrdered::SetCurrentTop(pOldTop);
					pOrdered.reset(pRef.release());
				} break; // VOT_SHELTER
				case CViewOrdered::VOT_EMPTY: {
					name.AssertEmpty();
					f.Ascend(); // ORDH
					pOrdered.reset(new CViewEmptyOrder());
				} break;
				default: throw std::runtime_error("bad scene order type");
			} // switch type
		// f.Ascend();	// ORDH
	f.Ascend();	// ORDR
	return	pOrdered.release();
}

CViewScene::CViewScene(CTaggedFile &f,bool bDeferPublication)
{
	m_nBases = 0;
	m_pBases = NULL;
	m_pTerrain = NULL;
	m_pOrder = NULL;
	m_pSkyref = NULL;
	m_pPreviousTop = CViewOrdered::GetCurrentTop();
	m_bBushInitialized = FALSE;
	m_nLandPieces = 0;
	if( m_pCurrent || m_pBuilding )
		throw std::logic_error("another scene is already active");
	m_pBuilding = this;

	try {
	//#if _KHAMIDULIN
	//	LoadLand();
	//#endif

	CViewLibTexturedFigure::LoadLib(NULL);
	if( !bush_Init() ) throw std::runtime_error("bush initialization failed");
	m_bBushInitialized = TRUE;

	{
		CTaggedFile	fs(FALSE);
		if( !fs.Open("sky.vbc",FALSE) )
			throw std::runtime_error("sky model is missing");
		m_skybase.Read(fs);
		m_skybase.Split(TRUE);
		if( !fs.IsOK() || !fs.Close(FALSE) )
			throw std::runtime_error("sky model decode failed");
		m_pSkyref = new CViewObjectRef();
		m_pSkyref->Attach(&m_skybase);
		m_pSkyref->GetDirModify().LoadIdentity();
		CViewOrdered::SetCurrentTop(m_pPreviousTop);
	}


	{ CTaggedFile::CDescend	chSCEN(f,"SCEN",FALSE,0);
		int nNamedBases, nDirectBases;
		{ CTaggedFile::CDescend	chSCEH(f,"SCEH",TRUE,0);
			f.ReadInt(nNamedBases);
			f.ReadInt(nDirectBases);
			int nRefsTotal;
			f.ReadInt(nRefsTotal);
			m_landParms.nSizeShift = 9;
			f.ReadDouble(m_landParms.fCellSize);
			f.ReadDouble(m_landParms.fHRatio);
			f.ReadInt(m_landParms.nReductions);
			//#if _KHAMIDULIN
			//	SetLandParams(m_landParms.fCellSize,m_landParms.fHRatio);
			//#else
				m_pTerrain = new CViewTerrain(m_landParms.fCellSize,m_landParms.fHRatio,10*1100);
			//#endif
			m_pBases = new CViewObjectModel[m_nBases=nNamedBases+nDirectBases];
			m_landDynamic.Create(CVector2(110,110),m_landParms.fCellSize);
			ASSERT( m_landDynamic.IsEmpty() );
			CViewObject::SetTerrainCellSize(m_landParms.fCellSize);
		}
		if( nNamedBases > 0 ) {
			CTaggedFile::CDescend	chOBJN(f,"OBJN",TRUE,0);
			for( int i = 0 ; i < nNamedBases ; i++ ) {
				char	name[80];
				f.Read(name,80); name[79] = 0;
				strlwr(name);
				CTaggedFile	fbase(TRUE);
				fbase.Open(name);
				WRITELOG1("\n%s\n------------\n\n",name);
				m_pBases[i].Read(fbase);
				#ifdef _DEBUG
				if( GetDebugFlags() & DBGF_LOG_MSG ) {
					double fErr = m_pBases[i].ComputeBSPError();
					WRITELOG("\n\n");
					if(fErr<-MAX_BSP_ERROR)	WRITELOG("\nBSP ERRORS DETECTED\n");
					WRITELOG2("BSPCHK%s = %lg",fErr<-MAX_BSP_ERROR?" ERR":"",fErr);
				}
				#endif
				WRITELOG("\n\n-----------------------------------------------\n\n");
				#ifndef RR2NW_SCENE_NO_HARDWARE_PUMP
			        g_hardware.MessageLoop();
				#endif
			}
		}
		if( nDirectBases > 0 ) {
			CTaggedFile::CDescend	chOBJN(f,"OBJD",TRUE,0);
			for( int i = 0 ; i < nDirectBases ; i++ ) {
				m_pBases[nNamedBases+i].Read(f);
				#ifdef _DEBUG
					double fErr = m_pBases[nNamedBases+i].ComputeBSPError();
					WRITELOG3("BSPCHK%s: DirectBase %d = %lg\n",fErr<-1e-5?" ERR":"",i,fErr);
				#endif
			}
		}
		f.SkipChunk("NRMS",TRUE,0);
		f.Descend("NAMS",FALSE,0);
			m_objRefNames.Read(f);
		f.Ascend();
		CViewOrdered::SetCurrentTop(NULL);
		m_pOrder = ReadOrder(f); //CViewOrder::Read(f);
		#ifdef _DEBUG
		if( GetDebugFlags() & DBGF_LOG_MSG ) {
			WRITELOG("\n\nScene Order\n------------\n\n");
			double fErr = m_pOrder->ComputeBSPError();
			WRITELOG("\n\n");
			if(fErr<-MAX_BSP_ERROR)	WRITELOG("\nBSP ERRORS DETECTED\n");
			WRITELOG2("BSPCHK%s = %lg",fErr<-1e-5?" ERR":"",fErr);
			WRITELOG("\n\n-----------------------------------------------\n\n");
		}
		#endif
		m_objRefNames.FinishResolve();
		if( !m_objRefNames.IsFullyResolved() )
			throw std::runtime_error("unresolved scene object references");
		m_pOrder->ApplyModelUsage();
		m_pOrder->ComputeMaxHeight();
		for( int  i = 0 ; i < m_nBases ; i++ )	m_pBases[i].Split(FALSE);
	}
	if( !f.IsOK() || !m_pOrder )
		throw std::runtime_error("scene decode failed");
	m_pBuilding = NULL;
	if( !bDeferPublication && !Commit() )
		throw std::logic_error("scene publication failed");
	} catch(...) {
		ReleaseOwnedResources();
		throw;
	}
}

bool CViewScene::Commit()
{
	if( m_pBuilding || (m_pCurrent && m_pCurrent != this) || !m_pOrder )
		return FALSE;
	m_pCurrent = this;
	return TRUE;
}

void CViewScene::ReleaseOwnedResources()
{
	if( m_pBuilding == this ) m_pBuilding = NULL;
	if( m_pCurrent == this ) m_pCurrent = NULL;
	CViewOrdered::SetCurrentTop(m_pPreviousTop);
	delete m_pOrder;
	m_pOrder = NULL;
	delete [] m_pBases;
	m_pBases = NULL;
	m_nBases = 0;
	m_nLandPieces = 0;
	delete m_pSkyref;
	m_pSkyref = NULL;
	//#if !_KHAMIDULIN
		delete m_pTerrain;
		m_pTerrain = NULL;
	//#else
   	//	UnloadLand();
	//#endif
	if( m_bBushInitialized || bush_IsInitialized() ) bush_Done();
	m_bBushInitialized = FALSE;
	pMap2 = NULL;
}

CViewScene::~CViewScene()
{
	ReleaseOwnedResources();
}

CRandomGenerator	sc_randomPos, sc_randomRed;
int	sc_countRed;
int __randomPos;

/*#if	defined(__NT__) && !defined(_DEBUG)
#define	DBGF_RR2_DEFAULT	(DBGF_DEFAULT&~DBGF_ERR_INT3)
#else
#define	DBGF_RR2_DEFAULT	DBGF_DEFAULT
#endif*/

#ifndef RR2NW_SCENE_DRAW_EXTERNAL
void CViewScene::PromoteDynamic(CViewDynamicList &list,bool bWillDraw)
{
	if( bWillDraw ) {
		CViewFigure::SetWaterline(m_pTerrain->Waterline());
		m_pTerrain->SetViewPoint();
		ASSERT( m_landDynamic.IsEmpty() );
		//m_landDynamic.SetOrigin(CViewObject::m_viewPointInvMx.Offset());
		m_landDynamic.SetOrigin(m_pTerrain->ViewpointL());
		m_landDynamic.SetupLights(CViewObject::EnabledLights());
		for( CViewDynamic *pd = list.First() ; pd ; pd = pd->Next() )
			m_landDynamic.ApplyLights(pd);
		m_landDynamic.SetupLandSticks(list,*m_pTerrain);
	}
	m_pOrder->LoadDyns(list);
	m_pOrder->PromoteDynamic();
}

extern IDirect3DDevice2       *_d3dDevice;
extern SDeviceList _dL;

void CViewScene::Draw(TCSFMatrix3x4 &viewDir,CViewDynamicList &list)
{
	//ASSERT(pViewNode==NULL);
	CViewOrdered	*pViewNode = NULL;
	if( __randomPos )
	{
		(void)pViewNode;
		sc_randomPos = vt_randomPos;
		//sc_randomRed = vt_randomRed;
		sc_countRed = vt_countRed;
		//if( *(long*)&sc_randomPos == 0xe86bf3ad )
		//		SetDebugFlags( DBGF_RR2_DEFAULT | DBGF_LOG_MSG );
		CRect2	rect;
		int w = vt_randomPos(1.01,512.99), h = vt_randomPos(1.01,512.99);
		rect.left = vt_randomPos(0.01,512.99-w);
		rect.top = vt_randomPos(0.01,512.99-h);
		rect.right = rect.left+w;
		rect.bottom = rect.top+h;

		double	hazeD = CViewFigure::HazeMax()*2;

		CFVector3	viewpt(
			vt_randomPos(rect.left*10-hazeD,rect.right*10+hazeD),
			vt_randomPos(0,256*0.125+hazeD),
			vt_randomPos(rect.top*-10+hazeD,rect.bottom*-10-hazeD)
		);
		//viewpt = CFVector3(rect.CenterX()*10,400,rect.CenterY()*-10);
		double yaw = vt_randomPos(0,2*M_PI);
		double t = vt_randomPos(-1,1);
		double pitch = t*M_PI/2; //*t*M_PI/2; if(t<0) pitch*=-1;
		double roll = vt_randomPos(0,2*M_PI);
		CFMatrix3x4	dir;
		//yaw=pitch=roll=0; pitch=M_PI/2;
		dir.LoadIdentity().TranslateL(-viewpt).
						  RotateOyL(yaw).RotateOxL(pitch).RotateOzL(roll);
		CViewObject::SetViewPoint(dir,CViewObject::m_viewPointScale);
		m_pTerrain->GenRed();
		m_pTerrain->SetViewPoint();

		m_pTerrain->Draw(rect,*pMap2);
		ASSERTMSG( !vt_randomPos.IsInitialState() &&
					!vt_randomRed.IsInitialState(), "TESTING COMPLETE !!!!!!!!" );
		return;
	}

	if( pViewNode == NULL ) pViewNode = m_pOrder;
	CViewObject::SetClipRect(_gr_clipRect);

	CViewObject::SetBelowWater(-viewDir.Column(1)*viewDir.Offset()<m_pTerrain->Waterline());

	if( !CViewObject::IsBelowWater() ) {
		GRClearScreen();
		dword	dwLights = CViewObject::EnabledLights();
		CViewObject::EnableLights(0);
		CFMatrix3x4	dir;
		dir = viewDir;
		dir.LoadOffset(CFVector3(0,0,0));
		CViewObject::SetViewPoint(dir,m_scale);
		CViewFigure::SetWaterline(1000);
		CViewObject::SetHaze(FALSE);
		CViewObject::SetClipPlanes(CViewObject::m_fFrontClip,10000);
		CViewObject::m_eWaterView = WATER_VIEW_ALL;
#if USE_Z_BUFFER
        if (_dL.currDevice->swHw == GR_HARDWARE)
           _d3dDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, FALSE);
#endif
		m_pSkyref->Draw();

#if USE_Z_BUFFER
        if (_dL.currDevice->swHw == GR_HARDWARE)
           _d3dDevice->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE, TRUE);
#endif

		//m_pSkyref->Draw();
		CViewObject::m_eWaterView = WATER_VIEW_ABOVE;
		CViewObject::EnableLights(dwLights);
	} else
		GRClearScreen(TRUE,CPaletteTranslator::Haze(1).nColor);


	CViewObject::SetViewPoint(viewDir,m_scale);
	GRSetScale(CViewObject::m_viewPointScale_1.x, -CViewObject::m_viewPointScale_1.y);

	CViewObject::SetHaze(TRUE);
	CViewFigure::SetHaze(CPaletteTranslator::Haze(MAKEBOOL(CViewObject::IsBelowWater())));
	CViewObject::SetClipPlanes(CViewObject::m_fFrontClip,CViewFigure::HazeMax());
	PromoteDynamic(list,TRUE);


	//m_pTerrain->SetViewPoint();

	//m_landDynamic.SetOrigin(m_pTerrain->ViewpointL());
			//CViewObject::m_viewPointInvMx.Offset());

	//m_dynamicRect = CRect2(CVector2(0,0),m_dynamicGridSize);
	//m_dynamicRect.MoveBy(landOrg-m_dynamicRect.Center());
	//CLandObjectMap1::SetDynamicsRect(m_dynamicRect,
	//	m_nDynamicOrigin = -landOrg.x-landOrg.y*m_dynamicGridSize.x);

	m_pOrder->LoadLights(CViewObject::EnabledLights());
	// m_landDynamic.SetupLights(CViewObject::EnabledLights());
	CViewObject::BeginDraw();

	pViewNode->DrawView();
	m_landDynamic.AfterList().ZSortDrawNDrop(); //Draw(TRUE);
	m_landDynamic.RemoveLights(CViewObject::EnabledLights());
	//m_pOrder->Draw();
	//m_pLand->Draw();
	m_pTerrain->EndDrawTerrain();
}
#endif

