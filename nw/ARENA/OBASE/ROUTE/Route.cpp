#include <stdio.h>
#include <string.h>
#include <float.h>

#include "route.h"
#include "kernel/h/s_debug.h"

#include "mathlib.h"

#define HANDLE int
#include "storage\h\savefile.h"

#define min Min
#define max Max
#define BYTE byte

static RouteTable __routeTable;

static bool IsFinite(double value)
{
    return value == value && value <= DBL_MAX && value >= -DBL_MAX;
}


CFVector3 Route::m_node[ROUTE_MAX_NODE_NUM];
CFVector3 Route::m_napr[ROUTE_MAX_NODE_NUM];
double	 Route::m_length[ROUTE_MAX_NODE_NUM];
int Route::m_totalNodePos=0;

//============================================================================
void Route::AddRef()
{
    m_refs++;
}

//============================================================================
void Route::DelRef()
{
    m_refs--;
    if(  m_refs==0  )
    {
         if(  context  )
         {
              Delete();
              context->removeObject( getObjectID() );
         }
    };
}

//============================================================================
void Route::Delete()
{
     int i;
     int cnt = m_totalNodePos-m_nodeQnty;

     for( i = m_base; i < cnt; ++i )
     {
          m_node[i]   = m_node[i+m_nodeQnty];
          m_napr[i]   = m_napr[i+m_nodeQnty];
          m_length[i] = m_length[i+m_nodeQnty];
     }

     m_totalNodePos -= m_nodeQnty;

     for(
           ct_Object *ro = __routeTable.getExist();
           ro != 0;
           ro = ro->next()

        )
     {
          Route *r = (Route*)ro;
          if(  r->m_base > m_base  )
               r->m_base -= m_nodeQnty;
     }
}

//============================================================================
enum EReadStrResult
{
    READ_STR_INVALID = -1,
    READ_STR_EOF = 0,
    READ_STR_OK = 1
};

static int ReadStr( FILE *f, char buff[], int maxSize ){
    int c = 0;
    int len = 0;
    bool overflow = false;
    bool readAnything = false;

    if( f == NULL || buff == NULL || maxSize <= 0 )
         return READ_STR_INVALID;

    buff[0] = 0;
    while( (c = fgetc(f)) != EOF ){
         readAnything = true;
         if( c == 13 || c == 10 ){
              if( c == 13 ){
                   int next = fgetc(f);
                   if( next != 10 && next != EOF )
                        ungetc(next, f);
              }
              break;
         }

         if( len < maxSize-1 )
              buff[len++] = (char)c;
         else
              overflow = true;
    }
    buff[len] = 0;

    while( len > 0 && buff[len-1] <= ' ' )
         buff[--len] = 0;

    if( overflow )
         return READ_STR_INVALID;
    return readAnything ? READ_STR_OK : READ_STR_EOF;
}
//============================================================================
void Route::addNotify()
{
    m_nodeQnty = 0; 
    m_refs     = 0;
    m_totalLenght = 0;
    m_base = 0;
    ct_Object::addNotify();
}

//============================================================================
void Route::removeNotify()
{
	m_nodeQnty = 0;
	m_totalLenght = 0;
	ct_Object::removeNotify();
}

//============================================================================
void Route::EvaluateLenght()
{
	int i;

	m_totalLenght = 0;
	if(  m_nodeQnty <= 1  )
		return;

	for( i= m_base ; i < m_base+m_nodeQnty-1 ; i++ )
		m_totalLenght += Abs( m_node[i] - m_node[i+1] );

	if(  m_totalLenght <= 1e-12  )
	{
		for( i = m_base ; i < m_base+m_nodeQnty-1 ; i++ )
		{
			m_length[i] = 0;
			m_napr[i]   = CFVector3(0,0,0);
		}
		return;
	}

	for( i = m_base ; i < m_base+m_nodeQnty-1 ; i++ ){
		double segLen = Abs( m_node[i] - m_node[i+1] );
		m_length[i] = segLen / m_totalLenght;
		if(  m_length[i] > 1e-12  )
			m_napr[i] = ( m_node[i+1] - m_node[i] ) / m_length[i] ;
		else
			m_napr[i] = CFVector3(0,0,0);
	}
}

//============================================================================
void Route::LoadRoute(const char *fName)
 {
    m_nodeQnty = 0;
    m_totalLenght = 0;
    m_base = m_totalNodePos;

    FILE *f = fopen(fName,"rb");
    char buff[256];

    if(f == NULL){
         echo("Route::load: File %s not found\n", fName);
         return;
    }
    if( ReadStr(f, buff, sizeof(buff)) != READ_STR_OK ){
         echo("Route::load: Bad route name %s\n", fName);
         fclose(f);
         return;
    }
    const char *routeName = context->searchObject(getObjectID());
    if( routeName != 0 && strcmp(buff, routeName) != 0 )
         echo("Route::load: Suacript_RouteName != file_RouteName\n");

    int declaredNodeQnty = 0;
    if( ReadStr(f, buff, sizeof(buff)) != READ_STR_OK ||
        sscanf(buff, "%i", &declaredNodeQnty) != 1 ){
         echo("Route::load: Bad node count %s\n", fName);
         fclose(f);
         return;
    }

    if(  declaredNodeQnty <= 0  )
    {
         echo("Route::load: Empty route %s\n", fName);
         fclose(f);
         return;
    }
    if(  m_totalNodePos + declaredNodeQnty >= ROUTE_MAX_NODE_NUM  )
    {
         echo("Route::load: Route overflow %s\n", fName);
         fclose(f);
         return;
    }

    int loadedNodeQnty = 0;
    for( ; loadedNodeQnty < declaredNodeQnty; ++loadedNodeQnty ){
         double x = 0;
         double y = 0;
         double z = 0;
         const int readResult = ReadStr(f,buff,sizeof(buff));
         if( readResult != READ_STR_OK ){
              if( readResult == READ_STR_EOF && loadedNodeQnty > 0 )
                   echo("Route::load: Truncated route %s (%i/%i nodes)\n",
                        fName, loadedNodeQnty, declaredNodeQnty);
              if( readResult == READ_STR_EOF )
                   break;
              echo("Route::load: Bad coord[%i]\n",m_base+loadedNodeQnty);
              fclose(f);
              return;
         }
         const int parsed = sscanf(buff,"[ %lf , %lf , %lf ]",&x,&y,&z);

         if( parsed != 3 || !IsFinite(x) || !IsFinite(y) || !IsFinite(z) ){
              echo("Route::load: Bad coord[%i]\n",m_base+loadedNodeQnty);
              fclose(f);
              return;
         }
         m_node[m_base+loadedNodeQnty].x = x;
         m_node[m_base+loadedNodeQnty].y = y;
         m_node[m_base+loadedNodeQnty].z = z;
    }

    if( loadedNodeQnty <= 0 ){
         echo("Route::load: Empty route %s\n", fName);
         fclose(f);
         return;
    }

    m_nodeQnty = loadedNodeQnty;
    m_totalNodePos += m_nodeQnty;
    fclose(f);
}
//============================================================================
int Route::receiveEvent(KR_Event &event){
    char msg[250];
	CFVector3 pos;
	int n;

    switch( event.label ){
		case KR_WAKE_UP:
                                break;
		case ROUTE_LOAD:
				event.data.open(EDO_READ)
							.getStr( msg , sizeof(msg) )
						  .close();
				Load(msg);
				EvaluateLenght();
				break;

  		case ro_EV_GET_NODE_CNT:
				event.data.open( EDO_WRITE )
							.putInt( m_nodeQnty )
						  .close();
				break;
			 
  		case ro_EV_GET_LENGHT:
				event.data.open( EDO_WRITE )
							.putDouble( m_totalLenght )
						  .close();
				break;

case ro_EV_GET_NODE:
		event.data.open( EDO_READ )
					.getInt( n )
				  .close();

		if(  m_nodeQnty <= 0  )
		{
			echo( "Route::GIVE_POINT: route has no nodes." );
			break;
		}

		pos = GetNode(n);

		event.data.open( EDO_WRITE )
						.descend( 5 , 1 )
								.putDouble( pos.x )
								.putDouble( pos.y )
								.putDouble( pos.z )
						.ascend()
				  .close();
	break;

case ro_EV_GET_POS:
		double part;
		int	   type;

		event.data.open( EDO_READ )
					.getDouble(part)
					.getInt( type )
				  .close();

		if(  m_nodeQnty <= 0  )
		{
			echo("Route::GIVE_INT_POS: route has no nodes." );
			break;
		}

		pos = GetPos(part);
		event.data.open( EDO_WRITE )
						.descend( 5 , 2 )
							.putDouble( pos.x )
							.putDouble( pos.y )
							.putDouble( pos.z )
						.ascend()
				      .close();
	break;

		default: return 0;
    }
    return 1;
}


bool	Route::dump(PIN_SaveFile & sf)
{
		if (!sf.WriteData( (char *) & m_nodeQnty, sizeof(RouteData)  ))
			return false;
		
		return true;
}

bool	Route::load(PIN_SaveFile & sf)
{
		if (!sf.GetData( (char *) & m_nodeQnty, sizeof(RouteData)  ))
			return false;
		
		return true;
}

void	Route::loadNotify()
{
}

bool Route::LoadStaticData(PIN_SaveFile & sf)
{
	if (!sf.GetData( (char *) m_node,   sizeof(CFVector3) * ROUTE_MAX_NODE_NUM) ||
		!sf.GetData( (char *) m_napr,   sizeof(CFVector3) * ROUTE_MAX_NODE_NUM) ||
		!sf.GetData( (char *) m_length, sizeof(double)    * ROUTE_MAX_NODE_NUM) ||
		!sf.GetData( (char *) & m_totalNodePos, sizeof(int)))
		return false;
	return true;
}

bool Route::SaveStaticData(PIN_SaveFile & sf)
{
	if (!sf.WriteData( (char *) m_node,		sizeof(CFVector3) * ROUTE_MAX_NODE_NUM) ||
		!sf.WriteData( (char *) m_napr,		sizeof(CFVector3) * ROUTE_MAX_NODE_NUM) ||
		!sf.WriteData( (char *) m_length,	sizeof(double)    * ROUTE_MAX_NODE_NUM) ||
		!sf.WriteData( (char *) & m_totalNodePos, sizeof(int)))
		return false;
	return true;
}


//============================================================================

//============================================================================
void RouteTable::allocObjects( int objectQnty ){
    Route::m_totalNodePos = 0; 
    m_table = new Route[objectQnty];
    if(m_table == NULL) m_maxObjectQnty = 0;
}

//============================================================================
void RouteTable::freeObjects(){
    delete [] m_table;
    m_table = NULL;
    m_maxObjectQnty = 0;
    Route::m_totalNodePos = 0;
}

//============================================================================
ct_Object *RouteTable::getObjectPTR( int index ){
    if( index<0 || index>=m_maxObjectQnty ){
         echo("RouteTable::SearchObject: Bad index");
         return NULL;
    }
    return &(m_table[index]);
}



/* End of file ROUTE.CPP */
