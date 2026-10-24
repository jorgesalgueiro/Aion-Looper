-- Actual REAPER MIDI item -> production plugin -> stereo render. No editor or
-- hardware, and no private plugin command/telemetry API. State fixtures come
-- from this disposable instance through documented vst_chunk/clap_chunk APIs.
return function(run,cfg,tr,fx,render,log)
 -- Record/finalize involves an asynchronous worker. Pace MIDI audio at 1x:
 -- full-speed offline rendering can outrun worker commit in virtual time.
 assert(reaper.set_config_var_string("projrenderlimit","1",0)==2,"Cannot set project 1x render speed")
 local function pacedRender(name)
  local valid,speed=reaper.get_config_var_string("projrenderlimit")
  assert(valid and speed=="1","MIDI fixture must render at 1x")
  local started=reaper.time_precise()
  render(name)
  local elapsed=reaper.time_precise()-started
  assert(elapsed>=0.9,"MIDI render was not paced for one second")
  log("midi-render-seconds="..name..":"..elapsed)
 end
 local key=cfg.format=="CLAP" and "clap_chunk" or "vst_chunk"
 local ok,source=reaper.TrackFX_GetNamedConfigParm(tr,fx,key)
 assert(ok and #source>0,"Cannot read documented plugin chunk "..key)
 local f=assert(io.open(run.."/midi-source.b64.tmp","wb"));f:write(source);f:close()
 assert(os.rename(run.."/midi-source.b64.tmp",run.."/midi-source.b64"))
 local deadline=reaper.time_precise()+15
 repeat
  f=io.open(run.."/midi-1.b64","rb")
  if not f then assert(reaper.time_precise()<deadline,"MIDI fixture timeout");coroutine.yield() end
 until f
 f:close()
 for kind=1,3 do
  -- Fresh instances prevent earlier renders/loops from supplying false positives.
  reaper.TrackFX_Delete(tr,fx)
  fx=reaper.TrackFX_AddByName(tr,cfg.format..": Aion-Looper",false,-1)
  assert(fx==0,"Cannot recreate exact format")
  f=assert(io.open(run.."/midi-"..kind..".b64","rb"));local chunk=f:read("*a");f:close()
  assert(reaper.TrackFX_SetNamedConfigParm(tr,fx,key,chunk),"Host rejected MIDI fixture")
  local item=assert(reaper.CreateNewMIDIItemInProj(tr,0,1,false))
  local take=assert(reaper.GetActiveTake(item))
  local events={}
  local function packet(time,status,a,b)
   local bytes=b and string.char(status,a,b) or string.char(status,a)
   local ppq=math.floor(reaper.MIDI_GetPPQPosFromProjTime(take,time)+0.5)
   events[#events+1]={ppq=ppq,bytes=bytes,order=#events+1}
  end
  packet(0.05,0x90,36,100);packet(0.06,0x80,36,0)
  packet(0.20,0x90,36,100);packet(0.21,0x80,36,0)
  for _,time in ipairs({0.4,0.7}) do
   if kind==1 then
    packet(time,0x9f,0,100);packet(time+0.001,0x9f,0,127);packet(time+0.02,0x8f,0,0)
   elseif kind==2 then
    packet(time,0xbf,0,127);packet(time+0.001,0xbf,0,64);packet(time+0.02,0xbf,0,0)
   else
    packet(time,0xcf,0);packet(time+0.001,0xcf,0);packet(time+0.002,0xcf,0)
   end
  end
  -- Individual InsertEvt calls cause REAPER to repair unmatched/overlapping
  -- notes between inserts. Publish the complete sorted raw item atomically.
  table.sort(events,function(a,b) if a.ppq==b.ppq then return a.order<b.order end return a.ppq<b.ppq end)
  local raw,previous={},0
  for _,e in ipairs(events) do raw[#raw+1]=string.pack("<i4Bs4",e.ppq-previous,0,e.bytes);previous=e.ppq end
  local ending=math.floor(reaper.MIDI_GetPPQPosFromProjTime(take,1)+0.5)
  -- REAPER MIDI sources terminate with CC123. Supply that real message at
  -- the item's intended end instead of an empty record that it rewrites.
  raw[#raw+1]=string.pack("<i4Bs4",ending-previous,0,string.char(0xb0,123,0))
  raw=table.concat(raw)
  assert(reaper.MIDI_SetAllEvts(take,raw),"Host rejected complete raw MIDI item")
  local exact,observed=reaper.MIDI_GetAllEvts(take,"")
  local evidence=assert(io.open(run.."/midi-"..kind..".expected-events","wb"));evidence:write(raw);evidence:close()
  evidence=assert(io.open(run.."/midi-"..kind..".events","wb"));evidence:write(observed);evidence:close()
  assert(exact and observed==raw,"Host changed source MIDI packets before rendering")
  local counted,notes,ccs,texts=reaper.MIDI_CountEvts(take)
  assert(counted and texts==0,"Unexpected non-MIDI source events")
  -- CountEvts groups paired notes and does not count raw repeated NoteOns as
  -- independent notes. Exact raw readback above is the packet/timing contract.
  log("midi-item="..kind..",notes="..notes..",ccs="..ccs..",rawPackets="..(#events+1))
  log("midi-render="..kind)
  pacedRender("midi-"..kind)
  -- Retain the exact project and item for independently inspectable evidence;
  -- recorded audio persistence is intentionally outside this contract.
  reaper.Main_SaveProjectEx(0,run.."/midi-"..kind..".rpp",8)
  if kind==3 then
   -- Reopen host state, then use the saved mapping and item to record a fresh
   -- loop. This proves assignment persistence, not recorded-loop persistence.
   reaper.Main_openProject("noprompt:"..run.."/midi-3.rpp")
   tr=reaper.GetTrack(0,0)
   assert(tr and reaper.TrackFX_GetCount(tr)==1,"MIDI project FX recall")
   local loaded,restored=reaper.TrackFX_GetNamedConfigParm(tr,0,key)
   assert(loaded,"MIDI project state missing")
   f=assert(io.open(run.."/midi-restored.b64","wb"));f:write(restored);f:close()
   local midiItem=reaper.GetTrackMediaItem(tr,1)
   local same,restoredEvents=reaper.MIDI_GetAllEvts(reaper.GetActiveTake(midiItem),"")
   assert(same and restoredEvents==raw,"Project reopen changed raw MIDI")
   pacedRender("midi-project_reload")
  else
   assert(reaper.DeleteTrackMediaItem(tr,item))
  end
 end
 log("midi-rendered=Note/CC/PC: record/finish/play/mute/unmute with repeats and releases")
end
