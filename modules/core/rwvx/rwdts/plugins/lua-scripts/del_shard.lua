--[[ /* STANDARD_RIFT_IO_COPYRIGHT */

  @file   del_shard.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local db = tonumber(ARGV[1])
local shard_num = ARGV[2] 
local delimiter = ":"
local i = 1
local count = 1
local del_count = 0
local total_del_count = 0
local keys_table = {}
local hkeys_table = {}
--redis.call("SELECT", db)
local key_result = redis.call("KEYS", "*")
while key_result[i] do
  local hkey_result = redis.call("HKEYS", key_result[i])
  if hkey_result[1] ~= nil then  
    local serial_shard = hkey_result[1]
    local result = {}
    local j = 0
    for match in (serial_shard..delimiter):gmatch("(.-)"..delimiter) do
         result[j] = match
         j = j + 1
    end
    if shard_num == (result[1]) then
      del_count = redis.call("DEL", key_result[i])
      total_del_count = total_del_count + del_count
    end
  end
  i = i + 1
end
return total_del_count
