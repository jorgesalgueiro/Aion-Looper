// SPDX-License-Identifier: AGPL-3.0-only
// Actual WebView2 OfflineAudioContext + unchanged original worklet and original
// smoothSetParam method. Creates test-only contexts; product audio stays native.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process'),{loadReference}=require('./EqReference.cjs');
const root=path.resolve(__dirname,'..'),reference=loadReference(),descriptors=reference.create(48000).Processor.parameterDescriptors;
const names=descriptors.map(p=>p.name),defaults=descriptors.map(p=>Math.fround(p.defaultValue));
const effectsSource=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8').replace(/\r\n/g,'\n');
const methodStart=effectsSource.indexOf('    static smoothSetParam('),methodEnd=effectsSource.indexOf('\n    }',methodStart)+6;assert(methodStart>0&&methodEnd>methodStart);
const smoothingMethod=effectsSource.slice(methodStart,methodEnd),probe=process.argv.slice(2).find(v=>!v.startsWith('--'))||path.join(root,'build/windows-core/Release/aion_eq_probe.exe');
let socket;const wait=ms=>new Promise(r=>setTimeout(r,ms));
async function connect(){
 let page;for(let i=0;i<80;++i){try{page=(await(await fetch('http://127.0.0.1:9236/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(page)break;await wait(250);}assert(page,'Owned EQ test WebView2 page not found');
 socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
 let sequence=0;const pending=new Map();
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++sequence,timer=setTimeout(()=>{pending.delete(id);reject(Error(`EQ CDP timeout: ${method}`));},30000);pending.set(id,r=>{clearTimeout(timer);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});socket.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const response=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!response.exceptionDetails,JSON.stringify(response.exceptionDetails));return response.result.value;};
 const testUrl='https://juce.backend/eq-sound-oracle.html';
 socket.addEventListener('message',e=>{const r=JSON.parse(e.data);const reply=pending.get(r.id);if(reply){pending.delete(r.id);reply(r);}
  if(r.method==='Fetch.requestPaused'){
   const html='<!doctype html><html><head><title>Test-only original EQ sound oracle</title></head><body>Offline sound comparison</body></html>';
   // Serve exactly one debugger-owned fixture document. No product CSP changes,
   // original scripts/bootstrap, network fetches or persistent resource changes.
   const requestId=r.params.requestId;
   const response=r.params.request.url===testUrl?call('Fetch.fulfillRequest',{requestId,responseCode:200,responseHeaders:[{name:'Content-Type',value:'text/html; charset=utf-8'}],body:Buffer.from(html).toString('base64')}):call('Fetch.continueRequest',{requestId});
   response.catch(error=>{console.error(error);socket.close();});
  }
 });
 if(process.argv.includes('--toggle-only')){
  // This oracle only uses built-in Gain/ConstantSource nodes. An allowed blank
  // document needs no Blob worklet/CSP override or synthetic resource response.
  await call('Page.navigate',{url:'about:blank'});
  for(let i=0;i<80;++i){if(await evaluate(`location.href==='about:blank'&&document.readyState==='complete'`))return evaluate;await wait(100);}
  assert.fail('Owned blank toggle document did not load');
 }
 await call('Fetch.enable',{patterns:[{urlPattern:testUrl,requestStage:'Request'}]});
 const navigation=await call('Page.navigate',{url:testUrl});
 let loaded=false;for(let i=0;i<80;++i){try{loaded=await evaluate(`location.href===${JSON.stringify(testUrl)}&&document.readyState==='complete'`);}catch{}if(loaded)break;await wait(100);}assert(loaded,'Offline EQ fixture must load in owned WebView2: '+JSON.stringify({navigation,document:await evaluate(`({url:location.href,state:document.readyState})`)}));
 return evaluate;
}
async function browserRender(evaluate,rate,initial,events,channels=2){
 return evaluate(`(async()=>{
  const rate=${rate},count=${events.length?16384:2048},channels=${channels},initial=${JSON.stringify(initial)},names=${JSON.stringify(names)};
  const ctx=new OfflineAudioContext(channels,count,rate),url=URL.createObjectURL(new Blob([${JSON.stringify(reference.source)}],{type:'text/javascript'}));
  try{
   await ctx.audioWorklet.addModule(url);
   const parameterData=Object.fromEntries(names.map((name,i)=>[name,name.endsWith('Freq')?Math.min(initial[i],rate*.5*(1-1e-6)):initial[i]]));
   const eq=new AudioWorkletNode(ctx,'eq-processor',{numberOfInputs:1,numberOfOutputs:1,outputChannelCount:[channels],parameterData});
   const buffer=ctx.createBuffer(channels,count,rate),input=[];
   for(let ch=0;ch<channels;++ch){const data=buffer.getChannelData(ch);for(let i=0;i<count;++i)data[i]=i>=count-1024?0:.12*Math.sin(i*(ch?.073:.117))+.025*Math.cos(i*(ch?.231:.037));input.push(Array.from(data));}
   const source=ctx.createBufferSource();source.buffer=buffer;source.connect(eq);eq.connect(ctx.destination);source.start();
   class Original {${smoothingMethod}}
   const events=${JSON.stringify(events)},pauses=events.map(e=>ctx.suspend(e[0]/rate)),applied=[];
   const rendered=ctx.startRendering();
   for(let i=0;i<events.length;++i){await pauses[i];const [requested,name,target]=events[i],parameter=eq.parameters.get(name),frame=Math.round(ctx.currentTime*rate);applied.push([frame,name,target,parameter.value]);Original.smoothSetParam(parameter,name.endsWith('Freq')?Math.min(target,rate*.5*(1-1e-6)):target,ctx.currentTime);await ctx.resume();}
   const output=await rendered;return {input,events:applied,pcm:Array.from({length:channels},(_,ch)=>Array.from(output.getChannelData(ch)))};
  }finally{URL.revokeObjectURL(url);}
 })()`);
}
let maximum=0,totalFrames=0,cases=0;
function compare(rate,block,label,initial,browser){
 const commands=[`${browser.events.length?'smooth':'prepare'} ${rate} ${initial.join(' ')}`],expected=[null];let position=0;const count=browser.pcm[0].length,channels=browser.input.length;
 const render=end=>{if(end<=position)return;commands.push(`render ${end-position} ${channels} ${block} `+Array.from({length:end-position},(_,i)=>browser.input.map(ch=>ch[position+i]).join(' ')).join(' '));expected.push([position,end]);position=end;};
 for(const [frame,name,value]of browser.events){render(frame);commands.push(`target ${names.indexOf(name)} ${Math.fround(value)}`);expected.push(null);}render(count);
 const run=spawnSync(probe,[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:16*1024*1024});assert.equal(run.status,0,run.stderr);const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,expected.length);let errorPower=0,power=0,peak=0;
 for(let l=0;l<lines.length;++l){if(!expected[l]){assert.equal(lines[l],'ok');continue;}const [start,end]=expected[l],pcm=lines[l].split(' ').slice(1).map(Number);assert.equal(pcm.length,(end-start)*2);
  for(let i=start;i<end;++i)for(let ch=0;ch<channels;++ch){const wanted=browser.pcm[ch][i],sample=pcm[(i-start)*2+ch],difference=sample-wanted;assert(Number.isFinite(wanted)&&Number.isFinite(sample));errorPower+=difference*difference;power+=wanted*wanted;peak=Math.max(peak,Math.abs(difference));}
 }
 const relative=Math.sqrt(errorPower/power);maximum=Math.max(maximum,peak);totalFrames+=count;cases++;
 console.log(JSON.stringify({rate,block,label,channels,maximumAbsoluteError:peak,relativeRmsError:relative}));
 if(peak>1e-6)console.error('Original AudioParam retarget anchors:',JSON.stringify(browser.events));
 assert(peak<=1e-6&&relative<=1e-5,`${label} actual WebAudio parity: peak ${peak}, relative RMS ${relative}`);
}
(async()=>{const evaluate=await connect();
 if(process.argv.includes('--toggle-only')){
  try{
   const audio=fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8').replace(/\r\n/g,'\n');
   const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
   const extract=(source,needle)=>{const start=source.indexOf(needle),end=source.indexOf('\n    }',start)+6;assert(start>0&&end>start);return source.slice(start,end);};
   const fade=extract(audio,'    static scheduledFade('),rebuild=extract(loops,'    rebuild(');const cases=[];
   for(const rate of [8000,22050,32000,44100,48000,96000])for(const level of [.25,.7,1,1.6]){
    const result=await evaluate(`(async()=>{
     const rate=${rate},level=${level},at=1024,count=at+Math.ceil(rate*.08),ctx=new OfflineAudioContext(1,count,rate);
     const gain=ctx.createGain();gain.gain.value=level;gain.connect(ctx.destination);
     const source=ctx.createConstantSource(),base=ctx.createGain();source.connect(base);base.connect(gain);source.start();
     const AudioEngine=new class {get currentTime(){return ctx.currentTime;} ${fade.replace('static scheduledFade','scheduledFade')}};
     const graph=new class {${rebuild}};graph.nodes={baseGain:base,volume:gain,effects:{}};graph.loop={effectiveVolume:level};
     graph.detachEffects=()=>{};graph.buildEffectsChain=node=>node;graph.destroyEffects=()=>{};
     const Log={debug(){}};let before,after;
     const suspended=ctx.suspend(at/rate).then(()=>{before=gain.gain.value;graph.rebuild();after=gain.gain.value;return ctx.resume();});
     const rendered=ctx.startRendering();await suspended;const buffer=await rendered;
     return {rate,level,before,after,pcm:Array.from(buffer.getChannelData(0).slice(at))};
    })()`);
    for(const sample of result.pcm)assert(Math.abs(sample-Math.fround(level))<=1e-6,'Unchanged original rebuild gain must retain its actual browser envelope');
    cases.push(result);console.log(JSON.stringify({rate,level,before:result.before,after:result.after,min:Math.min(...result.pcm),max:Math.max(...result.pcm),samples:[0,.01,.02,.03,.04].map(t=>result.pcm[Math.floor(t*rate)])}));
   }
   fs.writeFileSync(path.join(root,'build/eq-toggle-oracle-windows.json'),JSON.stringify(cases));
   const connected=spawnSync(process.execPath,[path.join(root,'tests/EqConnectedParity.cjs'),path.join(root,'build/windows-core/Release/aion_eq_connected_tests.exe')],{encoding:'utf8',maxBuffer:4*1024*1024});
   assert.equal(connected.status,0,connected.stdout+'\n'+connected.stderr);console.log(connected.stdout.trim());
   console.log('PASS: 24 actual-browser unchanged rebuild/scheduledFade gain cases; settled toggle gain flat, no invented native duck; strict connected native 64/128 PCM');
  }finally{socket?.close();}return;
 }
 try{
  for(const rate of [8000,22050,32000,44100,48000,96000])for(const name of ['Default','Radio','Master Air']){
   const initial=[...defaults];for(const [key,value]of Object.entries(reference.presets[name]||{}))initial[names.indexOf(key)]=Math.fround(value);
   const browser=await browserRender(evaluate,rate,initial,[]);for(const block of [64,128])compare(rate,block,name,initial,browser);
  }
  for(const channels of [1,2])for(const rate of [44100,48000,96000]){
   const events=[[256,'p1Gain',12],[512,'p1Freq',3200],[1024,'p1Gain',-9],[1408,'p1Q',2.5],[2048,'lsGain',4],[3072,'p1Gain',0],[4096,'p1Gain',12],[5120,'p1Gain',-12],[6144,'hcFreq',15000],[8192,'p2Gain',6],[10240,'p2Gain',0]];
   const browser=await browserRender(evaluate,rate,defaults,events,channels);for(const block of [64,128])compare(rate,block,'original smoothSetParam live retarget',defaults,browser);
   const anchor=browser.events.find(e=>e[0]===1024&&e[1]==='p1Gain');assert(anchor&&Math.abs(anchor[3]-Math.fround(12*(1-Math.exp(-767/(rate*.05)))))<=1e-6,'Original retarget anchors the previous rendered sample, not currentTime');
  }
  console.log(JSON.stringify({result:'PASS',scope:'Actual WebView2 OfflineAudioContext original EQProcessor/SVF, original smoothSetParam during suspended rendering; isolated worklet/mono-stereo and approved low-rate limit, not product graph/UI acceptance',cases,stereoOrMonoFrames:totalFrames,maximumAbsoluteError:maximum,peakTolerance:1e-6,relativeRmsTolerance:1e-5}));
 }finally{socket?.close();}
})().catch(error=>{console.error(error);socket?.close();process.exitCode=1;});
