-- snow!
-- by @freds72

-- game globals
import 'CoreLibs/graphics'
import 'CoreLibs/nineslice'
import 'models.lua'

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
local _flip_crank=1

local panelFont = gfx.font.new('font/Roobert-10-Bold')
local panelFontFigures = gfx.font.new('font/Roobert-24-Medium-Numerals-White')
local _angle=0

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
local function nop() end
local pal=nop
local palt=nop
local music=nop
local sfx=nop
local spr=nop
local sspr=nop
local fillp=nop
local rectfill=nop
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
local actors,ground,plyr,cam,_tracked={}

-- camera clip planes
local k_far,k_near,k_right,k_left,z_near=0,2,4,8,0.2

-- camera
function make_cam()
	--
	local up={0,1,0}

	-- screen shake
	local shkx,shky=0,0
	camera()

	return {
		pos={0,0,0},
		angle=0,
		m=make_m_from_v_angle(v_up,0),
		shake=function(self,scale)
			scale=scale or 24
			shkx,shkx=min(8,shkx+rnd(scale)),min(8,shky+rnd(scale))
		end,
		update=function()
			shkx=shkx*-0.7-rnd(0.4)
			shky=shky*-0.7-rnd(0.4)
			if abs(shkx)<0.5 and abs(shky)<0.5 then
				shkx,shky=0,0
			end
			camera(shkx,shky)
		end,
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
function make_body(p)
	-- last contact face
	local up,oldf={0,1,0}

	local velocity,angularv,forces,torque={0,0,0},0,{0,0,0},0
	local boost=0
	local angle,steering_angle,on_air_ttl,was_on_air=0,0,0

	local g={0,-4,0}
	return {
		pos=v_clone(p),
		on_ground=nil,
		height=0,
		get_pos=function(self)
	 		return self.pos,angle,steering_angle/0.625,velocity
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
			torque=torque+t
		end,
		boost=function(self,b)
			boost=b
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
			angularv=angularv+torque*0.5/30

			-- apply some damping
			angularv*=0.86
			-- kill boost while on ground
			if self.on_ground then boost*=0.9 end
			local f=self.on_ground and 0.08 or 0.01
			-- some friction
			--v_scale(velocity,1-f)
			v_add(velocity,velocity,-f*v_dot(velocity,velocity))
			
			-- update pos & orientation
			--local x,z=self.pos[1],self.pos[3]
			v_add(self.pos,velocity,1+boost)
			--self.pos[1],self.pos[3]=x,z

			-- limit rotating velocity
			angularv=mid(angularv,-1,1)
			angle=angle+angularv

			-- reset
			forces,torque={0,0,0},0
		end,
		steer=function(self,steering_dt)
			steering_angle=steering_angle+mid(steering_dt,-0.15,0.15)
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
				on_air_ttl=0
			else
				steering_angle*=0.85
				on_air_ttl+=1
			end
			
			-- find ground
			local pos=self.pos

			local newy,newn=ground:find_face(pos)
			-- stop at ground
			self.on_ground=nil
			local tgt_height=1
			if pos[2]<=newy then
				up=newn
				tgt_height=pos[2]-newy
				pos[2]=newy			
				self.on_ground=true
				-- was on air? big enough jump?
				if on_air_ttl>23 then cam:shake(16) sfx(12) on_air_ttl=0 sfx(11) end
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

	body.hp = 3
	body.distance = 0
	body.control=function(self)	
		local da=0
		if playdate.isCrankDocked() then
			if playdate.buttonIsPressed(playdate.kButtonLeft) then da=1 end
			if playdate.buttonIsPressed(playdate.kButtonRight) then da=-1 end
		else
			local change, acceleratedChange = playdate.getCrankChange()
			da = _flip_crank * acceleratedChange
		end

		if self.on_ground then
			-- was flying?			
			if air_t>23 then
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

		local hit_type=ground:collide(pos,0.2)
		if hit_type==2 then
			-- walls: insta-death
			cam:shake()
			self.dead=true
		elseif hit_type==3 then
			-- todo: remove coins
		elseif hit_type==4 then
			--
			if self.on_ground then
				self:apply_force_and_torque({0,32,0},0)
				self:boost(2)
				jump_ttl=0
			end
		elseif hit_ttl<0 and hit_type==1 then
			-- props: 
			_treehit_sfx:play()
			cam:shake()
			-- temporary invincibility
			hit_ttl=15
			self.hp-=1
			-- kill tricks
			reverse_t,spin_prev=0
		end

		self.on_track=true
		local slice=ground:get_track(pos)
		self.gps=slice.angle+angle
		if pos[1]<slice.xmin and pos[1]>slice.xmax then			
			self.on_track=nil
		end

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
		-- total distance
		-- 2: crude 1 game unit = 2 meters
		body.distance+=max(0,2*velocity[3])
	end

	-- wrapper
	return body
end

function make_npc(p)
	local body=make_body(p)
	local up={0,1,0}
	local dir=0
	local body_update=body.update
	body.id = models.PROP_SKIER
	
	body.update=function(self,offset)
		-- shift
		v_add(self.pos,offset)
		
		-- crude ai control!
		local _,angle=self:get_pos()
		local slice=ground:get_track(self.pos)
		local da=slice.angle+angle
		-- transition to "up" when going left/right
		dir=lerp(dir,da,0.6)
		if dir<-0.02 then
			self.id = models.PROP_SKIER_RIGHT
		elseif dir>0.02 then
			self.id = models.PROP_SKIER_LEFT
		else
			self.id = models.PROP_SKIER
		end
		self:steer(da/4)

		self:integrate()

		-- call parent
		self.boost=0.1
		body_update(self)

		-- create orientation matrix
		local pos=self.pos
		local newy,newn=ground:find_face(pos)
		up=v_lerp(up,newn,0.3)
		local _,angle=self:get_pos()
		local m=make_m_from_v_angle(up,angle)
		m[13]=pos[1]
		m[14]=pos[2]
		m[15]=pos[3]

		self.m = m

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
		local newy,newn=ground:find_face(pos)
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

function menu_state()
  local starting
	local best_y = -20
	local tree_prop,bush_prop,cow_prop={sx=112,sy=16,r=1.4,sfx={9,10}},{sx=96,sy=32,r=1,sfx={9,10}},{sx=112,sy=48,r=1,sfx={4}}
	local panels={
		{state=play_state,panel=make_panel("MARMOTTES","piste verte",12),c=1,params={name="Marmottes",slope=1.5,twist=2.5,num_tracks=3,tight_mode=0,props_rate=0.95,track_type=0,min_cooldown=30*5,max_cooldown=30*35}},
		{state=play_state,panel=make_panel("BIQUETTES","piste rouge",18),c=8,params={name="Biquettes",dslot=2,slope=2,twist=3,num_tracks=1,tight_mode=1,props_rate=0.97,track_type=3,min_cooldown=8,max_cooldown=12}},
		{state=play_state,panel=make_panel("CHAMOIS","piste noire",21),c=0,params={name="Chamois",dslot=3,slope=2.25,twist=6,num_tracks=1,tight_mode=0,props_rate=0.97,track_type=2,min_cooldown=8,max_cooldown=8}},
		{state=shop_state,panel=make_direction("Shop"),transition=station_state}
	}
	local sel,sel_tgt,blink=0,0,false
	-- background actors
	local actors={}
	ground=make_ground({slope=0,tracks=0,props_rate=0.90,twist=0,track_type=0,min_cooldown=9999,max_cooldown=9999})

	-- reset cam	
	cam=make_cam()

	music(0)
	_ski_sfx:stop()

	-- menu to get back to selection menu
	local menu = playdate.getSystemMenu()
	menu:removeAllMenuItems()
	local menuItem, error = menu:addMenuItem("start menu", function()
			_futures={}
			next_state(menu_state)
	end)	
	local menuItem, error = menu:addCheckmarkMenuItem("flip crank", false, function(value)
		_flip_crank = value and -1 or 1
	end)	

	return
		-- update
		function()
			if playdate.buttonJustReleased(playdate.kButtonLeft) then sel-=1 best_y=-20 end
			if playdate.buttonJustReleased(playdate.kButtonRight) then sel+=1 best_y=-20 end
			sel=mid(sel,0,#panels-1)
			
  		sel_tgt=lerp(sel_tgt,sel,0.18)
   		-- snap when close to target
			if abs(sel_tgt-sel)<0.01 then
				sel_tgt=sel
				best_y = lerp(best_y,0,0.3)
			end			

			if not starting and playdate.buttonJustReleased(playdate.kButtonA) then
				-- snap track
				sel_tgt=sel
				sfx(8)
				-- sub-state
        starting=true
				do_async(function()
					for i=1,15 do
						blink=i%2==0 and true
						coroutine.yield()
					end
					-- restore random seed
					math.randomseed(playdate.getSecondsSinceEpoch())
					local p=panels[sel+1]
					next_state(p.transition or zoomin_state,p.state,p.params)
				end)
			end
			if #actors<3 and rnd()<0.5 then
				local vel=0.2 + 0.2*rnd()
				local rot=0.1*rnd()
				local freq=1+2*rnd()
				if rnd()>0.5 then vel=-vel end
				add(actors,{
					id=pick{models.PROP_SKIER,models.PROP_SLED},
					pos={(16.5+rnd()*4)*4,0,vel>0 and 0.5*4 or 31.5*4},
					update=function(self)
						local pos=self.pos
						pos[3]+=vel
						if pos[3]>31.5*4 or pos[3]<0.5*4 then return end

						local m=make_m_y_rot((vel>=0 and 0 or 0.5)+rot*cos(time()/freq))
						m[13]=pos[1]
						m[14]=pos[2]
						m[15]=pos[3]			
						self.m=m
						return true
					end
				})
			end

			for i=#actors,1,-1 do
				local a=actors[i]
				if not a:update() then
					table.remove(actors,i)
				end
			end

			--
			cam:track({64,0,64},sel_tgt/#panels,v_up)
		end,
		-- draw
		function()
			for _,a in pairs(actors) do
				ground:add_render_prop(a.id,a.m)
				if a.m_shadow then
					ground:add_render_prop(models.PROP_SHADOW,a.m_shadow)
				end
			end

			ground:draw(cam)

			local a,da=0.25-0.05,-1/#panels
			for i=1,#panels do
				local v={8*cos(a),0.8,-8*sin(a)}
				v_add(v,cam.pos)
				v=m_x_v(cam.m,v)
				if v[3]>0 then
					local x0,y0=cam:project2d(v)
					local p=panels[i]
					local w,h=p.panel:getSize()
					_panel_pole:draw(x0-4,y0+20)
					p.panel:draw(x0-w/2,y0-16)
				end
				a+=da
			end
			
			-- ski mask
			_mask:draw(0,0)
			
			local help="⬅️➡️"
			if (time()%2)<1 then 
				help="Ⓐ"
			end
			print_regular(help,399-gfx.getTextSize(help),0,gfx.kColorWhite)

			print_regular("$".._save_state.coins,0,0,gfx.kColorWhite)

			local params=panels[sel+1].params
			if sel==sel_tgt and params and params.dslot then
				local s="".._save_state["best_"..params.dslot].."m"
				print_regular(s,nil,best_y,gfx.kColorWhite)
			end
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
function shop_state()
	local background = gfx.image.new("images/ski_shop")
	local left_button={
		up=gfx.image.new("images/shop_lbutton_up"),
		down=gfx.image.new("images/shop_lbutton"),
		ttl=0
	}
	local right_button={
		up=gfx.image.new("images/shop_rbutton_up"),
		down=gfx.image.new("images/shop_rbutton"),
		ttl=0
	}
	local function draw_button(btn,x)
		-- pressed?
		if btn.ttl>0 then
			btn.down:draw(x,19)
		else
			btn.up:draw(x,17)
		end
	end

	-- shop items
	local items={
		{image="images/mask",title="Classic",text="Mint condition",price=0},
		{image="images/mask_style",title="Modern",text="Want an aviator look?\nSearch no more",price=100},
		{image="images/mask_love",title="Love Love!",text="For smooth rides...",price=200},
		{image="images/mask_shades",title="Star System",text="To go incognito...\nor not!",price=400},
	}
	-- load preview
	for i=1,#items do
		items[i].preview = gfx.image.new(items[i].image.."_preview")
	end
	-- todo: remove price for items already bought

	local selection = 1
	local action_ttl=0

	return
		-- update
		function()
			left_button.ttl=max(0,left_button.ttl-1)
			right_button.ttl=max(0,right_button.ttl-1)
			if action_ttl==0 then
				if playdate.buttonJustReleased(playdate.kButtonLeft) then left_button.ttl=15 selection-=1 end
				if playdate.buttonJustReleased(playdate.kButtonRight) then right_button.ttl=15 selection+=1 end
				if selection<1 then selection=#items-1 end
				if selection>#items then selection=1 end
			end

			if not playdate.buttonIsPressed(playdate.kButtonA) then 
				action_ttl=0
			end
			if playdate.buttonIsPressed(playdate.kButtonA) then
				if action_ttl==0 then
					action_ttl = 30
				elseif action_ttl==1 then
					print("buy or equip: "..items[selection].title)
				end
				action_ttl=max(0,action_ttl-1)
			end
			if playdate.buttonJustReleased(playdate.kButtonB) then
				next_state(station_state,menu_state)
			end
		end,
		-- draw
		function()
			background:draw(0,0)
			gfx.setColor(gfx.kColorBlack)
			gfx.fillRect(228,10,167,225)
			local item=items[selection]

			gfx.setFont(memoFont[gfx.kColorWhite])
			-- title
			gfx.drawTextAligned(item.title,312,17,kTextAlignment.center)
			-- back menu
			gfx.drawTextAligned("Ⓑ Back",387,215,kTextAlignment.right)
			
			local w,h=item.preview:getSize()
			item.preview:draw(312-w/2,32)
			print_bold(item.text,232,108,gfx.kColorWhite)

			if item.price>0 then
				print_regular("Price: "..item.price.."$",232,156,gfx.kColorWhite)
			end

			-- buttons (pressed?)
			draw_button(left_button,232)
			draw_button(right_button,379)

			local y_offset = action_ttl>0 and 2 or 0
    	gfx.setPattern({ 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55 })
			gfx.fillRect(232,208,72,23)
			gfx.setColor(gfx.kColorWhite)
			gfx.fillRect(232,206+y_offset,72,23)
			local buy_text
			if item.price>0 then
				buy_text = "Ⓐ BUY"
			else
				buy_text = "Ⓐ EQUIP"
			end
			print_regular(buy_text,240,210+y_offset,gfx.kColorBlack)
		end		
end

function play_state(params)
	local help_ttl=0
	-- read data slot
	local best_distance = params.dslot and _save_state["best_"..params.dslot]	
	local prev_slice_id
	-- warning signs
	local warnings={}

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
	
	-- stop music
	music(-1,250)

	-- srand(15)

	-- start over
	actors,ground={},make_ground(params)

	-- create player in correct direction
	plyr=make_plyr(ground:get_pos(),register_trick)
	_tracked = plyr

	-- reset cam	
	cam=make_cam()

	-- 
	_ski_sfx:play(0)

	-- command handlers
	local command_handlers={
		-- snowball!!
		B=function(lane)
			local y_velocity,z_velocity = 0,1+rnd(0.25)
			local y_force,on_ground = 0
			local base_angle = rnd()
			local pos=v_clone(plyr.pos)
			pos[1]+=(lane-1)*4
			local prev_pos=v_clone(pos)
			v_add(pos,{0,0,-4*8})
			-- helper
			local function v2_sqrlen(x,z,b)
				local dx,dz=b[1]-x,b[3]-z
				return dx*dx+dz*dz
			end
			add(actors,{
				id=models.PROP_SNOWBALL,
				pos=pos,
				update=function(self,offset)
					-- shift
					v_add(pos,offset)
					v_add(prev_pos,offset)

					-- integrate
					y_force = -1
					if on_ground then
						-- todo: shake?
						-- force += 4
					end
					y_velocity+=y_force*0.5/30
					prev_pos=v_clone(prev_pos)
					pos[2]+=y_velocity
					pos[3]+=z_velocity
					y_force=0

					-- collision with player
					if plyr then
						local plyr_x,plyr_z=plyr.pos[1],plyr.pos[3]
						if v2_sqrlen(plyr_x,plyr_z,pos)<2.25 or 
							v2_sqrlen(plyr_x,plyr_z,prev_pos)<2.25 or 
							(plyr_x<pos[1]+1.5 and plyr_x>pos[1]-1.5 
							and plyr_z<pos[3] and plyr_z>prev_pos[3]) then
							plyr.dead = true
						end
					end
					
					-- update
					local newy,newn=ground:find_face(pos)
					-- out of bound: kill actor
					if not newy then return end

					on_ground = nil
					if pos[2]<=newy then
						pos[2]=newy
						on_ground = true
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
					-- offset ball radius
					m[14]=pos[2]+1.25
					m[15]=pos[3]
					self.m = m
					return true
				end
			})
		end,
		-- warning sign
		w=function(lane)
			do_async(function()
				warnings[lane]=_warning_avalanche
				wait_async(40)
				warnings[lane]=nil
				wait_async(5)
				warnings[lane]=_warning_avalanche
				wait_async(5)
				warnings[lane]=nil
			end)
		end,
		-- (tele)cabin
		t=function()
			add(actors,{
				id=models.PROP_CABINS,
				pos={15.5*4,-16,30.5*4},
				m={
					1,0,0,0,
					0,1,0,0,
					0,0,1,0,
					0,0,0,1},
				update=function(self,offset)
					local pos=self.pos

					-- shift
					v_add(pos,offset)
					-- todo: find a last costly solution
					local newy=ground:find_face(pos)
					if newy then pos[2]=newy end

					-- out of landscape?
					if pos[3]<0 then return end

					local m=self.m
					m[13]=pos[1]
					m[14]=pos[2]
					m[15]=pos[3]					
					return true
				end
			})
		end,
		-- hot air balloon
		h=function(lane)
			add(actors,{
				id=models.PROP_BALLOON,
				pos={(15.5-lane/2)*4,-16,30.5*4},
				m={
					1,0,0,0,
					0,1,0,0,
					0,0,1,0,
					0,0,0,1},
				update=function(self,offset)
					local pos=self.pos

					-- shift
					v_add(pos,offset)
					-- todo: find a last costly solution
					local newy=ground:find_face(pos)
					if newy then pos[2]=newy end

					-- out of landscape?
					if pos[3]<0 then return end

					local m=self.m
					m[13]=pos[1]
					m[14]=pos[2]
					m[15]=pos[3]
					return true
				end
			})
		end		
	}

	return
		-- update
		function()
			cam:update()
			-- 
			if plyr then
				plyr:control()	
				plyr:integrate()
				plyr:update()
			end

			-- adjust ground
			local z,slice_id,commands,wx,wy,wz = ground:update(_tracked.pos)
			_tracked.pos[3] = z

			if plyr then
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
								coins += 1
								wait_async(3)
							end
							_save_state.coins = coins
							bonus_coins = nil							
							bonus={}
						end)
					end
				end
				if best_distance and plyr.distance>best_distance then
					best_distance = plyr.distance
					_save_state["best_"..params.dslot] = flr(best_distance)
				end

				-- handle commands (only if new)
				if prev_slice_id~=slice_id then
					print("command: "..commands)
					for i=1,#commands do
						local c=string.sub(commands,i,i)
						if command_handlers[c] then
							command_handlers[c](i)
						end
					end
					prev_slice_id = slice_id
				end
			end

			local offset={0,0,wz}
			for i=#actors,1,-1 do
				local a=actors[i]
				if not a:update(offset) then
					table.remove(actors,i)
				end
			end

			if plyr then
				local pos,a,steering=plyr:get_pos()
				local up=plyr:get_up()
				cam:track({pos[1],pos[2]+0.5,pos[3]},a,up)

				if plyr.dead then
					_ski_sfx:stop()
					
					sfx(3)
					cam:shake()

					-- latest score
					next_state(plyr_death_state,pos,flr(plyr.distance),total_tricks,params)
					-- not active
					plyr=nil
				else	
					local volume=plyr.on_ground and 0.25-2*plyr.height or 0
					_ski_sfx:setVolume(volume)
					_ski_sfx:setRate(1-abs(steering/2))

					-- reset?
					if help_ttl>90 and playdate.buttonJustPressed(_input.back.id) then				
						_ski_sfx:setVolume(volume/2)		
						next_state(restart_state,params)
					end
				end
			end

			help_ttl+=1
		end,
		-- draw
		function()
			for _,a in pairs(actors) do
				ground:add_render_prop(a.id,a.m)
				if a.m_shadow then
					ground:add_render_prop(models.PROP_SHADOW,a.m_shadow)
				end
			end
			ground:draw(cam)			 

			if plyr then
				local pos,a,steering=plyr:get_pos()
				local dy=plyr.height*24
				-- ski
				local xoffset=6*cos(time()/4)
				_ski:draw(152+xoffset,210+dy-steering*14,gfx.kImageFlippedX)
				_ski:draw(228-xoffset,210+dy+steering*14)
			
				-- coins
				print_small("$"..coins,0,0,gfx.kColorBlack)

				-- current track
				local x = 399 - gfx.getTextSize(params.name)
				print_small(params.name,x,0,gfx.kColorBlack)
				_mountain_icon:draw(x - 14,3)

				-- chill mode?
				if best_distance then
					-- total distance
					print_regular(flr(plyr.distance).."m",nil,0,bk)
				end

				local y_bonus = 28
				local t=30*time()
				for i=1,#bonus do
					local b=bonus[i]										
					if b.t>15 or t%4<2 then
						print_small(b.msg,b.x,y_bonus,gfx.kColorBlack)
						y_bonus += 12
					end
				end
				if bonus_coins and t%4<2 then
					print_bold("="..bonus_coins.."x",4,y_bonus,gfx.kColorWhite)
				end

				if plyr.gps and not plyr.on_track then
					local idx=abs(plyr.gps)//0.0625
					if plyr.gps<0 then
						for i=0,min(idx-1,2) do
							_dir_icon:draw(220 + 8*i,24)
						end
					else
						for i=0,min(idx-1,2) do
							_dir_icon:draw(180 - 8*i,24,gfx.kImageFlippedX)
						end
					end
				end				
					
				if help_ttl<90 then
					-- help msg?
					print_regular(_input.back.glyph.."Restart/Jump".._input.action.glyph,nil,162)
				end					

				-- any warning messages?
				for i,img in pairs(warnings) do
					local w=img:getSize()
					img:draw(200-w/2,42)
				end
			end
		end		
end

function plyr_death_state(pos,total_distance,total_tricks,params)
	-- convert to string
	local active_msg,msgs=0,{
		"Distance: "..total_distance.."m",
		"Total Tricks: "..total_tricks}	
	local gameover_y,msg_y,msg_tgt_y,msg_tgt_i=260,-20,{16,-30},0

	local prev_update,prev_draw = _update_state,_draw_state

	-- snowballing!!!
	local snowball=add(actors,make_snowball(pos))
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
			if text_ttl<0 and ground:collide(p,1) then
				snowball:hit()
				active_text,text_ttl=pick(text),10
				turn_side=-turn_side
				cam:shake()
			end
			-- keep camera off side walls
			cam:track({mid(p[1],8,29*4),p[2],p[3]+16},0.5,v_up,0.2)

			if playdate.buttonJustReleased(_input.back.id) then
				next_state(zoomin_state,menu_state)
			end
			if playdate.buttonJustReleased(_input.action.id) then
				next_state(zoomin_state,play_state,params)
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
				print_regular(_input.back.glyph.."Menu/Restart".._input.action.glyph,nil,162)
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
				next_state(play_state,params)
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

			print_bold("Reset?".._input.back.glyph,nil,110,gfx.kColorBlack)
		end
end

------------------------------------------------
-- helper to create a direction panel
function make_direction(text)
	gfx.setFont(panelFont)
	local w,h = gfx.getTextSize(text)
	w = min(128,w)

	local panel = gfx.image.new(w+32,40,gfx.kColorClear)
	gfx.lockFocus(panel)
	_panel_slices:drawInRect(0,0,w+32,40)

	gfx.drawText(text,12,20-h/2)
	gfx.unlockFocus()
	return panel
end

-- helper to create a slope panel
function make_panel(text_up,text_low,figure)
	local glyphs={}
	local draw_glyphs=function(text,flipped)
		local angleSign,angleBase,radiusScale=1,0,0.80
		-- draw bottom of panel
		if flipped then
			angleSign,angleBase=-1,180
			radiusScale=0.72
		end
		local angle1,angle0=0
		-- find angle extents
		for i=1,#text do
			local s = string.sub(text,i,i)
			local w = gfx.getTextSize(s)
			angle1 += angleSign*math.max(4,w)*2
			if not angle0 then angle0 = angle1 end
		end				
		local angle=angleBase+90-(angle1-angle0)/2
		--angle = 90

		gfx.setFont(panelFont)
		for i=1,#text do
			local s=string.sub(text,i,i)
			local w = gfx.getTextSize(s)
			w = math.max(4,w)
			local tmp = gfx.image.new(24,24,gfx.kColorClear)
			local dst = gfx.image.new(24,24,gfx.kColorClear)
			gfx.lockFocus(tmp)
			gfx.drawTextAligned(s,11.5-w/2,6)	
			gfx.unlockFocus()
			gfx.lockFocus(dst)
			gfx.setColor(gfx.kColorWhite)
			tmp:drawRotated(12,12,angleBase+angle-90)
			gfx.unlockFocus()
			add(glyphs,{
				img=dst,
				radiusScale=radiusScale,
				angle=angle})
			angle += math.floor(angleSign*w*2)
		end
	end
	if text_up then draw_glyphs(text_up) end
	if text_low then draw_glyphs(text_low,true)	end

	local w=96
	local panel = gfx.image.new(w,w,gfx.kColorClear)
	gfx.lockFocus(panel)
	gfx.setColor(gfx.kColorWhite)
	gfx.fillCircleInRect(0, 0, w, w)
	gfx.setLineWidth(2)
	gfx.setColor(gfx.kColorBlack)
	gfx.drawCircleInRect(1, 1, w-2, w-2)
	gfx.setLineWidth(1)
	gfx.fillCircleInRect(4, 4, w-8, w-8)
	for _,glyph in pairs(glyphs) do
		local angle = (math.pi*glyph.angle)/180
		local r = w*glyph.radiusScale/2
		glyph.img:draw(47.5-math.cos(angle)*r-12,47.5-math.sin(angle)*r-12)
	end
	if figure then
		gfx.setFont(panelFontFigures)
		gfx.drawTextAligned(figure,47.5,47.5-18,kTextAlignment.center)
	end
	gfx.unlockFocus()
	return panel
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

	-- https://opengameart.org/content/pine-tree-pack
	_tree=gfx.image.new("images/pine_snow_0")
	assert(_tree)

	_mask = gfx.image.new("images/mask")
	_sun = gfx.image.new("images/sun")
	_ski = gfx.image.new("images/ski")
	_ski_sfx = playdate.sound.sampleplayer.new("sounds/skiing_loop")
	_coin_sfx = playdate.sound.sampleplayer.new("sounds/coin")
	_checkpoint_sfx = playdate.sound.sampleplayer.new("sounds/checkpoint")
	_treehit_sfx = playdate.sound.sampleplayer.new("sounds/tree-impact-1")
	_panel_pole = gfx.image.new("images/panel_pole")
	_panel_slices = gfx.nineSlice.new("images/panel",6,5,10,30)

	_game_over = gfx.image.new("images/game_over")
	_dir_icon = gfx.image.new("images/checkpoint_lock")
	_mountain_icon = gfx.image.new("images/mountain_icon")

	_warning_avalanche = gfx.image.new("images/warning_avalanche")
	_warning_skiier = gfx.image.new("images/warning_skiier")

	-- load game state
	-- Call near the start of your game to load saved data
	_save_state = playdate.datastore.read()
	if not _save_state then
		-- default values
		_save_state = {}
		_save_state.version = 1
		_save_state.coins = 0
		_save_state.best_1 = 1000
		_save_state.best_2 = 500
		_save_state.best_3 = 250
	end

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

-->8
-- map tools
-- generate ski tracks
function make_tracks(xmin,xmax,max_tracks)
	local seeds={}
	local function add_seed(x,u,branch)
		-- trick types:
		-- 0: slope
		-- 1: hole
		local ttl,trick_ttl,trick_type

		local function reset_seed_timers()
			ttl,trick_ttl,trick_type=12+rnd(20),4+rnd(4),flr(rnd(2))		
		end

		reset_seed_timers()

		local angle=0.05+rnd(0.45)
	 	return add(seeds,{
			age=0,
			h=0,
	 		x=x or xmin+rnd(xmax-xmin),
			u=u or cos(angle),
			angle=angle,
		 	update=function(self)
			 	if self.dead then del(seeds,self) return end
				self.age+=1
				trick_ttl-=1
				ttl-=1
				if branch and trick_ttl<0 then
					if trick_type==0 then
						self.h+=1.5
					elseif trick_type==1 then
						self.h=-4
					end
					-- not too high + ensure straight line
					if trick_ttl<-5 then self.h=0 ttl=4+rnd(2) end
				end
				if ttl<0 then
					-- reset
					reset_seed_timers()
					self.u=cos(0.05+rnd(0.45))
					-- offshoot?
					if rnd()<0.5 and #seeds<max_tracks then
						add_seed(self.x,-self.u,true)
					end
				end
				self.x+=self.u
				if self.x<xmin then
					self.u=-self.u
					self.x=xmin
				elseif self.x>xmax then
					self.u=-self.u
					self.x=xmax
				end
		 	end
	 	})
	end
 	-- init
 	add_seed().main=true

 	-- update function
 	return function()
		for _,s in ipairs(seeds) do
			s:update()
		end
		-- kill intersections
		for i=1,#seeds do
			local s0=seeds[i]
			for j=i+1,#seeds do
				local s1=seeds[j]
				-- don't kill new seeds
				-- don't kill main track
				if s1.age>0 and flr(s0.x-s1.x)==0 then
					-- don'kill main track
					s1=s1.main and s0 or s1
					s1.dead=true
				end
			end
		end
	
		-- active seeds
		return seeds
	end
end

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
		draw=function(self,cam)
			lib3d.render_ground(cam.pos[1],cam.pos[2],cam.pos[3],cam.angle%1,table.unpack(cam.m))
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
	_input=_inputs[playdate.isCrankDocked()]
  _update()
  if _draw_state then _draw_state() end
  playdate.drawFPS(0,228)
end