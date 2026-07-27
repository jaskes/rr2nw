#ifndef __CACHESMOKE_H_INCLUDED
#define __CACHESMOKE_H_INCLUDED

class cs_CacheSmoke
{
public:
        char        fname[128];
        void       *hand;
};

extern cs_CacheSmoke g_cacheSmoke[10];
extern int           g_cacheSmokeCnt;
void *g_loadSmoke( const char *fileName, void * , void *a = NULL);
int SmokeTextureCache_Checkpoint();
bool SmokeTextureCache_CanLoad(const char *const *fileNames, int count);
void SmokeTextureCache_Rollback(int checkpoint);

#endif
