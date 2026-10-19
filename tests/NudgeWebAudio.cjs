const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..');let socket;const delay=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{let page;for(let i=0;i<80;i++){try{page=(await(await fetch('http://127.0.0.1:9234/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(page)break;await delay(250);}assert(page);const ws=socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});let id=0;const pending=new Map();ws.addEventListener('message',e=>{const r=JSON.parse(e.data);if(pending.has(r.id)){pending.get(r.id)(r);pending.delete(r.id);}});const call=(method,params={})=>new Promise((resolve,reject)=>{const i=++id,timer=setTimeout(()=>reject(Error('CDP timeout')),15000);pending.set(i,r=>{clearTimeout(timer);r.error?reject(r.error):resolve(r.result);});ws.send(JSON.stringify({id:i,method,params}));});const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
try {
 const source=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
 const extract=name=>{const start=source.indexOf('    static '+name+'(');assert(start>=0);return source.slice(start,source.indexOf('\n    }',start)+6);};
 const methods=extract('nudgeLoopStart')+extract('setLoopStartDelay');
 const result=await evaluate(`(async()=>{
  const ctx=new OfflineAudioContext(1,128,8000),buffer=ctx.createBuffer(1,17,8000);buffer.getChannelData(0).fill(0.25);
  const node=ctx.createBufferSource();node.buffer=buffer;node.loop=true;node.connect(ctx.destination);
  const loop={duration:buffer.duration,startDelay:0.217,state:'playing',restart:t=>node.start(t,loop.startDelay*loop.duration)};
  const state={bpm:120,loops:[loop]},AudioEngine={currentTime:0},document={getElementById:()=>null};
  class Original {${methods}}
  let error='';try{Original.nudgeLoopStart(0,-1);}catch(e){error=e.name;}
  const raw=loop.startDelay,phase=((raw%1)+1)%1;
  const fixed=new OfflineAudioContext(1,128,8000),fixedBuffer=fixed.createBuffer(1,17,8000);fixedBuffer.getChannelData(0).fill(0.25);
  const playable=fixed.createBufferSource();playable.buffer=fixedBuffer;playable.loop=true;playable.connect(fixed.destination);playable.start(0,phase*fixedBuffer.duration);
  const pcm=(await fixed.startRendering()).getChannelData(0);
  return {raw,phase,error,finite:Array.from(pcm).every(Number.isFinite),peak:Math.max(...pcm)};
 })()`);
 assert(result.raw<0&&result.error==='RangeError','Reproduce unchanged nudge negative-offset failure: '+JSON.stringify(result));
 assert(result.phase>=0&&result.phase<1&&result.finite&&result.peak>0.1,'Wrapped tiny source remains valid/playable');
 console.log('PASS: actual WebAudio rejects unchanged tiny-clip negative nudge offset; wrapped source renders finite PCM: '+JSON.stringify(result));
}finally{socket?.close();}})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
