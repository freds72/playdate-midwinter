#ifndef lib3d_glue_h
#define lib3d_glue_h

#include <pd_api.h>

// helper macros
#define C_TO_LUA(s, name, type) \
	do {if ( strcmp(arg, #name) == 0 ) { pd->lua->push ## type (s->name); return 1; } } while(0)
#define LUA_TO_C(s, name, type) \
	do {if ( strcmp(arg, #name) == 0 ) { p->name = pd->lua->getArg ## type (argc++); } } while(0)

void lib3d_register(PlaydateAPI* playdate);
void lib3d_unregister(PlaydateAPI* playdate);
void* getArgObject(int n, char* type);

#endif
