local params = ...

local rv = {}

rv.cxx_flags = "-arch i386 -Fglorp/Glop/build/Glop -DMACOSX -I/opt/local/include"
rv.ld_flags = "-arch i386 -Lglorp/Glop/Glop/OSX/lib -Fglorp/Glop/build/Glop -framework OpenGL -framework Carbon -framework AGL -framework ApplicationServices -framework IOKit -framework Glop -ljpeg6b -lfreetype235"
rv.extension = ".prog"  -- have to use something or it'll conflict

local runnable_deps

rv.create_runnable = function(dat)
  local basepath = "build/" .. params.longname .. ".app"
  
  local runnable = {}
  
  local current_glop = params.glop.lib
  local current_glop_iteration = 0
  
  -- copy our main executable
  table.insert(runnable, ursa.rule{basepath .. "/Contents/MacOS/" .. params.longname, dat.mainprog, ursa.util.system_template{"cp $SOURCE $TARGET"}})
  
  local function tweak_glop(cli)
    current_glop_iteration = current_glop_iteration + 1
    current_glop = ursa.rule{"build/glop_lib_" .. current_glop_iteration, current_glop, ursa.util.system_template{("cp $SOURCE $TARGET && install_name_tool %s $TARGET"):format(cli)}}
    print("TWEAKING TO", current_glop[1])
    --assert(false)
  end
  
  -- copy subsidiary libraries
  for libname in ("libfmodex.dylib libz.dylib"):gmatch("[^%s]+") do
    table.insert(runnable, ursa.rule{("%s/Contents/Frameworks/Glop.framework/%s"):format(basepath, libname), ("glorp/Glop/Glop/OSX/lib/%s"):format(libname), ursa.util.system_template{"cp $SOURCE $TARGET"}})
    tweak_glop(("-change ./%s @executable_path/../Frameworks/Glop.Framework/%s"):format(libname, libname))
  end
  
  -- copy the main glop library
  table.insert(runnable, ursa.rule{basepath .. "/Contents/Frameworks/Glop.framework/Glop", current_glop, ursa.util.system_template{"cp $SOURCE $TARGET"}})
  
  runnable_deps = runnable
  
  return {deps = runnable, cli = basepath .."/Contents/MacOS/" .. params.longname}
end

-- installers
function rv.installers()
  assert(runnable_deps)
  local app_prefix = ("build/deploy/%s.app/"):format(params.longname)
  
  local binaries = {}
  
  -- first we mirror our run structure over, plus stripping (oh baby oh baby)
  -- hackery: pulling out filenames from literals
  -- this will be removed once we have a better system for it
  for k, v in pairs(runnable_deps) do
    local sufix = v[1]:match(("@build/%s.app/(.*)"):format(params.longname))
    assert(sufix)
    
    table.insert(binaries, ursa.rule{app_prefix .. sufix, v[1], ursa.util.system_template{"strip -S -x -o $TARGET $SOURCE"}})
  end
  
  -- second we generate our actual data copies
  ursa.token.rule{"built_data", "#datafiles", function ()
    local items = {}
    for _, v in pairs(ursa.token{"datafiles"}) do
      table.insert(items, ursa.rule{app_prefix .. "Contents/Resources/" .. v.dst, v.src, ursa.util.system_template{v.cli}})
    end
    --assert(false)
    return items
  end, always_rebuild = true}
  
  local icon = ursa.rule{app_prefix .. "Contents/Resources/mandible.icns", "glorp/resources/mandicon.png", ursa.util.system_template{("makeicns -in $SOURCE -out $TARGET")}}
  
  local infoplist = ursa.rule{app_prefix .. "Contents/Info.plist", "#version", function ()
    print("Writing info.plist")
    local pl = io.open(app_prefix .. "Contents/Info.plist", "w")
    
    pl:write([[
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>English</string>
  <key>CFBundleExecutable</key>
  <string>]] .. params.longname .. [[</string>
  <key>CFBundleIconFile</key>
  <string>mandible.icns</string>
  <key>CFBundleIdentifier</key>
  <string>com.mandible-games.]] .. params.name .. [[</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleName</key>
  <string>]] .. params.longname .. [[</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleVersion</key>
  <string>]] .. ursa.token{"version"} .. [[</string>
</dict>
</plist>]])
    
    pl:close()
    
  end}
  
  return ursa.rule{("build/%s.dmg"):format(ursa.token{"outputprefix"}), {binaries, ursa.util.token_deferred{"built_data"}, infoplist, icon}, ursa.util.system_template{('hdiutil create -srcfolder "build/deploy/%s.app" $TARGET -ov'):format(params.longname)}}
end

return rv
