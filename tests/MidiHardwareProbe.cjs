// Interactive real-device observation for a muted Standalone session on CDP
// 9236. No physical MIDI messages are synthesized by this script.
const assert=require('node:assert/strict'),delay=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 let target;for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9236/json')).json()).find(t=>t.type==='page'&&t.url.includes('juce.backend'));}catch{}if(target)break;await delay(150);}assert(target,'Real Standalone MIDI test page');
 const ws=socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});let seq=0;const pending=new Map();
 ws.addEventListener('message',e=>{const r=JSON.parse(e.data);pending.get(r.id)?.(r);});
 const call=(method,params)=>new Promise((resolve,reject)=>{const id=++seq,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout'));},10000);pending.set(id,r=>{clearTimeout(timer);pending.delete(id);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});ws.send(JSON.stringify({id,method,params}));});
 for(let n=0;n<100;++n){const r=await call('Runtime.evaluate',{expression:`typeof window.AionNative?.request==='function'&&!!document.getElementById('map-midi-0')`,returnByValue:true});if(r.result.value)break;await delay(100);}
 const rpc=async request=>{const r=await call('Runtime.evaluate',{expression:`AionNative.request(${JSON.stringify(request)})`,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));assert(r.result.value.ok,JSON.stringify(r.result.value));return r.result.value;};
 let devices=await rpc({operation:'midiDevices'}),s=await rpc({operation:'snapshot'});
 if(process.argv[2]==='prepare'){
  assert(devices.standalone,'Real Standalone owns hardware routing');
  for(const [id,value] of [['masterGain',0],['inputMonitor',0]])await rpc({operation:'setParameter',id,value});
  if(devices.devices.length){await rpc({operation:'midiConfig',action:'learn',target:0});await delay(150);devices=await rpc({operation:'midiDevices'});s=await rpc({operation:'snapshot'});
   await call('Runtime.evaluate',{expression:`document.getElementById('mod-mapping').scrollIntoView({block:'center',behavior:'instant'})`});}
 }
 if(process.argv[2]==='cancel'){await rpc({operation:'midiConfig',action:'cancelLearn'});s=await rpc({operation:'snapshot'});}
 console.log(JSON.stringify({devices:devices.devices,standalone:devices.standalone,audioReady:s.audioReady,sampleRate:s.sampleRate,frame:s.frame,inputAvailable:s.inputAvailable,syncMode:s.syncMode,syncBpm:s.syncBpm,syncBars:s.syncBars,mode:s.midi.mode,midi:s.midi,loop0:{state:s.tracks[0].state,length:s.tracks[0].length}},null,2));ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
