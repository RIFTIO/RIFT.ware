--[[ 
/*
 * 
 * (c) Copyright RIFT.io, 2013-2016, All Rights Reserved
 *
 */



  @file   del_pend_commit.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local db = tonumber(ARGV[1])
local serial = tonumber(ARGV[2])
local reply = 0
local max = 0;
local inter_key = {}
redis.call("SELECT", db)

local pend_key = {}
table.insert(pend_key, "pending-del-")
table.insert(pend_key, KEYS[1])
local final_pend_key = table.concat(pend_key)
local key  = redis.call("HKEYS", final_pend_key)
if key[1] == nil then
  return reply
else
  local delimiter = ":"
  local result = {}
  local i = 0
  for match in (key[1]..delimiter):gmatch("(.-)"..delimiter) do
      result[i] = match
       i = i + 1
  end
  local old_serial = tonumber(result[0])
  if serial == (old_serial + 1) then
    redis.call("DEL", final_pend_key)
    reply = redis.call("DEL", KEYS[1])
  end
end
return reply
