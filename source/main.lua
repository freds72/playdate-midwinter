-- game globals
import 'CoreLibs/graphics'
local gfx = playdate.graphics
local font = gfx.font.new('font/whiteglove-stroked')

function playdate.update()
  gfx.setColor(gfx.kColorBlack)
  gfx.fillRect(0, 0, 400, 240)

  local t = playdate.getCurrentTimeMilliseconds()/4000
  local verts={}
  for i=0,0.75,0.25 do
	local cc,ss = math.cos((t + i) * math.pi * 2), math.sin((t + i) * math.pi * 2)

	table.insert(verts,400/2+64*cc)
	table.insert(verts,240/2+64*ss)
  end

  -- lib3d.render(table.unpack(verts))

  t += 0.1
  local verts={}
  local yoffset = 100 * math.cos((t+0.15) * math.pi * 2)
  local n = 7 --3 + math.floor(math.random(4))
  for i=0, n-1 do
	local angle = ((i/n) + t) *  math.pi * 2
	local cc,ss = math.cos(angle), math.sin(angle)

	table.insert(verts,400/2+64*cc)
	table.insert(verts,240/2+64*ss + yoffset)
  end

  lib3d.render(table.unpack(verts))
  --gfx.setColor(gfx.kColorBlack)
  --gfx.drawPolygon(table.unpack(verts))
end