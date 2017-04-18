#!/usr/bin/env lua5.2

gamelicense = "Compatible"
require "subcritical"

local pngloader = assert(SC.Construct("PNGLoader"))

local png = assert(pngloader:Load(SC.Construct("Path",arg[1])))
local pw,ph = png:GetSize()
assert(pw == 8 and ph == 1024)
outfile = assert(io.open(SC.Construct("Path",arg[2]),"wb"))
for glyph=0,127 do
   local chary = glyph*8
   local bits = {}
   for cy=0,7 do
      local o = 0
      for cx=0,7 do
         local p = png:GetRawPixelNoAlpha(cx, chary+cy)
         if p ~= 0 then
            o = o + bit32.rshift(128, cx)
         end
      end
      bits[#bits+1] = o
   end
   local tile = string.char(table.unpack(bits))
   outfile:write(tile, tile)
end
outfile:close()
