-- snow!
-- by @freds72

-- game globals
import 'CoreLibs/graphics'
import 'CoreLibs/nineslice'
local gfx = playdate.graphics
local font = gfx.font.new('font/whiteglove-stroked')
local panelFont = gfx.font.new('font/Roobert-10-Bold')
local panelFontFigures = gfx.font.new('font/Roobert-24-Medium-Numerals-White')
local _dither={}
local _angle=0

-- some "pico-like" helpers
function cls(c)
  gfx.setColor(c or gfx.kColorBlack)
  gfx.fillRect(0, 0, 400, 240)
end
local sin = function(a)
  return -math.sin(2*a*math.pi)
end
local cos = function(a)
  return math.cos(2*a*math.pi)
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

-- fade ramp helpers
function whiteout_async(delay,reverse)
  wait_async(delay)
  -- todo: fade to white/black using dither
end

-->8
-- main engine
-- global vars
local actors,ground,plyr,cam={}

-- camera clip planes
local k_far,k_near,k_right,k_left,z_near=0,2,4,8,0.2

-- camera
function make_cam()
	--
	local up={0,1,0}

	-- screen shake
	local shkx,shky=0,0
	camera()

	local clouds,clouds2={},{}
	for i=-8,8 do
		add(clouds,{i=i,r=max(16,rnd(32))})
		add(clouds2,{i=i,r=max(24,rnd(38))})
	end

	return {
		pos={0,0,0},
		angle=0,
		m=make_m_from_v_angle(v_up,0),
		shake=function()
			shkx,shkx=min(4,shkx+rnd(8)),min(4,shky+rnd(8))
		end,
		update=function()
			shkx=shkx*-0.7-rnd(0.2)
			shky=shky*-0.7-rnd(0.2)
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
		end,
		project_poly=function(self,p,c0)
      local verts={}
      for i=1,#p do
        local x,y=self:project2d(p[i])
        verts[#verts+1]=x
        verts[#verts+1]=y
      end
      gfx.fillPolygon(table.unpack(verts))
		end,
		draw_horizon=function(self,ground_color,sky_color)
			cls(gfx.kColorWhite)
			
			-- cam up in world space
			local n=m_up(self.m)
			-- a bit ugly
			v_scale(n,-1)

			-- intersection between camera eye and up plane (world space)
			local x0,y0=self:project2d({0,-n[3]/n[2],1})

			-- horizon 'normal'
			n[3]=0
			v_normz(n)
			-- spread clouds
			v_scale(n,32)
			local u,v=n[1],n[2]
			
			-- sun
			_sun:draw(x0-3*u+2*v-16,y0+3*v+2*u-16)

			-- horizon intersections (clouds)
			local xl,yl,xr,yr=0,y0-u*x0/v,400,y0+u*(400-x0)/v			
			-- yl: min
			-- yr: max
			if yl>yr then xl,yl,xr,yr=xr,yr,xl,yl end
			
			gfx.setPattern(_dither[6])
			for _,c in pairs(clouds2) do
				gfx.fillCircleAtPoint(x0+c.i*v,y0+c.i*u,c.r,6)
			end
			gfx.setColor(gfx.kColorBlack)
			for _,c in pairs(clouds) do
				gfx.fillCircleAtPoint(x0+c.i*v,y0+c.i*u,c.r)
			end

			gfx.setColor(gfx.kColorBlack)
			gfx.fillRect(0,yr,400,240-yr)
			gfx.fillTriangle(xl,yl,xr,yr,xl,yr)
		end
	}
end

-- basic gravity+slope physic body
function make_body(p)
	-- last contact face
	local up,oldf={0,1,0}

	local velocity,angularv,forces,torque={0,0,0},0,{0,0,0},0

	local angle,steering_angle,on_air_ttl=0,0,0

	local g={0,-4,0}
	return {
		pos=v_clone(p),
		on_ground=false,
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
			return u
		end,
		apply_force_and_torque=function(self,f,t)
			-- add(debug_vectors,{f=f,p=p,c=11,scale=t})

			v_add(forces,f)
			torque=torque+t
		end,
		integrate=function(self)
			-- gravity and ground
			self:apply_force_and_torque(g,0)
			-- on ground?
			if self.on_ground==true then
				local n=v_clone(up)
				v_scale(n,-v_dot(n,g))
				-- slope pushing up
				self:apply_force_and_torque(n,0)
			end

			-- update velocities
			v_add(velocity,forces,0.5/30)
			angularv=angularv+torque*0.5/30

			-- apply some damping
			angularv=angularv*0.86
			local f=self.on_ground==true and 0.08 or 0.01
			-- some friction
			--v_scale(velocity,1-f)
			v_add(velocity,velocity,-f*v_dot(velocity,velocity))
			
			-- update pos & orientation
			--local x,z=self.pos[1],self.pos[3]
			v_add(self.pos,velocity)
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
			if self.on_ground==true and v_len(velocity)>0.001 then

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
					sa=sa*60*grip

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
			elseif self.on_ground==false then
				self:apply_force_and_torque({0,0,0},-steering_angle/4)
			end			
		end,
		update=function(self)
			steering_angle=steering_angle*0.8
			on_air_ttl=on_air_ttl-1

			-- find ground
			local pos=self.pos

			local newf,newpos,gps=ground:find_face(pos)
			if newf then
				oldf=newf
			end
			self.gps=gps-angle
			-- stop at ground
			self.on_ground=false
			local tgt_height=1
			if newpos and pos[2]<=newpos[2] then
				up=newf.n
				tgt_height=pos[2]-newpos[2]
				pos[2]=newpos[2]				
				self.on_ground=true
				-- big enough jump?
				if on_air_ttl>5 then sfx(12) on_air_ttl=0 sfx(11) end
			end
			if self.on_ground==false then on_air_ttl=10 end

			self.height=lerp(self.height,tgt_height,0.4)

			-- todo: alter sound
			-- mix: volume for base pitch
			-- alter with "height" to include slope "details"			
		end
	}	
end

function make_plyr(p,params)
	local body,hp=make_body(p),3

	local body_update=body.update

	local hit_ttl,jump_ttl,jump_pressed=0,0
	
	local spin_angle,spin_prev=0

	-- timers + avoid free airtime on drop!
	local t,bonus,total_t,total_tricks,reverse_t,air_t=params.total_t,{},0,0,0,-60
	local whoa_sfx={5,6,7}

	-- time bonus cannot be negative!
	local function add_time_bonus(tb,msg)
		t=t+tb*30
		local ttl=12+rnd(12)
		add(bonus,{t="+"..tb.."s",msg=msg,x=rnd(5),ttl=ttl,duration=ttl})
		-- trick?
		if msg then sfx(pick(whoa_sfx)) total_tricks=total_tricks+1 end
	end

	body.control=function(self)	
		local da=0
		if playdate.buttonIsPressed(playdate.kButtonLeft) then da=1 end
		if playdate.buttonIsPressed(playdate.kButtonRight) then da=-1 end
		local do_jump
		if self.on_ground==true then
			-- was flying?			
			if air_t>23 then
				-- pro trick? :)
				if reverse_t>0 then
					add_time_bonus(2,"reverse air!")
				else
					add_time_bonus(1,"air!")
				end
			end
			air_t=0

			if playdate.buttonIsPressed(playdate.kButtonA) then
				if not jump_pressed then jump_pressed,jump_ttl=true,0 end
				jump_ttl=min(jump_ttl+1,9)
			elseif jump_pressed then
				-- button released?
				do_jump=true
			end
		else
			-- kill jump
			jump_ttl,jump_pressed,do_jump=0
			-- record flying time
			air_t=air_t+1
		end

		if do_jump then
			self:apply_force_and_torque({0,jump_ttl*7,0},0)
			jump_ttl,jump_pressed,do_jump=0
		end

		self:steer(da/8)
	end

	body.update=function(self)
		t=t-1
		hit_ttl=hit_ttl-1
		total_t=total_t+1

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
			add_time_bonus(2,"360!")
			spin_prev=nil
		end

		local hit_type,hit_actor=ground:collide(pos,0.2)
		if hit_type==2 then
			-- walls: insta-death
			cam:shake()
			self.dead=true
		elseif hit_type==3 then
			-- coins: bonus time
			add_time_bonus(1)
			sfx(8)
		elseif hit_ttl<0 and hit_type==1 then
			-- props: 
			sfx(pick(hit_actor.sfx))
			cam:shake()
			-- temporary invincibility
			hit_ttl=20
			hp=hp-1
			-- kill tricks
			reverse_t,spin_prev=0
		end
		
		local slice,slice_extent=ground:get_track(pos)
		self.on_track=true
		if pos[1]>=slice_extent[1] and pos[1]<=slice_extent[2] then			
			if slice.is_checkpoint then
				if pos[3]>slice_extent[3] then
					add_time_bonus(params.bonus_t)
					sfx(1)
				end
				slice.is_checkpoint=nil
			end
		else
			self.on_track=nil
		end
		
		-- need to have some speed
		if v_dot(velocity,{-sin(angle),0,cos(angle)})<-0.2 then
			reverse_t+=1
		else
			reverse_t=0
		end

		if reverse_t>30 then
			add_time_bonus(3,"reverse!")
			reverse_t=0
		end

		for _,b in pairs(bonus) do
			b.ttl-=1
			if b.ttl<0 then del(bonus,b) end
		end

		if hp<=0 then
			self.dead=true			
		elseif t<0 then
			self.time_over,self.dead=true,true
		end

		-- call parent
		body_update(self)
	end

	body.score=function()
		return t,bonus,total_t,total_tricks
	end

	-- wrapper
	return body
end

function make_snowball(pos)
	local body=make_body(pos)
	
	local body_update=body.update

	body.sx,body.sy=112,0
	body.update=function(self)		

		-- physic update
		self:integrate()
		body_update(self)

		return true
	end
	return body
end

-- game states
-- transition to next state
-- game states
-- transition to next state
local states={}

function pop_state()
	assert(#states>0,"missing base state")	
	states[#states]=nil
end

function push_state(state,...)
	add(states,state(...))
end

function menu_state()
  local starting
	local cols={
		[0]=5,
		[1]=12,
		[6]=7,
		[8]=14}
	-- draw direction box
	-- mode:
	-- 0: normal
	-- 1: focus
	-- 2: selected
	function draw_box(s,x,y,c,blink,freeride)

		palt(0,false)
		palt(14,true)
		sspr(40,0,8,8,x-4,y-12,8,64)

		pal(12,c)
		if blink then
			if (30*time())%8<4 then
				pal(12,cols[c])
				pal(c,cols[c])
				pal(6,cols[6])
			end
		end
		spr(32,x-24,y-6,7,2)
		print(s,x-20,y-2,6)

		if freeride==true then
			spr(234,x-8,y-29,2,2)
			rectfill(x-18,y-13,x+24,y-8,10)
			print("FREERIDING",x-16,y-13,0)
		end
		pal()
	end
   
	-- default records (number of frames)
	local records={900,600,450}
	for i=0,2 do
		local t=3000--dget(i)
		-- avoid bogus data
		records[i+1]=t>0 and t or records[i+1]
	end

	local tree_prop,bush_prop,cow_prop={sx=112,sy=16,r=1.4,sfx={9,10}},{sx=96,sy=32,r=1,sfx={9,10}},{sx=112,sy=48,r=1,sfx={4}}
	local panels={
		{text="marmottes",c=1,params={dslot=0,slope=1.5,tracks=1,bonus_t=2,total_t=30*30,record_t=records[1],props={tree_prop},props_rate=0.85}},
		{text="biquettes★",c=8,params={dslot=1,slope=2,tracks=2,bonus_t=1.5,total_t=20*30,record_t=records[2],props={tree_prop,bush_prop},props_rate=0.87,}},
		{text="chamois★★",c=0,params={dslot=2,slope=3,tracks=3,bonus_t=1.5,total_t=15*30,record_t=records[3],props={tree_prop,tree_prop,tree_prop,cow_prop},props_rate=0.92,}}
	}
	local sel,sel_tgt,blink=0,0,false

	ground=make_ground({slope=0,tracks=0,props_rate=0.6,props={tree_prop}})

	-- reset cam	
	cam=make_cam()

	music(0)
	sfx(-1)
	return {
		-- draw
		draw=function()
			cam:draw_horizon(1,12)

			local out={}
			-- get visible voxels
			ground:collect_drawables(cam.pos,cam.angle,out,dist)

      table.sort(out,function(a,b) return a.key<b.key end)
			draw_drawables(out)

			local a,da=1/3,-1/3
			for i=1,#panels do
				local v={8*cos(a),0.8,-8*sin(a)}
				v_add(v,cam.pos)
				v=m_x_v(cam.m,v)
				if v[3]>0 then
					local x0,y0=cam:project2d(v)
					local p=panels[i]
					draw_box(p.text,x0+32,y0,p.c,blink,p.params.tracks>1)
				end
				a+=da
			end

			_panel_pole:draw(170,140)
			_panel_slices:drawInRect(135,100,100,40)

			gfx.setFont(panelFont)
			gfx.drawText("Le village",150,112)

			_panel_pole:draw(280,160)
			_panel:draw(240,70)

			-- ski mask
			_mask:draw(0,0)
			
			print("select track: 🎣",4,4)
			if (time()%1)<0.5 then printb("❎/🅾️ go!",50,120,10,5) end

			if sel==sel_tgt then
				local s="best: "..time_tostr(panels[sel+1].params.record_t)
				print(s,nil,4)
			end

			-- todo: snow particles
		end,
		-- update
		update=function()
			if playdate.buttonIsPressed(playdate.kButtonLeft) then sel-=1 end
			if playdate.buttonIsPressed(playdate.kButtonRight) then sel+=1 end
			sel=mid(sel,0,#panels-1)

  		sel_tgt=lerp(sel_tgt,sel,0.18)
   		-- snap when close to target
			if abs(sel_tgt-sel)<0.01 then
				sel_tgt=sel
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
					pop_state()
					-- restore random seed
					srand(time())
					push_state(zoomin_state,play_state,panels[sel+1].params)
				end)
			end

			--
			cam:track({64,0,64},sel_tgt/3,v_up)
		end
	}
end

function zoomin_state(next,params)
	local ttl,dttl=30,0.01
  local starting
  -- todo: zoom effect
	return {
		-- draw
		draw=function()
			-- zoom effect
			local s=3*(30-ttl)/30+1
			palt(0,false)
			local dx=-abs(64*s-64)		
			sspr(0,0,128,128,dx,dx,128*s,128*s)
		end,
		update=function(self)
			ttl-=dttl
			dttl+=0.08

      -- todo: fade to black
      if not starting then
        starting=true
        do_async(function()
          -- done with white out?
          pop_state()
          push_state(next,params)
        end)
      end
		end
	}
end

function play_state(params)

	-- stop music
	music(-1,250)

	-- srand(15)

	-- start over
	actors,ground={},make_ground(params)

	-- create player in correct direction
	plyr=make_plyr(ground.plyr_pos,params)

	-- reset cam	
	cam=make_cam()

	-- sprites
	local rot_sprites={
		-- flags
		make_rspr(112,32,128),
		make_rspr(48,0,128)}
	-- per track props
	local prev_prop
	for _,prop in pairs(params.props) do
		if prev_prop~=prop then
			add(rot_sprites,make_rspr(prop.sx,prop.sy,128))
			-- avoid double registration!
			prev_prop=prop
		end
	end

	local gps_sprite=make_rspr(96,48,256)

	return {
		-- draw
		draw=function()
			
			cam:draw_horizon(1,12)
	  
			local out={}
			-- get visible voxels
			ground:collect_drawables(cam.pos,cam.angle,out,dist)
			-- misc. actors
			for _,a in pairs(actors) do
				local p=m_x_v(cam.m,a.pos)
				local ax,ay,az=p[1],p[2],p[3]
				if az>z_near and az<64 then
					local x,y,w=cam:project2d(p)
					out[#out+1]={key=1/(ay*ay+az*az),a=a,x=x,y=y,w=4*w,dist=1}
				end
			end

			-- sprite cache
			local angle=atan2(cam.m[5],cam.m[6])+0.25
			for _,f in pairs(rot_sprites) do
				f(-angle)
			end

      table.sort(out,function(a,b) return a.key<b.key end)
			draw_drawables(out)
			 
			--local cpu=flr(10000*stat(1))/100
			-- print(stat(1),96,2,2)

			if plyr then
				local pos,a,steering=plyr:get_pos()
				local dy=plyr.height*24
				-- ski
				_ski:draw(180+6*cos(time()/4),220+dy-steering*14)
				_ski:draw(240-6*cos(time()/4),220+dy+steering*14)

				--spr(9,34+3*cos(time()/4),128+dy-steering*14,4,4)
				--spr(9,74-2*cos(time()/5),128+dy+steering*14,4,4,true)
			
				-- hands
				spr(140,abs(steering)*16-24,90-dy/3,4,4)
				spr(140,96-abs(steering)*16+24,90-dy/3,4,4,true)
			
				-- 
				local t,bonus,total_t=plyr:score()
				-- warning sound under 5s
				local bk,y_trick=1,110
				if t<150 then
					if t%30==0 then sfx(2) end
					if t%8<4 then bk=8 end
				end
				printb(time_tostr(t),nil,4,10,9,bk)

				--[[
				tt="total time:\n"..time_tostr(tt)
				print(tt,2,3,1)
				print(tt,2,2,7)
				]]

				for i=1,#bonus do
					local b=bonus[i]					
					
					if b.ttl/b.duration>0.5 or t%2==0 then
						printb(b.t,64+b.x-#b.t/1.5,40+b.ttl,10,9,1)
					end
					-- handle edge case if multiple tricks!
					if b.msg then printb(b.msg,nil,y_trick,6,5,1) y_trick-=9 end
				end

				if plyr.gps then gps_sprite(-plyr.gps) end
				if plyr.on_track and (32*time())%8<4 then
					local dx=plyr.gps-0.75
					if dx<-0.1 then
						sspr(64,112,16,16,2,32,32,32)
					elseif dx>0.1 then
						sspr(64,112,16,16,96,32,32,32,true)
					end
				end
				spr(108,56,12,2,2)
					
				-- help msg?
				if total_t<90 then
					printb("🅾️ charge jump",nil,102,6,5,1)
					printb("❎ restart",nil,112,8,2,1)
				end
			end			
		end,
		-- update
		update=function()
			cam:update()
			if plyr then
				plyr:control()	
				plyr:integrate()
				plyr:update()

				-- adjust ground
				ground:update(plyr.pos)

				local pos,a=plyr:get_pos()
				cam:track(pos,a,plyr:get_up())

				if plyr.dead then
					sfx(3)
					cam:shake()

					-- latest score
					local _,_,total_t,total_tricks=plyr:score()
					
					push_state(plyr_death_state,plyr:get_pos(),total_t,total_tricks,params,plyr.time_over)
					-- not active
					plyr=nil
				else	
					-- reset
					if playdate.buttonJustReleased(playdate.kButtonB) then
						pop_state()
						push_state(zoomin_state,play_state,params)
					end
				end
			end

      for i=#actors,1,-1 do
        local a=actors[i]
        if not a:update() then
          table.remove(actors,i)
        end
			end
		end
	}
end

function plyr_death_state(pos,total_t,total_tricks,params,time_over)
	-- convert to string
	local active_msg,msgs=0,{
		"total time: "..time_tostr(total_t),
		"total tricks: "..total_tricks}	
	local msg_colors={
		{10,9,1},{7,5,1}
	}
	local msg_y,msg_tgt_y,msg_tgt_i=-20,{16,-20},0

	-- snowballing!!!
	local snowball_sprite,snowball=make_rspr(112,0,32,0),add(actors,make_snowball(pos))
	local turn_side,tricks_rating=pick({-1,1}),{"    meh","rookie","junior🐱","★master★"}
	local text_ttl,active_text,text=10,"yikes!",{
		"ouch!","aie!","pok!","weee!"
	}

	-- save records (if any)
	if total_t>params.record_t then
    -- todo: save record
		-- dset(params.dslot,total_t)
	end
	
	return {
		draw=function()
			local c=msg_colors[active_msg+1]
			printb(msgs[active_msg+1],nil,msg_y,c[1],c[2],c[3])
			local x,y=rnd(2)-1,msg_y+8+rnd(2)-1
			if active_msg==0 and total_t>params.record_t then print("★new record★",x+50,y,c[2]) end
			if active_msg==1 then print(tricks_rating[min(flr(total_tricks/5)+1,4)],x+70,y,c[2]) end

			if text_ttl>0 and not time_over then
				print(active_text,60,50+text_ttl,8)
			end
			printb("game over!",nil,38,8,2,7)

			if (time()%1)<0.5 then printb("❎/🅾️ retry",42,120,10,5,1) end
		end,
		update=function()
			msg_y=lerp(msg_y,msg_tgt_y[msg_tgt_i+1],0.08)
			if abs(msg_y-msg_tgt_y[msg_tgt_i+1])<1 then msg_tgt_i+=1 end
			if msg_tgt_i>#msg_tgt_y-1 then msg_tgt_i=0 active_msg=(active_msg+1)%2 end

			text_ttl-=1
			snowball_sprite(turn_side*time()*2)
			-- adjust ground
			local p=snowball.pos
			ground:update(p)

			if text_ttl<0 and ground:collide(p,0.2) then
				active_text,text_ttl=pick(text),10
				turn_side=-turn_side
				cam:shake()
			end
			-- keep camera off side walls
			if time_over then
				cam:track(p,0,v_up)
			else
				cam:track({mid(p[1],8,29*4),p[2],p[3]+16},0.5,v_up,0.2)
			end

			if playdate.buttonJustReleased(playdate.kButtonA) then
				pop_state()
				pop_state()
				push_state(menu_state)
			end
		end		
	}
end

function _init()
	-- todo: save

	-- todo: remove (only for benchmarks)
	-- srand(12)
	for i=0,15 do
		local img=gfx.image.new("images/noise"..i)
		assert(img)
		_dither[i]=img
	end

	-- https://opengameart.org/content/pine-tree-pack
	_tree=gfx.image.new("images/pine_snow_0")
	assert(_tree)

	_mask = gfx.image.new("images/mask")
	_sun = gfx.image.new("images/sun")
	_ski = gfx.image.new("images/ski")
	_panel_pole = gfx.image.new("images/panel_pole")
	_panel_slices = gfx.nineSlice.new("images/panel",6,5,10,30)

	local glyphs={}
	local text="CHAMOIS - PISTE NOIRE"
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
	draw_glyphs("CHAMOIS")	
	draw_glyphs("Hard Track",true)	
	-- create a ski slope sign
	local w=96
	_panel = gfx.image.new(w,w,gfx.kColorClear)
	gfx.lockFocus(_panel)
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
	gfx.setFont(panelFontFigures)
	gfx.drawTextAligned("21",47.5,47.5-18,kTextAlignment.center)
	gfx.unlockFocus()
	-- init state machine
	push_state(menu_state)
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
	for _,state in ipairs(states) do
		state:update()
	end
end

function _draw()
	for _,state in ipairs(states) do
		state:draw()
	end
end

function draw_sprite(actor,x,y,w,dist)
	--if dist>8 then 
	--	memcpy(0x5f00,0x4310,16)
	--elseif dist>7 then
	--	memcpy(0x5f00,0x4300,16)
	--end
	--local sx,sy=actor.sx,actor.sy
	--if actor.strip then
	--	local strip=actor.strip[flr((actor.speed*time())%#actor.strip)+1]
	--	sx,sy=strip.sx,strip.sy
	--end
	--sspr(sx,sy,16,16,x-w/2,actor.y or y-w,w,w)
	--pal()
	w*=0.01
	_tree:drawRotated(x-44-w/2,actor.y or y-w,_angle,w,w)
end

function draw_drawables(objects)	
	for i=1,#objects do
		local d=objects[i]
		if d.a then
			draw_sprite(d.a,d.x,d.y,d.w,d.dist)
		else
			-- triangle
			local c0=0x76
			if d.f.m==0 then
				-- dirt
				c0=0x54
				if d.dist>7 then c0=0x4d end
				if d.dist>8 then c0=0xd5 end
			else
				if d.dist>7 then c0=0x6d end
				if d.dist>8 then c0=0xd5 end
			end
			--fillp(d.f.cf)
			local ratio=(d.dist/13)*d.f.cf
			gfx.setPattern(_dither[flr(16*ratio)])
			cam:project_poly(d.v,c0)
			-- face 'details' sprites
			if d.fa then
				local m,v0,u=cam.m,d.v0,d.fa.u
				local x,y,z=v0[1]+u[1],v0[2]+u[2],v0[3]+u[3]
				local az=m[3]*x+m[7]*y+m[11]*z+m[15]
				if az>z_near then	
					-- sprite
					local w=199.5/az
					draw_sprite(d.fa,199.5+w*(m[1]*x+m[5]*y+m[9]*z+m[13]),119.5-w*(m[2]*x+m[6]*y+m[10]*z+m[14]),4*w,d.dist)
				end
			end
		end
	end
	-- gfx.setPattern()
	-- fillp()
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
	local delta_slope=params.slope

	-- number of x/z slices + cell size
	local nx,nz,dx,dy,dz=32,32,4,0,4

	-- ground slices (from 0 to nz-1)
	local slices={}

	-- track generator
	local next_tracks=make_tracks(8,nx-8,params.tracks or 3)

	-- uses m (matrix) and v (vertices) from self
	-- saves the 'if not ...' in inner loop
	local v_ground_cache_cls={
		__index=function(t,k)
			-- inline: local a=m_x_v(t.m,t.v[k]) 
			-- slice index
			local m,i,j=t.m,k%nx,flr(k/nx)
			local s0=slices[j]
			-- generate vertex
			local x,y,z=i*dx,s0.h[i]+s0.y-dy,j*dz
			local ax,az=m[1]*x+m[5]*y+m[9]*z+m[13],m[3]*x+m[7]*y+m[11]*z+m[15]
		
			local outcode=az>z_near and k_far or k_near
			if ax>az then outcode+=k_right
			elseif -ax>az then outcode+=k_left
			end	

			t[k]={ax,m[2]*x+m[6]*y+m[10]*z+m[14],az,outcode=outcode}
			return t[k]
		end
	}

	local slice_id,slice_y=0,0
	local left_pole,right_pole,coin={sx=48,sy=0},{sx=112,sy=32},{speed=3,r=1,score=1,strip={{sx=64,sy=96},{sx=80,sy=96},{sx=96,sy=96},{sx=112,sy=96}}}
	local function make_slice(y)
		-- smooth altitude changes
		slice_y=lerp(slice_y,y,0.2)
		y=slice_y
		-- generate tracks 
		local tracks=next_tracks()
		
		-- height array + props array
		local h,actors={},{}
		for i=0,nx-1 do
			h[i]=rnd(2*delta_slope)
			-- props?
			if rnd()>params.props_rate then
				actors[i]=clone(pick(params.props))-- {sx=prop.sx,sy=prop.sy,sfx=prop.sfx,r=1.2}
			end
		end
			
		-- smoothing
		for k=1,2 do
 			for i=0,nx-1 do
				h[i]=(h[i]+h[(i+1)%nx])/2
			end
		end

		-- side walls
		h[0],h[nx-1]=15+rnd(5),15+rnd(5)

		-- flatten track
		local main_track_x,xmin,xmax,is_checkpoint
		for _,t in pairs(tracks) do
			local ii=flr(t.x)
			local i0,i1=ii-2,ii+1
			if t.main then
				main_track_x,xmin,xmax=t.x,i0*dx,i1*dx
				for i=i0,i1 do				
					-- smooth track
					h[i]=t.h+h[i]/4
					-- remove props from track
					actors[i]=nil
				end
				-- race markers
				if slice_id%8==0 then
					is_checkpoint={}
					actors[i0]=clone(left_pole)-- {sx=left_pole.sx,sy=left_pole.sy}
					actors[i1]=clone(right_pole)-- {sx=right_pole.sx,sy=right_pole.sy}					
				end
			else
				-- side tracks
				for i=i0,i1 do				
					h[i]=(t.h+h[i])/2
				end
				-- coins
				if slice_id%2==0 then actors[ii]=clone(coin) end -- {strip=coins_strip,speed=3,r=1,score=1}
			end
		end

		slice_id+=1
		return {
			y=y,
			h=h,
			-- track extent
			x=main_track_x*dx,
			xmin=xmin,
			xmax=xmax,
			actors=actors,
			is_checkpoint=is_checkpoint
		}
	end

	local function mesh(j)
		local s0,s1=slices[j],slices[j+1]
		-- base slope normal
		local sn={0,dz,s0.y-s1.y} 
		v_normz(sn)
		local function with_material(f,i)
			local c=5*f.n[2]
      -- todo: dither pattern
			--local cf=(#dither_pat-1)*(1-c%1)
			f.cf=f.n[2]--0--dither_pat[flr(cf)+1]

			if i==0 or i==nx-2 then f.m=1 return f end
			f.m=v_dot(sn,f.n)<0.8 and 0 or 1
			return f
		end
 	
		local f,fi,actor={},0
		for i=0,nx-2 do
			local v0={i*dx,s0.h[i]+s0.y,j*dz}
			-- v1-v0
			local u1={dx,s0.h[i+1]-s0.h[i],0}
			-- v2-v0
			local u2={dx,s1.h[i+1]+s1.y-v0[2],dz}
			-- v3-v0
			local u3={0,s1.h[i]+s1.y-v0[2],dz}

			-- normals
			local n0,n1=v_cross(u3,u2),v_cross(u2,u1)
			v_normz(n0)
			v_normz(n1)
			if v_dot(n0,n1)>0.995 then
				-- quad
				-- material:
				-- 0: snow
				-- 1: dirt
				f[fi]=with_material({n=n0},i)
			else
				-- 2 tri
				f[fi]=with_material({n=n0},i)
				f[fi+1]=with_material({n=n1},i)
			end

			-- actor position (if any)
			local u,t0,t1=v_clone(u2),rnd(),rnd()
			v_scale(u,t0)
			v_add(u,u3,t1)
			v_scale(u,1/(rnd()+t0+t1))
			if s0.actors[i] then s0.actors[i].u=u end

			fi+=2
		end
		-- attach faces to slice
		s0.f=f
	end

	for j=0,nz-1 do
		-- bugfix: avoid blocking area at startup
		slices[j]=make_slice(-j*delta_slope)
	end
	-- create faces
	for j=0,nz-2 do
		mesh(j)
	end

	local function collect_face(v0,face,actor,vertices,v_cache,cam_pos,out,dist)
		if v_dot(face.n,cam_pos)>v_dot(face.n,v0) then
			local z,y,outcode,verts,is_clipped=0,0,0xffff,{},0
			-- project vertices (indices)
			for ki,vi in pairs(vertices) do
				local a=v_cache[vi]
				y=y+a[2]
				z=z+a[3]
				outcode=outcode&a.outcode
				-- behind near plane?
				is_clipped=is_clipped+(a.outcode&2)
				verts[ki]=a
			end
			-- mix of near/far verts?
			if outcode==0 then
				-- average before clipping verts
				y=y/#verts
				z=z/#verts

				-- mix of near+far vertices?
				if is_clipped>0 then verts=z_poly_clip(z_near,verts) end
				-- still visible?
				if #verts>2 then
					out[#out+1]={key=1/(y*y+z*z),f=face,fa=actor,v=verts,dist=dist,v0=v0}
				end
			end
		elseif actor then
			-- face is not visible but props might be
			local m,u=v_cache.m,actor.u
			local x,y,z=v0[1]+u[1],v0[2]+u[2],v0[3]+u[3]
			local az=m[3]*x+m[7]*y+m[11]*z+m[15]
			if az>z_near then	
				local ay,w=m[2]*x+m[6]*y+m[10]*z+m[14],199.5/az
				out[#out+1]={key=1/(ay*ay+az*az),a=actor,x=199.5+w*(m[1]*x+m[5]*y+m[9]*z+m[13]),y=119.5-w*ay,w=4*w,dist=dist}
			end
		end
	end

	-- raycasting constants
	--[[
	local angles,dists={},{}
	for i=-16,16 do
		add(angles,atan2(8,i))
		add(dists,sqrt(11*11+i*i))
	end
	]]
	local angles={}
	for i=-32,32 do
		add(angles,atan2(16,i))
	end

	local function visible_tiles(pos,angle)
		local x,y=pos[1]/dx,pos[3]/dz
		local x0,y0=flr(x),flr(y)
		local tiles={[x0+y0*nx]=0}

		for i,a in pairs(angles) do
			local v,u=cos(a+angle),-sin(a+angle)
			
			local mapx,mapy=x0,y0
	
			local ddx,ddy,mapdx,distx,mapdy,disty=1/u,1/v
			if u<0 then
				mapdx,ddx=-1,-ddx
				distx=(x-mapx)*ddx
			else
				mapdx,distx=1,(mapx+1-x)*ddx
			end
			if v<0 then
				mapdy,ddy=-1,-ddy
				disty=(y-mapy)*ddy
			else
				mapdy,disty=1,(mapy+1-y)*ddy
			end	
			for dist=0,12 do
				if distx<disty then
					distx+=ddx
					mapx+=mapdx
				else
					disty+=ddy
					mapy+=mapdy
				end
				-- non solid visible tiles
				if (mapx|mapy)&0xffe0~=0 then break end
				tiles[mapx+mapy*nx]=dist
			end				
		end
		return tiles	
	end

	local plyr_z_index,max_pz=nz/2-1,-32000
	return {
		plyr_pos={slices[plyr_z_index].x,0,plyr_z_index*dz},
		to_tile_coords=function(self,v)
			return flr(v[1]/dx),flr(v[3]/dz)
		end,
		collect_drawables=function(self,cam_pos,cam_angle,out)
			local tiles=visible_tiles(cam_pos,cam_angle)
			-- vertex cache
			-- yoffset
			local v_cache={m=cam.m}
			setmetatable(v_cache,v_ground_cache_cls)

			for k,dist in pairs(tiles) do
				-- get slice(s)
				local i,j=k%nx,flr(k/nx)
				local s0=slices[j]
				if s0 and s0.f then
					-- generate vertex
					local v0={i*dx,s0.h[i]+s0.y-dy,j*dz}
					-- face(s)
					-- force quad rendering for far away tiles
					local f0,f1=s0.f[2*i],s0.f[2*i+1]
					if f1 then
						-- 2 triangles
						if f0 then collect_face(v0,f0,s0.actors[i],{k,k+1+nx,k+nx},v_cache,cam_pos,out,dist) end
						if f1 then collect_face(v0,f1,nil,{k,k+1,k+1+nx},v_cache,cam_pos,out,dist) end
					else
						-- 1 quad
						if f0 then collect_face(v0,f0,s0.actors[i],{k,k+1,k+1+nx,k+nx},v_cache,cam_pos,out,dist) end
					end
				end
			end
		end,
		update=function(self,p)
			-- prevent out of bounds
			p[3]=max(p[3],8*dz)
			local pz=p[3]/dz
			if pz>plyr_z_index then
				-- shift back
				p[3]-=dz
				max_pz-=dz
				local old_y=slices[0].y
				-- drop slice 0
				for i=1,nz-1 do
					slices[i-1]=slices[i]
					slices[i-1].y-=old_y
				end
				-- use previous baseline
				slices[nz-1]=make_slice(slices[nz-2].y-delta_slope*(rnd()+0.5))
				-- create mesh
				mesh(nz-2)
			end
			-- update y offset
			if p[3]>max_pz then
				dy,max_pz=lerp(slices[0].y,slices[1].y,pz%1),p[3]
			end
		end,
		find_face=function(self,p)
			-- z slice
			local i,j=self:to_tile_coords(p)

			local s0=slices[j]
			-- should not happen
			-- assert(s0,"outside of map: "..i.."/"..j)

			local f0,f1=s0.f[2*i],s0.f[2*i+1]
			local f=f0
			-- select face
			if f1 and (p[3]-dz*j<p[1]-dx*i) then f=f1 end

			-- intersection point
			local t=-v_dot(make_v({i*dx,s0.h[i]+s0.y-dy,j*dz},p),f.n)/f.n[2]
			-- todo: return y value only
			p=v_clone(p)
			p[2]+=t

			return f,p,atan2(slices[j+2].x-p[1],2*dz)
		end,
		get_track=function(self,p)
			-- z slice
			local i,j=self:to_tile_coords(p)
			local s0=slices[j]
			-- no need for y
			return s0,{s0.xmin,s0.xmax,j*dz}
		end,
		-- find all actors within a given radius from given position
		collide=function(self,p,r)
			local i0,j0=self:to_tile_coords(p)
			-- out of track
			if i0<=0 or i0>=nx-2 then return 2 end
			
			-- square radius
			r=r*r
			
			-- check all 9 cells (overkill but faster)
			for j=j0-1,j0+1 do
				local s0=slices[j]
				if s0 then
					for i=i0-1,i0+1 do
						local actor=s0.actors[i]
						-- collidable actor?
						if actor and actor.r then
							-- generate vertex
							local v0={i*dx,s0.h[i]+s0.y-dy,j*dz}
							v_add(v0,actor.u)
							local d=make_v(p,v0)
							d[2]=0
							if v_dot(d,d)<r+actor.r*actor.r then
								if actor.score then
									s0.actors[i]=nil
									return 3,actor
								end
								return 1,actor
							end
						end
					end
				end	
			end
		end
	}
end

-->8 
-- rotation cache builder
-- returns a function that copies rotated version in place
-- n: number of cache entries
-- tc: transparent color
function make_rspr(sx,sy,n,tc)
  -- todo: port to playdate

	-- update spritesheet with rotated version
	return function(angle)
		-- 
		_angle=90*angle
	end
end

-->8
-- print helpers
function padding(n)
	n=tostring(flr(min(n,99)))
	return string.sub("00",1,2-#n)..n
end

function time_tostr(t)
	-- frames per sec
	local s=padding(flr(t/30)%60).."''"..padding(flr(10*t/3)%100)
	-- more than a minute?
	if t>1800 then s=padding(flr(t/1800)).."'"..s end
	return s
end

function printb(s,x,y,cf,cs,cb)
  -- todo: use bold font
	print(s,x,y,cs)
end

-->8

function print(s,x,y)
  gfx.setFont(font)
  gfx.drawTextAligned(s,x or 200,y,x and kTextAlignment.left or kTextAlignment.center)
end

function z_poly_clip(znear,v)
	local res,v0={},v[#v]
	local d0=v0[3]-znear
	for i=1,#v do
		local v1=v[i]
		local d1=v1[3]-znear
		if d1>0 then
			if d0<=0 then res[#res+1]=v_lerp(v0,v1,d0/(d0-d1)) end
			res[#res+1]=v1
		elseif d0>0 then
      res[#res+1]=v_lerp(v0,v1,d0/(d0-d1))
		end
		v0,d0=v1,d1
	end
	return res
end

-- *********************
-- main

-- init
_init()

function playdate.update()
  _update()
  _draw()	
  playdate.drawFPS(0,0)
end