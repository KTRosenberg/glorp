function cull_data(path, dat)
  ursa.token.rule{"culled_data", {ursa.util.token_deferred{"built_data"}, "#built_data"}, function ()
    local valids = {}
    for _, v in pairs(ursa.relative_from{{dat, ursa.token{"built_data"}}}) do
      local val = v:match(path .. "(.*)")
      assert(val)
      valids[val] = true
    end
    
    for f in ursa.util.system{("cd \"%s\" && find . -type f"):format(path)}:gmatch("([^%s]+)") do
      local fi = f:match("%./(.*)")
      if not valids[fi] then
        print("======== REMOVING", fi)
        ursa.util.system{"rm " .. path .. fi}
      end
    end
    
    return "" -- alright we return nothing this is just a command basically. I need a better way to handle this.
  end, always_rebuild = true}
end