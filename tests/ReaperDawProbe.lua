-- Runs only inside the disposable profile/project created by TestReaper.py.
local _,script=reaper.get_action_context()
local run=script:match("^(.*)[/\\]")
run=run:gsub("\\","/")
local function log(s)
 local f=assert(io.open(run.."/events.txt","a")); f:write(s.."\n"); f:close()
end
local function settle()
 local untilTime=reaper.time_precise()+0.15
 repeat coroutine.yield() until reaper.time_precise()>=untilTime
end
local function main()
 local cfg=dofile(run.."/config.lua")
 assert(reaper.GetResourcePath():gsub("\\","/")==run, "Refusing non-test REAPER profile")
 assert(reaper.CountTracks(0)==0,"Refusing nonempty project")
 log("host="..reaper.GetAppVersion())
 local master=reaper.GetMasterTrack(0)
 for i=reaper.GetTrackNumSends(master,1)-1,0,-1 do assert(reaper.RemoveTrackSend(master,1,i)) end
 assert(reaper.GetTrackNumSends(master,1)==0,"Hardware output still connected")
 reaper.InsertTrackAtIndex(0,false)
 local tr=reaper.GetTrack(0,0)
 local fx=reaper.TrackFX_AddByName(tr,cfg.format..": Aion-Looper",false,-1)
 assert(fx==0,"Cannot load requested format "..cfg.format)
 local _,name=reaper.TrackFX_GetFXName(tr,fx,"")
 assert(name:find(cfg.format..":",1,true) and name:find("Aion-Looper",1,true),name)
 log("plugin="..name)
 local identOk,ident=reaper.TrackFX_GetNamedConfigParm(tr,fx,"fx_ident")
 assert(identOk,"Missing plugin identity");log("identity="..ident)
 local params={}
 for i=0,reaper.TrackFX_GetNumParams(tr,fx)-1 do
  local _,n=reaper.TrackFX_GetParamName(tr,fx,i,"")
  if n=="Master gain" or n=="Input monitor" then params[n]=i end
 end
 assert(params["Master gain"] and params["Input monitor"],"Missing parameters")
 assert(reaper.TrackFX_GetParamNormalized(tr,fx,params["Master gain"])==0,"Default master not muted")
 assert(reaper.TrackFX_GetParamNormalized(tr,fx,params["Input monitor"])==0,"Default monitor not off")
 local item=reaper.AddMediaItemToTrack(tr)
 local take=reaper.AddTakeToMediaItem(item)
 local src=assert(reaper.PCM_Source_CreateFromFile(run.."/input.wav"))
 reaper.SetMediaItemTake_Source(take,src)
 reaper.SetMediaItemInfo_Value(item,"D_LENGTH",1)
 reaper.SetMediaItemInfo_Value(item,"D_FADEINLEN",0)
 reaper.SetMediaItemInfo_Value(item,"D_FADEOUTLEN",0)
 reaper.GetSetProjectInfo(0,"PROJECT_SRATE",48000,true)
 reaper.GetSetProjectInfo(0,"PROJECT_SRATE_USE",1,true)
 for k,v in pairs({RENDER_SETTINGS=0,RENDER_BOUNDSFLAG=0,RENDER_STARTPOS=0,RENDER_ENDPOS=1,
    RENDER_SRATE=48000,RENDER_CHANNELS=2,RENDER_TAILFLAG=0,RENDER_NORMALIZE=0,RENDER_DITHER=0}) do
  reaper.GetSetProjectInfo(0,k,v,true)
 end
 reaper.GetSetProjectInfo_String(0,"RENDER_FILE",run,true)
 reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT","evaw",true)
 reaper.GetSetProjectInfo_String(0,"RENDER_FORMAT2","",true)
 local function render(name)
  reaper.GetSetProjectInfo_String(0,"RENDER_PATTERN",name,true)
  log("render="..name)
  reaper.Main_OnCommand(42230,0)
  local f=assert(io.open(run.."/"..name..".wav","rb"),"Render missing");f:close()
  settle()
 end
 local function verify(gain,monitor)
  assert(math.abs(reaper.TrackFX_GetParamNormalized(tr,fx,params["Master gain"])-gain)<1e-6,"Master recall")
  assert(reaper.TrackFX_GetParamNormalized(tr,fx,params["Input monitor"])==monitor,"Monitor recall")
 end
 local cases={{"default",0,0},{"unity",0.5,1},{"half",0.25,1},{"monitor_off",0.5,0},{"master_zero",0,1},{"recovery",0.5,1}}
 for _,c in ipairs(cases) do
  assert(reaper.TrackFX_SetParamNormalized(tr,fx,params["Master gain"],c[2]))
  assert(reaper.TrackFX_SetParamNormalized(tr,fx,params["Input monitor"],c[3]))
  -- With the device closed while stopped, REAPER delivers queued changes on
  -- processing. Render before saving state, and after mutation and restoration.
  render("set_"..c[1]);verify(c[2],c[3])
  local ok,chunk=reaper.GetTrackStateChunk(tr,"",false);assert(ok)
  local dump=assert(io.open(run.."/"..c[1]..".chunk","w"));dump:write(chunk);dump:close()
  reaper.TrackFX_SetParamNormalized(tr,fx,params["Master gain"],0.123)
  reaper.TrackFX_SetParamNormalized(tr,fx,params["Input monitor"],1-c[3])
  render("mutate_"..c[1]);verify(0.123,1-c[3])
  assert(reaper.SetTrackStateChunk(tr,chunk,false),"State restore rejected")
  render(c[1]);verify(c[2],c[3])
  reaper.Main_SaveProjectEx(0,run.."/"..c[1]..".rpp",8)
  log("rendered="..c[1])
 end
 reaper.Main_openProject("noprompt:"..run.."/recovery.rpp")
 tr=reaper.GetTrack(0,0);assert(tr and reaper.TrackFX_GetCount(tr)==1,"Project FX recall")
 render("project_reload");verify(0.5,1)
 if cfg.midi then dofile(run.."/midi.lua")(run,cfg,tr,fx,render,log) end
 log("PASS")
 reaper.Main_SaveProject(0,false)
 reaper.Main_OnCommand(40004,0) -- closes only this isolated instance
end
local worker=coroutine.create(main)
local function tick()
 local ok,err=coroutine.resume(worker)
 if not ok then log("FAIL="..debug.traceback(worker,tostring(err)));return end
 if coroutine.status(worker)~="dead" then reaper.defer(tick) end
end
tick()
