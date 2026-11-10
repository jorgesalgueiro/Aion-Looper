// SPDX-License-Identifier: AGPL-3.0-only
// Actual UNCHANGED original factory + worklet + smoothSetParam, test-seeded RNG.
// Awake/tail sound parity; approved causal near-silence wake-up is a separate gate.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{spawnSync}=require('node:child_process');
const {connect,close}=require('./ArpDelayWebAudio.cjs'),{original,factory,presets,hostDefaults,names,values}=require('./ArpDelayReference.cjs');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8');
const smooth=source.match(/    static smoothSetParam\(audioParam, val, now, tc = 0.05\) \{[\s\S]*?\n    \}/)[0];
const args=process.argv.slice(2),option=name=>{const n=args.indexOf(name);return n<0?null:args[n+1];};
const probe=option('--probe')||path.join(root,'build/windows-core/Release/aion_arp_probe.exe'),hash=crypto.createHash('sha256').update(original+'\n'+factory+'\n'+smooth).digest('hex');
async function render(evaluate,rate,preset,channels,events,label){
 const initial=[...values(preset),preset.panSpeed??0,preset.panDepth??.8];initial[6]=137;
 return {...await evaluate(`(async()=>{
  const rate=${rate},count=65536,channels=${channels},p=${JSON.stringify(preset)},names=${JSON.stringify(names)},changes=${JSON.stringify(events)};
  const ctx=new OfflineAudioContext(2,count,rate),state={audioContext:ctx,bpm:137},effects={arpDelay:p},Log={error(...v){throw Error(v.join(' '));}};
  const prefix=${JSON.stringify('const getWorkletSampleRate=()=>sampleRate;let testSeed=123456789;Math.random=()=>{testSeed=(Math.imul(testSeed,1664525)+1013904223)>>>0;return testSeed/4294967296;};')};
  // Test-only observation around the unchanged process body; no parameter/PCM edits.
  const observer=${JSON.stringify(`const originalProcess=ArpDelayProcessor.prototype.process;ArpDelayProcessor.prototype.process=function(inputs,outputs,parameters){this.port.postMessage([currentFrame,...${JSON.stringify(names)}.map(n=>parameters[n][0]),...${JSON.stringify(names)}.map(n=>parameters[n][parameters[n].length-1])]);return originalProcess.call(this,inputs,outputs,parameters);};`)};
  const url=URL.createObjectURL(new Blob([prefix,${JSON.stringify(original)},observer],{type:'text/javascript'}));
  try{
   await ctx.audioWorklet.addModule(url);class Graph {${factory}} class Original {${smooth}}
   const graph=new Graph();graph.loop={params:{}};const buffer=ctx.createBuffer(channels,count,rate),input=[];
   for(let ch=0;ch<channels;++ch){const a=buffer.getChannelData(ch);for(let i=0;i<count;++i)a[i]=i>=count-4096?0:.15*Math.sin(i*(ch?.073:.117))+.05*Math.cos(i*(ch?.021:.037));input.push(Array.from(a));}
   const src=ctx.createBufferSource();src.buffer=buffer;const fx=graph._createArpDelay(src);assertFactory();function assertFactory(){if(fx.nodes.length!==4)throw Error('Original factory must create worklet/panner/oscillator/gain');}
   const trace=[];fx.nodes[0].port.onmessage=e=>trace.push(e.data);fx.output.connect(ctx.destination);src.start();
   const pauses=changes.map(e=>ctx.suspend(e[0]/rate)),applied=[],rendering=ctx.startRendering();
   for(let i=0;i<changes.length;++i){await pauses[i];const [requested,p,v]=changes[i],now=ctx.currentTime;
    const param=p<10?fx.nodes[0].parameters.get(names[p]):p===10?fx.nodes[2].frequency:fx.nodes[3].gain;
    applied.push([Math.round(now*rate),p,v]);Original.smoothSetParam(param,v,now);await ctx.resume();}
   const output=await rendering;
   for(let n=0;n<100&&trace.length<count/128;++n)await new Promise(r=>setTimeout(r,10));
   if(trace.length!==count/128)throw Error('Incomplete test-only worklet parameter observation');
   return {rate,channels,input,events:applied,trace,pcm:[Array.from(output.getChannelData(0)),Array.from(output.getChannelData(1))]};
  }finally{URL.revokeObjectURL(url);}
 })()`),initial,label};
}
function compare(test,block){
 const commands=[`graphprepare ${test.rate} 123456789 ${test.initial.map(Math.fround).join(' ')}`],expected=[null];let frame=0;
 const count=test.pcm[0].length;
 function part(end){if(end===frame)return;commands.push(`graphrender ${end-frame} ${test.channels} ${block} `+Array.from({length:end-frame},(_,i)=>test.input.map(ch=>ch[frame+i]).join(' ')).join(' '));expected.push([frame,end]);frame=end;}
 for(const [frame,p,value]of test.events){part(frame);commands.push(`graphtarget ${p} ${Math.fround(value)}`);expected.push(null);}part(count);
 const run=spawnSync(probe,[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:40*1024*1024});assert.equal(run.status,0,run.stderr);
 const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,expected.length);let peak=0,energy=0,errorEnergy=0,location=[];
 for(let line=0;line<lines.length;++line){const segment=expected[line];if(!segment){assert.equal(lines[line],'ok');continue;}
  const [begin,end]=segment,actual=lines[line].split(' ').slice(1).map(Number);assert.equal(actual.length,(end-begin)*2);
  for(let i=begin;i<end;++i)for(let ch=0;ch<2;++ch){const a=actual[(i-begin)*2+ch],b=test.pcm[ch][i],err=a-b;assert(Number.isFinite(a)&&Number.isFinite(b));energy+=b*b;errorEnergy+=err*err;if(Math.abs(err)>peak){peak=Math.abs(err);location=[i,ch,a,b];}}
 }
 const relativeRms=Math.sqrt(errorEnergy/energy);assert(energy>1,'Nonzero original factory PCM required');
 console.log(JSON.stringify({label:test.label,rate:test.rate,block,peak,relativeRms,location}));
 assert(peak<=1e-6&&relativeRms<=1e-5,JSON.stringify({label:test.label,rate:test.rate,block,peak,relativeRms,location}));return {peak,frames:count};
}
(async()=>{
 try{
  let data;const fixture=option('--fixtures');
  if(fixture){data=JSON.parse(fs.readFileSync(fixture));assert.equal(data.version,1);assert.equal(data.sourceHash,hash,'Unchanged original factory/worklet/automation must match');}
  else{
   const {evaluate,browser}=await connect();data={version:1,sourceHash:hash,browser,tests:[]};
   for(const rate of [8000,22050,32000,44100,48000,96000])data.tests.push(await render(evaluate,rate,hostDefaults,2,[],'original factory defaults'));
   for(const [label,preset]of Object.entries(presets))data.tests.push(await render(evaluate,48000,preset,2,[],label));
   data.tests.push(await render(evaluate,48000,{...hostDefaults,panSpeed:5,panDepth:1},1,[],'mono into original stereo worklet + fast pan'));
   const events=[[256,2,.2],[512,2,.8],[768,0,.04],[2048,4,19],[4096,3,1],[6144,3,0],[8192,5,1],[10240,6,300],[12288,7,1.5],[14336,8,3],[16384,9,1],
    [18432,1,.9],[20480,10,5],[22528,11,1],[24576,10,0],[24704,10,0],[26624,11,0],[28672,0,.4],[30720,2,1],[32768,5,0],[34816,4,0],[36864,1,0]];
   for(const rate of [22050,44100,48000,96000])data.tests.push(await render(evaluate,rate,hostDefaults,2,events,'live original factory controls'));
   const lower={...hostDefaults,time:.01,repetitions:0,mix:0,stay:0,scale:0,sync:0,amplitude:0,range:.25,humanize:0,panSpeed:0,panDepth:0};
   const upper=[4,.98,1,1,19,1,300,2,3,1,5,1],minimum=[.01,0,0,0,0,0,30,0,.25,0,0,0];
   const bounds=upper.map((v,p)=>[256+p*128,p,v]).concat(minimum.map((v,p)=>[20480+p*128,p,v]));
   for(const rate of [8000,48000])data.tests.push(await render(evaluate,rate,lower,2,bounds,'AudioParam nominal endpoints/cancellation'));
   fs.writeFileSync(option('--save')||path.join(root,'build/arp-graph-browser-fixtures.json'),JSON.stringify(data));
  }
  let maximum=0,frames=0;for(const test of data.tests)for(const block of [64,128,37]){const result=compare(test,block);maximum=Math.max(maximum,result.peak);frames+=result.frames;}
  console.log(JSON.stringify({result:'PASS',scope:'Combined original factory/worklet/automation/pan PCM; causal wake-up separately approved/tested, no Engine/GUI/device claim',fixtures:data.tests.length,comparisons:data.tests.length*3,stereoFrames:frames,maximumAbsoluteError:maximum,browser:data.browser.product}));
 }finally{close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
