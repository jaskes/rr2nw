#define LAST_H__VIEW
#include "game.h"

#include "TerrainViewState.h"

// Bounded production owner for the recovered terrain view/frustum stage.
// TERRAIN.CPP remains a complete, separately compiled comparison archive.

#define MAX_REDCELL(X,Y) (m_red(X,Y)&7)
#define _RC_COUNT 0
#define _STATIC_REDUCTIONS 0
#define _PRECOMPUTE_REDS 0
#define _ALIGN_EDGES 1
#define _WATER_NOREDRESTRICT 0
#define _REDUCTION_IMMEDIATE 0

const int nTerrainMaxReduction = 4;
extern double __terrainWaterline;
int __nTerrainWaterHighest = 0;
int __nTerrainLandLowest = 0;
double __terrainCell = 10;
int __polygonReductionCall = 0;
int __reductionCall = 0;
int __cellCount = 0;
int __reductionCallLink = 0;

inline byte  *_CViewTerrain::CMipMapHeight::operator()(int x,int y)
{
	return m_pImage[m_nWidth*(y&(m_nHeight-1))+(x&(m_nWidth-1))];
}

inline byte  _CViewTerrain::CMipMapHeight::GetLimit(int x,int y,int nMinMax,int nShiftReduction)
{
	if( nShiftReduction <= m_nShiftR )
		return m_pImage[m_nWidth*((y>>m_nShiftR)&(m_nHeight-1))+
									((x>>m_nShiftR)&(m_nWidth-1))][nMinMax];
	int n = 1<<(nShiftReduction-m_nShiftR);
	x >>= m_nShiftR;
	y >>= m_nShiftR;
	byte nBest = (byte)(nMinMax-1);
	for( int i = 0 ; i < n ; i++ )
	for( int j = 0 ; j < n ; j++ ) {
		byte b = m_pImage[m_nWidth*((y+j)&(m_nHeight-1))+
								((x+i)&(m_nWidth-1))][nMinMax];
		nBest = nMinMax ? Max(nBest,b) : Min(nBest,b);
	}
	return nBest;
}
// ----------------------------------------------------------------------

inline bool _CViewTerrain::IsViewAlongX()
{
	return fabs(CViewObject::m_viewPointInvMx.m[0][2]) > fabs(CViewObject::m_viewPointInvMx.m[2][2]);
}

inline bool _CViewTerrain::IsViewAlongY()
{
	return !IsViewAlongX();
}

inline bool _CViewTerrain::IsViewAlongXP()
{
	//ASSERT(IsViewAlongX());
	return	CViewObject::m_viewPointInvMx.m[0][2] < 0;
}

inline bool _CViewTerrain::IsViewAlongXN()
{
	return	!IsViewAlongXP();
}

inline bool _CViewTerrain::IsViewAlongYP()
{
	//ASSERT(IsViewAlongY());
	return	CViewObject::m_viewPointInvMx.m[2][2] > 0;
}

inline bool _CViewTerrain::IsViewAlongYN()
{
	return	!IsViewAlongYP();
}


void _CViewTerrain::SetViewPoint()
{
	m_fCellSize = __terrainCell;
	__polygonReductionCall = __reductionCall = 0;
	__cellCount = __reductionCallLink = 0;
	// Generate projection data
	(m_viewPointDirMx = CViewObject::m_viewPointDirSMx).ScaleR(CFVector3(m_fCellSize,m_fCellSize*m_fHScale,-m_fCellSize));

	CFVector3	viewpoint = CViewObject::m_viewPointInvMx.Offset();
	m_viewpoint = CFVector2(viewpoint.x/m_fCellSize,-viewpoint.z/m_fCellSize);
	m_viewpointL[I_X] = static_cast<int>(floor(viewpoint.x/m_fCellSize));
	m_viewpointL[I_Y] = static_cast<int>(floor(-viewpoint.z/m_fCellSize));
	m_nearestOffset = CVector2(IsViewAlongXN() ? 1 : 0, IsViewAlongYN() ? 1 : 0);


	// compute water visibility flags
	double	fWater = __terrainWaterline*m_fCellSize*m_fHScale;
	(void)fWater;
	m_bIsAboveWater = !CViewObject::IsBelowWater();
		// CViewObject::m_viewPointInvMx.m[1][3] >= fWater;
	m_bIsAboveWaterCpl = m_bIsAboveWater;
			// CViewObject::m_viewPointInvMx.m[1][3] >= fWater+2;
	m_bIsBelowWaterCpl = !m_bIsAboveWater;
			// CViewObject::m_viewPointInvMx.m[1][3] <= fWater-2;
	CViewObject::m_eWaterView = m_bIsAboveWater ? WATER_VIEW_ABOVE : WATER_VIEW_BELOW;



	// Create view frustum and its projections
	CFVector3	pyramidVerts3[8];
	CFVector2	pyramidVerts[24];
	for( int i = 0 ; i < 4 ; i++ ) {
		pyramidVerts3[i] = viewpoint+CViewObject::m_clipRays[i]*CViewObject::m_fFrontClip;
		pyramidVerts[i] = CFVector2(pyramidVerts3[i].x,-pyramidVerts3[i].z)/m_fCellSize;
		pyramidVerts3[i+4] = viewpoint+CViewObject::m_clipRays[i]*CViewObject::m_fBackClip;
		pyramidVerts[i+4] = CFVector2(pyramidVerts3[i+4].x,-pyramidVerts3[i+4].z)/m_fCellSize;
	}
	int  nPyramidVerts = 8;

	// Optimize view frustum
	for( int nBoundSquare = 32000*32000+1 ;; ) {

		if( nPyramidVerts < 1 ) { m_bTerrainInView = FALSE; return; }

		// Compute bounding rect
		CRect2	boundRect;
		boundRect.left = (int)floor(pyramidVerts[0].x);
		boundRect.top = (int)floor(pyramidVerts[0].y);
		boundRect.right = boundRect.left+1;
		boundRect.bottom = boundRect.top+1;
		for( int i = 1 ; i < nPyramidVerts ; i++ ) {
			int x = (int)floor(pyramidVerts[i].x);
			int y = (int)floor(pyramidVerts[i].y);
			if( boundRect.left > x )	boundRect.left = x;
			if( boundRect.top > y )		boundRect.top = y;
			x++; y++;
			if( boundRect.right < x )	boundRect.right = x;
			if( boundRect.bottom < y )	boundRect.bottom = y;
		}
		boundRect.left >>= HRANGE_SHIFTREDUCTION;
		boundRect.top >>= HRANGE_SHIFTREDUCTION;
		boundRect.right = (boundRect.right+(1<<HRANGE_SHIFTREDUCTION)-1) >> HRANGE_SHIFTREDUCTION;
		boundRect.bottom = (boundRect.bottom+(1<<HRANGE_SHIFTREDUCTION)-1) >> HRANGE_SHIFTREDUCTION;
		ASSERT( boundRect.Width() <= 32000 && boundRect.Height() <= 32000 );

		// If gain == nothing leave
		int nNewSquare = boundRect.Width()*boundRect.Height();
		if( nNewSquare >= nBoundSquare )	break;
		nBoundSquare = nNewSquare;

		// Compute clipping height
		double	fClip[2];
		int     nClipL = (int)(byte)-1, nClipH = 0;
		for( int x = boundRect.left ; x < boundRect.right ; x ++ )
		for( int y = boundRect.top ; y < boundRect.bottom ; y++ ) {
			byte	*p = m_hrange(x,y);
			nClipH = Max(nClipH,(int)p[1]);
			nClipL = Min(nClipL,(int)p[0]);
		}
		ASSERT( nClipL <= nClipH );
		#if 1
		if( m_bIsAboveWater ) {
			if( nClipL < __nTerrainWaterHighest )	nClipL = __nTerrainWaterHighest;
			if( nClipH < __nTerrainWaterHighest )	nClipH = __nTerrainWaterHighest;
		}
		else {
			if( nClipL > __nTerrainLandLowest )	nClipL = __nTerrainLandLowest;
			if( nClipH > __nTerrainLandLowest )	nClipH = __nTerrainLandLowest;
		}
		ASSERT( nClipL <= nClipH );
		#endif
		fClip[0] = nClipL*m_fHScale*m_fCellSize;
		fClip[1] = nClipH*m_fHScale*m_fCellSize;

		// Clip frustum
		int cf[8];
		for( int i = 0 ; i < 8 ; i++ ) {
			if( pyramidVerts3[i].y < fClip[0] )	cf[i] = 1; else
			if( pyramidVerts3[i].y > fClip[1] )	cf[i] = 2; else
			cf[i] = 0;
		}
		nPyramidVerts = 0;
		for( int i = 0 ; i < 8 ; i++ ) {
			if( cf[i] ) {
				int i1;
				if( cf[i1=(i&3)>0?i-1:i+3] != cf[i] ) {
					ASSERT(nPyramidVerts<24);
					pyramidVerts[nPyramidVerts++] =
						(CFVector2(pyramidVerts3[i].x,-pyramidVerts3[i].z)+
						CFVector2(pyramidVerts3[i1].x-pyramidVerts3[i].x,
								pyramidVerts3[i].z-pyramidVerts3[i1].z)*
						((fClip[cf[i]-1]-pyramidVerts3[i].y)/
							AbsNoLess(pyramidVerts3[i1].y-pyramidVerts3[i].y,1e-5)))/m_fCellSize;
				}
				if( cf[i1=(i&3)<3?i+1:i-3] != cf[i] ) {
					ASSERT(nPyramidVerts<24);
					pyramidVerts[nPyramidVerts++] =
						(CFVector2(pyramidVerts3[i].x,-pyramidVerts3[i].z)+
						CFVector2(pyramidVerts3[i1].x-pyramidVerts3[i].x,
								pyramidVerts3[i].z-pyramidVerts3[i1].z)*
						((fClip[cf[i]-1]-pyramidVerts3[i].y)/
							AbsNoLess(pyramidVerts3[i1].y-pyramidVerts3[i].y,1e-5)))/m_fCellSize;
				}
				if( cf[i1=i^4] != cf[i] ) {
					ASSERT(nPyramidVerts<24);
					pyramidVerts[nPyramidVerts++] =
						(CFVector2(pyramidVerts3[i].x,-pyramidVerts3[i].z)+
						CFVector2(pyramidVerts3[i1].x-pyramidVerts3[i].x,
								pyramidVerts3[i].z-pyramidVerts3[i1].z)*
						((fClip[cf[i]-1]-pyramidVerts3[i].y)/
							AbsNoLess(pyramidVerts3[i1].y-pyramidVerts3[i].y,1e-5)))/m_fCellSize;
				}
			}
			else {
				ASSERT(nPyramidVerts<24);
				pyramidVerts[nPyramidVerts++] = CFVector2(pyramidVerts3[i].x,-pyramidVerts3[i].z)/m_fCellSize;
			}
		} // clip frustum by layer
		if( nPyramidVerts < 1 ) { m_bTerrainInView = FALSE; return; }
		ASSERT(nPyramidVerts>=3);

	} // optimizing frustum

	m_bTerrainInView = TRUE;


	// Sort all projections in ascending y order
	for( int i = 0 ; i < nPyramidVerts-1 ; i++ ) {
		int i0 = i;
		for( int j = i+1 ; j < nPyramidVerts ; j++ )
			if( pyramidVerts[j].y < pyramidVerts[i0].y ) i0 = j;
		if( i0 != i ) {
			CFVector2	temp = pyramidVerts[i];
			pyramidVerts[i] = pyramidVerts[i0];
			pyramidVerts[i0] = temp;
		}
	}

	// Begin forming a convex polygon on pyramid's vertices projections
	// starting from the least y
	CFVector2	polygonVertices[48];
	int nPolygonVertices = 0;
	polygonVertices[nPolygonVertices++] = pyramidVerts[0];

	// First we go along the right side
	for( int i = 1 ;; i++) {
		int		nNext = -1;
		double	fMaxAngle = 0.0;
		for( int j = i ; j < nPyramidVerts ; j++ ) {
			CFVector2	edge = pyramidVerts[j]-polygonVertices[nPolygonVertices-1];
			double		fEdgeAbs = Abs(edge);
			if( fEdgeAbs > 1e-3 ) {
				double	fAngle = edge.x/fEdgeAbs; // sin
				if( nNext < 0 || fAngle > fMaxAngle ) {
					nNext = j;
					fMaxAngle = fAngle;
				}
			}
		}
		if( nNext < 0 ) break;
		ASSERT(nPolygonVertices<48);
		polygonVertices[nPolygonVertices++] = pyramidVerts[i=nNext];
	}

	// Then we return along the left one
	for( int i = nPyramidVerts-2 ;; i--) {
		int		nNext = -1;
		double	fMinAngle = 0.0;
		for( int j = i ; j >= 0 ; j-- ) {
			CFVector2	edge = pyramidVerts[j]-polygonVertices[nPolygonVertices-1];
			double		fEdgeAbs = Abs(edge);
			if( fEdgeAbs > 1e-3 ) {
				double	fAngle = edge.x/fEdgeAbs; // sin
				if( nNext < 0 || fAngle < fMinAngle ) {
					nNext = j;
					fMinAngle = fAngle;
				}
			}
		}
		if( nNext < 1 )	break;	// 0th vertex already included
		if( Abs(polygonVertices[0]-pyramidVerts[nNext]) < 2e-3 ) break;
		ASSERT(nPolygonVertices<48);
		polygonVertices[nPolygonVertices++] = pyramidVerts[i=nNext];
	}
	if( nPolygonVertices < 3 ) { m_bTerrainInView = FALSE; return; }
	//ASSERT(nPolygonVertices>=3);

	WRITELOG2("ViewpointL=(%d,%d)\n",m_viewpointL[I_X],m_viewpointL[I_Y]);

	// Find out indices of the outmost vertices
	CRect2	polygonRectIdx(0,0,0,0);
	for( int i = 1 ; i < nPolygonVertices ; i++ ) {
		if( polygonVertices[i].x < polygonVertices[polygonRectIdx.left].x )	polygonRectIdx.left = i;
		if( polygonVertices[i].x > polygonVertices[polygonRectIdx.right].x )	polygonRectIdx.right = i;
		if( polygonVertices[i].y < polygonVertices[polygonRectIdx.top].y )	polygonRectIdx.top = i;
		if( polygonVertices[i].y > polygonVertices[polygonRectIdx.bottom].y )	polygonRectIdx.bottom = i;
	}
	m_polygonRect = CRect2((int)floor(polygonVertices[polygonRectIdx.left].x),
							(int)floor(polygonVertices[polygonRectIdx.top].y),
							(int)floor(polygonVertices[polygonRectIdx.right].x+1),
							(int)floor(polygonVertices[polygonRectIdx.bottom].y)+1);

	// Create zero reduction edges
	double	fMajorAxisZStep, fMinorAxisZStep;
	int		iMajorAxisStart0, iMajorAxisEnd0;
	int iL, iR, iL1, iR1, l, r, l1, r1, i;
	double tanL = 0.0, tanR = 0.0;
	m_edgesOuter[I_L] = 0, m_edgesOuter[I_R] = 0;
	if( IsViewAlongX() ) { // creating edges alongX
		RTCHECK(m_polygonRect.Width()+(1<<nTerrainMaxReduction)*3<m_nMaxEdges,"Not enough memory for terrain");
		fMajorAxisZStep = CViewObject::m_viewPointDirMx.m[2][0];
		fMinorAxisZStep = -CViewObject::m_viewPointDirMx.m[2][2];
		iMajorAxisStart0 = m_polygonRect.left;
		iMajorAxisEnd0 = m_polygonRect.right;
		iL = iR = iL1 = iR1 = polygonRectIdx.left;
		l1 = l = (int)floor(polygonVertices[iL].y); r1 = r = l+1;
		int x = (int)floor(polygonVertices[iL].x)+1;
		bool	bLeaveEdgesLoop = FALSE;
		WRITELOG1("m_polygonRect = CRect2(%d,%d,%d,%d)\n",m_polygonRect);
		for( i = 0 ;; i++, x++ ) {	// edge addition loop
			while( polygonVertices[iL1].x < x ) {	// search for next edgeL span
				l = Min(l,(int)floor(polygonVertices[iL1].y));
				if( iL1 == polygonRectIdx.right ) { bLeaveEdgesLoop = TRUE; break; } //goto FinishCreateEdges;
				iL = iL1; if( ++iL1 >= nPolygonVertices ) iL1 = 0;
				CFVector2	d = polygonVertices[iL1]-polygonVertices[iL];
				tanL = d.y/Max(d.x,1e-3);
			}
			while( polygonVertices[iR1].x < x ) {	// search for next edgeR span
				r = Max(r,(int)floor(polygonVertices[iR1].y)+1);
				if( iR1 == polygonRectIdx.right ) { bLeaveEdgesLoop = TRUE; break; } //goto FinishCreateEdges;
				iR = iR1; if( iR1 <= 0 ) iR1 = nPolygonVertices; iR1--;
				CFVector2	d = polygonVertices[iR1]-polygonVertices[iR];
				tanR = d.y/Max(d.x,1e-3);
			}
			if( bLeaveEdgesLoop ) break;
			l = Min(l,l1=(int)floor(polygonVertices[iL].y+tanL*(x-polygonVertices[iL].x)));
			r = Max(r,r1=(int)floor(polygonVertices[iR].y+tanR*(x-polygonVertices[iR].x))+1);
			ASSERT(i<m_pnEdges[0]);
			m_ppEdges[0][i][I_L] = l;
			m_ppEdges[0][i][I_R] = r;
			if( m_ppEdges[0][i][I_L] < m_ppEdges[0][m_edgesOuter[I_L]][I_L] ) m_edgesOuter[I_L] = i;
			if( m_ppEdges[0][i][I_R] > m_ppEdges[0][m_edgesOuter[I_R]][I_R] ) m_edgesOuter[I_R] = i;
			WRITELOG3("Edge[%d]: %d,%d\n",x-1,l,r);
			l = l1; r = r1;
		} // edge addition loop
	} // creating edges along X
	else { // creating edges along Y
		RTCHECK(m_polygonRect.Height()+(1<<nTerrainMaxReduction)*3<m_nMaxEdges,"Not enough memory for terrain");
		fMajorAxisZStep = -CViewObject::m_viewPointDirMx.m[2][2];
		fMinorAxisZStep = CViewObject::m_viewPointDirMx.m[2][0];
		iMajorAxisStart0 = m_polygonRect.top;
		iMajorAxisEnd0 = m_polygonRect.bottom;
		iL = iR = iL1 = iR1 = polygonRectIdx.top;
		l1 = l = (int)floor(polygonVertices[iL].x); r1 = r = l+1;
		int y = (int)floor(polygonVertices[iL].y)+1;
		bool	bLeaveEdgesLoop = FALSE;
		WRITELOG1("m_polygonRect = CRect2(%d,%d,%d,%d)\n",m_polygonRect);
		for( i = 0 ; /*y < m_polygonRect.bottom*/; i++, y++ ) {	// edge addition loop
			while( polygonVertices[iL1].y < y ) {	// search for next edgeL span
				l = Min(l,(int)floor(polygonVertices[iL1].x));
				if( iL1 == polygonRectIdx.bottom ) { bLeaveEdgesLoop = TRUE; break; } //goto FinishCreateEdges;
				iL = iL1; if( iL1 <= 0 ) iL1 = nPolygonVertices; iL1--;
				CFVector2	d = polygonVertices[iL1]-polygonVertices[iL];
				tanL = d.x/Max(d.y,1e-3);
			}
			while( polygonVertices[iR1].y < y ) {	// search for next edgeR span
				r = Max(r,(int)floor(polygonVertices[iR1].x)+1);
				if( iR1 == polygonRectIdx.bottom ) { bLeaveEdgesLoop = TRUE; break; } //goto FinishCreateEdges;
				iR = iR1; if( ++iR1 >= nPolygonVertices ) iR1 = 0;
				CFVector2	d = polygonVertices[iR1]-polygonVertices[iR];
				tanR = d.x/Max(d.y,1e-3);
			}
			if( bLeaveEdgesLoop ) break;
			l = Min(l,l1=(int)floor(polygonVertices[iL].x+tanL*(y-polygonVertices[iL].y)));
			r = Max(r,r1=(int)floor(polygonVertices[iR].x+tanR*(y-polygonVertices[iR].y))+1);
			ASSERT(i<m_pnEdges[0]);
			m_ppEdges[0][i][I_L] = l; m_ppEdges[0][i][I_R] = r;
			if( m_ppEdges[0][i][I_L] < m_ppEdges[0][m_edgesOuter[I_L]][I_L] ) m_edgesOuter[I_L] = i;
			if( m_ppEdges[0][i][I_R] > m_ppEdges[0][m_edgesOuter[I_R]][I_R] ) m_edgesOuter[I_R] = i;
			WRITELOG3("Edge[%d]: %d,%d\n",y-1,l,r);
			l = l1; r = r1;
		} // edge addition loop
	} // creating edges along Y
	//FinishCreateEdges:
	ASSERT(i==(IsViewAlongX()?m_polygonRect.Width():m_polygonRect.Height())-1);
	m_ppEdges[0][i][I_L] = l; m_ppEdges[0][i][I_R] = r;
	WRITELOG2("Edge[last]: %d,%d\n",l,r);

	int nEdges0 = i+1;
	ASSERT(nEdges0 < m_pnEdges[0]);
	ASSERT(nEdges0 == iMajorAxisEnd0-iMajorAxisStart0);


	// Needa provide convexibility of edges along y !!!
	{
		int outer = m_ppEdges[0][0][I_L];
		for( i = 1 ; i < m_edgesOuter[I_L] ; i++ ) {
			if( m_ppEdges[0][i][I_L] > outer )
				m_ppEdges[0][i][I_L] = outer;
			else
				outer = m_ppEdges[0][i][I_L];
		}

		outer = m_ppEdges[0][0][I_R];
		for( i = 1 ; i < m_edgesOuter[I_R] ; i++ ) {
			if( m_ppEdges[0][i][I_R] < outer )
				m_ppEdges[0][i][I_R] = outer;
			else
				outer = m_ppEdges[0][i][I_R];
		}

		outer = m_ppEdges[0][nEdges0-1][I_L];
		for( i = nEdges0-2 ; i > m_edgesOuter[I_L] ; i-- ) {
			if( m_ppEdges[0][i][I_L] > outer )
				m_ppEdges[0][i][I_L] = outer;
			else
				outer = m_ppEdges[0][i][I_L];
		}

		outer = m_ppEdges[0][nEdges0-1][I_R];
		for( i = nEdges0-2 ; i > m_edgesOuter[I_R] ; i-- ) {
			if( m_ppEdges[0][i][I_R] < outer )
				m_ppEdges[0][i][I_R] = outer;
			else
				outer = m_ppEdges[0][i][I_R];
		}
	}
	m_edgesOuter[I_L] += iMajorAxisStart0;
	m_edgesOuter[I_R] += iMajorAxisStart0;

	(void)iMajorAxisEnd0; (void)iMajorAxisStart0;
	(void)fMinorAxisZStep; (void)fMajorAxisZStep;
	{
		CFVector3	viewOffset = CViewObject::m_viewPointInvMx.Offset();
		#if 1
		m_zCoeff.x = -CViewObject::m_viewPointDirMx.m[2][0];
		m_zCoeff.y = -CViewObject::m_viewPointDirMx.m[2][1];
		m_zCoeff.z = -CViewObject::m_viewPointDirMx.m[2][2];
		m_zCoeff0 = m_zCoeff*viewOffset/-m_fCellSize;
		m_zCoeff.y *= m_fHScale;
		m_nNearestHeight = m_zCoeff.y < 0;
		// -CViewObject::m_viewPointDirMx.m[2][3]/m_fCellSize;
		#else
		CFVector2	view2( -CViewObject::m_viewPointDirMx.m[2][0],
							CViewObject::m_viewPointDirMx.m[2][2] );
		view2 = Normal(view2);
		m_zCoeff.x = view2.x;
		m_zCoeff.y = view2.y;
		m_zCoeff.z = 0;
		m_zCoeff0 = (m_zCoeff.x*viewOffset.x-m_zCoeff.y*viewOffset.z)/-m_fCellSize;
		#endif
	}


	#if _PRECOMPUTE_REDS
	{
		if( IsViewAlongX() ) {
			m_pReds = m_pRedsOrg0 - m_viewpointL[I_X]*m_nRedsP - m_viewpointL[I_Y];
		} else {
			m_pReds = m_pRedsOrg0 - m_viewpointL[I_Y]*m_nRedsP - m_viewpointL[I_X];
			// P=X  S=Y
		}
		// m_zCoeff.z has negative sign!!!!
		if( m_zCoeff.z < 0 ) {
			if( m_zCoeff.x >= 0 )	// Y+ X+
				DefineReductionsB();
			else					// Y+ X-
				DefineReductionsR();
		}
		else {
			if( m_zCoeff.x >= 0 )	// P- S+
				DefineReductionsL();
			else					// P- S-
				DefineReductionsT();
		}
	}
	#endif


	// Compute reductions for edges
	#if _ALIGN_EDGES		// !_INIT_CACHE
	if( IsViewAlongX() ) {
		for( i = 0 ; i < nEdges0 ; i++ ) {
			m_pEdgeR[i][I_L] = ReductionCell(m_polygonRect.left+i,m_ppEdges[0][i][I_L]);
			m_pEdgeR[i][I_R] = ReductionCell(m_polygonRect.left+i,m_ppEdges[0][i][I_R]-1);
		}
	}
	else {
		for( i = 0 ; i < nEdges0 ; i++ ) {
			m_pEdgeR[i][I_L] = ReductionCell(m_ppEdges[0][i][I_L],m_polygonRect.top+i);
			m_pEdgeR[i][I_R] = ReductionCell(m_ppEdges[0][i][I_R]-1,m_polygonRect.top+i);
		}
	}
	for( i = 0 ; i < nEdges0 ; i++ ) {
		int i1, rangeStart, rangeEnd;
		const int leftMask = -(1<<m_pEdgeR[i][I_L]);
		rangeStart = Max(((i+iMajorAxisStart0)&leftMask)-iMajorAxisStart0,0);
		rangeEnd = Min(rangeStart+(1<<m_pEdgeR[i][I_L]),nEdges0);
		for( i1 = rangeStart ; i1 < rangeEnd ; i1++ )
			m_pEdgeR[i1][I_L] = Max(m_pEdgeR[i1][I_L],m_pEdgeR[i][I_L]);
		const int rightMask = -(1<<m_pEdgeR[i][I_R]);
		rangeStart = Max(((i+iMajorAxisStart0)&rightMask)-iMajorAxisStart0,0);
		rangeEnd = Min(rangeStart+(1<<m_pEdgeR[i][I_R]),nEdges0);
		for( i1 = rangeStart ; i1 < rangeEnd ; i1++ )
			m_pEdgeR[i1][I_R] = Max(m_pEdgeR[i1][I_R],m_pEdgeR[i][I_R]);
	}
	__polygonReductionCall = __reductionCall;

	// Create edges for non-zero reductions (mind nEdges0, iMajorAxisStart and
	// iMajorAxisEnd are modified and no longer valid after this)
	for( int red = 1 ; red < m_nReductions ; red++ ) {
		int red0 = red-1;
		int iMajorAxisStart = iMajorAxisStart0>>1,
			iMajorAxisEnd = iMajorAxisEnd0>>1;
		int nEdges = iMajorAxisEnd-iMajorAxisStart;
		ASSERT(nEdges<=m_pnEdges[red]);
		int i0 = 0;
		i = 0;
		if( iMajorAxisStart0 & 1 ) {
			m_ppEdges[red][i][I_L] = m_ppEdges[red0][i0][I_L];
			m_ppEdges[red][i++][I_R] = m_ppEdges[red0][i0++][I_R];
		}
		for( ; i < nEdges ; i++, i0+=2 ) {
			//CEdge	&e = m_ppEdges[red0][i];
			m_ppEdges[red][i][I_L] = Min(m_ppEdges[red0][i0][I_L],m_ppEdges[red0][i0+1][I_L]);
			m_ppEdges[red][i][I_R] = Max(m_ppEdges[red0][i0][I_R],m_ppEdges[red0][i0+1][I_R]);
		}
		if( iMajorAxisEnd0 & 1 ) {
			m_ppEdges[red][i][I_L] = m_ppEdges[red0][i0][I_L];
			m_ppEdges[red][i][I_R] = m_ppEdges[red0][i0][I_R];
			nEdges++; iMajorAxisEnd++;
		}
		ASSERT( i0 + (iMajorAxisEnd0 & 1) == nEdges0 );
		nEdges0 = nEdges;
		iMajorAxisStart0 = iMajorAxisStart;
		iMajorAxisEnd0 = iMajorAxisEnd;
	}
	#endif

	m_polygonRectL[I_LEFT] = m_polygonRect.left;
	m_polygonRectL[I_TOP] = m_polygonRect.top;
	m_polygonRectL[I_RIGHT] = m_polygonRect.right;
	m_polygonRectL[I_BOTTOM] = m_polygonRect.bottom;


	// normal = CViewObject::m_viewPointDirMx%CFVector3(0,1,0)
	m_waterNormal.a = static_cast<float>(CViewObject::m_viewPointDirMx.m[0][1]);
	m_waterNormal.b = static_cast<float>(CViewObject::m_viewPointDirMx.m[1][1]);
	m_waterNormal.c = static_cast<float>(CViewObject::m_viewPointDirMx.m[2][1]);
	// d0 = normal0*CFVector3(0,terrainWLine*fHScale,0) = terrainWLine*fHScale
	// d = d0+normal*CViewObject::m_viewPointDirMx.Offset();
	m_waterNormal.d = static_cast<float>(CViewFigure::Waterline()+
		CViewObject::m_viewPointDirMx.m[0][3]*CViewObject::m_viewPointDirMx.m[0][1]+
		CViewObject::m_viewPointDirMx.m[1][3]*CViewObject::m_viewPointDirMx.m[1][1]+
		CViewObject::m_viewPointDirMx.m[2][3]*CViewObject::m_viewPointDirMx.m[2][1]
		);


	#if _RC_COUNT
	if( __countRc ) {
		for( int i = 0 ; i < 512 ; i++ )
		for( int j = 0 ; j < 512 ; j++ )
			rcCount[i][j] = 255;
	}
	#endif

}


inline double FASTLOG2(double x) { return log2(x); }

inline	void FASTLOG2INT(double x, int *p) { *p = (int)log2(x); }
/*#pragma aux FASTLOG2INT = \
		"fld1"	\
		"fxch"	\
		"fyl2x"	\
		"fistp dword ptr [eax]" \
		parm [8087] [eax]*/


inline double	_CViewTerrain::Reduction(double z)
{
	#if _STATIC_REDUCTIONS
	(void)z;
	return 0;
	#else
	if( z < m_fDistRed0 ) return 0;
	return FASTLOG2(z/m_fDistRed0);
	//return	Min(log2(z/m_fDistRed0),(double)nTerrainMaxReduction);
	#endif
}

inline int	_CViewTerrain::ReductionInt(double z)
{
	#if _STATIC_REDUCTIONS
	(void)z;
	return 0;
	#else
	if( z < m_fDistRed02 ) return 0;
	int n;
	FASTLOG2INT(z/m_fDistRed0Sq2,&n);
	return n;
	//return	Min(log2(z/m_fDistRed0Sq2),(double)nTerrainMaxReduction);
	#endif
}


int  _CViewTerrain::ReductionCell(int x,int y)
{
	__reductionCall++;
	#if _STATIC_REDUCTIONS
		WRITELOG3("ReductionCell(%d,%d)=%d\n",x,y,(int)m_red(x,y)&15);
		(void)x; (void)y;
		return MAX_REDCELL(x,y); //m_red(x,y)&15;
	#else
	//int	x = m_vertexL[I_X], y = m_vertexL[I_Y];
	//if( __countRc && y >= 0 && y < 512 && x >= 0 && x < 512 && rcCount[y][x]<255 )
	//	rcCount[y][x]++;
	int maxRed = MAX_REDCELL(x,y);
	#if _WATER_NOREDRESTRICT
	if( m_bIsAboveWaterCpl /*&& m_heights(x,y) <= __nTerrainWaterHighest*/ ) {
		while( maxRed < nTerrainMaxReduction ) {
			int r = maxRed+1, d = 1<<r, mask=-d,
				x0 = x&mask, y0=y&mask, x1 = x0+d, y1 = y0+d;
			for( int yy = y0 ; yy <= y1 ; yy++ )
			for( int xx = x0 ; xx <= x1 ; xx++ )
				if( m_heights(xx,yy) > __nTerrainWaterHighest ) goto NoLandInvis;
			maxRed = r;
		}
	}
NoLandInvis:
	#endif
	if( maxRed == 0 ) return 0;
	#if _REDUCTION_IMMEDIATE
	for( int r = 1 ; r <= maxRed ; r++ ) {
		int mask = -1<<r;
		int x0 = x&mask, y0 = y&mask;
		//if( m_red(x0,y0) <= 0xC0 ) jlkjl
		double z = m_zCoeff.x*(x0+(m_nearestOffset.x<<r))+
							m_zCoeff.y*m_hrange.GetLimit(x0,y0,m_nNearestHeight,r)-
							m_zCoeff.z*(y0+(m_nearestOffset.y<<r))+m_zCoeff0;
		if( z < m_fDistRed0 ) return r-1;
	}
	return maxRed;
	#else

	for( int n = 1 ; ; ) {
		ASSERT(n<20);
		int mask = -(1<<n);
		int x0 = x&mask, y0 = y&mask;
		double z = m_zCoeff.x*(x0+(m_nearestOffset.x<<n))+
							m_zCoeff.y*m_hrange.GetLimit(x0,y0,m_nNearestHeight,n)-
							m_zCoeff.z*(y0+(m_nearestOffset.y<<n))+m_zCoeff0;
		int s = Min(ReductionInt(z),maxRed);
		//int s = (int)r;
		ASSERT( s >= n-1 );
		if( s <= n ) {
			ASSERT( s < m_nReductions );
			WRITELOG3("ReductionCell(%d,%d)=%d\n",x,y,s);
			return s;
			//return Min(s,MAX_REDCELL(x,y)); //Min(r,8.);
		}
		n = s;
	}
	#endif // immediate
	#endif // static
}

bool TerrainView_FitCell(bool terrainInView,
                         const long polygonRect[4],
                         const int (*edges)[2],
                         int primaryAxis,
                         bool primaryReverse,
                         bool secondaryReverse,
                         SVector2& cell)
{
	// Parameterized form of the recovered B/T/L/R terrain4.inl variants.
	ASSERT(primaryAxis == 0 || primaryAxis == 1);
	ASSERT(polygonRect != NULL && edges != NULL);
	long *v = &cell.x;
	const int secondaryAxis = 1-primaryAxis;
	const int primaryStep = primaryReverse ? 1 : 0;
	const int secondaryStep = secondaryReverse ? 1 : 0;
	const int primaryTop = primaryAxis+2*primaryStep;
	const int primaryBottom = primaryAxis+2*(1-primaryStep);
	const int primary = v[primaryAxis]+primaryStep;

	if( !terrainInView ||
		(primaryReverse ? primary > polygonRect[primaryTop] :
		                  primary < polygonRect[primaryTop]) )
		return FALSE;
	if( primaryReverse ? primary <= polygonRect[primaryBottom] :
		                 primary >= polygonRect[primaryBottom] )
		v[primaryAxis] = polygonRect[primaryBottom]+(primaryStep-1);

	const int i = static_cast<int>(
		v[primaryAxis]-polygonRect[primaryAxis]);
	ASSERT(i >= 0 && i <
		polygonRect[primaryAxis+2]-polygonRect[primaryAxis]);
	const int secondaryLow = edges[i][secondaryStep]-secondaryStep;
	const int secondaryHigh = edges[i][1-secondaryStep]+(secondaryStep-1);
	if( secondaryReverse ? v[secondaryAxis] > secondaryLow :
		                    v[secondaryAxis] < secondaryLow )
		v[secondaryAxis] = secondaryLow;
	else if( secondaryReverse ? v[secondaryAxis] < secondaryHigh :
		                         v[secondaryAxis] > secondaryHigh )
		v[secondaryAxis] = secondaryHigh;
	return TRUE;
}

bool _CViewTerrain::FitInTrapezioid(SVector2 &v)
{
	const bool alongX = IsViewAlongX();
	const bool primaryReverse = alongX ? IsViewAlongXN() : IsViewAlongYN();
	const bool secondaryReverse = alongX ? !primaryReverse : primaryReverse;
	return TerrainView_FitCell(m_bTerrainInView,
		m_polygonRectL,m_ppEdges[0],alongX ? I_X : I_Y,
		primaryReverse,secondaryReverse,v);
}
