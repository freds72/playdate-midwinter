#include "lua3dmath.h"
#include "userdata.h"
#include "3dmath.h"

static PlaydateAPI* pd = NULL;

static float* getArgVec3(int n)	{ return getArgObject(n, "lib3d.Vec3"); }
static float* getArgMat4(int n) { return getArgObject(n, "lib3d.Mat4"); }


static const float v_up[VEC3] = { 0.f, 1.0f , 0.f };

/// allocation
static int vec3_new(lua_State* L)
{
	float *p = pop_vec3();
	p[0] = p[1] = p[2] = 0.f;

	int argc = pd->lua->getArgCount();
	for(int i=0;i<argc&&i<VEC3;i++) {
		p[i] = pd->lua->getArgFloat(i+1);
	}

	pd->lua->pushObject(p, "lib3d.Vec3", 0);
	return 1;
}

int vec3_gc(lua_State* L)
{
	float* p = getArgVec3(1);
	push_vec3(p);
	return 0;
}

static int vec3_index(lua_State* L)
{
	float* p = getArgVec3(1);
	const int i = pd->lua->getArgInt(2);
	if (i<1 || i>VEC3) {
		pd->lua->pushNil();	
	} else {
		// keep 1-base
		pd->lua->pushFloat(p[i-1]);
	}

	return 1;
}

static int vec3_clone(lua_State* L) {
	float* self = getArgVec3(1);
	float* p = pop_vec3();
	p[0] = self[0];
	p[1] = self[1];
	p[2] = self[2];

	pd->lua->pushObject(p, "lib3d.Vec3", 0);
	return 1;
}

static int vec3_dot(lua_State* L) {
	int argc = 1;
	float* a = getArgVec3(argc++);
	float* b = getArgVec3(argc++);

	pd->lua->pushFloat(v_dot(a,b));
	return 1;
}

static int vec3_scale(lua_State* L) {
	int argc = 1;
	float* self = getArgVec3(argc++);
	const float scale = pd->lua->getArgFloat(argc);

	for (int i = 0; i < VEC3; i++) {
		self[i] *= scale;
	}

	return 0;
}

static int vec3_len(lua_State* L) {
	int argc = 1;
	float* v = getArgVec3(argc++);

	pd->lua->pushFloat(sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
	return 1;
}

static int vec3_dist(lua_State* L) {
	int argc = 1;
	float* a = getArgVec3(argc++);
	float* b = getArgVec3(argc++);
	float v[VEC3];
	for (int i = 0; i < VEC3; i++) {
		v[i] = b[i] - a[i];
	}
	
	pd->lua->pushFloat(sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
	return 1;
}

static int vec3_normz(lua_State* L) {
	int argc = 1;
	float* self = getArgVec3(argc++);
	const float l = v_normz(self);
	if (l != 0.f) {
		pd->lua->pushFloat(1.0f / l);
	}
	else {
		pd->lua->pushFloat(0.f);
	}

	return 1;
}

static int vec3_lerp(lua_State* L) {
	int argc = 1;
	float* a = getArgVec3(argc++);
	float* b = getArgVec3(argc++);
	const float scale = pd->lua->getArgFloat(argc++);

	float* p = pop_vec3();

	v_lerp(a, b, scale, p);

	pd->lua->pushObject(p, "lib3d.Vec3", 0);
	return 1;
}

static int vec3_add(lua_State* L) {
	int argc = 1;
	float *self = getArgVec3(argc++);
	float *inc = getArgVec3(argc++);

	if (pd->lua->getArgCount() == 2) {
		// a += b
		for (int i = 0; i < VEC3; i++) {
			self[i] += inc[i];
		}
	}
	else {
		// a += scale * b
		const float scale = pd->lua->getArgFloat(argc++);
		for (int i = 0; i < VEC3; i++) {
			self[i] += scale * inc[i];
		}
	}

	// to chain operations
	pd->lua->pushObject(self, "lib3d.Vec3", 0);
	return 1;
}

// matrix functions

/// allocation
static int mat4_new(lua_State* L)
{
	float* p = pop_mat4();
	memset(p, 0, sizeof(float) * MAT4x4);
	// set diagonals
	p[0] = p[5] = p[10] = p[15] = 1.f;

	int argc = pd->lua->getArgCount();
	for (int i = 0; i < argc && i < MAT4x4; i++) {
		p[i] = pd->lua->getArgFloat(i + 1);
	}

	pd->lua->pushObject(p, "lib3d.Mat4", 0);
	return 1;
}

int mat4_gc(lua_State* L)
{
	float* p = getArgMat4(1);
	push_mat4(p);
	return 0;
}

static int mat4_index(lua_State* L)
{
	float* p = getArgMat4(1);
	const int i = pd->lua->getArgInt(2);
	if (i < 1 || i > MAT4x4) {
		pd->lua->pushNil();
	}
	else {
		// keep 1-base
		pd->lua->pushFloat(p[i-1]);
	}

	return 1;
}

static int mat4_m_x_v(lua_State* L) {
	int argc = 1;
	float* m = getArgVec3(argc++);
	float* v = getArgMat4(argc++);
	float* p = pop_vec3();

	m_x_v(m, v, p);

	pd->lua->pushObject(p, "lib3d.Vec3", 0);
	return 1;
}

static int mat4_m_x_m(lua_State* L) {
	int argc = 1;
	float* a = getArgMat4(argc++);
	float* b = getArgMat4(argc++);
	float* p = pop_mat4();

	m_x_m(a, b, p);

	pd->lua->pushObject(p, "lib3d.Mat4", 0);
	return 1;
}

static int mat4_m_x_rot(lua_State* L) {
	const float angle = detauify(pd->lua->getArgFloat(1));
	const float c = cosf(angle), s = sinf(angle);

	float* p = pop_mat4();
	memcpy(p, (float[]){
		1.f,0.f,0.f,0.f,
		0.f,c  ,-s ,0.f,
		0.f,s  ,c  ,0.f,
		0.f,0.f,0.f,1.f
	},MAT4x4 * sizeof(float));

	pd->lua->pushObject(p, "lib3d.Mat4", 0);
	return 1;
}

static int mat4_m_y_rot(lua_State* L) {
	const float angle = detauify(pd->lua->getArgFloat(1));
	const float c = cosf(angle), s = sinf(angle);

	float* p = pop_mat4();
	memcpy(p, (float[]){
		c   , 0.f, -s , 0.f,
		0.f , 1.f, 0.f, 0.f,
		s   , 0.f, c  , 0.f,
		0.f , 0.f, 0.f, 1.f
	}, MAT4x4 * sizeof(float));

	pd->lua->pushObject(p, "lib3d.Mat4", 0);
	return 1;
}

static int mat4_from_v_angle(lua_State* L) {
	int argc = 1;
	float* up = getArgVec3(argc++);
	const float angle = detauify(pd->lua->getArgFloat(argc++));
	const float c = cosf(angle), s = sinf(angle);

	float fwd[VEC3] = { sinf(angle), 0.f, cosf(angle) };
	float right[VEC3];
	v_cross(up, fwd, right);
	v_normz(right);
	v_cross(right, up, fwd);

	float* p = pop_mat4();
	memcpy(p, (float[]){
		right[0], right[1], right[2], 0,
		up[0],	  up[1],    up[2], 0,
		fwd[0],   fwd[1],   fwd[2], 0,
		0.f, 0.f, 0.f, 1.f
	}, MAT4x4 * sizeof(float));

	pd->lua->pushObject(p, "lib3d.Mat4", 0);
	return 1;
}

static int mat4_from_v(lua_State* L) {
	int argc = 1;
	float* up = getArgVec3(argc++);

	float fwd[VEC3] = { 0.f, 0.f, 1.f };
	float right[VEC3];
	v_cross(up, fwd, right);
	v_normz(right);
	v_cross(right, up, fwd);

	float* p = pop_mat4();
	memcpy(p, (float[]){
		right[0], right[1], right[2], 0,
			up[0], up[1], up[2], 0,
			fwd[0], fwd[1], fwd[2], 0,
			0.f, 0.f, 0.f, 1.f
	}, MAT4x4 * sizeof(float));

	pd->lua->pushObject(p, "lib3d.Mat4", 0);
	return 1;
}

static int mat4_lookat(lua_State* L) {
	int argc = 1;
	float* from = getArgVec3(argc++);
	float* to = getArgVec3(argc++);

	float fwd[VEC3] = { 
		to[0] - from[0],
		to[1] - from[1],
		to[2] - from[2]};
	v_normz(fwd);
	float right[VEC3];
	v_cross(v_up, fwd, right);
	v_normz(right);
	float up[VEC3];
	v_cross(fwd, right, up);

	float* p = pop_mat4();
	memcpy(p, (float[]){
		right[0], right[1], right[2], 0,
			up[0], up[1], up[2], 0,
			fwd[0], fwd[1], fwd[2], 0,
			0.f, 0.f, 0.f, 1.f
	}, MAT4x4 * sizeof(float));

	pd->lua->pushObject(p, "lib3d.Mat4", 0);
	return 1;
}

static int mat4_inv(lua_State* L) {
	float* m = getArgVec3(1);
	float tmp;

	tmp = m[4];
	m[4] = m[1];
	m[1] = tmp;

	tmp = m[8];
	m[8] = m[2];
	m[2] = tmp;

	tmp = m[9];
	m[9] = m[6];
	m[6] = tmp;

	return 0;
}


static int mat4_right(lua_State* L) {
	int argc = 1;
	float* m = getArgMat4(argc++);

	float* p = pop_vec3();
	p[0] = m[0];
	p[1] = m[1];
	p[2] = m[2];

	pd->lua->pushObject(p, "lib3d.Vec3", 0);
	return 1;
}

static int mat4_up(lua_State* L) {
	int argc = 1;
	float* m = getArgMat4(argc++);

	float* p = pop_vec3();
	p[0] = m[4];
	p[1] = m[5];
	p[2] = m[6];

	pd->lua->pushObject(p, "lib3d.Vec3", 0);
	return 1;
}

static int mat4_fwd(lua_State* L) {
	int argc = 1;
	float* m = getArgMat4(argc++);

	float* p = pop_vec3();
	p[0] = m[8];
	p[1] = m[9];
	p[2] = m[10];

	pd->lua->pushObject(p, "lib3d.Vec3", 0);
	return 1;
}

static int mat4_m_x_translate(lua_State* L) {
	int argc = 1;
	float* m = getArgMat4(argc++);
	float* v = getArgVec3(argc++);

	float* p = pop_mat4();
	m_x_m(m, (float[]) {
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		-v[0], -v[1], -v[2], 1.f
	}, p);

	pd->lua->pushObject(p, "lib3d.Mat4", 0);
	return 1;
}

// LUA api
static const lua_reg _vec3_methods[] =
{
	{ "new",		vec3_new },
	{ "__gc", 		vec3_gc },
	{ "__index",	vec3_index },
	{ "add",		vec3_add },
	{ "clone",		vec3_clone },
	{ "dot",		vec3_dot },
	{ "scale",		vec3_scale },
	{ "normz",		vec3_normz },
	{ "lerp",		vec3_lerp },
	{ "length",		vec3_len },
	{ "dist",		vec3_dist },
	{ NULL,			NULL }
};

static const lua_reg _mat4_methods[] =
{
	{ "new",					mat4_new },
	{ "__gc", 					mat4_gc },
	{ "__index",				mat4_index },
	{ "m_x_v",					mat4_m_x_v },
	{ "m_x_m",					mat4_m_x_m },
	{ "make_m_x_rot",			mat4_m_x_rot },
	{ "make_m_y_rot",			mat4_m_y_rot },
	{ "make_m_from_v_angle",	mat4_from_v_angle },
	{ "make_m_from_v",			mat4_from_v },
	{ "make_m_lookat",			mat4_lookat },
	{ "m_inv",					mat4_inv },
	{ "m_right",				mat4_right },
	{ "m_up",					mat4_up },
	{ "m_fwd",					mat4_fwd },
	{ "m_x_translate",			mat4_m_x_translate },
	{ NULL,						NULL }
};

void lua3dmath_init(PlaydateAPI* playdate) {
	const char* err;

	pd = playdate;

	userdata_init(playdate);

	// registers entry points
	if (!pd->lua->registerClass("lib3d.Vec3", _vec3_methods, NULL, 0, &err))
		pd->system->error("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);

	if (!pd->lua->registerClass("lib3d.Mat4", _mat4_methods, NULL, 0, &err))
		pd->system->error("%s:%i: registerClass failed, %s", __FILE__, __LINE__, err);
}