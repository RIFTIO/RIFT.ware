--[[ 
/*
 * 
 *   Copyright 2016 RIFT.IO Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */



  @file   get_next.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local val = {}
local key = {}
local serial_num = {}
local delimiter = ":"
local db = tonumber(ARGV[1])
local scan_num = tonumber(ARGV[2])
local shard_num = ARGV[3]
local node_indx = tonumber(ARGV[4])
local i = 1
local count = 1
local scan_table = {}
local hscan_table = {}
local build_search = {}
table.insert(build_search, "*:")
table.insert(build_search, shard_num);
local shard_search = table.concat(build_search)
--redis.call("SELECT", db)
local scan_result = redis.call("SCAN", scan_num)
local next_scan_num = scan_result[1]
local serial_shard
scan_table = scan_result[2]
while scan_table[i] do
  local hscan_result = redis.call("HSCAN", scan_table[i], 0, "MATCH", shard_search)
  hscan_table = hscan_result[2]
  if hscan_table[1] ~= nil then
    serial_shard = hscan_table[1]
    local result = {}
    local j = 0
    if serial_shard ~= nil then
      for match in (serial_shard..delimiter):gmatch("(.-)"..delimiter) do
         result[j] = match
         j = j + 1
      end
    end
    val[count] = hscan_table[2]
    key[count] = scan_table[i]
    if (result[0] ~= nil) then
      serial_num[count] = tonumber((result[0]+1))
    else 
      serial_num[count] = 0
    end
    count = count + 1
  end
  i = i + 1
end
return {node_indx, val, key, serial_num, tonumber(next_scan_num)}
