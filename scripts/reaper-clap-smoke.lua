-- Run only in the isolated portable REAPER directory created by the test script.
-- No audio devices, physical input, playback or recording are requested here.
local _, script_path = reaper.get_action_context()
local root = script_path:match('^(.*)[/\\]')
local resource = reaper.GetResourcePath():gsub('\\','/'):gsub('/+$','')
local expected = root:gsub('\\','/'):gsub('/+$','')
local checks = {}
local function check(value, label)
  if not value then error(label) end
  checks[#checks+1] = label
  local progress = io.open(root..'/reaper-clap-progress.log','a')
  if progress then progress:write(label..'\n') progress:close() end
end
local function quote(value) return '"'..tostring(value):gsub('\\','\\\\'):gsub('"','\\"'):gsub('\n','\\n'):gsub('\r','\\r')..'"' end
local ok, err = xpcall(function()
  check(resource:lower() == expected:lower(), 'Isolated portable REAPER resources')
  reaper.SetMediaTrackInfo_Value(reaper.GetMasterTrack(0), 'D_VOL', 0)
  local found_name
  for index=0,10000 do
    local found, name = reaper.EnumInstalledFX(index)
    if not found then break end
    if name:find('Rust%-Looper Prototype') then found_name=name break end
  end
  check(found_name, 'REAPER discovers the packaged CLAP prototype')
  reaper.InsertTrackAtIndex(0, false)
  local track = reaper.GetTrack(0,0)
  reaper.SetMediaTrackInfo_Value(track, 'I_RECARM', 0)
  reaper.SetMediaTrackInfo_Value(track, 'I_RECMON', 0)
  local fx = reaper.TrackFX_AddByName(track, found_name, false, -1)
  check(fx >= 0, 'REAPER instantiates the native CLAP')
  local _, label = reaper.TrackFX_GetFXName(track,fx,'')
  check(label:find('Rust%-Looper Prototype'), 'Host reports the expected plugin name')
  -- REAPER can append host bypass/wet parameters after the plugin's own 22.
  check(reaper.TrackFX_GetNumParams(track,fx) >= 22, 'Native plugin parameters are available')
  for index=0,21 do
    local found, name = reaper.TrackFX_GetParamName(track,fx,index,'')
    check(found and #name>0, 'Parameter '..index..' has a host-visible name')
  end
  reaper.InsertTrackAtIndex(1,false)
  local second = reaper.GetTrack(0,1)
  local fx2 = reaper.TrackFX_AddByName(second,found_name,false,-1)
  check(fx2>=0, 'REAPER instantiates a second native CLAP')
  local _, label2 = reaper.TrackFX_GetFXName(second,fx2,'')
  check(label2:find('Rust%-Looper Prototype'), 'Second host instance reports the expected plugin name')
  local mode_file=io.open(root..'/state-mode.txt','r')
  local state_mode=mode_file and mode_file:read('*a'):match('1')
  if mode_file then mode_file:close() end
  if state_mode then
  local chunk_ok, chunk = reaper.TrackFX_GetNamedConfigParm(track,fx,'clap_chunk')
  check(chunk_ok and #chunk>0, 'REAPER obtains the native plugin state chunk')
  local project = root..'/state-test.RPP'
  reaper.Main_SaveProjectEx(0,project,8)
  local file = io.open(project,'r')
  check(file ~= nil, 'REAPER writes a project containing native CLAP state')
  if file then file:close() end
  -- This is solely the isolated test project; discard its prompt state.
  reaper.Main_openProject('noprompt:'..project)
  local reopened = reaper.GetTrack(0,0)
  check(reopened and reaper.TrackFX_GetCount(reopened)==1, 'REAPER reopens the native CLAP project')
  local restored_ok, restored_chunk = reaper.TrackFX_GetNamedConfigParm(reopened,0,'clap_chunk')
  check(restored_ok and restored_chunk==chunk, 'REAPER restores the exact native state chunk')
  end
  -- This host test has no audio takes; float-exact PCM state is exercised by
  -- the separate dynamically loaded C-ABI host without physical recording.
  for index=reaper.CountTracks(0)-1,0,-1 do reaper.DeleteTrack(reaper.GetTrack(0,index)) end
end, debug.traceback)
local report = assert(io.open(root..'/reaper-clap-smoke.json','w'))
local labels={}
for _,label in ipairs(checks) do labels[#labels+1]=quote(label) end
report:write('{"ok":'..tostring(ok)..',"reaper":'..quote(reaper.GetAppVersion())..',"resource":'..quote(resource)..',"expected":'..quote(expected)..',"checks":['..table.concat(labels,',')..']'..(ok and '' or ',"error":'..quote(err))..'}')
report:close()
if resource:lower() == expected:lower() then
  reaper.Main_SaveProjectEx(0,root..'/empty-test.RPP',8)
  reaper.Main_OnCommand(40004,0) -- File: Quit REAPER, test instance only.
end
