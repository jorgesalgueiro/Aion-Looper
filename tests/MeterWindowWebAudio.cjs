// Real WebView2 OfflineAudioContext, test host only; no physical audio device.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),delay=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 let page;for(let i=0;i<100;++i){try{page=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.type==='page'&&t.url.includes('juce.backend'));}catch{}if(page)break;await delay(100);}assert(page,'Owned synthetic host page');
 const ws=socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((resolve,reject)=>{ws.addEventListener('open',resolve,{once:true});ws.addEventListener('error',reject,{once:true});});
 let id=0;const pending=new Map();ws.addEventListener('message',e=>{const r=JSON.parse(e.data);pending.get(r.id)?.(r);});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const seq=++id,timer=setTimeout(()=>{pending.delete(seq);reject(Error('CDP timeout '+method));},15000);pending.set(seq,r=>{clearTimeout(timer);pending.delete(seq);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});ws.send(JSON.stringify({id:seq,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 let cases=0,samples=0,maximum=0;
 for(const rate of [8000,44100,48000,96000])for(const block of [64,128])for(const kind of ['mono','stereo','cancel','varying','short']){
  const channels=['mono','short'].includes(kind)?1:2,frames=kind==='short'?128:kind==='varying'?4096:2176;
  const rows=Array.from({length:frames},(_,i)=>Array.from({length:channels},(_,c)=>Math.fround(kind==='short'?0.5-i/512:kind==='varying'?0.8*Math.sin(i*0.031+c*0.7):kind==='cancel'?(c?-0.25:0.25):c?-0.125:0.375)));
  const browser=await evaluate(`(async()=>{const rows=${JSON.stringify(rows)},ctx=new OfflineAudioContext(2,${frames},${rate}),buffer=ctx.createBuffer(${channels},${frames},${rate});for(let c=0;c<${channels};++c)buffer.getChannelData(c).set(rows.map(row=>row[c]));const src=ctx.createBufferSource(),analyser=ctx.createAnalyser();analyser.fftSize=2048;analyser.smoothingTimeConstant=0.5;src.buffer=buffer;src.connect(analyser);analyser.connect(ctx.destination);src.start();await ctx.startRendering();const result=new Float32Array(2048);analyser.getFloatTimeDomainData(result);return Array.from(result);})()`);
  const native=spawnSync(path.join(root,'build/windows-core/Release/aion_meter_window_probe.exe'),[],{input:`${channels} ${frames} ${block}\n`+rows.map(row=>row.join(' ')).join('\n')+'\n',encoding:'utf8'});assert.equal(native.status,0,native.stderr);
  const actual=native.stdout.trim().split(/\s+/).map(Number);assert.equal(actual.length,2048);assert.equal(browser.length,2048);
  for(let i=0;i<2048;++i){const error=Math.abs(actual[i]-browser[i]);maximum=Math.max(maximum,error);assert(error<=1e-6,JSON.stringify({kind,rate,block,i,native:actual[i],browser:browser[i]}));++samples;}
  ++cases;
 }
 console.log(JSON.stringify({result:'PASS',cases,comparedSamples:samples,maximumAbsoluteError:maximum,tolerance:1e-6,scope:'Actual WebView2 2048-sample AnalyserNode mono/stereo downmix, zero prefix, varying signal chronology and wrap; 64/128 native partitions. Native audible-clock/IPC/UI/physical latency excluded.'},null,2));ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
