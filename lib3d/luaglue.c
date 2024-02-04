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

#include "gfx.h"
#include "3dmath.h"
#include "3dmathi.h"

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

static inline uint32_t __SADD16(uint32_t op1, uint32_t op2)
{
#if TARGET_PLAYDATE
	uint32_t result;

	__asm volatile ("sadd16 %0, %1, %2" : "=r" (result) : "r" (op1), "r" (op2));
	return(result);
#else
	return op1 + op2;
#endif
}

static int lib3d_bench_gfx(lua_State* L) {
	volatile float a = 846.8f, b = 86.5f;

	volatile int32_t ia0 = 846, ib0 = 89;
	volatile int32_t ia1 = 95, ib1 = 981;
	volatile int32_t ia2 = 95, ib2 = 981;
	volatile int32_t ia3 = 95, ib3 = 981;

	const Point3d verts[] = {
		(Point3d) {
 .v = {52.f,52.f,0.f}
},
		(Point3d) {
		.v = { 76,120.f,0.f }
	},
	(Point3d) {
	.v = { 196.f,150.f,0.f }
},
(Point3d) {
.v = { 150.f,96.f,0.f }
}
	};

	const Point2di vertsi[] = {
		(Point2di) {
 .v = {52.f,52.f}
},
		(Point2di) {
		.v = { 76,120.f }
	},
	(Point2di) {
	.v = { 196.f,150.f }
},
(Point2di) {
.v = { 150.f,96.f }
}
	};
	uint32_t dither[32];
	for (int i = 0; i < 32; i++)
		dither[i] = (int) - 1;

	uint32_t* bitmap = (uint32_t*)pd->graphics->getFrame();
	uint8_t* bitmap8 = pd->graphics->getFrame();
	memset(bitmap, 0, LCD_ROWS * LCD_ROWSIZE);

	PDButtons buttons;
	PDButtons pushed;
	PDButtons released;
	pd->system->getButtonState(&buttons, &pushed, &released);

	pd->system->resetElapsedTime();
	float t0 = pd->system->getElapsedTime();

	// do something
	// volatile int32_t res = 0;
	volatile float res = 0;
	for (int i = 0; i < 100; ++i) {
		// cycles: 0.027985
		// res = a * b;
			
		// cycles: 0.033682
		// res = __SADD16(ia0, ib0);
		// res = __SADD16(ia1, ib1);

		// cycles: 0.094497
		// res = __SADD16(ia0, ib0);
		// res = __SADD16(ia1, ib1);

		// cycles: 0.202327
		// res = __SADD16(ia0, ib0);
		// res = __SADD16(ia1, ib1);
		// res = __SADD16(ia0, ib0);
		// res = __SADD16(ia1, ib1);

		// cycles: 0.089627
		//res = a + b;
		//res = a + b;
		//res = a + b;
		//res = a + b;

		// cycles: 0.048681
		// res = a + b;
		// res = a + b;

		// cycles: 0.195919
		// res = ia0 + ib0;
		// res = ia0 + ib0;
		// res = ia0 + ib0;
		// res = ia0 + ib0;

		// fixed version
		/*
		cycles: 0.005194
		cycles: 0.006007
		cycles: 0.007061
		*/

		// float version
		/*
		cycles: 0.006345
		cycles: 0.005774
		cycles: 0.007292
		*/
		if (buttons & kButtonA) {
			polyfill(&verts, 4, dither, bitmap);
		}
		else {
			trifill(&vertsi[0], &vertsi[2], &vertsi[1], bitmap8);
			trifill(&vertsi[0], &vertsi[3], &vertsi[2], bitmap8);
		}
	}

	// print the counter value
	pd->system->logToConsole("cycles: %f", (pd->system->getElapsedTime() - t0));

	pd->graphics->markUpdatedRows(0, LCD_ROWS - 1);

	return 0;
}

#define FIXED_SCALE 4
static int16_t tofixed(const float a) {
	return (int16_t)(a * (1 << FIXED_SCALE));
}

static int lib3d_bench(lua_State* L) {
	Point3di a = (Point3di){ .x = tofixed(0.85f), .y = tofixed(-0.5f), .z = tofixed(0.986f), .w = 0 };
	Point3di b = (Point3di){ .x = tofixed(-0.5f), .y = tofixed(.5f), .z = tofixed(-.1f), .w = 0 };
	pd->system->logToConsole("fixed a: 0x%02X 0x%02X 0x%02X", a.x, a.y, a.z);
	pd->system->logToConsole("fixed b: 0x%02X 0x%02X 0x%02X", b.x, b.y, b.z);

	Point3d af = (Point3d){ .x = 0.85f, .y = -0.5f, .z = 0.986f };
	Point3d bf = (Point3d){ .x = -0.5f, .y = .5f, .z = -.1f };

	pd->system->resetElapsedTime();
	float t0 = pd->system->getElapsedTime();
	
	for (int i = 0; i < 1000000; i++) {
		volatile uint32_t res = v_doti(&a, &b);		
	}
	pd->system->logToConsole("cycles: %f", (pd->system->getElapsedTime() - t0));

	pd->system->resetElapsedTime();
	t0 = pd->system->getElapsedTime();
	for (int i = 0; i < 1000000; i++) {
		volatile float resf = v_dot(&af, &bf);
	}
	
	// fixed+asm cycles: 0.069264
	// float    cycles : 0.116462
	// float: 100%
	// fixed: 59%

	// print the counter value
	pd->system->logToConsole("cycles: %f", (pd->system->getElapsedTime() - t0));

	return 0;
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

	if (!pd->lua->addFunction(lib3d_bench_gfx, "lib3d.bench", &err))
		pd->system->logToConsole("%s:%i: addFunction failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->registerClass("lib3d.GroundParams", lib3D_GroundParams, NULL, 0, &err))
		pd->system->logToConsole("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);

    lib3d_setRealloc(pd->system->realloc);

    // 
    // ground_load_assets(playdate);
}

void lib3d_unregister(PlaydateAPI* playdate) {

}