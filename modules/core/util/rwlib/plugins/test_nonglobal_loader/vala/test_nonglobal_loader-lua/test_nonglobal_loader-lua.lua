local lgi = require 'lgi'
local GObject = lgi.GObject
local TestNonglobalLoader = lgi.TestNonglobalLoader

TestNonglobalLoader = GObject.Object:derive('TestNonglobalLoader', {
    TestNonglobalLoader.Api
})

function TestNonglobalLoader:do_add(secs, a, b)
  if secs ~= 0 then
    print("sleeping for " .. secs .. " secs")
    os.execute("sleep " .. tonumber(secs))
    print("sleep done")
  end

  return a + b
end

return { TestNonglobalLoader }
