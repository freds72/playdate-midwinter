-- snow!
-- by @freds72

-- game globals
import 'CoreLibs/graphics'
import 'CoreLibs/nineslice'
import 'models.lua'
import 'store.lua'

local gfx = playdate.graphics
local font = gfx.font.new('font/whiteglove-stroked')
local smallFont = {
	[gfx.kColorBlack]=gfx.font.new('font/Memo-Black'),
	[gfx.kColorWhite]=gfx.font.new('font/Memo-White')
}
local largeFont = {
	[gfx.kColorBlack]=gfx.font.new('font/More-15-Black'),
	[gfx.kColorWhite]=gfx.font.new('font/More-15-White')
}
local outlineFont = {
	[gfx.kColorBlack]=gfx.font.new('font/More-15-Black-Outline'),
	[gfx.kColorWhite]=gfx.font.new('font/More-15-White-Outline')
}
-- 50% dither pattern
local _50pct_pattern = { 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55 }

local _inputs={
	-- crank docked
	[true]={
		action={id=playdate.kButtonA,glyph="Ⓐ"},
		back={id=playdate.kButtonB,glyph="Ⓑ"}
	},
	-- undocked
	[false]={
		action={id=playdate.kButtonB,glyph="Ⓑ"},
		back={id=playdate.kButtonA,glyph="Ⓐ"}
	}
}
local _input=_inputs[true]

-- ground limits
local _ground_width = 32*4
local _ground_height = 40*4

-- some "pico-like" helpers
function cls(c)
  gfx.clear(c or gfx.kColorBlack)
end
local sin = function(a)
  return -math.sin(2*a*math.pi)
end
local cos = function(a)
  return math.cos(2*a*math.pi)
end
local function sgn(a)
	return a>=0 and 1 or -1
end

local sqrt = math.sqrt
local flr = math.floor
local max=math.max
local min=math.min
local rnd = function(r) 
	return r and r*math.random() or math.random()
end
local abs = math.abs
local srand = function(s) 
  math.randomseed(flr(s))
end
local atan2 = function(x,y)
  return math.atan2(y,x)/math.pi
end
-- 
local camera=function(x,y)
  playdate.display.setOffset(x or 0,y or 0)
end
local function add(t,v)
  table.insert(t,v)
  return v
end
local function mid(a,b,c)
  return math.min(math.max(a,b),c)
end
local function time()
  return playdate.getCurrentTimeMilliseconds()/1000
end
local function del(t,v)
  local i = table.indexOfElement(t, v)
  if i then 
    table.remove(t, i)
  end
end

-- registers a new coroutine
-- returns a handle to the coroutine
-- used to cancel a coroutine
local _futures={}
function do_async(fn)
  return table.insert(_futures,{co=coroutine.create(fn)})
end
-- wait until timer
function wait_async(t)
	for i=1,t do
		coroutine.yield()
	end
end

-- install next state
local _update_state,_draw_state
-- note: won't be active before next frame
function next_state(state,...)
	local u,d,i=state(...)
	-- ensure update/draw pair is consistent
	_update_state=function()
			-- init function (if any)
			if i then i() end
			-- 
			_update_state,_draw_state=u,d
			-- actually run the update
			u()
	end
end


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
	v[1]*=scale
	v[2]*=scale
	v[3]*=scale
end
function v_add(v,dv,scale)
	scale=scale or 1
	v[1]+=scale*dv[1]
	v[2]+=scale*dv[2]
	v[3]+=scale*dv[3]
end

-- vector length
function v_len(v)
	local x,y,z=v[1],v[2],v[3]
	return sqrt(x*x+y*y+z*z)
end
function v_normz(v)
	local d=v_len(v)
  if d==0 then return v end
	v[1]/=d
	v[2]/=d
	v[3]/=d
	return d
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
function make_m_z_rot(angle)
	local c,s=cos(angle),-sin(angle)
	return {
		c,-s,0,0,
		s,c,0,0,
		0,0,1,0,
		0,0,0,1
	}
end

function make_m_x_rot(angle)
	local c,s=cos(angle),-sin(angle)
	return {
		1,0,0,0,
		0,c,-s,0,
		0,s,c,0,
		0,0,0,1
	}
end

function make_m_y_rot(angle)
	local c,s=cos(angle),-sin(angle)
	return {
		c,0,-s,0,
		0,1,0,0,
		s,0,c,0,
		0,0,0,1
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

function make_m_from_v(up)
	local fwd={0,0,1}
	local right={up[2],-up[1],0}
	v_normz(right)
	fwd=v_cross(right,up)
	return {
		right[1],right[2],right[3],0,
		up[1],up[2],up[3],0,
		fwd[1],fwd[2],fwd[3],0,
		0,0,0,1
	}
end

function make_m_lookat(from,to)
	local fwd=make_v(from,to)
	v_normz(fwd)
	local right=v_cross(v_up,fwd)
	v_normz(right)
	local up=v_cross(fwd,right)
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

-- main engine
-- global vars
local _actors,_ground,_plyr,_tracked={}

-- screen efects
local screen={}
-- screen shake
local shkx,shky=0,0
function screen:shake(scale)
	scale=scale or 24
	shkx,shkx=min(8,shkx+rnd(scale)),min(8,shky+rnd(scale))
end
function screen:update()
	shkx=shkx*-0.7-rnd(0.4)
	shky=shky*-0.7-rnd(0.4)
	if abs(shkx)<0.5 and abs(shky)<0.5 then
		shkx,shky=0,0
	end
end

-- camera
function make_cam(pos)
	--
	local up={0,1,0}

	camera()

	return {
		pos=pos and v_clone(pos) or {0,0,0},
		angle=0,
		m=make_m_from_v_angle(v_up,0),
		update=function()
			screen:update()
			camera(shkx,shky)
		end,

		look=function(self,to)
			local pos=self.pos
			local m=make_m_lookat(pos,to)
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
		track=function(self,pos,a,u,power,snap)
   		pos=v_clone(pos)
   		-- lerp angle
			self.angle=lerp(self.angle,a,power or 0.8)
			-- lerp orientation (or snap to angle)
			up=v_lerp(up,u,snap or 0.1)
			v_normz(up)

			-- shift cam position			
			local m=make_m_from_v_angle(up,self.angle)
			-- 1.8m player
			-- v_add(pos,v_up,64)
			v_add(pos,m_up(m),1.2)
			
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

-- basic gravity+slope physic body
function make_body(p,angle)
	-- last contact face
	local up,oldf={0,1,0}

	local velocity,angularv,forces,torque={0,0,0},0,{0,0,0},0
	local boost,perm_boost=0,0
	local steering_angle=0
	angle=angle or 0

	local g={0,-4,0}
	return {
		pos=v_clone(p),
		on_ground=nil,
		height=0,
		drag=0,
		get_pos=function(self)
	 		return self.pos,angle,steering_angle/0.625,velocity,boost
		end,
		get_up=function()
			-- compensate slope when not facing slope
			local scale=abs(cos(angle))
			local u=v_lerp(v_up,up,scale)
			local m=make_m_from_v_angle(u,angle)
			
			local right=m_right(m)
			v_add(u,right,sin(steering_angle)*scale/2)
			v_normz(u)
			return u,m
		end,
		apply_force_and_torque=function(self,f,t)
			-- add(debug_vectors,{f=f,p=p,c=11,scale=t})

			v_add(forces,f)
			torque+=t
		end,
		boost=function(self,b)
			boost=b
		end,
		perma_boost=function(self,b)
			perm_boost = b
		end,
		integrate=function(self)
			-- gravity and ground
			self:apply_force_and_torque(g,0)
			-- on ground?
			if self.on_ground then
				local n=v_clone(up)
				v_scale(n,-v_dot(n,g))
				-- slope pushing up
				self:apply_force_and_torque(n,0)
			end

			-- update velocities
			v_add(velocity,forces,0.5/30)
			angularv+=torque*0.5/30

			-- apply some damping
			angularv*=0.86
			self.drag*=0.9
			-- kill boost while on ground
			if self.on_ground then boost*=0.9 end
			-- some friction
			local f=self.on_ground and 0.08 or 0.01
			v_add(velocity,velocity,-(f+self.drag)*v_dot(velocity,velocity))
			
			-- update pos & orientation
			v_add(self.pos,velocity,1 + boost + perm_boost)

			-- limit rotating velocity
			angularv=mid(angularv,-1,1)
			angle+=angularv

			-- reset
			forces,torque={0,0,0},0
		end,
		steer=function(self,steering_dt)
			-- stiff direction when boosting!
			if boost>0.1 then steering_dt/=2 end
			steering_angle+=mid(steering_dt,-0.15,0.15)
			-- on ground?
			if self.on_ground and v_len(velocity)>0.001 then

				-- desired ski direction
				local m=make_m_from_v_angle(up,angle-steering_angle/16)
				local right,fwd=m_right(m),m_fwd(m)
				
				-- slip angle
				local sa=-v_dot(velocity,right)
				if abs(sa)>0.001 then
					-- max grip
					local vn=v_clone(velocity)
					v_normz(vn)
					local grip=1-abs(v_dot(fwd,vn))
					-- more turn: more grip
					sa*=60*grip

					--grip*=grip

					-- todo: review
					sa=mid(sa,-3,3)
				
					-- ski length for torque
					local ski_len=0.8
					--[[
					local fwd=m_fwd(m)
					v_scale(fwd,ski_len)
					local torque=v_cross(fwd,right)
					local l=torque[1]+torque[2]+torque[3]
					]]

					v_scale(right,sa)

					self:apply_force_and_torque(right,-steering_angle*ski_len/4)
				end
			elseif not self.on_ground then
				self:apply_force_and_torque({0,0,0},-steering_angle/4)
			end			
		end,
		update=function(self)
			if self.on_ground then
				steering_angle*=0.8				
			else
				steering_angle*=0.85
			end
			
			-- find ground
			local pos=self.pos

			local newy,newn=_ground:find_face(pos)
			-- stop at ground
			self.on_ground=nil
			self.on_cliff=newn[2]<0.5
			local tgt_height=1
			if pos[2]<=newy then
				up=newn
				tgt_height=pos[2]-newy
				pos[2]=newy			
				self.on_ground=true
			end

			self.height=lerp(self.height,tgt_height,0.4)

			-- todo: alter sound
			-- mix: volume for base pitch
			-- alter with "height" to include slope "details"			
		end
	}	
end

function make_plyr(p,on_trick)
	local body=make_body(p)

	local body_update=body.update	
	local hit_ttl,jump_ttl=8,0
	
	local spin_angle,spin_prev=0

	-- timers + avoid free airtime on drop!
	local reverse_t,air_t=0,0

	body.distance = 0
	body.on_coin=function() end
	body.control=function(self)	
		local da=0
		if playdate.isCrankDocked() then
			if playdate.buttonIsPressed(playdate.kButtonLeft) then da=1 end
			if playdate.buttonIsPressed(playdate.kButtonRight) then da=-1 end
		else
			local change, acceleratedChange = playdate.getCrankChange()
			local flip = _save_state.flip_crank and -1 or 1
			da = flip * acceleratedChange
		end

		if self.on_ground then
			-- was flying?			
			if air_t>30 then
				-- pro trick? :)
				if reverse_t>0 then
					on_trick(2,"reverse air!")
				else
					on_trick(1,"air!")
				end
			end
			air_t=0

			if jump_ttl>8 and playdate.buttonJustPressed(_input.action.id) then
				self:apply_force_and_torque({0,63,0},0)
				jump_ttl=0
			end
		else
			-- avoid auto-jump on land
			jump_ttl=4
			-- record flying time
			air_t+=1
		end
		jump_ttl=min(jump_ttl+1,9)

		self:steer(da/8)
	end

	body.update=function(self)
		hit_ttl-=1

		-- collision detection
		local pos,angle,_,velocity=self:get_pos()
		if not spin_prev then
			spin_angle,spin_prev=0,angle
		else
			local da=spin_prev-angle
			-- shortest angle
			if abs(da)>0.5 then da=da+0.5 end
			-- doing nothing or breaking the spin?
			if abs(spin_angle)>=abs(spin_angle+da) then
				spin_prev=nil
			else
				spin_angle+=da
				spin_prev=angle
			end
		end
		
		if abs(spin_angle)>1 then
			on_trick(2,"360!")
			spin_prev=nil
		end

		local hit_type=_ground:collide(pos,0.2)
		if hit_type==2 then
			-- walls: insta-death
			screen:shake()
			self.dead=true
		elseif hit_type==3 then
			-- notify controller
			self.on_coin(1)
		elseif hit_type==4 then
			-- accel pad
			if self.on_ground then
				_boost_sfx:play(1)
				self:boost(1.5)
			end
		elseif hit_ttl<0 and hit_type==1 then
			-- props: 
			_treehit_sfx:play()
			screen:shake()
			-- temporary invincibility
			hit_ttl=15
			self.hp-=1
			-- kill tricks
			reverse_t,spin_prev=0
		end

		local slice=_ground:get_track(pos)
		self.gps=slice.angle+angle
		self.on_track=pos[1]>=slice.xmin and pos[1]<=slice.xmax
		
		-- need to have some speed
		if v_dot(velocity,{-sin(angle),0,cos(angle)})<-0.2 then
			reverse_t+=1
		else
			reverse_t=0
		end

		if reverse_t>30 then
			on_trick(3,"reverse!")
			reverse_t=0
		end			

		if self.hp<=0 then
			self.dead=true
		end

		-- call parent
		body_update(self)
		
		-- prevent "hill" climbing!
		if self.on_cliff then
			_plyr.dead=true
		end

		-- total distance
		-- 2: crude 1 game unit = 2 meters
		body.distance+=max(0,2*velocity[3])
	end

	-- wrapper
	return body
end

function make_jinx(id,pos,velocity,effect)
	local base_angle=rnd()
	return {
		id=id,
		pos=pos,
		m=make_m_y_rot(time()),
		-- blinking=true,
		update=function(self)
			-- gravity
			velocity[2]-=0.25
			v_add(pos,velocity)
			-- find ground
			local newy=_ground:find_face(pos)
			-- out of bound: kill actor
			if not newy then return end

			if pos[2]<newy then
				pos[2] = newy
				-- damping
				velocity[1]*=0.97
				velocity[3]*=0.97
				velocity[2] = 0
			end

			-- distance to player
			if _plyr then
				local dist=v_len(make_v(pos,_plyr.pos))
				if dist<3 then
					-- effect()
					return
				end
			end

			if rnd()>0.1 then
				lib3d.spawn_particle(1, table.unpack(pos))
			end
	
			local m=self.m
			m[13]=pos[1]
			m[14]=pos[2]
			m[15]=pos[3]			
			self.m = m
			return true
		end
	}
end

function make_npc(p,cam)
	local body=make_body(p,0)
	local up={0,1,0}
	local dir,boost=0,0
	local boost_ttl=0
	local jinx_ttl=90
	local body_update=body.update
	body.id = models.PROP_SKIER
	local sfx=_ski_sfx:copy()
	sfx:play(0)

	-- distance to player
	body.dist = 0
	body.update=function(self)
		local pos=self.pos

		-- crude ai control!
		local _,angle=self:get_pos()
		local slice=_ground:get_track(pos)
		if slice.z>29*4 or slice.z<4 then
			sfx:stop()
			-- kill npc
			self.dead=true
			return
		end

		if _plyr then
			-- distance to player?			
			local dist=2*(pos[3]-_plyr.pos[3])
			self.warning=nil
			-- behind player?
			if dist<2 then
				self.warning = pos[1]//4
			end
			boost_ttl-=1
			if dist<-4 and boost_ttl<0 then
				self:boost(1.8)
				boost_ttl = 8
			end

			jinx_ttl-=1
			if self.on_ground and dist>24 and jinx_ttl<0 then
				do_async(function()
					add(_actors,make_jinx(models.PROP_DYNAMITE, v_clone(pos), {0,2,-0.1}, function()
						print("boom!")
						if _plyr then _plyr.dead=true end
					end))
				end)
				jinx_ttl = 90 + rnd(90)
			end
			self.dist = dist
		end

		local da=slice.angle+angle
		-- transition to "up" when going left/right
		dir=lerp(dir,da+angle,0.6)
		if dir<-0.02 then
			self.id = models.PROP_SKIER_RIGHT
		elseif dir>0.02 then
			self.id = models.PROP_SKIER_LEFT
		else
			self.id = models.PROP_SKIER
		end
		self:steer(da/2)

		self:integrate()

		-- call parent	
		body_update(self)

		-- create orientation matrix
		local newy,newn=_ground:find_face(pos)
		up=v_lerp(up,newn,0.3)
		local _,angle=self:get_pos()
		local m=make_m_from_v_angle(up,angle)
		m[13]=pos[1]
		m[14]=pos[2]
		m[15]=pos[3]

		self.m = m

		-- spawn particles
		if self.on_ground and abs(dir)>0.04 and rnd()>0.1 then
			local v=m_x_v(m,v_lerp(vgroups.SKIER_LEFT_SKI,vgroups.SKIER_RIGHT_SKI,rnd()))
			
			lib3d.spawn_particle(0, table.unpack(v))
		end

		-- update sfx
		local volume=self.on_ground and 1 or 0
		-- distance to camera
		local dist=v_len(make_v(self.pos,cam.pos))
		if dist<4 then dist=4 end
		sfx:setVolume(volume*4/dist)
		sfx:setRate(1-abs(da/2))

		return true
	end

	-- wrapper
	return body
end

function make_snowball(pos)
	local body=make_body(pos)
	body.id = models.PROP_SNOWBALL_PLAYER
	local body_update=body.update
	local base_angle=rnd()
	local angle=rnd()
	local n={cos(angle),0,sin(angle)}
	body.pre_update=function(self)		

		-- physic update
		self:integrate()
		body_update(self)		
	end
	body.hit=function()
		angle=rnd()
		n={cos(angle),0,sin(angle)}
	end
	body.update=function(self)
		local pos=self.pos
		-- update
		local newy,newn=_ground:find_face(pos)
		-- out of bound: kill actor
		if not newy then return end

		-- shadow plane projection matrix
		local m = make_m_from_v(newn)
		m[13]=pos[1]
		-- avoid z-fighting
		m[14]=newy+0.1
		m[15]=pos[3]
		self.m_shadow = m

		-- roll!!!
		local m = make_m_x_rot(base_angle-3*time())
		m=m_x_m(m,make_m_from_v(n))
		m[13]=pos[1]
		-- offset ball radius
		m[14]=pos[2]+1.5
		m[15]=pos[3]
		self.m = m

		return true
	end

	return body
end

-- game states
function loading_state()
	local step = 0
	local cabin=gfx.image.new("images/cabin")
	local cabin2=gfx.image.new("images/cabin2")
	local t=-1
	
	do_async(function()
		local t0 = playdate.getCurrentTimeMilliseconds()
		while lib3d.load_assets_async() do
			step += 1
			local t1 = playdate.getCurrentTimeMilliseconds()
			if t1-t0>250 then
				t0 = t1
				coroutine.yield()
			end
		end
		next_state(menu_state)
		--next_state(bench_state)
	end)

	return 
		-- update
		function()
			t+=1/30
			if t>3 then
				t = -1
			end
		end,
		-- draw
		function()
			cls()
			gfx.setColor(gfx.kColorWhite)
			gfx.setLineWidth(2)
			gfx.drawLine(16,0, 399, 128)
			gfx.drawLine(188,0, 399, 82)
			gfx.setLineWidth(1)
			cabin:draw(
				lerp(16,400,t),
				lerp(0,128,t))
			cabin2:draw(
				lerp(400,188,t),
				lerp(82,0,t))
			print_regular("Altitude: "..(step*10).."m",4,220)
		end
end

function station_state(state,...)
	local step = 0
	local cabin=gfx.image.new("images/cabin")
	local cabin2=gfx.image.new("images/cabin2")
	local t=-1
	local args={...}
	do_async(function()
		for i=1,90 do
			coroutine.yield()
		end
		next_state(state,table.unpack(args))
	end)

	return 
		-- update
		function()
			t+=1/30
			if t>3 then
				t = -1
			end
		end,
		-- draw
		function()
			cls()
			gfx.setColor(gfx.kColorWhite)
			gfx.setLineWidth(2)
			gfx.drawLine(16,0, 399, 128)
			gfx.drawLine(188,0, 399, 82)
			gfx.setLineWidth(1)
			cabin:draw(
				lerp(16,400,t),
				lerp(0,128,t))
			cabin2:draw(
				lerp(400,188,t),
				lerp(82,0,t))
		end
end

function menu_state(angle)
  local starting
	local best_y = -20
	local frame_t=0
	local panels={
		{state=play_state,loc=vgroups.MOUNTAIN_GREEN_TRACK,help="Chill mood?\nEnjoy the snow!",params={hp=3,name="Marmottes",slope=1.5,twist=2.5,num_tracks=3,tight_mode=0,props_rate=0.90,track_type=0,min_cooldown=30*2,max_cooldown=30*4}},
		{state=play_state,loc=vgroups.MOUNTAIN_RED_TRACK,help=function()
			return "Death Canyon\nHow far can you go?\nBest: ".._save_state.best_2.."m"
		end
		,params={hp=1,name="Biquettes",dslot=2,slope=2,twist=4,num_tracks=1,tight_mode=1,props_rate=1,track_type=1,min_cooldown=4,max_cooldown=8}},
		{state=race_state,loc=vgroups.MOUNTAIN_BLACK_TRACK,help=function()
			return "Endless Race\nTake over mania!\nBest: ".._save_state.best_3.."m"
		end,params={hp=1,name="Chamois",dslot=3,slope=2.25,twist=6,num_tracks=1,tight_mode=0,props_rate=0.97,track_type=2,min_cooldown=4,max_cooldown=12}},
		{state=shop_state,loc=vgroups.MOUNTAIN_SHOP,help=function()
			return "Buy gear!\n$".._save_state.coins
		end ,params={name="Shop"},transition=false}
	}
	local sel,blink=0,false
	-- background actors
	local actors={}

	-- menu cam	
	local look_at = {0,0,1}
	local cam=make_cam({0,0.8,-0.5})

	-- clean up
	local music = playdate.sound.fileplayer.new("sounds/alps_polka")
	music:play(0)

	_ski_sfx:stop()
	lib3d.clear_particles()

	-- menu to get back to selection menu
	local menu = playdate.getSystemMenu()
	menu:removeAllMenuItems()
	local menuItem, error = menu:addMenuItem("start menu", function()
			_futures={}
			next_state(menu_state)
	end)	
	local menuItem, error = menu:addCheckmarkMenuItem("flip crank", _save_state.flip_crank, function(value)
		_save_state.flip_crank = value
	end)	

	local scale = 1
	local angle = angle or 0
	add(actors,{
		id=models.PROP_MOUNTAIN,
		pos={0.5,0,1},
		m={
			1,0,0,0,
			0,1,0,0,
			0,0,1,0,
			0.5,0,1,1},
		update=function(self)
			local m
			if starting then
				local panel = panels[sel+1]
				-- project location into world space
				local world_loc = m_x_v(self.m,panel.loc)
				local dir=make_v(cam.pos,world_loc)
				dir[2] = 0
				v_normz(dir)
				local right=m_right(self.m)
				right[2] = 0
				v_normz(right)
				local d=v_dot(right,dir)
				angle += d/16
				m=make_m_y_rot(angle)
				m[1]  = scale
				m[6]  = scale
				m[11] = scale
			else
				-- if crank out, use crank rotation!
				if playdate.isCrankDocked() then
					angle += 0.001
				else
					local change, acceleratedChange = playdate.getCrankChange()
					local flip = _save_state.flip_crank and -1 or 1
					angle += flip * acceleratedChange/360
				end
				
				m = make_m_y_rot(angle)
			end

			local pos=self.pos
			m[13]=pos[1]
			m[14]=pos[2]
			m[15]=pos[3]

			self.m=m
		end
	})
	
	-- snowflake
	local snowflake=gfx.image.new(2,2)
	snowflake:clear(gfx.kColorWhite)

	-- starting position (outside screen)
	gfx.setFont(largeFont[gfx.kColorBlack])
	for _,p in pairs(panels) do
		p.x = -gfx.getTextSize(p.params.name) - 8
		p.x_start = p.x
	end

	return
		-- update
		function()
			if playdate.buttonJustReleased(playdate.kButtonUp) then sel-=1 _button_click:play(1) end
			if playdate.buttonJustReleased(playdate.kButtonDown) then sel+=1 _button_click:play(1) end
			sel=mid(sel,0,#panels-1)
			
			if not starting and playdate.buttonJustReleased(playdate.kButtonA) then
				-- sub-state
        starting=true
				do_async(function()
					-- fade out music
					music:setVolume(0,0,1)
					local panel = panels[sel+1]
					-- project location into world space
					local world_loc = m_x_v(actors[1].m,panel.loc)		
					for i=1,30 do
						scale = lerp(scale,1.8,0.1)
						cam.pos=v_lerp(cam.pos, {0.5,0.5,-0.75},0.1)
						look_at=v_lerp(look_at,world_loc,0.1)
						coroutine.yield()
					end
					music:stop()
					-- restore random seed
					srand(playdate.getSecondsSinceEpoch())
					local p=panels[sel+1]
					if p.transition==false then
						next_state(p.state,v_clone(cam.pos),v_clone(look_at),scale,angle)
					else
						next_state(p.transition or zoomin_state,p.state,p.params)
					end
				end)
			end

			for _,a in pairs(actors) do
				a:update()
			end

			-- slide menu in
			local sel_y = (sel//1)*28 + 80
			local sel_panel = panels[sel+1]

			for _,p in pairs(panels) do
				-- target
				local x = p == sel_panel and 14 or 10
				if starting then
					x = p.x_start
				end
				p.x = lerp(p.x,x,0.25)
			end
		
			--
			cam:look(look_at)
			frame_t+=1
		end,
		-- draw
		function()
			cls(gfx.kColorWhite)
			
			for _,a in pairs(actors) do
				lib3d.add_render_prop(a.id,table.unpack(a.m))
			end
			lib3d.render_props(cam.pos[1],cam.pos[2],cam.pos[3],table.unpack(cam.m))

			if not starting then

				_game_title_anim:drawImage((frame_t%#_game_title_anim)+1,10,6)
				print_small("by FReDS72",10,60)
			end

			local sel_panel = panels[sel+1]
			local v = m_x_v(actors[1].m, sel_panel.loc)
			v = m_x_v(cam.m, v)
			local x,y = cam:project2d(v)
			-- help popup
			if not starting then
				-- get size
				local help = sel_panel.help
				if type(help)=="function" then
					help=help()
				end
				local help_w,help_h = gfx.getTextSize(help)
				local help_y = 240 - help_h - 32
				local help_x = 280 - help_w/2
				gfx.setPattern(_50pct_pattern)
				gfx.fillRect(help_x - 8,help_y + help_h + 16,help_w+16,2)
				gfx.setColor(gfx.kColorBlack)
				gfx.fillRect(help_x - 8,help_y,help_w+16,help_h+16)
				print_small(help,help_x,help_y+8,gfx.kColorWhite)			

				-- draw "level" position on 3d model
				gfx.setColor(gfx.kColorBlack)
				local tri_lx = help_x + 2
				local tri_rx = help_x + 16
				if x>280 then
					tri_lx = help_x + help_w - 16
					tri_rx = help_x + help_w - 2
				end
				gfx.setColor(gfx.kColorBlack)
				gfx.fillTriangle(x,y,tri_lx,help_y,tri_rx,help_y)
			end

			local y=90
			for i=1,#panels do
				local panel = panels[i]
				local name = panel.params.name
				if panel==sel_panel then
					-- text length
					gfx.setFont(largeFont[gfx.kColorBlack])
					local sel_w = gfx.getTextSize(name)
					gfx.setPattern(_50pct_pattern)
					gfx.fillRect(0,y+32,sel_w + panel.x + 16,2)
					gfx.setColor(gfx.kColorBlack)
					gfx.fillRect(0,y,sel_w + panel.x + 16,32)
	
					print_regular(name, panel.x, y,gfx.kColorWhite)
				else
					print_regular(name, panel.x, y, gfx.kColorBlack)
				end
				y += 28
			end			
			
		end		
end

function menu_zoomout_state(cam_pos, look_at, scale, angle)
	-- menu cam	
	local cam=make_cam(cam_pos)

	local mountain_actor={
		id=models.PROP_MOUNTAIN,
		pos={0.5,0,1},
		m={
			1,0,0,0,
			0,1,0,0,
			0,0,1,0,
			0.5,0,1,1},
		update=function(self)
			local m=make_m_y_rot(angle)
			m[1]  = scale
			m[6]  = scale
			m[11] = scale

			local pos=self.pos
			m[13]=pos[1]
			m[14]=pos[2]
			m[15]=pos[3]
			self.m=m
		end
	}
	
	do_async(function()
		local s=scale
		for i=1,30 do
			local t=(i-1)/30
			scale=lerp(s,1,t)
			cam.pos=v_lerp(cam.pos,{0,0.8,-0.5},0.2)
			look_at=v_lerp(look_at,{0,0,1},0.2)
			coroutine.yield()
		end
		next_state(menu_state, angle)
	end)

	return
		-- update
		function()
			mountain_actor:update()
			--
			cam:look(look_at)
		end,
		-- draw
		function()
			cls(gfx.kColorWhite)
			
			lib3d.add_render_prop(mountain_actor.id,table.unpack(mountain_actor.m))
			lib3d.render_props(cam.pos[1],cam.pos[2],cam.pos[3],table.unpack(cam.m))

		end		
end

function zoomin_state(go_state,...)
	local ttl,dttl=30,0.01
  local starting
	local fade=0
	local args={...}
	do_async(function()
		while fade<15 do
			coroutine.yield()
			fade+=1
		end
		next_state(go_state,table.unpack(args))
	end)	
	return
		-- update
		function(self)
		end,
		-- draw
		function()
			local screen=gfx.getDisplayImage()
			local screenWidth, screenHeight = screen:getSize()
			gfx.lockFocus(screen)
			for i=1,4 do
				gfx.setColor(gfx.kColorWhite)
				gfx.fillCircleAtPoint(rnd(screenWidth), rnd(screenHeight), 8 + rnd(32))
			end
			gfx.unlockFocus()

			screen:drawBlurred(0,0,2,1,gfx.image.kDitherTypeScreen)
		end
end


-- buy collectibles
function shop_state(...)
	-- capture current backbuffer
	local background=gfx.getDisplayImage()
	
	local coins = _save_state.coins
	local selection = 1
	local action_ttl=0
	local y_offset = -240
	local button_x = -80
	local menu_params={...}
	return
		-- update
		function()
			if action_ttl==0 then
				if playdate.buttonJustReleased(playdate.kButtonUp) then selection-=1 button_x = -80 _button_click:play(1) end
				if playdate.buttonJustReleased(playdate.kButtonDown) then selection+=1 button_x = -80 _button_click:play(1) end
				selection=mid(selection,1,#_store_items)
			end

			local item=_store_items[selection]
			if not playdate.buttonIsPressed(playdate.kButtonA) then 
				action_ttl=0
			end
			local is_valid = _save_state[item.uuid] or _save_state.coins>=item.price
			if is_valid and playdate.buttonIsPressed(playdate.kButtonA) then
				action_ttl=min(60,action_ttl+1)
				if action_ttl==60 then
					action_ttl = 0
					do_async(function()
						-- commit basket instantly
						_save_state[item.uuid] = 1
						_save_state.coins -= item.price
						for i=1,30 do
							coins -= 1
							coroutine.yield()
						end
					end)
				end
			end
			if playdate.buttonJustReleased(playdate.kButtonB) then
				_button_click:play(1)
				next_state(menu_zoomout_state,table.unpack(menu_params))
			end

			y_offset = lerp(y_offset,(selection-1) * 74,0.5)
			button_x = lerp(button_x,0,0.8)

		end,
		-- draw
		function()
			background:draw(0,0)

			local y=48 - y_offset

			for i=1,#_store_items do
				-- out of screen?
				if y>240 then
					break
				end
				local item = _store_items[i]
				gfx.setColor(gfx.kColorBlack)
				gfx.fillRect(0,y,400,65)
    		gfx.setPattern(_50pct_pattern)
				gfx.fillRect(0,y+65,400,2)

				item.preview:draw(2,y+2)
				-- title
				gfx.setStencilPattern(_50pct_pattern)
				print_regular(item.title,104,y+2,gfx.kColorWhite)
				gfx.clearStencil()
				print_regular(item.title,104,y,gfx.kColorWhite)

				-- scrolling if description line is too large
				local w=item.text_w
				if w>300 then
					local s=item.text.." / "			
					w=gfx.getTextSize(s)
					local offset=(16*time() + i*24)%w
					gfx.setClipRect(104, y+24, 300, 32)
					print_regular(s,104-offset,y+24,gfx.kColorWhite)
					print_regular(s,104-offset+w,y+24,gfx.kColorWhite)
					gfx.clearClipRect()
				else
					print_regular(item.text,104,y+24,gfx.kColorWhite)
				end
				
				local is_sold=_save_state[item.uuid]
				if is_sold then
					local s="SOLD"
					local w = gfx.getTextSize(s)
					gfx.setColor(gfx.kColorWhite)
					gfx.fillRect(399 - w - 8,y+2,w+6,24)
					print_regular(s,399 - w - 4,y - 2,gfx.kColorBlack)
				else
					local s="$"..item.price
					local w = gfx.getTextSize(s)
					print_regular(s,399 - w,y+2,gfx.kColorWhite)
				end
				y += 65
				-- already bought?
				if i==selection and not is_sold then
					local buy_text
					if action_ttl>0 then
						buy_text = "Buying..."
					else
						buy_text = "Buy".._input.action.glyph
					end
		
					local action_y=action_ttl>0 and 2 or 0
					local w=gfx.getTextSize(buy_text)
					gfx.setPattern(_50pct_pattern)
					gfx.fillRect(button_x,y+36,w+24,2)
					gfx.setColor(gfx.kColorBlack)
					gfx.fillRect(button_x,y+action_y,w+24,36)
		
					print_regular(buy_text,10+button_x,y+2+action_y,gfx.kColorWhite)

					-- back
					local back_text = "Back".._input.back.glyph
					local w=gfx.getTextSize(back_text)
					print_regular(back_text,399 - w,y+2,gfx.kColorBlack)
					y += 40
				end
				
				y+=8
			end
			-- current coins
			local s="$"..coins
			local w=gfx.getTextSize(s)+16
			gfx.setColor(gfx.kColorWhite)
			gfx.fillRect(200-w/2,0,w,24)
			gfx.setPattern(_50pct_pattern)
			gfx.fillRect(200-w/2,24,w,2)

			print_regular(s,nil,-4,gfx.kColorBlack)
	end		
end

-- -----------------------------	
-- command handlers
-- generic static prop
function make_static_actor(id,x,sfx_name,update)
	return function(lane,cam)
		local pos={x or (lane+0.5)*4,-16,_ground_height - 2}
		local y=_ground:find_face(pos)
		pos[2]=y
		-- sfx?
		local sfx=_ENV[sfx_name]
		if sfx then
			sfx:play(0)
			sfx:setVolume(0)
		end	
		return {
			id=id,
			pos=pos,
			m={
				1,0,0,0,
				0,1,0,0,
				0,0,1,0,
				0,0,0,1},
			update=function(self)
				local pos=self.pos
				-- out of landscape?
				if pos[3]<0.5 then
					if sfx then
						sfx:stop()
					end
					return 
				end

				-- update pos?
				local m=self.m
				if update then					
					update(pos)
				end
				-- if sound
				if sfx then
					local d=v_len(make_v(pos,cam.pos))
					if d<16 then d=16 end
					sfx:setVolume(16/d)
				end
				m[13]=pos[1]
				m[14]=pos[2]
				m[15]=pos[3]
				return true
			end
		}
	end	
end

-- custom provides additional commands (optional)
function make_command_handlers(custom)
	return setmetatable({
		-- snowball!!
		B=function(lane)
			local y_velocity,z_velocity = 0,1.5
			local y_force,on_ground = 0
			local base_angle = rnd()
			local pos={(lane+0.5)*4,0,0.5}
			local prev_pos=v_clone(pos)
			-- helper
			local function v2_sqrlen(x,z,b)
				local dx,dz=b[1]-x,b[3]-z
				return dx*dx+dz*dz
			end
			return {
				id=models.PROP_SNOWBALL,
				pos=pos,
				warning=lane,
				shift=function(self,offset)
					-- shift
					v_add(pos,offset)
					v_add(prev_pos,offset)
				end,
				update=function(self)
					-- gravity
					y_force = -4
					if on_ground then
						-- todo: shake?
						-- force += 4
						-- y_force += 0.5
					end
					y_velocity+=y_force*0.5/30
					prev_pos=v_clone(prev_pos)
					pos[2]+=y_velocity
					pos[3]+=z_velocity
					y_force=0

					-- capsule collision with player
					if _plyr then
						local plyr_x,plyr_z=_plyr.pos[1],_plyr.pos[3]
						-- kill warning if past player
						if pos[3]-plyr_z>4 then
							self.warning = nil
						end
						if v2_sqrlen(plyr_x,plyr_z,pos)<2.25 or 
							v2_sqrlen(plyr_x,plyr_z,prev_pos)<2.25 or 
							(plyr_x<pos[1]+1.5 and plyr_x>pos[1]-1.5 
							and plyr_z<pos[3] and plyr_z>prev_pos[3]) then
							_plyr.dead = true
						end
					end
					
					-- update
					local newy,newn=_ground:find_face(pos)
					-- out of bound: kill actor
					if not newy then return end

					on_ground = nil
					if pos[2]<=newy then
						pos[2]=newy
						on_ground = true
						y_velocity = 0
					end
					-- shadow plane projection matrix
					local m = make_m_from_v(newn)
					m[13]=pos[1]
					-- avoid z-fighting
					m[14]=newy+0.1
					m[15]=pos[3]
					self.m_shadow = m

					-- roll!!!
					local m = make_m_x_rot(base_angle-4*time())
					m[13]=pos[1]
					-- offset with ball radius
					m[14]=pos[2]+1.25
					m[15]=pos[3]
					self.m = m
					return true
				end
			}
		end,
		-- skidoo
		K=function(lane)
			local pos={(lane+0.5)*4,-16,_ground_height-2}
			-- sfx?
			return {
				id = models.PROP_SKIDOO,
				pos = pos,
				update=function(self)
					local pos=self.pos
					-- move upward!!
					pos[3]-=0.25

					-- update
					local newy,newn=_ground:find_face(pos)
					-- out of bound: kill actor
					if not newy then return end
					
					-- orientation matrix
					local m = make_m_from_v(newn)
					m[13]=pos[1]
					m[14]=newy
					m[15]=pos[3]
					self.m = m
		
					-- spawn particles
					if rnd()>0.2 then
						local v=m_x_v(m,v_lerp(vgroups.SKIDOO_LTRACK,vgroups.SKIDOO_RTRACK,rnd()))					
						lib3d.spawn_particle(0, table.unpack(v))
					end

					return true
				end
			}
		end,
		-- (tele)cabin
		t=make_static_actor(models.PROP_CABINS,_ground_width/2),
		-- hot air balloon
		h=make_static_actor(models.PROP_BALLOON),
		-- ufo
		u=make_static_actor(models.PROP_UFO,nil,"_ufo_sfx"),
		-- eagles
		a=make_static_actor(models.PROP_EAGLES,nil,"_eagle_sfx"),
		-- heli
		e=make_static_actor(models.PROP_HELO,nil,"_helo_sfx",function(pos)
			-- move toward player
			pos[3]-=1
			-- get current height
			local ny=_ground:find_face(pos)
			-- wooble over ground
			pos[2] = ny + 16 + 2*cos(time())
		end)
	},{__index=custom or {}})
end

-- -------------------------
-- bench mode
function bench_state()
	_ground = make_ground({slope=2,twist=4,num_tracks=1,tight_mode=0,props_rate=0.87,track_type=0,min_cooldown=8,max_cooldown=12})
	local pos={15.5*4,-16,12.5*4}
	local newy,newn=_ground:find_face(pos)
	pos[2]=newy+1
	local cam=make_cam()
	local u={0,0.9,1}
	v_normz(u)
	cam:track(pos,0,u,1,1)

	return 
		-- update
		function() end,
		-- draw
		function()
			_ground:draw(cam)
		end
end

-- endless run mode
function play_state(params,help_ttl)
	help_ttl=help_ttl or 0
	-- read data slot
	local best_distance = params.dslot and _save_state["best_"..params.dslot]	
	local prev_slice_id
	local frame_t=0
	-- trick event
	local combo_ttl,bonus,total_tricks,coins,bonus_coins=0,{},0,_save_state.coins
	local function register_trick(type,msg)
		total_tricks+=1
		local prev=bonus[#bonus]
		-- add to combo only if different trick
		if not prev or prev.msg~=msg then
			add(bonus,{x=-60,t=0,msg=msg})
			-- 3s to get another trick
			combo_ttl=90
		end
	end

	-- reset particles
	lib3d.clear_particles()

	-- start over
	_actors,_ground={},make_ground(params)

	-- create player in correct direction
	_plyr=make_plyr(_ground:get_pos(),register_trick)
	_plyr.on_coin=function(c)
		_coin_sfx:play(1)
		coins+=c
		_save_state.coins+=c
	end
	_plyr.hp = params.hp
	_tracked = _plyr
		
	-- create cam (or grab from caller)
	local cam=params.cam or make_cam()

	-- command handler
	local handlers = make_command_handlers(params.commands)
	-- 
	_ski_sfx:play(0)

	-- init goggle selection menu
	local menu = playdate.getSystemMenu()
	local menu_options = menu:getMenuItems()[3]
	if menu_options then
		-- clear previous entries
		menu:removeMenuItem(menu_options)
	end

	local masks={"none"}
	for i=1,#_store_items do
		local item=_store_items[i]
		-- unlocked?
		if _save_state[item.uuid] then
			add(masks,item.title)
		end
	end
	-- active mask (if any)
	local selected_mask = _store_by_uuid[_save_state.mask_uuid or -1]
	local mask=selected_mask and selected_mask.image
	local menuItem, error = menu:addOptionsMenuItem("mask", masks, selected_mask and selected_mask.title or "none", function(value)
		mask = nil
		_save_state.mask_uuid = nil
		-- 
		local item=_store_by_name[value]
		if item then
			_save_state.mask_uuid = item.uuid
			mask = item.image
		end
	end)	

	return
		-- update
		function()
			cam:update()
			-- 
			if _plyr then
				_plyr:control()	
				_plyr:integrate()
				_plyr:update()
			end

			-- adjust ground
			local z,slice_id,commands,wx,wy,wz = _ground:update(_tracked.pos)
			local offset={0,wy,wz}
			if _plyr then
				-- player position is already "corrected"
				_plyr.pos[2]+=wy
				_plyr.pos[3]=z
			end

			if _plyr then
				for _,b in pairs(bonus) do
					b.t+=1
					b.x=lerp(b.x,4,0.25)
				end
				combo_ttl-=1
				if combo_ttl<=0 then
					combo_ttl=0
					if #bonus>0 then
						local tmp={table.unpack(bonus)}
						-- add total line
						do_async(function()
							local multi=min(#tmp,4)
							bonus_coins = 1<<multi
							wait_async(30)
							for i=1,bonus_coins do
								_coin_sfx:play(1)
								coins += 1
								wait_async(3)
							end
							_save_state.coins = coins
							bonus_coins = nil							
							bonus={}
						end)
					end
				end
				if best_distance and _plyr.distance>best_distance then
					best_distance = _plyr.distance
					_save_state["best_"..params.dslot] = flr(best_distance)
				end

				-- handle commands (only if new)
				if prev_slice_id~=slice_id then
					for i=1,#commands do
						local c=string.sub(commands,i,i)
						local cmd=handlers[c]
						if cmd then
							local actor=cmd(i-1,cam)
							if actor then
								add(_actors,actor)
							end
						end
					end
					prev_slice_id = slice_id
				end
			end

			local offset={0,wy,wz}
			for i=#_actors,1,-1 do
				local a=_actors[i]
				if a.shift then
					a:shift(offset)
				else
					v_add(a.pos,offset)
				end
				if not a:update() then
					table.remove(_actors,i)
				end
			end
			if _plyr then
				local pos,a,steering=_plyr:get_pos()
				local up=_plyr:get_up()
				cam:track({pos[1],pos[2]+0.5,pos[3]},a,up)

				if _plyr.dead then
					_ski_sfx:stop()

					-- todo: game over music
					screen:shake()

					-- latest score
					next_state(plyr_death_state,cam,pos,flr(_plyr.distance),total_tricks,params)
					-- not active
					_plyr=nil
				else	
					local volume=_plyr.on_ground and 0.25-2*_plyr.height or 0
					_ski_sfx:setVolume(volume)
					_ski_sfx:setRate(1-abs(steering/2))
					-- simulates fresh snow
					_ski_sfx:setRate(1-8*_plyr.drag)
					-- reset?
					if help_ttl>90 and playdate.buttonJustPressed(_input.back.id) then				
						_ski_sfx:setVolume(volume/2)
						next_state(restart_state,params)
					end
				end
			end

			help_ttl+=1
			frame_t+=1
		end,
		-- draw
		function()
			blink_mask = 0
			for _,a in pairs(_actors) do
				if not a.blinking or frame_t%2==0 then
					_ground:add_render_prop(a.id,a.m)
					if a.m_shadow then
						_ground:add_render_prop(models.PROP_SHADOW,a.m_shadow)
					end
				end	
				-- foreshadowing lines
				if _plyr then
					if a.warning then
						if (frame_t+a.warning)%8<4 then
							blink_mask |= (1<<a.warning)
						end
					end
				end
			end
			_ground:draw(cam, blink_mask)			 

			if _plyr then
				local pos,a,steering,_,boost=_plyr:get_pos()
				local dy=_plyr.height*24
				-- ski
				local xoffset=6*cos(time()/4)
				_ski:draw(152+xoffset,210+dy-steering*14,gfx.kImageFlippedX)
				_ski:draw(228-xoffset,210+dy+steering*14)
				
				-- boost "speed lines" effect 
				if boost>0.25 then
					gfx.setColor(gfx.kColorBlack)
					local r0 = 100 + 32*(1 - boost / 1.5)
					for i=1,10 do
						local angle=rnd()
						local c,s=cos(angle),sin(angle)
						local radius=r0 + 32*rnd() 
						local x0,y0=200 + c*radius,120 + s*radius
						local x1,y1=x0+c*100-s*4,y0+s*100+c*4
						local x2,y2=x0+c*100+s*4,y0+s*100-c*4
						gfx.setDitherPattern(rnd(), gfx.image.kDitherTypeBayer4x4)
						gfx.fillTriangle(x0,y0,x1,y1,x2,y2)
					end
				end

				if mask then
					mask:draw(0,0)
				end

				local text_color = mask and gfx.kColorWhite or gfx.kColorBlack
				-- coins				
				print_small("$"..coins,0,0,text_color)

				-- current track
				local x = 399 - gfx.getTextSize(params.name)
				print_small(params.name,x,0,text_color)
				_mountain_icon:draw(x - _mountain_icon:getSize(),3)

				-- chill mode?
				if best_distance then
					-- total distance
					print_regular(flr(_plyr.distance).."m",nil,0,text_color)
				end

				local y_bonus = 28
				local t=30*time()
				for i=1,#bonus do
					local b=bonus[i]										
					if b.t>15 or t%4<2 then
						print_small(b.msg,b.x,y_bonus,text_color)
						y_bonus += 12
					end
				end
				if bonus_coins and t%4<2 then
					print_bold("="..bonus_coins.."x",4,y_bonus,gfx.kColorWhite)
				end

				if help_ttl<90 then
					-- help msg?
					if help_ttl<80 or help_ttl%2==0 then
						local text
						if _input.flipped then
							text = "ⒷJump/RestartⒶ"
						else
							text = "ⒷRestart/JumpⒶ"
						end
						print_regular(text,nil,mask and 132 or 162,gfx.kColorBlack)
					end
				end					
			end
		end		
end

function race_state(params)
	-- custom handling of 
	local npc
	local droping_in,dropped
	local frame_t=0
	local cam=make_cam()
	local make_helo=function(lane,cam)
		droping_in=true
		local pos={(lane+0.5)*4,-16,4}
		local y=_ground:find_face(pos)
		pos[2]=y+16
		-- sfx?
		_helo_sfx:play(0)
		_helo_sfx:setVolume(0)

		return {
			id=models.PROP_HELO,
			pos=pos,
			m=make_m_y_rot(0.5),
			update=function(self)
				local pos=self.pos
				-- update pos?
				pos[3]+=1
				-- get current height
				local ny=_ground:find_face(pos)
				-- out of landscape?
				if not ny then
					_helo_sfx:stop()
					return 
				end
				if not dropped then
					local slice=_ground:get_track(pos)
					pos[1]=lerp(pos[1],(slice.xmin+slice.xmax)/2,0.2)
				end

				-- wooble over ground
				pos[2] = ny + 16 + cos(time())

				if not dropped and pos[3]>_plyr.pos[3]+24 then
					-- avoid reentrancy
					dropped=true
					do_async(function()
						npc=make_npc(v_clone(pos),cam)
						add(_actors,npc)
					end)
				end
				local d=v_len(make_v(pos,cam.pos))
				if d<16 then d=16 end
				_helo_sfx:setVolume(16/d)

				local m=self.m
				m[13]=pos[1]
				m[14]=pos[2]
				m[15]=pos[3]
				return true
			end
		}
	end

	-- custom command handlers
	params.commands = {
		-- Nelicopter :)
		n = function(lane,cam)
			-- run helo sequence only once
			if params.skip_helo then return end
			params.skip_helo = true
			if droping_in then return end
			return make_helo(lane,cam)
		end,
		-- nPc
		p = function(lane,cam)
			if dropped then return end
			dropped=true
			local y=0
			if _plyr then y=_plr.pos[2]+12 end
			npc=make_npc({(lane+0.5)*4,y,4},cam)
			return npc
		end
	}
	-- use main "play" state for core loop
	params.cam = cam
	local play_update,play_draw=play_state(params)

	return
		-- update
		function()
			play_update()
			if _plyr then
				if _plyr.on_track then
					_plyr.drag = 0
				else
					_plyr.drag = rnd(0.1)
				end
			end
			if npc then
				if npc.dead then
					if _plyr then _plyr.dead = true end
					npc = nil
					return					
				end
			end
			frame_t+=1
		end,
		-- draw
		function()
			play_draw()
			if npc and npc.dist>30 then	
				local s=flr(npc.dist).."m"
				gfx.setFont(smallFont[gfx.kColorWhite])
				local sw,sh=gfx.getTextSize(s)
				sw += 4
				sh += 4
				-- project npc pos
				local x,y,w=cam:project2d(m_x_v(cam.m,npc.pos))
				x=mid(0,x - sw/2,399 - sw)
				y=mid(48,y-7*w,240-sh)
				gfx.setColor(gfx.kColorBlack)
				gfx.fillRect(x,y,sw,sh)
				print_small(s,x+2,y,gfx.kColorWhite)
				if npc.dist>70 and frame_t%4<2 then
					_warning_small:draw(x + sw/2 - 12,y - 28)
				end
			end
		end
end

function plyr_death_state(cam,pos,total_distance,total_tricks,params)
	-- convert to string
	local active_msg,msgs=0,{
		"Distance: "..total_distance.."m",
		"Total Tricks: "..total_tricks}	
	local gameover_y,msg_y,msg_tgt_y,msg_tgt_i=260,-20,{16,-40},0

	local prev_update,prev_draw = _update_state,_draw_state

	-- snowballing!!!
	local snowball=add(_actors,make_snowball(pos))
	-- hugh :/
	snowball:update()
	_tracked = snowball

	local turn_side,tricks_rating=pick({-1,1}),{"meh","rookie","junior","master"}
	local text_ttl,active_text,text=10,"yikes!",{"ouch!","aie!","pok!","weee!"}

	-- save records (if any)
	if params.dslot and total_distance>_save_state["best_"..params.dslot] then
		_save_state["best_"..params.dslot] = flr(total_distance)
	end
	
	-- stop ski sfx
	_ski_sfx:stop()

	return
		-- update
		function()		
			snowball:pre_update()	
			prev_update()

			msg_y=lerp(msg_y,msg_tgt_y[msg_tgt_i+1],0.08)
			gameover_y=lerp(gameover_y,48+8*sin(time()/4),0.06)

			if abs(msg_y-msg_tgt_y[msg_tgt_i+1])<1 then msg_tgt_i+=1 end
			if msg_tgt_i>#msg_tgt_y-1 then msg_tgt_i=0 active_msg=(active_msg+1)%2 end

			text_ttl-=1

			local p=snowball.pos
			if text_ttl<0 and _ground:collide(p,1) then
				snowball:hit()
				active_text,text_ttl=pick(text),10
				turn_side=-turn_side
				screen:shake()
			end
			-- keep camera off side walls
			cam:track({mid(p[1],8,29*4),p[2],p[3]+16},0.5,v_up,0.2)

			if playdate.buttonJustReleased(_input.back.id) then
				next_state(zoomin_state,menu_state)
			end
			if playdate.buttonJustReleased(_input.action.id) then
				next_state(zoomin_state,play_state,params,90)
			end
		end,
		-- draw
		function()
			prev_draw()

			print_bold(msgs[active_msg+1],nil,msg_y,gfx.kColorWhite)
			local x,y=rnd(4)-2,msg_y+8+rnd(4)-2

			if text_ttl>0 and not time_over then
				print_regular(active_text,120,80+text_ttl)
			end
			_game_over:draw(200-211/2,gameover_y)

			if (time()%1)<0.5 then
				local text
				if _input.flipped then
					text = "ⒷMenu/RestartⒶ"
				else
					text = "ⒷRestart/MenuⒶ"
				end				
				print_regular(text,nil,162)
			end
		end
end

function restart_state(params)
	local prev_update,prev_draw = _update_state,_draw_state
	local screen=gfx.getDisplayImage()
	local screenWidth, screenHeight = screen:getSize()

	local ttl,max_ttl=0,24
	return
		-- update
		function()
			if ttl>max_ttl then
				-- reset game (without the zoom effect)
				next_state(play_state,params,90)
			elseif not playdate.buttonIsPressed(_input.back.id) then
				-- back to game ("unpause")
				_update_state,_draw_state=prev_update,prev_draw
			end
			ttl+=1
		end,
		-- draw
		function()
			screen = screen:blurredImage(2,1,gfx.image.kDitherTypeScreen)
			gfx.lockFocus(screen)
			for i=1,4 do
				gfx.setColor(gfx.kColorWhite)
				gfx.fillCircleAtPoint(rnd(screenWidth), rnd(screenHeight), 8 + rnd(32))
			end
			gfx.unlockFocus()
			screen:draw(0,0)

			print_bold("Reset?".._input.back.glyph,nil,162,gfx.kColorBlack)
		end
end

-------------------------------------
-- load & save helpers
-- Function that saves game data
function saveGameData()
	-- Serialize game data table into the datastore
	if _save_state then
		playdate.datastore.write(_save_state)
	end
end

-- Automatically save game data when the player chooses
-- to exit the game via the System Menu or Menu button
function playdate.gameWillTerminate()
	saveGameData()
end

-- Automatically save game data when the device goes
-- to low-power sleep mode because of a low battery
function playdate.gameWillSleep()
	saveGameData()
end

--------------------------------------
-- startup function - mimics pico8 semantic
function _init()
	-- todo: remove (only for benchmarks)
	-- srand(12)

	_ski = gfx.image.new("images/ski")
	_ski_sfx = playdate.sound.sampleplayer.new("sounds/skiing_loop")
	_eagle_sfx = playdate.sound.sampleplayer.new("sounds/eagle_loop")
	_ufo_sfx = playdate.sound.sampleplayer.new("sounds/ufo_loop")
	_helo_sfx = playdate.sound.sampleplayer.new("sounds/helo_loop")
	_coin_sfx = playdate.sound.sampleplayer.new("sounds/coin")
	_checkpoint_sfx = playdate.sound.sampleplayer.new("sounds/checkpoint")
	_treehit_sfx = playdate.sound.sampleplayer.new("sounds/tree-impact-1")
	_button_click = playdate.sound.sampleplayer.new("sounds/ui_button_click")
	_boost_sfx = playdate.sound.sampleplayer.new("sounds/boost")

	_game_over = gfx.image.new("images/game_over")
	_dir_icon = gfx.image.new("images/checkpoint_lock")
	_mountain_icon = gfx.image.new("images/mountain_icon")

	_warning_small = gfx.image.new("images/warning_small")
	_warning_avalanche = gfx.image.new("images/warning_avalanche")
	_warning_skiier = gfx.image.new("images/warning_skiier")

	_game_title_anim = playdate.graphics.imagetable.new("images/generated/game_title")

	-- inverse lookup tables
	_store_by_name = {}
	_store_by_uuid = {}
	gfx.setFont(largeFont[gfx.kColorBlack])
	-- load preview & mask pictures
	for _,item in pairs(_store_items) do
		item.preview = gfx.image.new(item.image.."_preview")
		item.image = gfx.image.new(item.image)
		-- text width
		item.text_w = gfx.getTextSize(item.text)
		_store_by_name[item.title] = item
		_store_by_uuid[item.uuid] = item
	end
	
	-- load game state
	-- Call near the start of your game to load saved data
	_save_state = playdate.datastore.read()
	if not _save_state then
		-- default values
		_save_state = {}		
		_save_state.version = 1
		_save_state.coins = 0
		_save_state.flip_crank = false
		_save_state.best_1 = 1000
		_save_state.best_2 = 500
		_save_state.best_3 = 250
	end
	-- default "midwinter" mask is "free"
	_save_state["e4efa4d1-330b-434e-b4d4-b7f2eab7d92b"] = 1
	
	-- init state machine
	next_state(loading_state)
end

function _update()
  -- any futures?
  for i=#_futures,1,-1 do
    -- get actual coroutine
    local f=_futures[i].co
    -- still active?
    local cs=coroutine.status(f)
    if cs=="suspended" then
      coroutine.resume(f)
    else
      table.remove(_futures,i)
    end
  end

	-- state mgt
	if _update_state then _update_state() end
	
end

-- interface to ground (C) functions
function make_ground(params)
	-- ground params
	local gp = lib3d.GroundParams.new()
	for k,v in pairs(params) do
		gp[k] = v
	end
	lib3d.make_ground(gp)
	
	-- interface with C library
	return {		
		_gp = gp,
		get_pos=function(self)
			return {lib3d.get_start_pos()}
		end,
		update=function(self,p)
			return lib3d.update_ground(table.unpack(p))
		end,
		draw=function(self,cam,blink_mask)
			lib3d.render_ground(cam.pos[1],cam.pos[2],cam.pos[3],cam.angle%1,blink_mask,table.unpack(cam.m))
		end,
		draw_props=function(self,cam)
			lib3d.render_props(cam.pos[1],cam.pos[2],cam.pos[3],table.unpack(cam.m))
		end,
		find_face=function(self,p)
			local y,nx,ny,nz=lib3d.get_face(table.unpack(p))
			return y, {nx,ny,nz}
		end,
		get_track=function(self,p)
			local xmin,xmax,z,checkpoint,angle = lib3d.get_track_info(table.unpack(p))
			return {
				xmin=xmin,
				xmax=xmax,
				z=z,
				is_checkpoint=checkpoint,
				angle=angle
			}
		end,
		get_props=function(self,p)
			-- layout: 
			-- type: int
			-- coords float x 3
			local props = {lib3d.get_props(table.unpack(p))}
			local out={}
			for i=1,#props,4 do
				add(out,{
					type = props[i],
					pos = {props[i+1],props[i+2],props[i+3]}
				})				
			end
			return out
		end,
		clear_checkpoint=function(self,p)
			lib3d.clear_checkpoint(table.unpack(p))
		end,
		-- find all actors within a given radius from given position
		collide=function(self,p,r)
			local hit_type = lib3d.collide(p[1],p[2],p[3],r)
			return hit_type>0 and hit_type
		end,
		update_snowball=function(self,p,r)
			lib3d.update_snowball(p[1],p[2],p[3],r)
		end,
		add_render_prop=function(self,id,m)
			lib3d.add_render_prop(id,table.unpack(m))
		end
	}
end

-->8
-- print helpers
function padding(n)
	n=tostring(min(flr(n),99))
	return string.sub("00",1,2-#n)..n
end

function time_tostr(t,sep)
	-- frames per sec
	sep=sep or "''"
	return padding(flr(t/30)%60)..sep..padding(flr(10*t/3)%100)
end

function print_text(s,x,y)
	if not x then
		x=200 - gfx.getTextSize(s) / 2
	end
  gfx.drawTextAligned(s,x,y,kTextAlignment.left)
end

function print_bold(s,x,y,c)  
	gfx.setFont(outlineFont[c or gfx.kColorBlack])
	print_text(s,x,y)
end

function print_regular(s,x,y,c,align)
	gfx.setFont(largeFont[c or gfx.kColorBlack])
	print_text(s,x,y)
end

function print_small(s,x,y,c,align)
	gfx.setFont(smallFont[c or gfx.kColorBlack])
	print_text(s,x,y)
end

-- *********************
-- main

-- init
_init()

function playdate.update()
	-- switch input using crank state
	_input.flipped = playdate.isCrankDocked()
	_input = _inputs[playdate.isCrankDocked()]
  _update()
  if _draw_state then _draw_state() end
  playdate.drawFPS(0,228)
end