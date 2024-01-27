-- game globals
import 'CoreLibs/graphics'
local gfx = playdate.graphics
local font = gfx.font.new('font/whiteglove-stroked')

local sqrt = math.sqrt
local flr = math.floor
local max=math.max
local min=math.min
local sin = function(a)
  return -math.sin(2*a*math.pi)
end
local cos = function(a)
  return math.cos(2*a*math.pi)
end

local rnd = function(r) 
  return r and r*math.random() or math.random()
end
local abs = math.abs


-- vector & tools
function lerp(a,b,t)
	return a*(1-t)+b*t
end

-- pick a random item for the given array
-- @TheTomster for method name :)
function pick(a)
	return a[flr(rnd(#a))+1]
end

-- duplicate array
function clone(src)
	local dst={}
	for k,v in pairs(src) do
		dst[k]=v
	end
	return dst
end

-- vector tools
function make_v(a,b)
	return {
		b[1]-a[1],
		b[2]-a[2],
		b[3]-a[3]}
end
function v_clone(v)
	return {v[1],v[2],v[3]}
end
function v_dot(a,b)
	return a[1]*b[1]+a[2]*b[2]+a[3]*b[3]
end
function v_scale(v,scale)
	v[1]=v[1]*scale
	v[2]=v[2]*scale
	v[3]=v[3]*scale
end
function v_add(v,dv,scale)
	scale=scale or 1
	v[1]=v[1]+scale*dv[1]
	v[2]=v[2]+scale*dv[2]
	v[3]=v[3]+scale*dv[3]
end
-- safe vector length
function v_len(v)
	local x,y,z=v[1],v[2],v[3]
	return sqrt(x*x+y*y+z*z)
end
function v_normz(v)
	local d=v_len(v)
  if d==0 then return v end
	v[1]=v[1]/d
	v[2]=v[2]/d
	v[3]=v[3]/d
end

function v_lerp(a,b,t)
	return {
		lerp(a[1],b[1],t),
		lerp(a[2],b[2],t),
		lerp(a[3],b[3],t)
	}
end
function v_cross(a,b)
	local ax,ay,az=a[1],a[2],a[3]
	local bx,by,bz=b[1],b[2],b[3]
	return {ay*bz-az*by,az*bx-ax*bz,ax*by-ay*bx}
end

local v_up={0,1,0}

-- matrix functions
function m_x_v(m,v)
	local x,y,z=v[1],v[2],v[3]
	return {m[1]*x+m[5]*y+m[9]*z+m[13],m[2]*x+m[6]*y+m[10]*z+m[14],m[3]*x+m[7]*y+m[11]*z+m[15]}
end
function m_x_m(a,b)
	local a11,a12,a13,a14=a[1],a[5],a[9],a[13]
	local a21,a22,a23,a24=a[2],a[6],a[10],a[14]
	local a31,a32,a33,a34=a[3],a[7],a[11],a[15]

	local b11,b12,b13,b14=b[1],b[5],b[9],b[13]
	local b21,b22,b23,b24=b[2],b[6],b[10],b[14]
	local b31,b32,b33,b34=b[3],b[7],b[11],b[15]

	return {
			a11*b11+a12*b21+a13*b31,a21*b11+a22*b21+a23*b31,a31*b11+a32*b21+a33*b31,0,
			a11*b12+a12*b22+a13*b32,a21*b12+a22*b22+a23*b32,a31*b12+a32*b22+a33*b32,0,
			a11*b13+a12*b23+a13*b33,a21*b13+a22*b23+a23*b33,a31*b13+a32*b23+a33*b33,0,
			a11*b14+a12*b24+a13*b34+a14,a21*b14+a22*b24+a23*b34+a24,a31*b14+a32*b24+a33*b34+a34,1
		}
end
function make_m_from_v_angle(up,angle)
	local fwd={-sin(angle),0,cos(angle)}
	local right=v_cross(up,fwd)
	v_normz(right)
	fwd=v_cross(right,up)
	return {
		right[1],right[2],right[3],0,
		up[1],up[2],up[3],0,
		fwd[1],fwd[2],fwd[3],0,
		0,0,0,1
	}
end
-- only invert 3x3 part
function m_inv(m)
	m[2],m[5]=m[5],m[2]
	m[3],m[9]=m[9],m[3]
	m[7],m[10]=m[10],m[7]
end

-- returns basis vectors from matrix
function m_right(m)
	return {m[1],m[2],m[3]}
end
function m_up(m)
	return {m[5],m[6],m[7]}
end
function m_fwd(m)
	return {m[9],m[10],m[11]}
end

-- camera
function make_cam()
	--
	local up={0,1,0}

  return {
		pos={0,0,0},
		angle=0,
		m=make_m_from_v_angle(v_up,0),
		track=function(self,pos,a,u,power)
   			pos=v_clone(pos)
   			-- lerp angle
			self.angle=lerp(self.angle,a,power or 0.8)
			-- lerp orientation
			up=v_lerp(up,u,0.1)
			v_normz(up)

			-- shift cam position			
			local m=make_m_from_v_angle(up,self.angle)
			-- 1.8m player
			-- v_add(pos,v_up,64)
			v_add(pos,m_up(m),1.6)
			
			-- inverse view matrix
			m_inv(m)
			self.m=m_x_m(m,{
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
				-pos[1],-pos[2],-pos[3],1
			})
			
			self.pos=pos
		end,
		project2d=function(self,v)
			local w=199.5/v[3]
			return 199.5+w*v[1],119.5-w*v[2],w
		end
	}
end

function _init()
  _cam = make_cam()
	local params = lib3d.GroundParams.new()
	params.slope = 0.5
	lib3d.make_ground(params)
end

local _pos,_vel,_angle={16*4,8,16*4},0,0
function _update()
  local t = playdate.getCurrentTimeMilliseconds()/1000

	local dx,dz = 0,0
	if playdate.buttonIsPressed(playdate.kButtonLeft) then dx=-1 end
	if playdate.buttonIsPressed(playdate.kButtonRight) then dx=1 end
	if playdate.buttonIsPressed(playdate.kButtonUp) then dz=-1 end
	if playdate.buttonIsPressed(playdate.kButtonDown) then dz=1 end
	
	_vel *= 0.9
	_vel += dz
	_angle += dx / 128
	local cc,ss = sin(_angle),-cos(_angle)
	_pos[1] += _vel * cc
	_pos[3] += _vel * ss
	local up = {_vel * cc,8,_vel * ss}
	v_normz(up)
  _cam:track(_pos,_angle,up)
end

function _draw()
  gfx.setColor(gfx.kColorWhite)
  gfx.fillRect(0, 0, 400, 240)  

	lib3d.render_ground(table.unpack(_cam.m))
end

-- *********** playdate entry point ******************
_init()
function playdate.update()
  _update()
  _draw()
  --gfx.setColor(gfx.kColorBlack)
  --gfx.drawPolygon(table.unpack(verts))
end