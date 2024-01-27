//
//  lib3d entry point & LUA extensions

// compile
// cmake .. -G "NMake Makefiles" --toolchain="%PLAYDATE_SDK_PATH%\C_API\buildsupport\arm.cmake" -DCMAKE_BUILD_TYPE=Release
// nmake

#include <math.h>
#include <float.h>
#include "math.h"
#include "luaglue.h"
#include "realloc.h"
#include "ground.h"

#define fswap(a,b) {float tmp = b; b=a; a=tmp;}

static PlaydateAPI* pd = NULL;


static void* getArgObject(int n, char* type)
{
	void* obj = pd->lua->getArgObject(n, type, NULL);
	
	if ( obj == NULL )
		pd->system->error("object of type %s not found at stack position %i", type, n);
	
	return obj;
}

static GroundParams* getGroundParams(int n)			{ return getArgObject(n, "lib3d.GroundParams"); }


/// Ground Params

static int ground_params_new(lua_State* L)
{
	GroundParams* p = lib3d_malloc(sizeof(GroundParams));
    p->slope = 0;
    p->num_tracks = 0;
    p->props_rate = 0.1f;
    
	pd->lua->pushObject(p, "lib3d.GroundParams", 0);
	return 1;
}

static int ground_params_gc(lua_State* L)
{
	GroundParams* p = getGroundParams(1);
	lib3d_free(p);
	return 0;
}

static int ground_params_index(lua_State* L)
{
	GroundParams* p = getGroundParams(1);
	const char* arg = pd->lua->getArgString(2);
	
	if ( strcmp(arg, "slope") == 0 )
		pd->lua->pushFloat(p->slope);
	else if ( strcmp(arg, "num_tracks") == 0 )
		pd->lua->pushFloat(p->num_tracks);
	else if ( strcmp(arg, "props_rate") == 0 )
		pd->lua->pushFloat(p->props_rate);
	else
		pd->lua->pushNil();
	
	return 1;
}

static int ground_params_newindex(lua_State* L)
{
	GroundParams* p = getGroundParams(1);
	const char* arg = pd->lua->getArgString(2);
	
	if ( strcmp(arg, "slope") == 0 )
		p->slope = pd->lua->getArgFloat(3);
	else if ( strcmp(arg, "num_tracks") == 0 )
		p->num_tracks = pd->lua->getArgFloat(3);
	else if ( strcmp(arg, "props_rate") == 0 )
		p->props_rate = pd->lua->getArgFloat(3);
	
	return 0;
}

static const lua_reg lib3D_GroundParams[] =
{
	{ "new",			ground_params_new },
	{ "__gc", 			ground_params_gc },
	{ "__index",		ground_params_index },
	{ "__newindex",		ground_params_newindex },
	{ NULL,				NULL }
};

// 
static int lib3d_render_ground(lua_State* L)
{
	int argc = 1;
	// cam pos
	Point3d pos;
	for (int i = 0; i < 3; ++i) {
		// Get it's value.        
		pos.v[i] = pd->lua->getArgFloat(argc++);
	}

    // camera matrix
    float m[16];

    for (int i = 0; i < 16; ++i) {
        // Get it's value.        
        m[i] = pd->lua->getArgFloat(argc++);
    }

    // draw_polygon(verts, len / 2, (uint32_t*)pd->graphics->getFrame());
    uint32_t* bitmap = (uint32_t*)pd->graphics->getFrame();

	render_ground(pos, m, bitmap);

    pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);

	return 0;
}

static int lib3d_get_start_pos(lua_State* L) {
    Point3d out;
    get_start_pos(&out);
    for(int i=0;i<3;++i) {
        pd->lua->pushFloat(out.v[i]);
    }
    return 1;
}

static int lib3d_make_ground(lua_State* L) {
	GroundParams* p = getGroundParams(1);

	make_ground(*p);

	return 0;
}

static int lib3d_get_face(lua_State* L) {
	Point3d pos;
	for (int i = 0; i < 3; ++i) {
		pos.v[i] = pd->lua->getArgFloat(i+1);
	}

	Point3d n;
	float y;
	get_face(pos, &n, &y);

	pd->lua->pushFloat(y);
	
	pd->lua->pushFloat(n.x);
	pd->lua->pushFloat(n.y);
	pd->lua->pushFloat(n.z);

	// arg count
	return 4;
}

static int lib3d_update_ground(lua_State* L) {
	Point3d pos;
	for (int i = 0; i < 3; ++i) {
		pos.v[i] = pd->lua->getArgFloat(i + 1);
	}

	update_ground(&pos);

	pd->lua->pushFloat(pos.z);
	return 1;
}

void lib3d_register(PlaydateAPI* playdate)
{
	pd = playdate;

	const char* err;

	if (!pd->lua->addFunction(lib3d_make_ground, "lib3d.make_ground", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if ( !pd->lua->addFunction(lib3d_render_ground, "lib3d.render_ground", &err) )
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if ( !pd->lua->addFunction(lib3d_get_start_pos, "lib3d.get_start_pos", &err) )
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_get_face, "lib3d.get_face", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_update_ground, "lib3d.update_ground", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->registerClass("lib3d.GroundParams", lib3D_GroundParams, NULL, 0, &err))
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);

    lib3d_setRealloc(pd->system->realloc);

    // 
    ground_load_assets(playdate);
}
