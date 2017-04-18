#!/usr/bin/env lua5.2

gamelicense = "Compatible"
require "subcritical"

local pngloader = assert(SC.Construct("PNGLoader"))
local src = assert(pngloader:Load(P"tinytext/tinytext.png"))
local data = assert(dofile(P"tinytext/tinytext.data.lua"))
local srcw,srch = src:GetSize()

local function has_ink(x, y)
   return x >= 0 and x < srcw and y >= 0 and y < srch
      and src:GetRawPixel(x,y) == 0xFF000000
end

local o = assert(io.open(P"../include/gen/messagefont.hh","w"))

o:write([[
const int message_font_height = ]],data.h,[[;
const struct MessageGlyph {
  // w does not account for the outline. w+1 is the skip distance.
  int w;
  const uint16_t* data;
} message_font_glyphs[] = {
]])
local char_table = {}
local question_mark_index
for i,rec in ipairs(data.chars) do
   if rec.char == "?" then
      question_mark_index = i-1
   end
   assert(rec.char:byte() >= 32 and rec.char:byte() <= 127)
   char_table[rec.char:byte()-32] = i-1
   o:write("  {",rec.w,", (const uint16_t[]){");
   for y=-1,data.h do
      for x=-1,rec.w do
         local ix,iy = rec.x+x, rec.y+y
         local ink = has_ink(ix, iy)
         if ink then o:write("0xFFFF,")
         else
            local shadow = false
            for oy=(y==-1 and 0 or -1),(y==data.h and 0 or 1) do
               for ox=(x==-1 and 0 or -1),(x==rec.w and 0 or 1) do
                  if ox ~= 0 or oy ~= 0 then
                     shadow = shadow or has_ink(ix+ox, iy+oy)
                     if shadow then break end
                  end
                  if shadow then break end
               end
            end
            if shadow then
               o:write("0xF000,")
            else
               o:write("0x0000,")
            end
         end
      end
   end
   o:write("}},\n")
end
o:write[[};
const uint8_t message_font_glyph_indices[96] = {]]
assert(question_mark_index, "No question mark?")
for n=("a"):byte(), ("z"):byte() do
   if not char_table[n] then char_table[n] = char_table[n-32] end
end
for n=0,95 do
   if not char_table[n] then
      char_table[n] = question_mark_index
   end
   o:write(char_table[n], ",")
end
o:write[[};
]]
