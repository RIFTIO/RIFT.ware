--[[ 
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



  @file   set_shash.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17

 --]]

local db = tonumber(ARGV[1])
local shard = ARGV[3]
local value = ARGV[4]
local reply = "FAIL"
redis.call("SELECT", db)

reply = redis.call("HSET", shard, KEYS[1], value)
return reply
