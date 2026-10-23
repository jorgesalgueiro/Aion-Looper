-- Real libardour session and callback test, launched only by TestArdour.py.
local root=assert(arg[1]);local block=assert(tonumber(arg[2]))
assert(block==64 or block==128)
local backend=assert(AudioEngine:set_backend("None (Dummy)","Aion isolated test",""))
assert(backend:set_device_name("Sine Wave")==0)
assert(backend:set_buffer_size(block)==0)
local session=assert(create_session(root.."/session","probe",48000))
assert(AudioEngine:current_backend_name()=="Dummy")
assert(backend:buffer_size()==block and backend:sample_rate()==48000)
print("BACKEND",block,backend:sample_rate())
local uri="https://github.com/jorgesalgueiro/Aion-Looper/plugins/main"
local proc=ARDOUR.LuaAPI.new_plugin(session,uri,ARDOUR.PluginType.LV2,"")
assert(not proc:isnil(),"Cannot load Aion LV2")
assert(proc:to_insert():plugin(0):unique_id()==uri,"Wrong plugin identity")
local routes=session:new_audio_route(2,2,nil,1,"Aion test",ARDOUR.PresentationInfo.Flag.AudioBus,ARDOUR.PresentationInfo.max_order)
local route=routes:front()
assert(route:add_processor_by_index(proc,0,nil,true)==0)
for ch=0,1 do
 route:input():nth(ch):disconnect_all()
 assert(route:input():nth(ch):connect("system:capture_"..(ch+1))==0)
end
local function getControls(p)
 local a=p:to_automatable();local result={}
 for param in a:all_automatable_params():iter() do
  local c=a:automation_control(param,false)
  result[c:desc().label]=c
 end
 for _,name in ipairs({"Master gain","Input monitor","Auto-play after Rec","Sync mode","Tempo","Bars",
  "Beats per bar","Time signature denominator","Visual count-in","Sound count-in","Metronome","Metronome volume"}) do
  assert(result[name],"Missing automatable property "..name)
 end
 return result
end
local controls=getControls(proc)
local function peaks(name,expectedGain)
 ARDOUR.LuaAPI.usleep(300000) -- exceed gain smoothing and meter update interval
 local meter=route:peak_meter()
 assert(meter:input_streams():n_audio()==2,"Expected stereo meter")
 local midiOffset=meter:input_streams():n_midi() -- MIDI meters precede audio
 meter:reset_max();ARDOUR.LuaAPI.usleep(300000)
 local values={}
 for ch=0,1 do
  local db=meter:meter_level(midiOffset+ch,ARDOUR.MeterType.MeterMaxPeak)
  if expectedGain==0 then assert(db==-math.huge,"Expected silence in "..name)
  else
   local expectedDb=-18+20*math.log(expectedGain,10)
   assert(db==db and math.abs(db-expectedDb)<0.5,"Missing/invalid audio in "..name)
  end
  values[ch+1]=db
 end
 print("AUDIO",name,values[1],values[2])
 return values
end
-- Default native master/monitor are muted, before any parameter writes.
peaks("default",0)
local unity
for _,c in ipairs({{"unity",1,1},{"half",0.5,1},{"monitor_off",1,0},{"master_zero",0,1},{"recovery",1,1}}) do
 controls["Master gain"]:set_value(c[2],PBD.GroupControlDisposition.NoGroup)
 controls["Input monitor"]:set_value(c[3],PBD.GroupControlDisposition.NoGroup)
 local values=peaks(c[1],c[2]*c[3])
 if c[1]=="unity" then unity=values end
 if c[1]=="half" or c[1]=="recovery" then
  for ch=1,2 do
   local delta=c[1]=="half" and -6.020599913279624 or 0
   assert(math.abs(values[ch]-unity[ch]-delta)<0.001,"Gain ratio/recovery mismatch")
  end
 end
end
session:save_state("")
controls=nil;proc=nil;routes=nil;route=nil;collectgarbage();close_session();session=nil
session=assert(load_session(root.."/session","probe"))
assert(backend:buffer_size()==block and backend:sample_rate()==48000,"Reload changed engine configuration")
for r in session:get_routes():iter() do if r:name()=="Aion test" then route=r end end
assert(route,"Saved bus missing")
proc=route:nth_plugin(0);controls=getControls(proc)
peaks("session_reload",1)
controls=nil;proc=nil;route=nil;collectgarbage();close_session();session=nil
print("PASS")
