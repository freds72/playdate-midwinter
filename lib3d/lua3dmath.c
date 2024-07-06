#include "lua3dmath.h"
#include "userdata.h"
#include "3dmath.h"

static PlaydateAPI* pd = NULL;

Point3d* getArgVec3(int n)	{ return getArgObject(n, "lib3d.Vec3"); }
Mat4* getArgMat4(int n) { return getArgObject(n, "lib3d.Mat4"); }

void pushArgVec3(Point3d* p) { pd->lua->pushObject(p, "lib3d.Vec3", 0); }
void pushArgMat4(Mat4* p) { pd->lua->pushObject(p, "lib3d.Mat4", 0); }

static const Point3d v_up = { .v = { 0.f, 1.0f , 0.f } };

/// allocation
static int vec3_new(lua_State* L)
{
	Point3d *p = pop_vec3();
	*p = (Point3d){.v = {0} };

	int argc = pd->lua->getArgCount();
	for(int i=0;i<argc&&i<VEC3;i++) {
		p->v[i] = pd->lua->getArgFloat(i+1);
	}

	pushArgVec3(p);
	return 1;
}

int vec3_gc(lua_State* L)
{
	Point3d* p = getArgVec3(1);
	push_vec3(p);
	return 0;
}

static int vec3_index(lua_State* L)
{
	int argc = 1;
	Point3d* p = getArgVec3(argc++);
	const int i = pd->lua->getArgInt(argc++);
	if (i < 1 || i > VEC3) {
		pd->lua->pushNil();
	}
	else {
		// keep 1-base
		pd->lua->pushFloat(p->v[i - 1]);
	}

	return 1;
}

static int vec3_newindex(lua_State* L)
{
	int argc = 1;
	Point3d* p = getArgVec3(argc++);
	const int i = pd->lua->getArgInt(argc++);
	if (i>0 && i<=VEC3) {
		// keep 1-base
		p->v[i - 1] = pd->lua->getArgFloat(argc++);
	}

	return 0;
}

static int vec3_clone(lua_State* L) {
	Point3d* self = getArgVec3(1);
	Point3d* p = pop_vec3();
	*p = *self;

	pushArgVec3(p);
	return 1;
}

static int vec3_dot(lua_State* L) {
	int argc = 1;
	Point3d* a = getArgVec3(argc++);
	Point3d* b = getArgVec3(argc++);

	pd->lua->pushFloat(v_dot(*a,*b));
	return 1;
}

static int vec3_scale(lua_State* L) {
	int argc = 1;
	Point3d* self = getArgVec3(argc++);
	const float scale = pd->lua->getArgFloat(argc);

	for (int i = 0; i < VEC3; i++) {
		self->v[i] *= scale;
	}

	return 0;
}

static int vec3_len(lua_State* L) {
	int argc = 1;
	Point3d* v = getArgVec3(argc++);

	pd->lua->pushFloat(sqrtf(v_dot(*v,*v)));
	return 1;
}

static int vec3_dist(lua_State* L) {
	int argc = 1;
	Point3d* a = getArgVec3(argc++);
	Point3d* b = getArgVec3(argc++);
	Point3d v;
	for (int i = 0; i < VEC3; i++) {
		v.v[i] = b->v[i] - a->v[i];
	}
	
	pd->lua->pushFloat(sqrtf(v_dot(v, v)));
	return 1;
}

static int vec3_normz(lua_State* L) {
	int argc = 1;
	Point3d* self = getArgVec3(argc++);
	const float l = v_normz(self);
	if (l != 0.f) {
		pd->lua->pushFloat(1.0f / l);
	}
	else {
		pd->lua->pushFloat(0.f);
	}

	return 1;
}

static int vec3_zero(lua_State* L) {
	int argc = 1;
	Point3d* p = getArgVec3(argc++);
	*p = (Point3d){ .v = {0} };

	return 0;
}

static int vec3_lerp(lua_State* L) {
	int argc = 1;
	Point3d* a = getArgVec3(argc++);
	Point3d* b = getArgVec3(argc++);
	const float scale = pd->lua->getArgFloat(argc++);

	Point3d* p = pop_vec3();

	v_lerp(*a, *b, scale, p);

	pushArgVec3(p);
	return 1;
}

// in-place lerp
static int vec3_move(lua_State* L) {
	int argc = 1;
	Point3d* a = getArgVec3(argc++);
	Point3d* b = getArgVec3(argc++);
	const float scale = pd->lua->getArgFloat(argc++);

	Point3d p;
	v_lerp(*a, *b, scale, &p);
	*a = p;

	return 0;
}

static int vec3_add(lua_State* L) {
	int argc = 1;
	Point3d* self = getArgVec3(argc++);
	Point3d* inc = getArgVec3(argc++);

	if (pd->lua->getArgCount() == 2) {
		// a += b
		for (int i = 0; i < VEC3; i++) {
			self->v[i] += inc->v[i];
		}
	}
	else {
		// a += scale * b
		const float scale = pd->lua->getArgFloat(argc++);
		for (int i = 0; i < VEC3; i++) {
			self->v[i] += scale * inc->v[i];
		}
	}

	return 0;
}

// matrix functions

/// allocation
static int mat4_new(lua_State* L)
{
	Mat4* p = pop_mat4();
	memset(*p, 0, sizeof(float) * MAT4x4);
	// set diagonals
	(*p)[0] = (*p)[5] = (*p)[10] = (*p)[15] = 1.f;

	int argc = pd->lua->getArgCount();
	for (int i = 0; i < argc && i < MAT4x4; i++) {
		(*p)[i] = pd->lua->getArgFloat(i + 1);
	}

	pushArgMat4(p);
	return 1;
}

int mat4_gc(lua_State* L)
{
	Mat4* p = getArgMat4(1);
	push_mat4(p);
	return 0;
}

static int mat4_index(lua_State* L)
{
	Mat4* p = getArgMat4(1);
	const int i = pd->lua->getArgInt(2);
	if (i < 1 || i > MAT4x4) {
		pd->lua->pushNil();
	}
	else {
		// keep 1-base
		pd->lua->pushFloat((*p)[i - 1]);
	}

	return 1;
}

static int mat4_newindex(lua_State* L)
{
	int argc = 1;
	Mat4* p = getArgMat4(argc++);
	const int i = pd->lua->getArgInt(argc++);
	if (i > 0 && i <= MAT4x4) {
		// keep 1-base
		(*p)[i - 1] = pd->lua->getArgFloat(argc++);
	}

	return 0;
}

static int mat4_m_x_v(lua_State* L) {
	int argc = 1;
	Mat4* m = getArgMat4(argc++);
	Point3d* v = getArgVec3(argc++);
	Point3d* p = pop_vec3();

	m_x_v(*m, *v, p);

	pushArgVec3(p);
	return 1;
}

static int mat4_m_x_m(lua_State* L) {
	int argc = 1;
	Mat4* a = getArgMat4(argc++);
	Mat4* b = getArgMat4(argc++);
	Mat4* p = pop_mat4();

	m_x_m(*a, *b, *p);

	pushArgMat4(p);
	return 1;
}

static int mat4_m_x_rot(lua_State* L) {
	const float angle = detauify(pd->lua->getArgFloat(1));
	const float c = cosf(angle), s = sinf(angle);

	Mat4* p = pop_mat4();
	memcpy(*p, (Mat4){
		1.f,0.f,0.f,0.f,
		0.f,c  ,-s ,0.f,
		0.f,s  ,c  ,0.f,
		0.f,0.f,0.f,1.f
	}, sizeof(Mat4));

	pushArgMat4(p);
	return 1;
}

static int mat4_m_y_rot(lua_State* L) {
	const float angle = detauify(pd->lua->getArgFloat(1));
	const float c = cosf(angle), s = sinf(angle);

	Mat4* p = pop_mat4();
	memcpy(*p, (Mat4){
		c   , 0.f, -s , 0.f,
		0.f , 1.f, 0.f, 0.f,
		s   , 0.f, c  , 0.f,
		0.f , 0.f, 0.f, 1.f
	}, sizeof(Mat4));

	pushArgMat4(p);
	return 1;
}

static int mat4_from_v_angle(lua_State* L) {
	int argc = 1;
	Point3d* up = getArgVec3(argc++);
	const float angle = detauify(pd->lua->getArgFloat(argc++));

	Point3d fwd = { .v = { sinf(angle), 0.f, cosf(angle) } };
	Point3d right;
	v_cross(*up, fwd, &right);
	v_normz(&right);
	v_cross(right, *up, &fwd);

	Mat4* p = pop_mat4();
	memcpy(*p, (Mat4){
		right.x, right.y, right.z, 0.f,
		up->x,	  up->y,    up->z, 0.f,
		fwd.x,   fwd.y,   fwd.z, 0.f,
		0.f, 0.f, 0.f, 1.f
	}, sizeof(Mat4));

	pushArgMat4(p);
	return 1;
}

static int mat4_from_v(lua_State* L) {
	int argc = 1;
	Point3d* up = getArgVec3(argc++);

	Point3d fwd = { .v = { 0.f, 0.f, 1.f } };
	Point3d right;
	v_cross(*up, fwd, &right);
	v_normz(&right);
	v_cross(right, *up, &fwd);

	Mat4* p = pop_mat4();
	memcpy(*p, (Mat4){
		right.x, right.y, right.z, 0.f,
		up->x, up->y, up->z, 0.f,
		fwd.x, fwd.y, fwd.z, 0.f,
		0.f, 0.f, 0.f, 1.f
	}, sizeof(Mat4));

	pushArgMat4(p);
	return 1;
}

static int mat4_lookat(lua_State* L) {
	int argc = 1;
	Point3d* from = getArgVec3(argc++);
	Point3d* to = getArgVec3(argc++);

	Point3d fwd = { .v = {
		to->v[0] - from->v[0],
		to->v[1] - from->v[1],
		to->v[2] - from->v[2]} };
	v_normz(&fwd);
	Point3d right;
	v_cross(v_up, fwd, &right);
	v_normz(&right);
	Point3d up;
	v_cross(fwd, right, &up);

	Mat4* p = pop_mat4();
	memcpy(*p, (Mat4) {
		right.x, right.y, right.z, 0.f,
		up.x, up.y, up.z, 0.f,
		fwd.x, fwd.y, fwd.z, 0.f,
		0.f, 0.f, 0.f, 1.f
	}, sizeof(Mat4));

	pushArgMat4(p);
	return 1;
}

static int mat4_inv(lua_State* L) {
	Mat4* m = getArgMat4(1);
	float tmp;

	tmp =     (*m)[4];
	(*m)[4] = (*m)[1];
	(*m)[1] = tmp;

	tmp =     (*m)[8];
	(*m)[8] = (*m)[2];
	(*m)[2] = tmp;

	tmp =     (*m)[9];
	(*m)[9] = (*m)[6];
	(*m)[6] = tmp;

	return 0;
}


static int mat4_right(lua_State* L) {
	int argc = 1;
	Mat4* m = getArgMat4(argc++);

	Point3d* p = pop_vec3();
	p->v[0] = (*m)[0];
	p->v[1] = (*m)[1];
	p->v[2] = (*m)[2];

	pushArgVec3(p);
	return 1;
}

static int mat4_up(lua_State* L) {
	int argc = 1;
	Mat4* m = getArgMat4(argc++);

	Point3d* p = pop_vec3();
	p->v[0] = (*m)[4];
	p->v[1] = (*m)[5];
	p->v[2] = (*m)[6];

	pushArgVec3(p);
	return 1;
}

static int mat4_fwd(lua_State* L) {
	int argc = 1;
	Mat4* m = getArgMat4(argc++);

	Point3d* p = pop_vec3();
	p->v[0] = (*m)[8];
	p->v[1] = (*m)[9];
	p->v[2] = (*m)[10];

	pushArgVec3(p);
	return 1;
}

static int mat4_m_x_translate(lua_State* L) {
	int argc = 1;
	Mat4* m = getArgMat4(argc++);
	Point3d* v = getArgVec3(argc++);

	Mat4* p = pop_mat4();
	m_x_m(*m, (Mat4) {
		1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		-v->x, -v->y, -v->z, 1.f
	}, *p);
	memcpy(*m, *p, sizeof(Mat4));

	push_mat4(p);

	return 0;
}

// LUA api
static const lua_reg _vec3_methods[] =
{
	{ "new",		vec3_new },
	{ "__gc", 		vec3_gc },
	{ "__index",	vec3_index },
	{ "__newindex",	vec3_newindex },
	{ "add",		vec3_add },
	{ "clone",		vec3_clone },
	{ "dot",		vec3_dot },
	{ "scale",		vec3_scale },
	{ "normz",		vec3_normz },
	{ "lerp",		vec3_lerp },
	{ "length",		vec3_len },
	{ "dist",		vec3_dist },
	{ "zero",		vec3_zero },
	{ "move",		vec3_move },
	{ NULL,			NULL }
};

static const lua_reg _mat4_methods[] =
{
	{ "new",					mat4_new },
	{ "__gc", 					mat4_gc },
	{ "__index",				mat4_index },
	{ "__newindex",				mat4_newindex },
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
	{ "m_translate",			mat4_m_x_translate },
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