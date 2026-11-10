// SPDX-License-Identifier: AGPL-3.0-only
// Actual original worklet in test-only OfflineAudioContexts. No product changes.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{spawnSync}=require('node:child_process');
const {original,presets,hostDefaults,names,values}=require('./ArpDelayReference.cjs');
const root=path.resolve(__dirname,'..'),args=process.argv.slice(2),option=name=>{const n=args.indexOf(name);return n<0?null:args[n+1];};
const probe=option('--probe')||path.join(root,'build/windows-core/Release/aion_arp_probe.exe'),hash=crypto.createHash('sha256').update(original).digest('hex');
const wait=ms=>new Promise(r=>setTimeout(r,ms));let socket;
async function connect(){
 let page;for(let n=0;n<80;++n){try{page=(await(await fetch('http://127.0.0.1:9239/json')).json()).find(p=>p.url.includes('juce.backend'));}catch{}if(page)break;await wait(250);}assert(page,'Owned Arp Delay host unavailable');
 socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
 let serial=0;const pending=new Map();
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial,timer=setTimeout(()=>{pending.delete(id);reject(Error(method+' timeout'));},30000);pending.set(id,r=>{clearTimeout(timer);pending.delete(id);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});socket.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 socket.addEventListener('message',e=>{const r=JSON.parse(e.data);pending.get(r.id)?.(r);});
 const browser=await call('Browser.getVersion');await call('Page.enable');
 let ready=false;for(let n=0;n<100;++n){try{ready=await evaluate(`location.href.includes('juce.backend')&&document.readyState==='complete'&&!!document.querySelector('#effects-oracle')`);}catch{}if(ready)break;await wait(100);}assert(ready,'Use the owned effects browser fixture, not a product document');
 const context=await evaluate(`({secure:isSecureContext,worklet:!!new OfflineAudioContext(2,128,48000).audioWorklet})`);assert(context.secure&&context.worklet,JSON.stringify(context));return {evaluate,browser};
}
async function render(evaluate,rate,initial,channels=2){
 return evaluate(`(async()=>{
  const rate=${rate},count=Math.ceil(Math.max(65536,rate*.9)/128)*128,channels=${channels},initial=${JSON.stringify(initial)},names=${JSON.stringify(names)};
  const ctx=new OfflineAudioContext(2,count,rate);
  const preamble=${JSON.stringify('const getWorkletSampleRate=()=>sampleRate;let testSeed=123456789;Math.random=()=>{testSeed=(Math.imul(testSeed,1664525)+1013904223)>>>0;return testSeed/4294967296;};')};
  const url=URL.createObjectURL(new Blob([preamble,${JSON.stringify(original)}],{type:'text/javascript'}));
  try{
   await ctx.audioWorklet.addModule(url);
   const effect=new AudioWorkletNode(ctx,'arp-delay-processor',{numberOfInputs:1,numberOfOutputs:1,outputChannelCount:[2],parameterData:Object.fromEntries(names.map((name,i)=>[name,initial[i]]))});
   const buffer=ctx.createBuffer(channels,count,rate),input=[];
   for(let ch=0;ch<channels;++ch){const a=buffer.getChannelData(ch);for(let i=0;i<count;++i)a[i]=i>=count-4096?0:.15*Math.sin(i*(ch?.073:.117))+.05*Math.cos(i*(ch?.021:.037));input.push(Array.from(a));}
   const source=ctx.createBufferSource();source.buffer=buffer;source.connect(effect);effect.connect(ctx.destination);source.start();
   const out=await ctx.startRendering();return {rate,initial,channels,input,pcm:[Array.from(out.getChannelData(0)),Array.from(out.getChannelData(1))]};
  }finally{URL.revokeObjectURL(url);}
 })()`);
}
function compare(test){
 const count=test.pcm[0].length,command=`prepare ${test.rate} 123456789 ${test.initial.join(' ')}\nrender ${count} ${test.channels} 128 `+Array.from({length:count},(_,i)=>test.input.map(a=>a[i]).join(' ')).join(' ')+'\n';
 const run=spawnSync(probe,[],{input:command,encoding:'utf8',maxBuffer:40*1024*1024});assert.equal(run.status,0,run.stderr);
 const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,2);assert.equal(lines[0],'ok');const pcm=lines[1].split(' ').slice(1).map(Number);assert.equal(pcm.length,count*2);
 let peak=0,energy=0,errorEnergy=0,location=[];
 for(let i=0;i<count;++i)for(let ch=0;ch<2;++ch){const a=pcm[i*2+ch],b=test.pcm[ch][i],error=a-b;assert(Number.isFinite(a)&&Number.isFinite(b));energy+=b*b;errorEnergy+=error*error;if(Math.abs(error)>peak){peak=Math.abs(error);location=[i,ch,a,b];}}
 const rms=energy?Math.sqrt(errorEnergy/energy):Math.sqrt(errorEnergy);console.log(JSON.stringify({label:test.label,rate:test.rate,channels:test.channels,peak,relativeRms:rms}));assert(peak<=1e-6&&rms<=1e-5,JSON.stringify({label:test.label,peak,rms,location}));return {peak,count};
}
if(require.main===module)(async()=>{
 try{
  let data;const fixture=option('--fixtures');
  if(fixture){data=JSON.parse(fs.readFileSync(fixture));assert.equal(data.version,1);assert.equal(data.sourceHash,hash,'Original worklet changed');}
  else {const {evaluate,browser}=await connect();data={version:1,sourceHash:hash,browser,tests:[]};
   for(const rate of [8000,22050,32000,44100,48000,96000])data.tests.push({...await render(evaluate,rate,values(hostDefaults)),label:'original defaults'});
   for(const [label,preset]of Object.entries(presets))data.tests.push({...await render(evaluate,48000,values(preset)),label});
   data.tests.push({...await render(evaluate,48000,values(hostDefaults),1),label:'mono duplication'});
   fs.writeFileSync(option('--save')||path.join(root,'build/arp-browser-fixtures.json'),JSON.stringify(data));
  }
  let maximum=0,frames=0;for(const test of data.tests){const result=compare(test);maximum=Math.max(maximum,result.peak);frames+=result.count;}
  console.log(JSON.stringify({result:'PASS',scope:'Actual original AudioWorklet only, test-seeded randomness; no post-pan/AudioParam/connected-device claim',cases:data.tests.length,stereoFrames:frames,maximumAbsoluteError:maximum,browser:data.browser.product}));
 }finally{socket?.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
module.exports={connect,close:()=>socket?.close()};
