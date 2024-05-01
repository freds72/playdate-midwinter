//
//  lib3d entry point & LUA extensions

// compile
// cmake .. -G "NMake Makefiles" --toolchain="%PLAYDATE_SDK_PATH%\C_API\buildsupport\arm.cmake" -DCMAKE_BUILD_TYPE=Release
// nmake

#include <stdint.h>
#include <math.h>
#include <float.h>
#include "luaglue.h"
#include "realloc.h"
#include "ground.h"
#include "tracks.h"
#include "gfx.h"

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
	else if ( strcmp(arg, "tracks") == 0 )
		pd->lua->pushInt(p->num_tracks);
	else if ( strcmp(arg, "props_rate") == 0 )
		pd->lua->pushFloat(p->props_rate);
	else if (strcmp(arg, "twist") == 0)
		pd->lua->pushFloat(p->twist);
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
	else if ( strcmp(arg, "tracks") == 0 )
		p->num_tracks = pd->lua->getArgInt(3);
	else if ( strcmp(arg, "props_rate") == 0 )
		p->props_rate = pd->lua->getArgFloat(3);
	else if (strcmp(arg, "twist") == 0)
		p->twist = pd->lua->getArgFloat(3);

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


LCDBitmap* _mire_bitmap = NULL;
uint8_t* _mire_data = NULL;

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
	// cam angle
	const float tau_angle = pd->lua->getArgFloat(argc++);

    // camera matrix
    float m[16];

    for (int i = 0; i < 16; ++i) {
        // Get it's value.        
        m[i] = pd->lua->getArgFloat(argc++);
    }

    uint8_t* bitmap = pd->graphics->getFrame();

	render_ground(pos, tau_angle, m, bitmap);

    pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);

	return 0;
}

static int lib3d_get_start_pos(lua_State* L) {
    Point3d out;
    get_start_pos(&out);
    for(int i=0;i<3;++i) {
        pd->lua->pushFloat(out.v[i]);
    }
    return 3;
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
	float angle;
	get_face(pos, &n, &y, &angle);

	pd->lua->pushFloat(y);
	
	pd->lua->pushFloat(n.x);
	pd->lua->pushFloat(n.y);
	pd->lua->pushFloat(n.z);

	pd->lua->pushFloat(angle);

	// arg count
	return 5;
}

static int lib3d_get_track_info(lua_State* L) {
	Point3d pos;
	for (int i = 0; i < 3; ++i) {
		pos.v[i] = pd->lua->getArgFloat(i + 1);
	}
	
	float xmin, xmax,z;
	int is_checkpoint;
	get_track_info(pos, &xmin, &xmax, &z, &is_checkpoint);
	
	pd->lua->pushFloat(xmin);
	pd->lua->pushFloat(xmax);
	pd->lua->pushFloat(z);
	pd->lua->pushBool(is_checkpoint);

	return 4;
}

static int lib3d_get_props(lua_State* L) {
	Point3d pos;
	for (int i = 0; i < 3; ++i) {
		pos.v[i] = pd->lua->getArgFloat(i + 1);
	}
	int n = 0;
	PropInfo* props;
	get_props(pos, &props, &n);
	for (int i = 0; i < n; ++i) {
		PropInfo* prop = &props[i];
		pd->lua->pushInt(prop->type);
		pd->lua->pushFloat(prop->pos.x);
		pd->lua->pushFloat(prop->pos.y);
		pd->lua->pushFloat(prop->pos.z);
	}
	return n * 4;
}

static int lib3d_clear_checkpoint(lua_State* L) {
	Point3d pos;
	for (int i = 0; i < 3; ++i) {
		pos.v[i] = pd->lua->getArgFloat(i + 1);
	}

	clear_checkpoint(pos);

	return 0;
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

static int lib3d_update_snowball(lua_State* L) {
	Point3d pos;
	for (int i = 0; i < 3; ++i) {
		pos.v[i] = pd->lua->getArgFloat(i + 1);
	}
	float angle = pd->lua->getArgFloat(4);
	update_snowball(pos,(int)angle);

	return 0;
}

static int lib3d_collide(lua_State* L) {
	Point3d pos;
	for (int i = 0; i < 3; ++i) {
		pos.v[i] = pd->lua->getArgFloat(i + 1);
	}
	float radius = pd->lua->getArgFloat(4);

	int out = 0;
	collide(pos, radius, &out);

	pd->lua->pushInt(out);
	return 1;
}

// async load asset function
static int lib3d_load_assets_async(lua_State* L) {
	const int res = ground_load_assets_async();
	if (res) {
		pd->lua->pushBool(1);
		return 1;
	}

	return 0;
}

void lib3d_register(PlaydateAPI* playdate)
{
	pd = playdate;
	lib3d_setRealloc(pd->system->realloc);

	const char* err;

	// init modules
	gfx_init(playdate);
	ground_init(playdate);
	tracks_init(playdate);

	if (!pd->lua->addFunction(lib3d_make_ground, "lib3d.make_ground", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_render_ground, "lib3d.render_ground", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_get_start_pos, "lib3d.get_start_pos", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_get_face, "lib3d.get_face", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_clear_checkpoint, "lib3d.clear_checkpoint", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_get_track_info, "lib3d.get_track_info", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_get_props, "lib3d.get_props", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_update_snowball, "lib3d.update_snowball", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_collide, "lib3d.collide", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_load_assets_async, "lib3d.load_assets_async", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->addFunction(lib3d_update_ground, "lib3d.update_ground", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->registerClass("lib3d.GroundParams", lib3D_GroundParams, NULL, 0, &err))
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);	
}

void lib3d_unregister(PlaydateAPI* playdate) {
}