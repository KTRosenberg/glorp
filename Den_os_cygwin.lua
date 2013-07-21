local params = ...

token_literal("CC", "/cygdrive/c/mingw/bin/gcc")
token_literal("CCFLAGS", "")

token_literal("CXX", "/cygdrive/c/mingw/bin/g++")
token_literal("CXXFLAGS", "")

token_literal("LDFLAGS", "")

token_literal("FINALCXXFLAGS", "-Wl,-subsystem,windows -DWIN32")
token_literal("FINALLDFLAGS", "-Wl,--large-address-aware -Wl,-subsystem,windows -static -lopengl32 -lmingw32 -lwinmm -lkernel32 -luser32 -lgdi32 -lwinspool -lcomdlg32 -ladvapi32 -lshell32 -lole32 -loleaut32 -luuid -lodbc32 -lodbccp32 -ldinput -ldxguid -lglu32 -lws2_32 -limagehlp -lpsapi -ldinput -ldinput8")

token_literal("FLAC", "\"/cygdrive/c/Program\ Files\ \(x86\)/FLAC/flac.exe\"")

token_literal("LUA_FLAGS", "")

local rv = {}

rv.extension = ".exe"
rv.lua_buildtype = "cygwin"

rv.extraLinkSources = {}
do
  local iconsrc  = params.icon or "glorp/resources/cygwin/mandicomulti.ico"
  local icondst = params.builddir .. "glorp/icon.ico"
  local rcsrc = "glorp/resources/cygwin/resource.rc"
  local rcdst = params.builddir .. "glorp/resource.rc"
  local icod = ursa.rule{icondst, iconsrc, ursa.util.copy{}}
  local rc = ursa.rule{rcdst, rcsrc, ursa.util.copy{}}
  
  table.insert(rv.extraLinkSources, ursa.rule{params.builddir .. "glorp/resource.res", {icondst, rcdst}, ursa.util.system_template{"nice windres " .. rcdst .. " -O coff -o $TARGET"}})
end

-- runnable
rv.create_runnable = function(dat)
  local liboutpath = params.builddir

  return {deps = {dat.mainprog}, cli = ("%s%s.exe"):format(params.builddir, params.name)}
end

rv.appprefix = params.builddir .. "deploy/"
rv.dataprefix = rv.appprefix

-- installers
function rv.installers()
  -- first we have to build the entire path layout
  local data = {}

  -- DLLs and executables
  table.insert(data, ursa.rule{params.builddir .. "deploy/" .. params.name .. ".exe", params.builddir .. params.name .. ".exe", ursa.util.system_template{"cp $SOURCE $TARGET && strip -s $TARGET"}})
  
  cull_data({data})

  ursa.token.rule{"installers", "#version", function ()
    local v = ursa.token{"version"}
    
    local exesuffix = ("%s-%s-installer.exe"):format(params.midname, v)
    local exedest = "build/" .. exesuffix
    ursa.rule{params.builddir .. "installer.nsi", {data, ursa.util.token_deferred{"built_data"}, "#culled_data", "#version", "glorp/resources/cygwin/installer.nsi.template", "!" .. exesuffix}, function()
      local files = ursa.system{("cd %sdeploy && find . -type f | sed s*\\\\./**"):format(params.builddir)}
      local dir = ursa.system{("cd %sdeploy && find . -type d | sed s*\\\\./**"):format(params.builddir)}
      
      local inp = io.open("glorp/resources/cygwin/installer.nsi.template", "rb")
      local otp = io.open(params.builddir .. "installer.nsi", "w")
      local function outwrite(txt)
        otp:write(txt .. "\n")
      end
      
      local install, uninstall = "", ""
      
      for line in dir:gmatch("[^\n]+") do
        line = line:gsub("/", "\\")
        install = install .. ('CreateDirectory "$INSTDIR\\%s"\n'):format(line)
        uninstall = ('RMDir "$INSTDIR\\%s"\n'):format(line) .. uninstall
      end
      
      for line in files:gmatch("[^\n]+") do
        line = line:gsub("/", "\\")
        install = install .. ('File "/oname=%s" "%s"\n'):format(line, "deploy\\" .. line)
        uninstall = ('Delete "$INSTDIR\\%s"\n'):format(line) .. uninstall
      end
      
      for line in inp:lines() do
        if line == "$$$INSTALL$$$" then
          outwrite(install)
        elseif line == "$$$UNINSTALL$$$" then
          outwrite(uninstall)
        elseif line == "$$$VERSION$$$" then
          outwrite(('!define PRODUCT_VERSION "%s"'):format(v))
        elseif line == "$$$TYPE$$$" then
          outwrite('!define PRODUCT_TYPE "release"')
        elseif line == "$$$OUTFILE$$$" then
          outwrite(('OutFile ../%s'):format(exesuffix))
        else
          outwrite(line:gsub("$$$LONGNAME$$%$", params.longname):gsub("$$$EXENAME$$%$", params.name .. ".exe"))
        end
      end
      
      inp:close()
      otp:close()
    end}
    
    local standalonesuffix = ("%s-%s.exe"):format(params.midname, v)
    local standalonedest = "build/" .. standalonesuffix
    local ziptemp = params.builddir .. "standalone.zip"
    local zipresult = ursa.rule{ziptemp, {data, ursa.util.token_deferred{"built_data"}, "#culled_data", "#version"}, ursa.util.system_template{string.format("cd #BUILDDIR/deploy ; zip -0 -j ../standalone.zip %s.exe ; zip -9 -r ../standalone.zip data", params.name)}}
    
    return {
      ursa.rule{("build/%s-%s.zip"):format(params.midname, v), {data, ursa.util.token_deferred{"built_data"}, "#culled_data", "#version"}, ursa.util.system_template{"cd #BUILDDIR/deploy ; zip -9 -r ../../../$TARGET *"}},
      ursa.rule{exedest, {data, ursa.util.token_deferred{"built_data"}, "#culled_data", "#version", params.builddir .. "installer.nsi"}, ursa.util.system_template{"cd #BUILDDIR && /cygdrive/c/Program\\ Files\\ \\(x86\\)/NSIS/makensis.exe installer.nsi"}},
      ursa.rule{standalonedest, zipresult, function ()
        local exec = io.open(params.builddir .. "deploy/" .. params.name .. ".exe", "rb")
        local dat = exec:read("*a")
        exec:close()
        
        local zip = io.open(params.builddir .. "standalone.zip", "rb")
        zip:read(#dat)
        local ziprest = zip:read("*a")
        zip:close()
        
        local out = io.open(standalonedest, "wb")
        out:write(dat)
        out:write(ziprest)
        out:close()
        
        ursa.system{"chmod +x " .. standalonedest}
      end}
    }
  end, always_rebuild = true}
  
  return ursa.util.token_deferred{"installers"}
end

return rv
