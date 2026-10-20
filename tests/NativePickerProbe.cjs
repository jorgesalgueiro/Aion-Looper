// Setup/status helper for interactive OS-picker validation. Run ONLY against
// aion_mouse_host --native-picker on CDP 9235 (synthetic audio, no device).
// Destination selection remains an actual operating-system dialog action.
const assert=require('node:assert/strict');
const delay=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 let target;for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.type==='page'&&t.url.includes('juce.backend'));}catch{}if(target)break;await delay(150);}assert(target);
 const ws=socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});
 let seq=0;const pending=new Map();ws.addEventListener('message',e=>{const r=JSON.parse(e.data);pending.get(r.id)?.(r);});
 const call=(method,params)=>new Promise((resolve,reject)=>{const id=++seq,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout'));},10000);pending.set(id,r=>{clearTimeout(timer);pending.delete(id);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});ws.send(JSON.stringify({id,method,params}));});
 const rpc=async request=>{const r=await call('Runtime.evaluate',{expression:`AionNative.request(${JSON.stringify(request)})`,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails);assert(r.result.value.ok,JSON.stringify(r.result.value));return r.result.value;};
 let s=await rpc({operation:'snapshot'});
 if(process.argv[2]==='cancel'){await rpc({operation:'cancelExport'});await delay(100);s=await rpc({operation:'snapshot'});}
 if(process.argv[2]==='prepare'){
  assert(s.tracks.every(t=>t.state==='empty'),'Use a fresh synthetic host');
  for(const [id,value] of [['syncMode',0],['masterGain',0],['autoPlayAfterRecord',1]])await rpc({operation:'setParameter',id,value});
  await rpc({operation:'loop',action:'record',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0});await delay(400);
  await rpc({operation:'loop',action:'finish',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0});
  for(let n=0;n<100;++n){s=await rpc({operation:'snapshot'});if(s.tracks[0].state==='playing'&&s.tracks[0].waveformId)break;await delay(20);}
 }
 if(['prepare','export'].includes(process.argv[2])){
  assert(s.tracks[0].length>0);await rpc({operation:'exportLoop',track:0,generation:s.generation,waveformId:s.tracks[0].waveformId,filename:`NativePicker_${Date.now()}.wav`});await delay(150);s=await rpc({operation:'snapshot'});
 }
 console.log(JSON.stringify({state:s.tracks[0].state,length:s.tracks[0].length,sampleRate:s.sampleRate,exportBusy:s.exportBusy,exportStatus:s.exportStatus,filename:s.exportFilename}));ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
