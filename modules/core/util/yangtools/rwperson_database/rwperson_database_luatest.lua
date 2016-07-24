#!/usr/bin/luajit

require('luaunit')

local lgi = require 'lgi'
local GObject = lgi.GObject
local RwpersonDB = lgi.RwpersonDbYang

-- Enable the following two line if we want to pause in the script
-- require('debugger')
-- pause('Hey this is a GDB test')
--

TestRwpersonDB = {} --class
  function TestRwpersonDB:setUp()
    self.person = RwpersonDB.Person()
  end

  function TestRwpersonDB:tearDown()
  end

  function TestRwpersonDB:test_basic_person()
    self.person.name = 'test-name'
    assertEquals(self.person.name, 'test-name')

    self.person.email = 'test@test.com'
    assertEquals(self.person.email, 'test@test.com')
  end

  function TestRwpersonDB:test_containter_in_person()
    emergency_phone = self.person.emergency_phone

    -- Check that emergency_phone points to phone's memory
    emergency_phone.number = "123-456-7890"
    assertEquals(emergency_phone.number, '123-456-7890')
    assertEquals(self.person.emergency_phone.number, '123-456-7890')
  end
-- class TestRwpersonDB

-- person = RwpersonDB.Person()
-- new_phone = { RwpersonDB.Person_Phone(), RwpersonDB.Person_Phone() }
-- new_phone[1].number = '#1'
-- new_phone[2].number = '#2'
-- print(new_phone[1])
-- person.phone = new_phone
-- print(person)
--

LuaUnit:run()
