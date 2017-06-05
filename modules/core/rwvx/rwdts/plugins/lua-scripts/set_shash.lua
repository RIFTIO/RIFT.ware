--[[ /* STANDARD_RIFT_IO_COPYRIGHT */

  @file   set_shash.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17

 --]]

local db = tonumber(ARGV[2])
local ky = ARGV[1]
local value = ARGV[4]
local reply = "FAIL"
--redis.call("SELECT", db)

reply = redis.call("HSET", KEYS[1], ky, value)
return reply
