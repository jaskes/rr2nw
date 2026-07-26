#include "route.h"
#include "kernel/h/s_debug.h"

void *Route::queryInterface( int interNum ){
    switch( interNum ){
		case IUnknownIID:       return (KR_Object*)this;
		case IRouteObjectIID:   return (IRouteObject*) this;
    }
    return NULL;
}
void Route::Load(const char * fName) 
{
   LoadRoute(fName); 
}

int Route::GetNodeCnt() const 
{
   return(m_nodeQnty);
}

int Route::GetLen() const
{
   return(m_totalLenght);
}

CFVector3 Route::GetNode(int n)  const 
{
   if(  m_nodeQnty <= 0  )
   {
      echo("Route::GetNode: route has no nodes");
      return CFVector3(0,0,0);
   }

   if(  n < 0  )
   {
      echo("Route::GetNode: clamp node index %i", n);
      n = 0;
   }
   else
   if(  n >= m_nodeQnty  )
   {
      echo("Route::GetNode: clamp node index %i", n);
      n = m_nodeQnty-1;
   }

   return(m_node[m_base+n]); 
}

CFVector3 Route::GetPos(double part) const{
	CFVector3 pos;

	if(  m_nodeQnty <= 0  )
	{
		echo("Route::GIVE_INT_POS: route has no nodes.");
		return CFVector3(0,0,0);
	}
	if(  m_nodeQnty == 1  )
		return m_node[m_base];

	if(  part < 0  )
		part = 0;
	else
	if(  part > 1  )
		part = 1;

	for(int i = m_base ;  ; i++ )
		if( part <= m_length[i] || i == m_base+m_nodeQnty-2 ){
			pos = m_node[i] + m_napr[i] * part;
			break;
		}
		else part -= m_length[i] ;

	return(pos);
}
