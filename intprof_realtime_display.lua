#!/usr/bin/env lua

-- Takes `fast_intprof` output in realtime on stdin, and turns it into a
-- realtime performance monitor for your interrupt code

local eof = false
repeat
   local worst_totals = {}
   for n=1,60 do
      local l = io.read()
      if not l then
         eof = true
         break
      end
      local t = {}
      local numframes
      for d in l:gmatch("[^\t]+") do
         d = assert(tonumber(d))
         if not numframes then
            numframes = d
         else
            t[#t+1] = d*100/numframes
         end
      end
      for n=1,#t do
         if not worst_totals[n] or worst_totals[n] < t[n] then
            worst_totals[n] = t[n]
         end
      end
   end
   io.stderr:write(("N:%6.2f-C:%6.2f-B%6.2f  I:%6.2f  N:%6.2f  O:%6.2f\r"):format(table.unpack(worst_totals)))
until eof
