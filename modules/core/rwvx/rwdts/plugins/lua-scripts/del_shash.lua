--[[ /* STANDARD_RIFT_IO_COPYRIGHT */

  @file   del_shash.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local ky = ARGV[1]
local db = tonumber(ARGV[2])
local reply = "FAIL"
--redis.call("SELECT", db)

reply = redis.call("HDEL", KEYS[1], ky)
return reply
