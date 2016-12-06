#!/usr/bin/luajit

local lgi = require 'lgi'
lgi.GIRepository.Repository.prepend_search_path('./')

local GObject = lgi.GObject
local NS = lgi.NAMESPACE

-- Required for NS to be populated,
-- everything is loaded lazily
NS:_resolve(true)

-- Every type to override
local types = {}

-- An array is a sequential list of values
local function is_array(value)
  local i = 0

  for k in pairs(value) do
    i = i + 1

    if value[i] == nil or type(k) ~= 'number' then
      return false
    end
  end

  return true
end

local function key_string(value, indent)
  local t = type(value)

  if t == 'string' or t == 'number' then
    return indent .. '"' .. tostring(value) .. '"'
  end

  error('Table key must be a number or string')
end

local function insert_many(t, ...)
  assert(type(t) == 'table')

  for _, value in ipairs({...}) do
    assert(value ~= nil)
    table.insert(t, value)
  end
end

function to_json_impl(value, indent, table_indent)
  local t = type(value)

  if value == nil or t == 'boolean' or t == 'number' then
    return indent .. tostring(value)
  end

  if t == 'string' then
    return key_string(value, indent)
  end

  if t == 'table' then
    if next(value) == nil then
      return indent .. '{}'
    end

    local str = {}
    local next_indent = '    '
    local is_array = is_array(value)

    if table_indent ~= '' then
      next_indent = string.rep(table_indent, 2)
    end

    if is_array then
      insert_many(str, indent, '[\n',
                  to_json_impl(value[1], next_indent, next_indent))

      for _, v in ipairs({}), value, 1 do
        insert_many(str, ',\n',
                    to_json_impl(v, next_indent, next_indent))
      end

      insert_many(str, '\n', table_indent, ']')
    else
      local k, v = next(value)

      insert_many(str, indent, '{\n',
                  key_string(k, next_indent), ': ',
                  to_json_impl(v, '', next_indent))

      for k, v in next, value, k do
        insert_many(str, ',\n',
                    key_string(k, next_indent), ': ',
                    to_json_impl(v, '', next_indent))
      end

      insert_many(str, '\n', table_indent, '}')
    end

    return table.concat(str)
  end

  local gtype = value._gtype
  if gtype ~= nil then
    local lgi_type = GObject.Type.type(gtype)
    local attributes = rawget(lgi_type, '_attribute')
    local new_value = {}

    for name, attrib in pairs(attributes) do
      if attrib.get ~= nil then
        new_value[name] = attrib.get(value)
      end
    end

    return to_json_impl(new_value, indent, table_indent)
  end

  error('Value type ' .. t .. ' not allowed')
end

local function to_json(value)
  return to_json_impl(value, '', '')
end

-- Find all classes and boxed types
for _, type_array in pairs({ NS._struct, NS._class }) do
  for _, t in pairs(type_array) do
    if t._gtype ~= nil then
      table.insert(types, t)
    end
  end
end

for _, t in pairs(types) do
  rawset(t, '_tostring', to_json)
  t._attribute = rawget(t, '_attribute') or {}

  if t._method ~= nil then
    for method, func in pairs(t._method) do
      local prefix = string.sub(method, 1, string.len('get_'))

      -- Make sure to support types with only a get or a set, not both
      if prefix == 'get_' or prefix == 'set_' then
        local name = string.sub(method, string.len(prefix) + 1,
                                string.len(method))

        --print(string.format('%s: '%s' => '%s'', t._name, method, name))
        t._attribute[name] = { get = t['get_' .. name],
                               set = t['set_' .. name] }
      end
    end
  end
end

