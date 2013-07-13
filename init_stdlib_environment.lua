-- Environment creation and destruction framework. Must be called after the global state is set up.

-- First, we make a deep copy of the existing environment
function CopyDeep(src)
  if type(src) == "table" then
    local rv = {}
    for k, v in pairs(src) do
      rv[CopyDeep(k)] = CopyDeep(v)
    end
    return rv
  else
    return src  -- lulz "copy"
  end
end

local basic = {}
for k, v in pairs(External) do
  -- we kill _G because of infinite loops
  -- we kill package because package.loaded causes infinite loops, and honestly, I don't use package for anything
  if k ~= "_G" and k ~= "package" then
    basic[CopyDeep(k)] = CopyDeep(v)
  end
end
basic.Event = nil
basic.Command = nil
basic.Inspect = nil
basic.Utility = nil
basic.Frame.Root = nil

local function RecursiveEventRecreate(target, source, context, eventlist)
  for k, v in pairs(source) do
    assert(type(v) == "table")
    if type(v) ~= "table" then return end
    
    if v.CreateContextHandle then
      target[k] = v:CreateContextHandle(context)
      local blob = target[k]
      table.insert(eventlist, function () blob:ClearContext(context) end)
    else
      target[k] = {}
      RecursiveEventRecreate(target[k], source[k], context, eventlist)
    end
  end
end

local contextlist = {}
local contextshutdown = setmetatable({}, {__mode = 'k'})

local envchildren = {}
local envparents = {}

InsertItem(External, "Command.Environment.Create", function (root, label, filename, ...)
  assert(root)
  assert(label)
  
  if not envchildren[root] then envchildren[root] = {} end
  
  local contextmeta = {}
  contextmeta.teardown = {}
  
  local nenv = CopyDeep(basic)
  nenv._G = nenv
  envchildren[root][nenv] = true
  envparents[nenv] = root
  
  nenv.Command = CopyDeep(root.Command)
  nenv.Inspect = CopyDeep(root.Inspect)
  nenv.Utility = CopyDeep(root.Utility)
  
  nenv.Frame.Root = nenv.Frame.Frame(root.Frame.Root)
  nenv.Frame.Root:SetPoint("TOPLEFT", root.Frame.Root, "TOPLEFT")
  nenv.Frame.Root:SetPoint("BOTTOMRIGHT", root.Frame.Root, "BOTTOMRIGHT")
  
  nenv.Event = {}
  
  RecursiveEventRecreate(nenv.Event, root.Event, nenv, contextmeta.teardown)
  
  contextlist[nenv] = contextmeta

  function nenv.loadfile(...)
    local res, rv = loadfile(...)
    if res then
      setfenv(res, nenv)
    end
    return res, rv
  end
  function nenv.load(...)
    local res, rv = load(...)
    if res then
      setfenv(res, nenv)
    end
    return res, rv
  end
  function nenv.loadstring(...)
    local res, rv = loadstring(...)
    if res then
      setfenv(res, nenv)
    end
    return res, rv
  end
  
  assert(nenv.loadfile(filename))(...)
  
  return nenv
end)

InsertItem(External, "Command.Environment.Destroy", function (target)
  local meta = contextlist[target]
  assert(meta)
  if not meta then return end
  
  -- kill all children first
  while envchildren[target] and next(envchildren[target]) do
    External.Command.Environment.Destroy(next(envchildren[target]))
  end
  
  -- delink everything
  envchildren[target] = nil -- we no longer have children
  envchildren[envparents[target]][target] = nil -- our parent no longer has us as a child
  envparents[target] = nil  -- we no longer have a parent
  
  for _, v in ipairs(meta.teardown) do
    v()
  end
  
  target.Frame.Root:Obliterate()
  
  contextlist[target] = nil
  contextshutdown[target] = External.Inspect.System.Time.Real()
end)

External.Event.System.Update.Begin:Attach(function ()
  local gctimeout = 2
  
  local gc = false
  
  for k, v in pairs(contextshutdown) do
    if v < External.Inspect.System.Time.Real() - gctimeout then
      gc = true
    end
  end
  
  if gc then
    print("Doing full garbage collection pass to look for leaks")
    collectgarbage("collect")
    for k, v in pairs(contextshutdown) do
      assert(v > External.Inspect.System.Time.Real() - gctimeout, string.format("Leak in context!"))
    end
  end
end)
