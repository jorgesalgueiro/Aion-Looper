// SPDX-License-Identifier: AGPL-3.0-only
// Actual unchanged AudioGraph._createCompressor + original smoothSetParam in
// test-only OfflineAudioContexts. Native test process receives identical PCM.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{spawnSync}=require('node:child_process');
const reference=require('./CompressorReference.cjs'),{names,defaults,create,smooth,presets,presetValues}=reference;
const root=path.resolve(__dirname,'..'),args=process.argv.slice(2),argument=name=>{const i=args.indexOf(name);return i<0?undefined:args[i+1];};
const probe=argument('--probe')||path.join(root,'build/windows-core/Release/aion_compressor_probe.exe');
const sourceHash=crypto.createHash('sha256').update(create+'\n'+smooth).digest('hex');
const wait=ms=>new Promise(r=>setTimeout(r,ms));let socket,maximum=0,cases=0,totalFrames=0;
async function connect(){
 let page;for(let i=0;i<80;++i){try{page=(await(await fetch('http://127.0.0.1:9237/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(page)break;await wait(250);}assert(page,'Owned compressor WebView2 page unavailable');
 socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
 let sequence=0;const pending=new Map();
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++sequence,timer=setTimeout(()=>{pending.delete(id);reject(Error('Compressor CDP timeout: '+method));},30000);pending.set(id,r=>{clearTimeout(timer);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});socket.send(JSON.stringify({id,method,params}));});
 socket.addEventListener('message',e=>{const r=JSON.parse(e.data),reply=pending.get(r.id);if(reply){pending.delete(r.id);reply(r);}});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 const version=await call('Browser.getVersion');await call('Page.navigate',{url:'about:blank'});
 let ready=false;for(let i=0;i<80;++i){try{ready=await evaluate(`location.href==='about:blank'&&document.readyState==='complete'`);}catch{}if(ready)break;await wait(100);}assert(ready,'Owned blank oracle must load');return {evaluate,version};
}
async function render(evaluate,rate,initial,kind='burst',channels=2,events=[]){
 return evaluate(`(async()=>{
  const rate=${rate},count=32768,channels=${channels},kind=${JSON.stringify(kind)},initial=${JSON.stringify(initial)},names=${JSON.stringify(names)};
  const ctx=new OfflineAudioContext(2,count,rate),state={audioContext:ctx},AudioEngine={currentTime:0},effects={compressor:Object.fromEntries(names.map((n,i)=>[n,initial[i]]))};
  class Graph {${create}} class Original {${smooth}}
  const graph=new Graph();graph.loop={params:{}};
  const buffer=ctx.createBuffer(channels,count,rate),input=[];let seed=123456789;
  for(let ch=0;ch<channels;++ch){const data=buffer.getChannelData(ch);for(let i=0;i<count;++i){seed=(Math.imul(seed,1664525)+1013904223)>>>0;
   data[i]=kind==='silence'?0:kind==='impulse'?(i===0?(ch?-.25:1):0):kind==='step'?(i>=128&&i<12000?(ch?-.7:.5):0):
    i>=count-8192?0:((Math.floor(i/2048)%3===0?.02:Math.floor(i/2048)%3===1?.7:1.4)*(Math.sin(i*(ch?.073:.117))+.12*(seed/4294967296-.5)));
  }input.push(Array.from(data));}
  const source=ctx.createBufferSource();source.buffer=buffer;const fx=graph._createCompressor(source);fx.output.connect(ctx.destination);source.start();
  const changes=${JSON.stringify(events)},pauses=changes.map(e=>ctx.suspend(e[0]/rate)),applied=[];
  const rendering=ctx.startRendering();
  for(let i=0;i<changes.length;++i){await pauses[i];const [requested,p,value]=changes[i],param=p<5?fx.nodes[0][names[p]]:fx.nodes[p===5?1:3].gain;
   applied.push([Math.round(ctx.currentTime*rate),p,value,param.value]);Original.smoothSetParam(param,value,ctx.currentTime);
   if(p===6)Original.smoothSetParam(fx.nodes[2].gain,1-value,ctx.currentTime);await ctx.resume();}
  const out=await rendering;return {rate,initial,kind,channels,input,events:applied,pcm:[Array.from(out.getChannelData(0)),Array.from(out.getChannelData(1))]};
 })()`);
}
function compare(test,block){
 const commands=[`prepare ${test.rate} ${test.initial.map(Math.fround).join(' ')}`],expected=[null];let position=0;const count=test.pcm[0].length;
 function part(end){if(end<=position)return;commands.push(`render ${end-position} ${test.channels} ${block} `+Array.from({length:end-position},(_,i)=>test.input.map(ch=>ch[position+i]).join(' ')).join(' '));expected.push([position,end]);position=end;}
 for(const [frame,p,value]of test.events){part(frame);commands.push(`target ${p} ${Math.fround(value)}`);expected.push(null);}part(count);
 const run=spawnSync(probe,[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);
 const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,expected.length);
 let peak=0,power=0,errorPower=0,location=[],firstErrors=[];
 for(let j=0;j<lines.length;++j){if(!expected[j]){assert.equal(lines[j],'ok');continue;}const [start,end]=expected[j],pcm=lines[j].split(' ').slice(1).map(Number);assert.equal(pcm.length,(end-start)*2);
  for(let i=start;i<end;++i)for(let ch=0;ch<2;++ch){const actual=pcm[(i-start)*2+ch],wanted=test.pcm[ch][i],error=actual-wanted;assert(Number.isFinite(actual)&&Number.isFinite(wanted));power+=wanted*wanted;errorPower+=error*error;if(Math.abs(error)>1e-6&&firstErrors.length<12)firstErrors.push([i,ch,actual,wanted]);if(Math.abs(error)>peak){peak=Math.abs(error);location=[i,ch,actual,wanted];}}
 }
 const relative=power?Math.sqrt(errorPower/power):Math.sqrt(errorPower);maximum=Math.max(maximum,peak);++cases;totalFrames+=count;
 console.log(JSON.stringify({rate:test.rate,label:test.label,kind:test.kind,channels:test.channels,block,peak,relativeRms:relative,location}));
 assert(peak<=1e-6&&relative<=1e-5,`Original compressor sound mismatch ${JSON.stringify({rate:test.rate,label:test.label,block,peak,relative,location,firstErrors,events:test.events})}`);
}
(async()=>{
 try{
  const fixtureFile=argument('--fixtures');let data;
  if(fixtureFile){data=JSON.parse(fs.readFileSync(fixtureFile));assert.equal(data.version,1);assert.equal(data.sourceHash,sourceHash,'Unchanged reference methods must match the recorded browser oracle');}
  else{
   const {evaluate,version}=await connect();data={version:1,sourceHash,browser:version,tests:[]};
   for(const rate of [8000,22050,32000,44100,48000,96000,192000])data.tests.push({...await render(evaluate,rate,defaults),label:'untouched defaults'});
   for(const name of Object.keys(presets))data.tests.push({...await render(evaluate,48000,presetValues(name)),label:name});
   for(const kind of ['silence','impulse','step'])data.tests.push({...await render(evaluate,48000,defaults,kind),label:kind});
   data.tests.push({...await render(evaluate,48000,defaults,'burst',1),label:'mono duplicated'});
   for(const [label,initial]of [['immediate dry',[...defaults.slice(0,6),0]],['parallel',[...defaults.slice(0,6),.35]],['zero attack / unity ratio',[-16,1,0,0,.01,1,1]],['extreme',[-100,20,40,0,1,20,1]]])data.tests.push({...await render(evaluate,48000,initial),label});
   for(const rate of [22050,44100,48000,96000])data.tests.push({...await render(evaluate,rate,defaults,'burst',2,[[256,0,-40],[1024,0,-8],[2048,1,15],[4096,2,0],[6144,3,.15],[8192,4,.8],[10240,5,4],[12288,6,.3],[14336,6,1],[16384,5,0],[18432,5,0],[20480,6,0],[22528,6,1]]),label:'original live smoothSetParam'});
   fs.mkdirSync(path.join(root,'build'),{recursive:true});fs.writeFileSync(argument('--write-fixtures')||path.join(root,'build/compressor-browser-fixtures.json'),JSON.stringify(data));
  }
  for(const test of data.tests)for(const block of [64,128])compare(test,block);
  console.log(JSON.stringify({result:'PASS',scope:'Isolated original compressor graph/presets/automation, actual WebView2 oracle; not connected feature or Linux WebKit parity',cases,totalFrames,maximumAbsoluteError:maximum,peakTolerance:1e-6,relativeRmsTolerance:1e-5,browser:data.browser.product}));
 }finally{socket?.close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
