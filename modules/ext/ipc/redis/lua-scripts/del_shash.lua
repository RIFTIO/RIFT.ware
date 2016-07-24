--[[ 
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



  @file   del_shash.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local db = tonumber(ARGV[1])
local shard = ARGV[2]
local reply = "FAIL"
redis.call("SELECT", db)

reply = redis.call("HDEL", shard, KEYS[1])
return reply
