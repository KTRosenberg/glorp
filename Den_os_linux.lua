local params = ...

token_literal("CC", "gcc")
token_literal("CCFLAGS", "-m32")

token_literal("CXX", "g++")
token_literal("CXXFLAGS", "-m32")

token_literal("LDFLAGS", "-m32")

token_literal("FINALCXXFLAGS", "-DWIN32")
token_literal("FINALLDFLAGS", " -lGL -lGLU -lrt")

token_literal("FLAC", "flac")

token_literal("LUA_FLAGS", "-DLUA_USE_LINUX -m32")

local rv = {}

rv.extension = ".prog"  -- have to use something or it'll conflict
rv.lua_buildtype = "linux"

local runnable_deps

rv.create_runnable = function(dat)
  local liboutpath = params.builddir

  return {deps = {dat.mainprog}, cli = ("%s%s.prog"):format(params.builddir, params.name)}
end

rv.appprefix = params.builddir .. "deploy/" .. params.midname .. "/"
rv.dataprefix = rv.appprefix
  
-- installers
function rv.installers()
  -- first we have to build the entire path layout
  local data = {}

  -- DLLs and executables
  table.insert(data, ursa.rule{rv.dataprefix .. params.midname, params.builddir .. params.name .. ".prog", ursa.util.system_template{"cp $SOURCE $TARGET && strip -s $TARGET"}})
  
  table.insert(data, ursa.rule{rv.dataprefix .. "data/reporter", params.builddir .. "reporter.prog", ursa.util.system_template{"cp $SOURCE $TARGET && strip -s $TARGET"}})
  
  local dfn = {}
  for i = 0, 2 do
    table.insert(dfn, rv.dataprefix .. "data/mandible_icon-" .. i .. ".png")
  end
  table.insert(data, ursa.rule{dfn, "glorp/resources/linux/mandicomulti.ico", ursa.util.system_template{"convert $SOURCE " .. rv.dataprefix .. "data/mandible_icon.png"}})
  
  cull_data({data})

  local installers = {}
  
  table.insert(installers, ursa.rule{("build/%s-%s.tgz"):format(params.midname, ursa.token{"version"}), {data, ursa.util.token_deferred{"built_data"}, "#culled_data", "#version"}, ursa.util.system_template{("cd %sdeploy && tar -pczf ../../%s-%s.tgz \"%s\""):format(params.builddir, params.midname, ursa.token{"version"}, params.midname)}})
  
  return installers
end

return rv
