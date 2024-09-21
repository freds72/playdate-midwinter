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
#include "particles.h"
#include "drawables.h"
#include "lua3dmath.h"

#define REGISTER_LUA_FUNC(func) \
	do {\
		if (!pd->lua->addFunction(lib3d_##func,"lib3d." #func, &err)) \
			pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err); \
	} while(0)

static PlaydateAPI* pd = NULL;

void* getArgObject(int n, char* type)
{
	void* obj = pd->lua->getArgObject(n, type, NULL);
	
	if ( obj == NULL )
		pd->system->error("object of type %s not found at stack position %i", type, n);
	
	return obj;
}

// ************************
// Ground Params
// ************************
static GroundParams* getGroundParams(int n)			{ return getArgObject(n, "lib3d.GroundParams"); }

static int ground_params_new(lua_State* L)
{
	GroundParams* p = lib3d_malloc(sizeof(GroundParams));
    p->slope = 0;
    p->num_tracks = 0;
    p->props_rate = 0.1f;
	p->tight_mode = 0;
	unsigned milli = 0;
	p->r_seed = pd->system->getSecondsSinceEpoch(&milli);

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
	
	C_TO_LUA(p, slope, Float);
	C_TO_LUA(p, num_tracks, Int);
	C_TO_LUA(p, props_rate, Float);
	C_TO_LUA(p, twist, Float);
	C_TO_LUA(p, min_cooldown, Int);
	C_TO_LUA(p, max_cooldown, Int);
	C_TO_LUA(p, track_type, Int);
	C_TO_LUA(p, tight_mode, Int);
	C_TO_LUA(p, r_seed, Int);
	
	// fallback
	pd->lua->pushNil();	
	return 1;
}

static int ground_params_newindex(lua_State* L)
{
	int argc = 1;
	GroundParams* p = getGroundParams(argc++);
	const char* arg = pd->lua->getArgString(argc++);
	
	LUA_TO_C(p, slope, Float);
	LUA_TO_C(p, num_tracks, Int);
	LUA_TO_C(p, props_rate, Float);
	LUA_TO_C(p, twist, Float);
	LUA_TO_C(p, min_cooldown, Int);
	LUA_TO_C(p, max_cooldown, Int);
	LUA_TO_C(p, track_type, Int);
	LUA_TO_C(p, tight_mode, Int);
	LUA_TO_C(p, r_seed, Int);

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
	Point3d* cam_pos = getArgVec3(argc++);
	// cam angle
	const float tau_angle = pd->lua->getArgFloat(argc++);

	// blinking mask (warning)
	uint32_t blink = pd->lua->getArgInt(argc++);
	
	// camera matrix
	Mat4* m = getArgMat4(argc++);

    uint8_t* bitmap = pd->graphics->getFrame();

	render_ground(*cam_pos, tau_angle, *m, blink, bitmap);

	// get userdata stats
	// int vlen, vmax, mlen, mmax;
	// userdata_stats(&vlen, &vmax, &mlen, &mmax);
	// pd->system->logToConsole("vec pool: %i/%i matrix pool: %i/%i", vlen, vmax, mlen, mmax);

    pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);

	return 0;
}

static int lib3d_render_props(lua_State* L)
{
	int argc = 1;
	// cam pos
	Point3d* pos = getArgVec3(argc++);

	// camera matrix
	Mat4* m = getArgMat4(argc++);

	uint8_t* bitmap = pd->graphics->getFrame();

	render_props(*pos, *m, bitmap);

	pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);

	return 0;
}

static int lib3d_get_start_pos(lua_State* L) {
	Point3d* p = pop_vec3();
    get_start_pos(p);

	pushArgVec3(p);
    return 1;
}

static int lib3d_make_ground(lua_State* L) {
	GroundParams* p = getGroundParams(1);

	make_ground(*p);

	return 0;
}

static int lib3d_get_face(lua_State* L) {
	Point3d* pos = getArgVec3(1);

	Point3d *n = pop_vec3();
	float y;
	if (get_face(*pos, n, &y)) {

		pd->lua->pushFloat(y);

		pushArgVec3(n);

		// arg count
		return 2;
	}
	return 0;
}

static int lib3d_get_track_info(lua_State* L) {
	Point3d* pos = getArgVec3(1);
	
	float xmin, xmax,z,angle;
	int is_checkpoint;
	get_track_info(*pos, &xmin, &xmax, &z, &is_checkpoint,&angle);
	
	pd->lua->pushFloat(xmin);
	pd->lua->pushFloat(xmax);
	pd->lua->pushFloat(z);
	pd->lua->pushBool(is_checkpoint);
	pd->lua->pushFloat(angle);

	return 5;
}

static int lib3d_clear_checkpoint(lua_State* L) {
	Point3d* pos = getArgVec3(1);

	clear_checkpoint(*pos);

	return 0;
}

static int lib3d_update_ground(lua_State* L) {
	Point3d* pos = getArgVec3(1);

	int slice_id;
	char* pattern;
	Point3d* offset = pop_vec3();;
	update_ground(*pos, &slice_id, &pattern, offset);

	pd->lua->pushInt(slice_id);
	pd->lua->pushString(pattern);
	pushArgVec3(offset);

	return 3;
}

static int lib3d_add_render_prop(lua_State* L) {
	int argc = 1;
	const int id = pd->lua->getArgInt(argc++);
	Mat4* m = getArgMat4(argc++);

	add_render_prop(id, *m);

	return 0;
}

static int lib3d_collide(lua_State* L) {
	int argc = 1;

	Point3d* pos = getArgVec3(argc++);
	float radius = pd->lua->getArgFloat(argc++);

	int out = 0;
	collide(*pos, radius, &out);

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

// spawn a particle
static int lib3d_spawn_particle(lua_State* L) {
	int argc = 1;
	int id = pd->lua->getArgInt(argc++);
	Point3d pos;
	if (pd->lua->getArgCount() == 2) {
		// standard mode
		Point3d* v = getArgVec3(argc++);
		pos = *v;
	}
	else {
		// local matrix + lerp a b
		Mat4* m = getArgMat4(argc++);
		Point3d* a = getArgVec3(argc++);
		Point3d* b = getArgVec3(argc++);
		Point3d tmp;
		v_lerp(*a, *b, randf(), &tmp);
		m_x_v(*m, tmp, &pos);
	}

	spawn_particle(id, pos);

	return 0;
}

// reset particles
static int lib3d_clear_particles(lua_State* L) {
	clear_particles();
	return 0;
}

// https://www.partow.net/programming/hashfunctions/index.html#DEKHashFunction
static int lib3d_DEKHash(lua_State* L)
{
	int argc = 1;
	char* str = pd->lua->getArgString(argc++);
	int length = strlen(str);

	unsigned int hash = length;
	unsigned int i = 0;

	for (i = 0; i < length; ++str, ++i)
	{
		hash = ((hash << 5) ^ (hash >> 27)) ^ (*str);
	}
	pd->lua->pushInt(hash);
	return 1;
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
	particles_init(playdate);
	drawables_init(playdate);
	lua3dmath_init(playdate);

	REGISTER_LUA_FUNC(make_ground);
	REGISTER_LUA_FUNC(render_ground);
	REGISTER_LUA_FUNC(render_props);
	REGISTER_LUA_FUNC(get_start_pos);
	REGISTER_LUA_FUNC(get_face);
	REGISTER_LUA_FUNC(clear_checkpoint);
	REGISTER_LUA_FUNC(get_track_info);
	REGISTER_LUA_FUNC(add_render_prop);
	REGISTER_LUA_FUNC(collide);
	REGISTER_LUA_FUNC(load_assets_async);
	REGISTER_LUA_FUNC(update_ground);
	REGISTER_LUA_FUNC(spawn_particle);
	REGISTER_LUA_FUNC(clear_particles);
	REGISTER_LUA_FUNC(DEKHash);
	
	if (!pd->lua->registerClass("lib3d.GroundParams", lib3D_GroundParams, NULL, 0, &err))
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);	
}

void lib3d_unregister(PlaydateAPI* playdate) {
}