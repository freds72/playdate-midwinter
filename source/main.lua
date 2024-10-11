-- snow!
-- by @freds72

-- game globals
import 'CoreLibs/graphics'
import 'CoreLibs/nineslice'
import 'CoreLibs/easing'
import 'CoreLibs/timer'
import 'CoreLibs/qrcode'
import 'models.lua'
import 'store.lua'

local gfx <const> = playdate.graphics
local smallFont <const> = {
	[gfx.kColorBlack]=gfx.font.new('font/Memo-Black'),
	[gfx.kColorWhite]=gfx.font.new('font/Memo-White')
}
local largeFont <const> = {
	[gfx.kColorBlack]=gfx.font.new('font/More-15-Black'),
	[gfx.kColorWhite]=gfx.font.new('font/More-15-White')
}
local outlineFont <const> = {
	[gfx.kColorBlack]=gfx.font.new('font/More-15-Black-Outline'),
	[gfx.kColorWhite]=gfx.font.new('font/More-15-White-Outline')
}
-- 50% dither pattern
local _50pct_pattern <const> = { 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55 }

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
local _ground_width <const> = 32*4
local _ground_height <const> = 40*4

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
  return min(max(a,b),c)
end
-- returns time as a fraction of seconds
local function time()
  return playdate.getElapsedTime()
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

local _sounds={}

function stop_sounds_state(state,...)
	-- dispose all active sounds when moving to next state
	for sfx in pairs(_sounds) do
		sfx:stop()
	end
	_sounds = {}
	
	return state(...)
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
local vec3 = lib3d.Vec3.new
local v_clone = lib3d.Vec3.clone
local v_copy = lib3d.Vec3.copy
local v_dot = lib3d.Vec3.dot
local v_scale = lib3d.Vec3.scale
local v_add = lib3d.Vec3.add
local v_len = lib3d.Vec3.length
local v_dist = lib3d.Vec3.dist
local v_normz = lib3d.Vec3.normz
local v_lerp = lib3d.Vec3.lerp
local v_move = lib3d.Vec3.move
local v_zero = lib3d.Vec3.zero
local v_up <const> =vec3(0,1,0)


-- matrix functions
local m_x_v = lib3d.Mat4.m_x_v
local m_x_m = lib3d.Mat4.m_x_m
local make_m_x_rot = lib3d.Mat4.make_m_x_rot
local make_m_y_rot = lib3d.Mat4.make_m_y_rot
local make_m_from_v_angle = lib3d.Mat4.make_m_from_v_angle
local make_m_from_v = lib3d.Mat4.make_m_from_v
local make_m_lookat = lib3d.Mat4.make_m_lookat
local m_inv = lib3d.Mat4.m_inv
local m_right = lib3d.Mat4.m_right
local m_up = lib3d.Mat4.m_up
local m_fwd = lib3d.Mat4.m_fwd
local m_inv_translate = lib3d.Mat4.m_inv_translate
local m_translate = lib3d.Mat4.m_translate

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
	local up=vec3(0,1,0)

	camera()

	return {
		pos=pos and v_clone(pos) or vec3(0,0,0),
		angle=0,
		m=make_m_from_v_angle(v_up,0),
		update=function()
			screen:update()
			camera(shkx,shky)
		end,
		look=function(self,to)
			local pos=self.pos
			local m=make_m_lookat(pos,to)
			self.angle=0.25-0.5*math.atan2(m[11],m[9])/math.pi
			-- inverse view matrix
			m_inv(m)
			m_inv_translate(m,pos)
			self.m=m
		end,
		track=function(self,p,a,u,power,snap)
   		local pos=self.pos
			v_copy(p,pos)
			pos[2]+=0.5
   		-- lerp angle
			self.angle=lerp(self.angle,a,power or 0.8)
			-- lerp orientation (or snap to angle)
			v_move(up,u,snap or 0.1)
			v_normz(up)

			-- shift cam position			
			local m=make_m_from_v_angle(up,self.angle)
			-- 1.8m player
			-- v_add(pos,v_up,64)
			v_add(pos,m_up(m),1.2)
			
			-- inverse view matrix
			m_inv(m)
			m_inv_translate(m,pos)
			
			self.pos = pos
			self.m = m
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
	local up,oldf=vec3(0,1,0)

	local velocity,angularv,forces,torque=vec3(0,0,0),0,vec3(0,0,0),0
	local boost,perm_boost=0,0
	local steering_angle=0
	angle=angle or 0
	
	local no_force=vec3(0,0,0)
	local g=vec3(0,-3.5,0)
	return {
		pos=v_clone(p),
		on_ground=nil,
		height=0,
		drag=0,
		get_velocity=function()
			return velocity
		end,
		get_pos=function(self)
	 		return self.pos,angle,steering_angle/0.625,velocity,boost
		end,
		get_up=function()
			-- compensate slope when not facing slope
			local scale=abs(cos(angle))
			local u=v_lerp(v_up,up,scale)
			local m=make_m_from_v_angle(u,angle)
			
			local right <close> = m_right(m)
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
				local n <close> = v_clone(up)
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
			-- print("move len: "..v_len(velocity) * (1 + boost + perm_boost))

			-- limit rotating velocity
			angularv=mid(angularv,-1,1)
			angle+=angularv

			-- reset
			v_zero(forces)
			torque=0
		end,
		steer=function(self,steering_dt)
			-- stiff direction when boosting!
			if boost>0.1 then steering_dt/=2 end
			steering_angle+=mid(steering_dt,-0.15,0.15)
			-- on ground?
			if self.on_ground and v_len(velocity)>0.001 then

				-- desired ski direction
				local m <close> = make_m_from_v_angle(up,angle-steering_angle/16)
				local right <close> = m_right(m)
				local fwd <close> = m_fwd(m)
				
				-- slip angle
				local sa=-v_dot(velocity,right)
				if abs(sa)>0.001 then
					-- max grip
					local vn <close> =v_clone(velocity)
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
				self:apply_force_and_torque(no_force,-steering_angle/4)
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

			local newy,newn <close> =_ground:find_face(pos)
			if not newy then
				-- hum...
				self.dead = true
				return
			end

			-- stop at ground
			self.on_ground=nil
			self.on_cliff=newn[2]<0.5
			local tgt_height=1
			if pos[2]<=newy then
				v_copy(newn,up)
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
	body.invert_ttl = 0
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
		-- inverted controls?
		if self.invert_ttl>0 then
			da = -da
		end

		if self.on_ground then
			-- was flying?			
			if air_t>35 then
				-- pro trick? :)
				if reverse_t>0 then
					on_trick(2,"reverse air!")
				else
					on_trick(1,"air!")
				end
			end
			air_t=0

			if jump_ttl>8 and playdate.buttonJustPressed(_input.action.id) then
				self:apply_force_and_torque(vec3(0,56,0),0)
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
		self.invert_ttl-=1
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
		elseif hit_type==5 then
			if reverse_t>0 then
				on_trick(5,"reverse over!")
			else
				on_trick(4,"jump over")
			end
		elseif hit_ttl<0 and hit_type==1 then
			-- props: 
			pick(_treehit_sfxs):play()
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
		if v_dot(velocity,-sin(angle),0,cos(angle))<-0.2 then
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

function make_jinx(id,pos,velocity,params)
	local base_angle=rnd()
	return {
		id=id,
		pos=pos,
		-- random orientation
		m=make_m_y_rot(0),
		-- blinking=true,
		update=function(self)
			if params.ttl and params.ttl<time() then
				params.die(self)
				return
			end
			-- gravity
			velocity[2]-=0.25
			v_add(pos,velocity)
			-- find ground
			local newy, _ <close> =_ground:find_face(pos)
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
				local dist=v_dist(pos,_plyr.pos)
				if dist<params.radius then
					params.effect(self)
					-- stay on game?
					if not params.stay then
						return
					end
				end
			end

			if params.particle and rnd()>0.1 then
				lib3d.spawn_particle(params.particle, pos)
			end
	
			m_translate(self.m,pos)

			return true
		end
	}
end

local surfer_model={
	idle = models.PROP_SURFER,
	left = models.PROP_SURFER_LEFT,
	right = models.PROP_SURFER_RIGHT,
	p0 = vgroups.SURFER_LEFT_SKI,
	p1 = vgroups.SURFER_RIGHT_SKI
}
local skier_model={
	idle = models.PROP_SKIER,
	left = models.PROP_SKIER_LEFT,
	right = models.PROP_SKIER_RIGHT,
	p0 = vgroups.SKIER_LEFT_SKI,
	p1 = vgroups.SKIER_RIGHT_SKI
}

function make_npc(p,cam)
	local body=make_body(p,0)
	local up=vec3(0,1,0)
	local dir,boost=0,0
	local boost_ttl=0
	local jinx_ttl=90
	local model = pick{surfer_model, skier_model}
	local body_update=body.update
	body.id = model.idle
	local sfx=_ski_sfx:copy()
	_sounds[sfx] = true
	sfx:play(0)

	-- all jinxes
	local jinxes={
		-- 1: dynamite
		function(pos)
			add(_actors,make_jinx(models.PROP_DYNAMITE, pos, vec3(0,2,-0.1), {
				particle=1,
				radius=3,
				ttl=time()+3,
				die=function(self)
					screen:shake()
					_dynamite_sfx:play(1)
					for i=1,5+rnd(3) do
						add(_actors,make_smoke_trail(self.pos))
					end
					do_async(function()
						-- spawn a rolling ball on player :)
						if _plyr then
							add(_actors,make_rolling_ball(_plyr.pos[1]/4))
						end
					end)
				end,
				-- self = jinx instance
				effect=function(self)
					_dynamite_sfx:play(1)
					for i=1,5+rnd(3) do
						add(_actors,make_smoke_trail(self.pos))
					end
					if _plyr then _plyr.dead=true end
				end}
			))
		end,
		-- 2: reverse!
		function(pos)
			add(_actors,make_jinx(models.PROP_INVERT, pos, vec3(0,2,-0.1), {
				radius=2,
				effect=function()
					if _plyr then 
						_invert_sfx:play(1)
						_plyr.invert_ttl=30 
					end
				end}
			))
		end,
		-- 3: coin!
		function(pos)
			add(_actors,make_jinx(models.PROP_COIN, pos, vec3(0,2,-0.1), {
				radius=2,
				effect=function()
					if _plyr then _plyr.on_coin(1) end
				end}
			))
		end,
		-- 4: log!
		function(pos)
			add(_actors,make_jinx(models.PROP_LOG, pos, vec3(0,2,-0.1), {
				radius=1.5,
				stay=true,
				effect=function()
					if _plyr then _plyr.dead=true end
				end}				
			))
		end,
		-- 5: cone!
		function(pos)
			add(_actors,make_jinx(models.PROP_CONE, pos, vec3(0,2,-0.1), {
				radius=1.25,
				stay=true,
				effect=function()
					if _plyr then _plyr.dead=true end
				end}				
			))
		end,
	}
	-- jinxes distribution over time
	local jinxes_stats={3,2,3,2,3,5,2,5,5,3,3,4,2,4,2,2,1,1,1,1,1}
	local start_t=time()

	-- distance to player
	body.dist = 0
	body.update=function(self)
		local pos=self.pos

		-- crude ai control!
		local _,angle=self:get_pos()
		local slice=_ground:get_track(pos)
		if slice.z>38*4 or slice.z<4 then
			_sounds[sfx] = nil
			sfx:stop()
			-- kill npc
			self.dead=true
			return
		end

		if _plyr then
			if v_dist(pos,_plyr.pos)<1.5 then
				_npc_hit:play(1)
				_plyr.dead=true
			end

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
			-- 3 minutes of difficulty ramp up
			local rating=min(1,(time()-start_t)/180)
			if self.on_ground and dist>24 and jinx_ttl<0 then
				local n=#jinxes_stats
				do_async(function()
					-- pick up jinxes according to time played
					local id=jinxes_stats[flr(rnd(n * rating))+1]
					jinxes[id](v_clone(pos))
				end)
				jinx_ttl = 90 + lerp(90,45,rating) * lib3d.seeded_rnd()
			end
			self.dist = dist
	
			-- make npc slightly faster than player
			body:perma_boost(rating * 0.2)
		end

		local da=slice.angle+angle
		-- transition to "up" when going left/right
		dir=lerp(dir,da+angle,0.6)
		if dir<-0.02 then
			self.id = model.right
		elseif dir>0.02 then
			self.id = model.left
		else
			self.id = model.idle
		end
		self:steer(da/2)

		self:integrate()

		-- call parent	
		body_update(self)

		-- create orientation matrix
		local newy,newn <close> =_ground:find_face(pos)
		v_move(up,newn,0.3)
		local _,angle=self:get_pos()
		local m=make_m_from_v_angle(up,angle)
		m_translate(m,pos)

		-- spawn particles
		if self.on_ground and abs(dir)>0.04 and rnd()>0.1 then
			lib3d.spawn_particle(0, m, model.p0, model.p1)
		end

		-- update sfx
		local volume=self.on_ground and 1 or 0
		-- distance to camera
		local dist=v_dist(self.pos,cam.pos)
		if dist<4 then dist=4 end
		sfx:setVolume(volume*4/dist)
		sfx:setRate(1-abs(da/2))

		self.m = m
		return true
	end

	-- wrapper
	return body
end

function make_smoke_trail(pos, vel)
	local angle=rnd()
	local s,c=cos(angle),sin(angle)	
	local velocity=vec3(8*c,16+rnd(8),8*s)
	if vel then v_add(velocity,vel,0.5) end
	local ttl=flr(rnd(5))
	
	return {
		pos=vec3(pos[1],pos[2]+2,pos[3]),
		update=function(self)
			velocity[2]-=1
			v_add(self.pos,velocity,0.5/30)
			local y, _ <close> =_ground:find_face(self.pos)
			-- edge case - too close to map borders
			if not y then return end
			-- below ground?
			if y>self.pos[2] then				
				return
			end
			ttl+=1
			if ttl%2==0 then
				lib3d.spawn_particle(pick{0,1}, self.pos)
			end
			return true
		end
	}
end

function make_snowball(pos)
	local body=make_body(pos)
	body.id = models.PROP_SNOWBALL_PLAYER
	local body_update=body.update
	local base_angle=rnd()
	local angle=rnd()
	local n=vec3(cos(angle),0,sin(angle))
	-- hp
	local hp=3
	local jump_force = vec3()
	body.pre_update=function(self)		
		-- physic update
		self:integrate()
		body_update(self)		
	end
	body.hit=function(self,force)
		angle=rnd()
		n[1] = cos(angle)
		n[3] = sin(angle)
		-- fly a bit :)
		jump_force[2] = lerp(25,50,rnd())
		body:apply_force_and_torque(jump_force,0)

		hp-=1
		if not self.dead and (hp<0 or force) then
			self.dead = true
			local vel=body:get_velocity()
			for i=1,5+rnd(3) do
				add(_actors,make_smoke_trail(self.pos,vel))
			end
		end
	end
	body.update=function(self)
		if self.dead then return end
		local pos=self.pos
		-- update
		local newy,newn <close> =_ground:find_face(pos)
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
		--next_state(vec3_test)
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
	local daily
	local daily_y_offset_target,daily_y_offset=0,0

	local panels={
		{state=play_state,loc=vgroups.MOUNTAIN_GREEN_TRACK,help="Chill mood?\nEnjoy the snow!",daily=false,params={
			hp=3,
			name="Marmottes",
			music="2-AlpineAirtime",
			slope=1.5,
			twist=2.5,
			num_tracks=2,
			tight_mode=0,
			props_rate=0.90,
			track_type=0,
			min_cooldown=30*2,
			max_cooldown=30*4}},
		{state=play_state,loc=vgroups.MOUNTAIN_RED_TRACK,help=function()
			return "Death Canyon\nHow far can you go?\nBest: ".._save_state.best_2.."m"
		end,params={
			hp=1,
			name="Biquettes",
			music="4-TreelineTrekkin",
			dslot=2,
			slope=2,
			twist=4,
			min_boost=0,
			max_boost=1,
			boost_t=180,
			num_tracks=1,
			tight_mode=1,
			props_rate=1,
			track_type=1,
			min_cooldown=2,
			max_cooldown=6}},
		{state=race_state,loc=vgroups.MOUNTAIN_BLACK_TRACK,help=function()
			return "Special Ski Course\nStay alert!\nBest: ".._save_state.best_3.."m"
		end,
			daily=false,params={
			hp=1,
			name="Chamois",
			music="3-BackcountryBombing",
			dslot=3,
			slope=2,
			twist=6,
			num_tracks=1,
			tight_mode=0,
			props_rate=0.96,
			track_type=2,
			min_cooldown=4,
			max_cooldown=12}},
		{state=shop_state,loc=vgroups.MOUNTAIN_SHOP,help=function()
			return "Buy gear!\n$".._save_state.coins
		end ,params={name="Shop"},transition=false,daily=false},		
	}
	local sel,blink=0,false
	-- background actors
	local actors={}

	-- menu cam	
	local look_at = vec3(0,0,1)
	local cam=make_cam(vec3(0,0.8,-0.5))

	-- background music
	_music = playdate.sound.fileplayer.new("sounds/1-PowderyPolka")
	_music:play(0)

	_ski_sfx:stop()
	lib3d.clear_particles()

	-- menu to get back to selection menu
	local menu = playdate.getSystemMenu()
	menu:removeAllMenuItems()
	local menuItem, error = menu:addMenuItem("start menu", function()
			_futures={}
			if _music then
				_music:stop()
				_music = nil
			end
			next_state(menu_state)
	end)	
	local menuItem, error = menu:addCheckmarkMenuItem("flip crank", _save_state.flip_crank, function(value)
		_save_state.flip_crank = value
	end)	

	local scale = 1
	local angle = angle or 0

	-- help text
	local help_y = 300
	local help_y_target = 220
	add(actors,{
		id=models.PROP_MOUNTAIN,
		pos=vec3(0.5,0,1),
		update=function(self)
			local m
			if starting then
				local panel = panels[sel+1]
				-- project location into world space
				local world_loc = m_x_v(self.m,panel.loc)
				local dir=vec3(
					world_loc[1] - cam.pos[1],
					0,
					world_loc[3] - cam.pos[3])
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
			m_translate(m,self.pos)

			self.m=m
		end
	})
	
	-- snowflake
	local snowflake=gfx.image.new(2,2)
	snowflake:clear(gfx.kColorWhite)

	-- starting position (outside screen)
	gfx.setFont(largeFont[gfx.kColorBlack])
	for i,p in pairs(panels) do
		p.x = -gfx.getTextSize(p.params.name) - 16
		p.t = time()
		p.x_start = p.x
	end

	return
		-- update
		function()
			if playdate.buttonJustReleased(playdate.kButtonUp) then sel-=1 _button_click:play(1) end
			if playdate.buttonJustReleased(playdate.kButtonDown) then sel+=1 _button_click:play(1) end
			sel=mid(sel,0,#panels-1)
			
			-- daily mode?
			if playdate.buttonJustReleased(playdate.kButtonRight) then daily=not daily _button_click:play(1) end

			-- help?
			if playdate.buttonJustReleased(playdate.kButtonB) then
				_music:setVolume(0,0,1)
				next_state(help_state, angle)
			end

			if not starting and playdate.buttonJustReleased(playdate.kButtonA) then
				-- sub-state
        starting=true
				for _,p in pairs(panels) do
					p.t = time()
				end
				help_y_target = 300

				do_async(function()
					-- fade out music
					_music:setVolume(0,0,1)
					_button_go:play(1)
					local panel = panels[sel+1]
					-- project location into world space
					local world_loc = m_x_v(actors[1].m,panel.loc)		
					for i=1,30 do
						scale = lerp(scale,1.8,0.1)
						v_move(cam.pos, vec3(0.5,0.5,-0.75),0.1)
						v_move(look_at,world_loc,0.1)
						coroutine.yield()
					end
					_music:stop()
					-- set global random seed
					srand(playdate.getSecondsSinceEpoch())					
					local p=panels[sel+1]
					-- daily mode?
					if panel.daily~=false and daily then
						-- make sure all players get the same seed all over the world!!
						local t = playdate.getGMTTime()
						local date = string.format("%04d/%02d/%02d",t.year,t.month,t.day)
						p.params.r_seed = lib3d.DEKHash(date)
						-- keep date string
						p.params.daily = date
					else
						p.params.r_seed = nil
						p.params.daily = nil
					end

					if p.transition==false then
						next_state(p.state,v_clone(cam.pos),v_clone(look_at),scale,angle)
					else
						--
						p.params.state = p.state
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

			if sel_panel.daily~=false then
				daily_y_offset_target = 12
			else
				daily_y_offset_target = 0
			end

			for _,p in pairs(panels) do
				-- target
				local x = p == sel_panel and 14 or 10
				local dt=0.5
				local t=min(dt,time()-p.t)
				if starting then
					t=dt-t
				end
				p.x = playdate.easingFunctions.inOutBack(t,p.x_start,x-p.x_start,dt)
			end
		
			help_y = lerp(help_y,help_y_target,0.75)
			--
			cam:look(look_at)
			frame_t+=1
		end,
		-- draw
		function()
			cls(gfx.kColorWhite)
			
			for _,a in pairs(actors) do
				lib3d.add_render_prop(a.id,a.m)
			end
			lib3d.render_props(cam.pos,cam.m)

			if not starting then
				_game_title_anim:drawImage((frame_t%#_game_title_anim)+1,10,6)
				print_small("by Freds72&ridgek",10,54)
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
			daily_y_offset=lerp(daily_y_offset,daily_y_offset_target,0.4)			
			for i=1,#panels do
				local panel = panels[i]
				local name = panel.params.name
				if panel==sel_panel then
					if panel.daily~=false and daily then
						name = name.."#"
					end
					
					-- text length
					gfx.setFont(largeFont[gfx.kColorBlack])
					local sel_w = gfx.getTextSize(name)
					gfx.setPattern(_50pct_pattern)
					gfx.fillRect(0,y+32,sel_w + panel.x + 16,2)
					gfx.setColor(gfx.kColorBlack)
					gfx.fillRect(0,y,sel_w + panel.x + 16,32)
				
					print_regular(name, panel.x, y,gfx.kColorWhite)

					if panel.daily~=false then
						local daily_help = "➡️ for daily"
						if daily then
							daily_help = "➡️ for random"
						end
						print_small(daily_help, panel.x,y + 30,gfx.kColorBlack)
						y += daily_y_offset
					end
				else
					print_regular(name, panel.x, y, gfx.kColorBlack)
				end
				y += 28
			end
			
			print_small("Ⓑ directions",10,help_y, gfx.kColorBlack)
		end		
end

function help_state(angle)
	local mute = gfx.image.new("images/help-mute")
	local speak_a = gfx.image.new("images/help-a")
	local speak_o = gfx.image.new("images/help-o")
	local speak_a_sfx = playdate.sound.sampleplayer.new("sounds/speak-a")
	local speak_o_sfx = speak_a_sfx:copy()

	local vocalizer = {		
		[' ']=mute,
		b=speak_o,
		d=speak_o,
		e=speak_o,
		o=speak_o,
		k=speak_o,
		u=speak_o,
		r=speak_o,
	}

	local help_msgs={
		{
			title="How to play?",
			text="⬆️⬇️ select level - Ⓐ start\nAvoid hazards and collect $\nGet more $ with tricks\nDaily mode: use QR code to post score"
		},
		{
			title="D-pad skiing",
			text="⬅️ left - right ➡️\nⒶ jump\nⒷ restart (long press)"
		},
		{
			title="Cranked skiing",
			text="🎣 direction\nⒷ jump\nⒶ restart (long press)"
		},
		{
			title="Menu button",
			text="start menu: back to main menu\nflip crank: invert crank input\nmask: choose from your collection"
		},
		{
			title="Credits",
			text="Engine+Graphics: Freds72\nMusic: ridgek\nFont: somepx.itch.io\nSfx: freesound:org (cc0)\nMany thanks to: Eli Piilonen, Scott Hall,\nJordan Carroll + all the discord folks!"
		},
	}
	local w,h=mute:getSize()
	local y=400
	-- active message
	local help
	local prompt=1
	local box_h=0
	local box_w=300
	-- capture screen
	local screen=gfx.getDisplayImage()

	do_async(function()
		local i=1
		while true do
			box_h=0
			help=nil
			local tmp=help_msgs[i]
			gfx.setFont(largeFont[gfx.kColorWhite])
			local title_w,title_h=gfx.getTextSize(tmp.title)
			gfx.setFont(smallFont[gfx.kColorWhite])
			local msg_w,msg_h=gfx.getTextSize(tmp.text)
			-- reserve space for next button
			msg_h += 40 + 12
			msg_w = max(msg_w,title_w) + 19
			wait_async(15)
			for i=1,15 do
				box_h=lerp(box_h,msg_h,0.7)
				box_w=lerp(box_w,msg_w,0.7)
				coroutine.yield()
			end
			box_h = msg_h
			box_w = msg_w
			help = tmp
			prompt = 1
			-- reading time!
			while not playdate.buttonJustReleased(playdate.kButtonA) do
				coroutine.yield()
			end
			_button_click:play(1)
			help = nil
			for i=1,15 do
				box_h=lerp(box_h,0,0.7)
				box_w=lerp(box_w,0,0.7)
				coroutine.yield()
			end
			box_h=0
			box_w=0
			i+=1			
			if i>#help_msgs then i=1 end
		end
	end)
	return
		-- update
		function()
			-- press b again: back
			if playdate.buttonJustReleased(playdate.kButtonB) then
				next_state(menu_state,angle)
			end
			y=lerp(y,240-h,0.8)
			prompt+=1
		end,
		-- draw
		function()
			screen:draw(0,0)
			gfx.setColor(gfx.kColorWhite)
			gfx.setStencilPattern(_50pct_pattern)
			gfx.fillRect(0,0,400,240)
			gfx.clearStencil()
					
			local speak_img = mute
			if help then
				local ch=prompt<=#help.text and string.sub(help.text,prompt,prompt)
				if ch then
					speak_img=vocalizer[ch] or speak_a
					if speak_img==speak_o then
						if not speak_o_sfx:isPlaying() then
							speak_o_sfx:play(1,0.9+0.5*rnd())
						end
					end
				end
			end
			speak_img:draw(400-w,y)

			if box_h>0 then
				gfx.setColor(gfx.kColorBlack)
				local box_t = 20
				local box_l = 64
				local box_b = box_t+box_h
				gfx.fillRect(box_l,box_t,box_w,box_h)
				if box_h>8 and box_w>32 then
					gfx.setPattern(_50pct_pattern)
					gfx.fillRect(box_l,box_b,box_w,2)
					gfx.setColor(gfx.kColorBlack)
					gfx.fillTriangle(295,184,box_l+2*box_w/3-16,box_b,box_l+2*box_w/3+16,box_b)

					if flr(time())%2==0 then print_small("Ⓐ next",box_l + box_w - 56,box_b - 20,gfx.kColorWhite) end
				end

				if help then
					print_regular(help.title, box_l+10, box_t, gfx.kColorWhite)
					print_small(string.sub(help.text,1,prompt), box_l+10, box_t + 32, gfx.kColorWhite)
				end
			end
		end
end

function menu_zoomout_state(cam_pos, look_at, scale, angle)
	-- menu cam	
	local cam=make_cam(cam_pos)

	local mountain_actor={
		id=models.PROP_MOUNTAIN,
		pos=vec3(0.5,0,1),
		update=function(self)
			local m=make_m_y_rot(angle)
			m[1]  = scale
			m[6]  = scale
			m[11] = scale

			m_translate(m,self.pos)
			self.m=m
		end
	}
	
	do_async(function()
		local s=scale
		for i=1,30 do
			local t=(i-1)/30
			scale=lerp(s,1,t)
			v_move(cam.pos,vec3(0,0.8,-0.5),0.2)
			v_move(look_at,vec3(0,0,1),0.2)
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
			
			lib3d.add_render_prop(mountain_actor.id,mountain_actor.m)
			lib3d.render_props(cam.pos,cam.m)

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
			-- don't already own item
			local is_valid = not _save_state[item.uuid] and _save_state.coins>=item.price
			if is_valid and playdate.buttonIsPressed(playdate.kButtonA) then
				action_ttl=min(30,action_ttl+1)
				if action_ttl==1 then
					_button_go:play(1)
				end
				if action_ttl==30 then
					action_ttl = 0
					_button_buy:play(1)
					do_async(function()
						-- commit basket instantly
						_save_state[item.uuid] = 1
						local start_coins = _save_state.coins
						_save_state.coins -= item.price
						local target_coins = _save_state.coins
						for i=0,29,3 do
							coins = flr(lerp(start_coins,target_coins,i/30))
							_coin_sfx:play(1)
							wait_async(3)
						end
						coins = _save_state.coins 
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
				if i~=selection then
					gfx.setStencilPattern(_50pct_pattern)
				else
					gfx.clearStencil()
				end
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
					local can_buy = _save_state.coins >= item.price
					local buy_text
					if action_ttl>0 then
						buy_text = "Keep pressing"
						for i=1,flr(action_ttl/10) do
							buy_text = buy_text .. "."
						end
					elseif can_buy then
						buy_text = "Buy".._input.action.glyph
					else
						buy_text = "Tricks for $!"
					end

		
					local action_y=action_ttl>0 and 2 or 0
					local w=gfx.getTextSize(buy_text)
					gfx.setPattern(_50pct_pattern)
					gfx.fillRect(button_x,y+36,w+24,2)
					gfx.setColor(gfx.kColorBlack)
					gfx.fillRect(button_x,y,w+24,36)
		
					if not can_buy then
						gfx.setStencilPattern(_50pct_pattern)
					end	
					print_regular(buy_text,10+button_x,y+2+action_y,gfx.kColorWhite)
					gfx.clearStencil()

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
	return function(lane,row,cam)
		local pos=vec3(x or (lane+0.5)*4,-16,row*4 - 2)
		local y, _ <close> = _ground:find_face(pos)
		pos[2]=y
		-- sfx?
		local sfx=_ENV[sfx_name]
		if sfx then
			sfx = sfx:copy()
			_sounds[sfx] = true
			sfx:play(0)
			sfx:setVolume(0)
		end	
		return {
			id=id,
			pos=pos,
			m=make_m_from_v(v_up),
			update=function(self)
				local pos=self.pos
				-- out of landscape?
				if pos[3]<0.5 then
					if sfx then
						_sounds[sfx] = nil
						sfx:stop()
					end
					return 
				end

				-- update pos?
				if update then					
					update(pos)
				end
				-- if sound
				if sfx then
					local d=v_dist(pos,cam.pos)
					if d<16 then d=16 end
					sfx:setVolume(16/d)
				end
				m_translate(self.m,pos)
				return true
			end
		}
	end	
end

-- 
function make_rolling_ball(lane)
	local y_velocity,z_velocity = 0,1.5
	local y_force,on_ground = 0
	local base_angle = rnd()
	local pos=vec3((lane+0.5)*4,0,0.5)
	local prev_pos=v_clone(pos)
	-- helper
	local function v2_sqrlen(x,z,b)
		local dx,dz=b[1]-x,b[3]-z
		return dx*dx+dz*dz
	end
	return {
		id=models.PROP_SNOWBALL,
		pos=pos,
		warning=flr(lane),
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
				y_force = 0.5
			end
			y_velocity+=y_force*0.5/30
			v_copy(pos,prev_pos)
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
			local newy,newn <close> =_ground:find_face(pos)
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
end

-- custom provides additional commands (optional)
function make_command_handlers(custom)
	return setmetatable({
		-- snowball!!
		B=make_rolling_ball,
		-- skidoo
		K=function(lane,_,cam)
			local pos=vec3((lane+0.5)*4,-16,_ground_height-2)
			-- sfx
			local sfx = _skidoo_sfx:copy()
			_sounds[sfx] = true
			sfx:setOffset(rnd())
			sfx:play(0)
			return {
				id = models.PROP_SKIDOO,
				pos = pos,
				update=function(self)
					local pos=self.pos
					-- move upward!!
					pos[3]-=0.25

					-- update
					local newy,newn <close> =_ground:find_face(pos)
					-- out of bound: kill actor
					if not newy then 
						_sounds[sfx] = nil
						sfx:stop()
						return 
					end
					
					-- orientation matrix
					local m = make_m_from_v(newn)
					m[13]=pos[1]
					m[14]=newy
					m[15]=pos[3]
					self.m = m
		
					-- spawn particles
					if rnd()>0.2 then
						local v=m_x_v(m,v_lerp(vgroups.SKIDOO_LTRACK,vgroups.SKIDOO_RTRACK,rnd()))					
						lib3d.spawn_particle(0, v)
					end

					-- distance to camera
					local dist=v_dist(self.pos,cam.pos)
					if dist<32 then dist=32 end
					sfx:setVolume(32/dist)

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
			local ny, _ <close> =_ground:find_face(pos)
			-- wooble over ground
			pos[2] = ny + 16 + 2*cos(time())
		end)
	},{__index=custom or {}})
end

-- -------------------------
-- bench mode
function bench_state()
	_ground = make_ground({slope=2,twist=4,num_tracks=1,tight_mode=0,props_rate=0.87,track_type=0,min_cooldown=8,max_cooldown=12})
	print("ground ok")
	local pos=vec3(15.5*4,-16,12.5*4)
	print("new pos")
	local newy,newn=_ground:find_face(pos)
	print("location: "..newy)
	pos[2]=newy+1	
	local cam=make_cam()
	print("cam ok")
	local u=vec3(0,0.9,1)
	v_normz(u)
	print("normz ok")
	cam:track(pos,0,u,1,1)
	
	for j=0,3 do
		local s=""
		for i=0,3 do
			s=s.." "..cam.m[j*4+i+1]
		end
		print(s)
	end

	return 
		-- update
		function() end,
		-- draw
		function()
			_ground:draw(cam)
		end
end

function vec3_test()
	local vecs={}
	return
		-- update
		function()
			if playdate.buttonJustReleased(playdate.kButtonA) then
				add(vecs,lib3d.Vec3.new(rnd(),rnd(),rnd()))
			end
			if playdate.buttonJustReleased(playdate.kButtonB) then
				table.remove(vecs)
			end
		end,
		-- draw
		function()
			cls(gfx.kColorWhite)
			local out=lib3d.Vec3.new()
			local y=0
			for i=1,#vecs do
				local v=vecs[i]
				print_small(v[1].." "..v[2].." "..v[3],2,y,gfx.kColorBlack);
				lib3d.Vec3.add(out,v,-1)
				y+=11
			end
			print_small("--------------------------",2,y,gfx.kColorBlack);
			y+=11
			print_small(out[1].." "..out[2].." "..out[3],2,y,gfx.kColorBlack);
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
			_trick_sfx:play(1)
			add(bonus,{x=-60,t=0,msg=msg})
			-- 3s to get another trick
			combo_ttl=90
		end
	end

	-- reset particles
	lib3d.clear_particles()

	-- start over
	local patterns
	_actors,_ground,patterns={},make_ground(params)

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

	-- starting commands
	for j=1,40 do
		local commands=patterns[j]
		for i=1,#commands do
			local c=string.sub(commands,i,i)
			local cmd=handlers[c]
			if cmd then
				local actor=cmd(i-1,j-1,cam)
				if actor then
					add(_actors,actor)
				end
			end
		end
	end

	-- music & sfx
	if params.music then
		_music = playdate.sound.fileplayer.new("sounds/"..params.music)
		_music:play(1)
	end

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

	local track_name = params.name
	gfx.setFont(smallFont[gfx.kColorWhite])
	local track_name_x = 399 - gfx.getTextSize(track_name)
	local track_icon = params.daily and _calendar_icon or _mountain_icon

	-- startup time
	local start_t=time()
	return
		-- update
		function()
			cam:update()
			-- 
			if _plyr then
				-- boost?
				if params.boost_t then
					_plyr:perma_boost(lerp(params.min_boost,params.max_boost,min((time()-start_t)/params.boost_t),1))
				end
				_plyr:control()	
				_plyr:integrate()
				_plyr:update()
			end

			-- adjust ground
			local slice_id,commands,offset = _ground:update(_tracked.pos)

			if _plyr then
				v_add(_plyr.pos,offset)
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
							local multi=min(#tmp,5)
							bonus_coins = 1<<(multi-1)
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
							local actor=cmd(i-1,40,cam)
							if actor then
								add(_actors,actor)
							end
						end
					end
					prev_slice_id = slice_id
				end
			end

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
				cam:track(pos,a,up)

				if _plyr.dead then
					_ski_sfx:stop()

					-- todo: game over music
					screen:shake()

					if _music then _music:stop() _music=nil end
					-- latest score
					next_state(stop_sounds_state,plyr_death_state,cam,pos,flr(_plyr.distance),total_tricks,params)
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
						if _music then _music:setVolume(0.5,0.5) end
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
				-- skip non 3d model actors
				if a.id and (not a.blinking or frame_t%2==0) then
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
				print_small(track_name,track_name_x,0,text_color)
				track_icon:draw(track_name_x - track_icon:getSize() - 2,3)

				-- chill mode?
				if best_distance then
					-- total distance
					print_regular(flr(_plyr.distance).."m",nil,-7,text_color)
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
	local frame_t=0
	local cam=make_cam()
	local make_helo=function(lane,row,cam)
		local dropped
		local pos=vec3((lane+0.5)*4,-16,row*4-2)
		local y, _ <close> =_ground:find_face(pos)
		pos[2]=y+24
		-- sfx?
		_sounds[_helo_sfx] = true
		_helo_sfx:play(0)
		_helo_sfx:setVolume(0)

		return {
			id=models.PROP_HELO,
			pos=pos,
			m=make_m_y_rot(0.5),
			update=function(self)
				local pos=self.pos
				-- update pos?
				pos[3]+=0.9
				-- get current height
				local ny, _ <close> =_ground:find_face(pos)
				-- out of landscape?
				if not ny then
					_sounds[_helo_sfx] = nil
					_helo_sfx:stop()
					return 
				end
				if not dropped then
					local slice=_ground:get_track(pos)
					pos[1]=lerp(pos[1],(slice.xmin+slice.xmax)/2,0.2)
				end

				-- wooble over ground
				pos[2] = lerp(pos[2], ny + 18 + cos(time()),0.2)

				if not dropped and _plyr and pos[3]>_plyr.pos[3]+38 then
					-- avoid reentrancy
					dropped=true
					do_async(function()
						npc=make_npc(pos,cam)
						add(_actors,npc)
					end)
				end
				local d=v_dist(pos,cam.pos)
				if d<16 then d=16 end
				_helo_sfx:setVolume(16/d)

				m_translate(self.m,pos)
				return true
			end
		}
	end

	-- 
	local faster_messages={
		"faster!",
		"where are you?",
		"that's skiing!",
		"swwoooshh!",
		"watch out!",
		"oops!"
	}
	local bye_messages={
		"bye!",
		"game over!",
		"see you!"
	}	

	-- custom command handlers
	params.commands = {
		-- Nelicopter :)
		n = make_helo
	}
	-- use main "play" state for core loop
	params.cam = cam
	local play_update,play_draw=play_state(params)

	-- npc message loop
	local message
	do_async(function()
		while _plyr do
			-- wait for npc to be live
			if npc then
				if npc.dead then return end
				if npc.dist>30 then	
					local msgs=npc.dist>70 and bye_messages or faster_messages
					message=pick(msgs)
					wait_async(90)
				end			
				message=nil
				wait_async(90+rnd(180))
			end
			coroutine.yield()
		end
	end)		

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
				end
			end
			frame_t+=1
		end,
		-- draw
		function()
			play_draw()
			if message and npc then	
				gfx.setFont(smallFont[gfx.kColorWhite])
				local sw,sh=gfx.getTextSize(message)
				sw += 4
				sh += 4
				-- project npc pos
				local p <close> = m_x_v(cam.m,npc.pos)
				local x,y,w=cam:project2d(p)
				x=mid(0,x - sw/2,399 - sw)
				y=mid(48,y-7*w,240-sh)
				gfx.setColor(gfx.kColorBlack)
				gfx.fillRect(x,y,sw,sh)
				print_small(message,x+2,y,gfx.kColorWhite)
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
	local hit_ttl = 0
	-- limit free rolling to 10s
	local ttl = 10*30
	-- save records (if any)
	if params.dslot and total_distance>_save_state["best_"..params.dslot] then
		_save_state["best_"..params.dslot] = flr(total_distance)
	end
	
	-- stop ski sfx
	_ski_sfx:stop()
	_sounds[_snowball_sfx] = true
	_snowball_sfx:play(0)

	-- generate QR code if daily
	local qrcode_timer
	local qrcode_img
	if params.daily then
		local hash = lib3d.DEKHash(string.format("b437f227-eece-46b2-a81d-70a8c8224a0f-%s%i",params.daily,total_distance))
		local url = string.format("https://freds72.github.io/snow-scores.html?h=%X&m=%i&d=%s&t=%i",hash,flr(total_distance),params.daily,params.track_type)
		qrcode_timer = gfx.generateQRCode(url, 70, function(img, error)
			qrcode_img = img
			if error then print("Unable to generate QR code: "..error) end
		end)
	end

	return
		-- update
		function()	
			-- for qr code generation
			playdate.timer.updateTimers()

			if not snowball.dead then
				snowball:pre_update()	
			end
			prev_update()

			msg_y=lerp(msg_y,msg_tgt_y[msg_tgt_i+1],0.08)
			gameover_y=lerp(gameover_y,48+8*sin(time()/4),0.06)

			if abs(msg_y-msg_tgt_y[msg_tgt_i+1])<1 then msg_tgt_i+=1 end
			if msg_tgt_i>#msg_tgt_y-1 then msg_tgt_i=0 active_msg=(active_msg+1)%2 end

			text_ttl-=1
			hit_ttl-=1
			ttl-=1
			local p=snowball.pos			
			if not snowball.dead and ((hit_ttl<0 and text_ttl<0 and _ground:collide(p,1)==1) or ttl<0) then
				-- force kill?
				snowball:hit(ttl<0)
				pick(_treehit_sfxs):play()
				if snowball.dead then
					_sounds[_snowball_sfx] = nil
					_snowball_sfx:stop()
				end
				active_text,text_ttl=pick(text),10
				turn_side=-turn_side
				screen:shake()
				hit_ttl=15
			end
			-- keep camera off side walls
			local slice=_ground:get_track(cam.pos)
			cam.pos=vec3(lerp(cam.pos[1],mid(p[1],slice.xmin+4,slice.xmax-4),0.1),p[2],p[3]+16)
			cam:look(p)

			if playdate.buttonJustReleased(_input.back.id) then
				if qrcode_timer then qrcode_timer:remove() qrcode_timer = nil end
				next_state(stop_sounds_state, zoomin_state, menu_state)
			end
			if playdate.buttonJustReleased(_input.action.id) then
				if qrcode_timer then qrcode_timer:remove() qrcode_timer = nil end
				next_state(stop_sounds_state, zoomin_state,params.state,params,90)
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

			if qrcode_img then
				local w,h = qrcode_img:getSize()
				qrcode_img:draw(400-w,240-h)
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
				if _music then
					_music:stop()
					_music = nil
				end
				-- reset game + stop game sounds
				next_state(stop_sounds_state,params.state,params,90)
			elseif not playdate.buttonIsPressed(_input.back.id) then
				-- back to game ("unpause")
				if _music then
					_music:setVolume(1,1)
				end
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
		-- demo: don't save!
		-- playdate.datastore.write(_save_state)
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
	_trick_sfx = playdate.sound.sampleplayer.new("sounds/trick")
	_eagle_sfx = playdate.sound.sampleplayer.new("sounds/eagle_loop")
	_ufo_sfx = playdate.sound.sampleplayer.new("sounds/ufo_loop")
	_helo_sfx = playdate.sound.sampleplayer.new("sounds/helo_loop")
	_coin_sfx = playdate.sound.sampleplayer.new("sounds/coin")
	_checkpoint_sfx = playdate.sound.sampleplayer.new("sounds/checkpoint")
	_treehit_sfxs = {
		--playdate.sound.sampleplayer.new("sounds/tree-hit-1"),
		playdate.sound.sampleplayer.new("sounds/tree-hit-1"),
		playdate.sound.sampleplayer.new("sounds/tree-hit-2"),
		playdate.sound.sampleplayer.new("sounds/tree-hit-3"),
		}
	_button_click = playdate.sound.sampleplayer.new("sounds/ui_button_click")
	_button_go = playdate.sound.sampleplayer.new("sounds/ui_button_go")
	_button_buy = playdate.sound.sampleplayer.new("sounds/ui_button_buy")
	_boost_sfx = playdate.sound.sampleplayer.new("sounds/boost")
	_invert_sfx = playdate.sound.sampleplayer.new("sounds/invert_jinx")
	_dynamite_sfx = playdate.sound.sampleplayer.new("sounds/dynamite_jinx")
	_skidoo_sfx = playdate.sound.sampleplayer.new("sounds/skidoo")
	_snowball_sfx = playdate.sound.sampleplayer.new("sounds/snowball")

	_npc_hit = playdate.sound.sampleplayer.new("sounds/npc_hit")

	_game_over = gfx.image.new("images/game_over")
	_dir_icon = gfx.image.new("images/checkpoint_lock")
	_mountain_icon = gfx.image.new("images/mountain_icon")
	_calendar_icon = gfx.image.new("images/calendar_icon")

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
	local patterns = lib3d.make_ground(gp)

	-- interface with C library
	return {		
		_gp = gp,
		get_pos=function(self)
			return lib3d.get_start_pos()
		end,
		update=function(self,p)
			return lib3d.update_ground(p)
		end,
		draw=function(self,cam,blink_mask)
			lib3d.render_ground(cam.pos,cam.angle%1,blink_mask,cam.m)
		end,
		draw_props=function(self,cam)
			lib3d.render_props(cam.pos,cam.m)
		end,
		find_face=function(self,p)
			return lib3d.get_face(p)
		end,
		get_track=function(self,p)
			local xmin,xmax,z,checkpoint,angle = lib3d.get_track_info(p)
			return {
				xmin=xmin,
				xmax=xmax,
				z=z,
				is_checkpoint=checkpoint,
				angle=angle
			}
		end,
		clear_checkpoint=function(self,p)
			lib3d.clear_checkpoint(p)
		end,
		-- find all actors within a given radius from given position
		collide=function(self,p,r)
			local hit_type = lib3d.collide(p,r)
			return hit_type>0 and hit_type
		end,
		update_snowball=function(self,p,r)
			lib3d.update_snowball(p,r)
		end,
		add_render_prop=function(self,id,m)
			lib3d.add_render_prop(id,m)
		end
	},patterns
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
  -- playdate.drawFPS(0,228)
end