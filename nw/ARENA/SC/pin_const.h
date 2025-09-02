
void s_lev_LOAD( TStackCell *cell )    { cell->i = lev_LOAD; }
void s_lev_SAVE( TStackCell *cell )    { cell->i = lev_SAVE; }
void s_lev_RESTART( TStackCell *cell ) { cell->i = lev_RESTART; }

 //===========================================================================
void s_Const_lmp_EV_START( TStackCell *cell )
 {
     cell->i = lmp_EV_START;
 }

 //===========================================================================
void s_Const_lmp_EV_SETENDPOS( TStackCell *cell )
 {
     cell->i = lmp_EV_SETENDPOS;
 }

