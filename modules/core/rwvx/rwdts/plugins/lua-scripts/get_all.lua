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



  @file   get_all.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local val = {}
local key = {}
local db = tonumber(ARGV[1])
local i = 1
local count = 1
--redis.call("SELECT", db)
local keys_result = redis.call("KEYS", "*")
while keys_result[i] do
  local hget_result = redis.call("HGETALL", keys_result[i])
  val[count] = hget_result[2]
  key[count] = keys_result[i]
  count = count + 1
  i = i + 1
end
return {key, val}
