const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..');let socket;const delay=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{let page;for(let i=0;i<80;i++){try{page=(await(await fetch('http://127.0.0.1:9234/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(page)break;await delay(250);}assert(page);const ws=socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});let id=0;const pending=new Map();ws.addEventListener('message',e=>{const r=JSON.parse(e.data);if(pending.has(r.id)){pending.get(r.id)(r);pending.delete(r.id);}});const call=(method,params={})=>new Promise((resolve,reject)=>{const i=++id,timer=setTimeout(()=>reject(Error('CDP timeout')),15000);pending.set(i,r=>{clearTimeout(timer);r.error?reject(r.error):resolve(r.result);});ws.send(JSON.stringify({id:i,method,params}));});const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
try {
 const source=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
 const extract=name=>{const start=source.indexOf('    '+name+'(');assert(start>=0);return source.slice(start,source.indexOf('\n    }',start)+6);};
 const methods=extract('stutterStart')+extract('stutterStop');let samples=0,maximum=0;
 for(const rate of [8000,48000,96000])for(const tap of [256,3968])for(const origin of [0,8192]){
  const length=4096,release=6144,total=7168,commands=[`prepare 8192 ${rate}`,'record 0 0'];
  for(let i=0;i<length;i++)commands.push(`tick 0 ${Math.fround(0.3*Math.sin(i*0.071))} ${Math.fround(0.2*Math.cos(i*0.13))}`);
  commands.push('finish 0 0','wait 0 0','retrigger 0 0');const baselineAt=commands.length;
  for(let i=0;i<length;i++)commands.push('tick 0 0');if(origin===0)commands.push('timeline 0 0');commands.push('retrigger 0 0');const positions=[];
  for(let i=0;i<total;i++){if(i===tap)commands.push('stut 0 1','region 0 0');if(i===release)commands.push('stut 0 0');positions.push(commands.length);commands.push('tick 0 0');}
  const result=spawnSync(path.join(root,'build/windows-core/Release/aion_start_probe.exe'),[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(result.status,0,result.stderr);const rows=result.stdout.trim().split(/\r?\n/).map(r=>r.split(' ').map(Number));rows.forEach((r,i)=>assert.equal(r[0],1,commands[i]));
  const baseline=rows.slice(baselineAt,baselineAt+length).map(r=>r[6]),native=positions.map(i=>rows[i][6]);
  const rendered=await evaluate(`(async()=>{
   const ctx=new OfflineAudioContext(1,${total+origin},${rate}),buffer=ctx.createBuffer(1,${length},${rate});buffer.getChannelData(0).set(${JSON.stringify(baseline)});
   const node=ctx.createBufferSource();node.buffer=buffer;node.loop=true;node.loopEnd=buffer.duration;node.connect(ctx.destination);node.start(${origin/rate});
   const AudioEngine={get currentTime(){return ctx.currentTime;}},state={bpm:120};class Original {${methods}}
   const loop={state:'playing',duration:buffer.duration,effectivePlaybackRate:1,graph:{source:node,startOffset:0,startTime:${origin/rate}}};
   const down=ctx.suspend(${(origin+tap)/rate}),up=ctx.suspend(${(origin+release)/rate}),render=ctx.startRendering();await down;Original.prototype.stutterStart.call(loop);const bounds=[node.loopStart*${rate},node.loopEnd*${rate}];await ctx.resume();await up;Original.prototype.stutterStop.call(loop);await ctx.resume();
   return {pcm:Array.from((await render).getChannelData(0)).slice(${origin}),bounds};
  })()`);
  console.log(JSON.stringify({rate,tap,origin,browserRegion:rendered.bounds,nativeRegion:rows[commands.indexOf('region 0 0')].slice(2,4)}));
  const browser=rendered.pcm;assert.equal(browser.length,native.length);let max=0;for(let i=0;i<total;i++){const e=Math.abs(native[i]-browser[i]);max=Math.max(max,e);assert(e<=1e-6,`Actual WebAudio STUT ${rate}/${tap}/origin${origin}, frame ${i}: ${native[i]} vs ${browser[i]}`);}maximum=Math.max(maximum,max);samples+=total;
 }
 console.log(JSON.stringify({result:'PASS',cases:12,samples,maximumAbsoluteError:maximum,tolerance:1e-6,scope:'Actual WebAudio unchanged STUT methods at 1x, quantum-aligned press/release, ordinary and clip-tail fragments; native-conditioned stereo-fold source, no playback envelopes'}));
}finally{socket?.close();}})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
