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



  @file   single_command.lua
  @author Prashanth Bhaskar(prashanth.bhaskar@riftio.com)
  @date   2014/10/17
 
 --]]

local command = ARGV[1]
local key = KEYS[1]
local db = tonumber(ARGV[2])
local serial = tonumber(ARGV[3])
local reply = 0
--redis.call("SELECT", db)

if command == "SET" then
  local value = ARGV[4]
  reply = redis.call(command, key, value)
else
  reply = redis.call(command, key)
end

return reply
