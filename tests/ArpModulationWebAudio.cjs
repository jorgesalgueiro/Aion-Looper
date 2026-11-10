// SPDX-License-Identifier: AGPL-3.0-only
// Actual a-rate AudioParams + Oscillator/Gain/StereoPanner. Test-only parameter
// tap; not the worklet PCM gate and not connected Engine or device validation.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto'),{spawnSync}=require('node:child_process');
const {connect,close}=require('./ArpDelayWebAudio.cjs');
const {original,factory,hostDefaults,values,harness}=require('./ArpDelayReference.cjs');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(path.join(root,'resources/ui/Modules/effects.js'),'utf8');
const smooth=source.match(/    static smoothSetParam\(audioParam, val, now, tc = 0.05\) \{[\s\S]*?\n    \}/)[0];
const args=process.argv.slice(2),option=name=>{const n=args.indexOf(name);return n<0?null:args[n+1];};
const probe=option('--probe')||path.join(root,'build/windows-core/Release/aion_arp_probe.exe');
const hash=crypto.createHash('sha256').update(original+'\n'+factory+'\n'+smooth).digest('hex');
const descriptors=[...harness(48000,1).descriptors,{name:'panSpeed',defaultValue:0,minValue:0,maxValue:5},{name:'panDepth',defaultValue:.8,minValue:0,maxValue:1}];
const names=descriptors.map(p=>p.name);
async function render(evaluate,rate,initial,events,label){
 return {...await evaluate(`(async()=>{
  const rate=${rate},count=32768,initial=${JSON.stringify(initial)},changes=${JSON.stringify(events)},names=${JSON.stringify(names)};
  const ctx=new OfflineAudioContext(14,count,rate);
  const tapSource=${JSON.stringify(`class ParamTap extends AudioWorkletProcessor {
   static get parameterDescriptors(){return ${JSON.stringify(descriptors)};}
   process(inputs,outputs,parameters){for(let p=0;p<12;++p){const a=parameters[${JSON.stringify(names)}[p]],out=outputs[0][p];if(a.length===1)out.fill(a[0]);else out.set(a);}return true;}
  }registerProcessor('arp-param-tap',ParamTap);`)};
  const url=URL.createObjectURL(new Blob([tapSource],{type:'text/javascript'}));
  try{
   await ctx.audioWorklet.addModule(url);
   const tap=new AudioWorkletNode(ctx,'arp-param-tap',{numberOfInputs:0,outputChannelCount:[12],parameterData:Object.fromEntries(names.map((p,i)=>[p,initial[i]]))});
   const splitter=ctx.createChannelSplitter(12),merge=ctx.createChannelMerger(14);tap.connect(splitter);
   for(let p=0;p<12;++p)splitter.connect(merge,p,p);
   // Same always-present post-worklet routing as unchanged _createArpDelay.
   const panner=ctx.createStereoPanner(),lfo=ctx.createOscillator(),gain=ctx.createGain();
   lfo.frequency.value=initial[10];gain.gain.value=initial[11];lfo.connect(gain);gain.connect(panner.pan);lfo.start();
   const input=[],buffer=ctx.createBuffer(2,count,rate);
   for(let ch=0;ch<2;++ch){const a=buffer.getChannelData(ch);for(let i=0;i<count;++i)a[i]=.15*Math.sin(i*(ch?.073:.117))+.05*Math.cos(i*(ch?.021:.037));input.push(Array.from(a));}
   const src=ctx.createBufferSource();src.buffer=buffer;src.connect(panner);src.start();
   const stereo=ctx.createChannelSplitter(2);panner.connect(stereo);stereo.connect(merge,0,12);stereo.connect(merge,1,13);merge.connect(ctx.destination);
   class Original {${smooth}}
   const pauses=changes.map(e=>ctx.suspend(e[0]/rate)),applied=[],rendering=ctx.startRendering();
   for(let i=0;i<changes.length;++i){await pauses[i];const [requested,p,v]=changes[i],now=ctx.currentTime;
    applied.push([Math.round(now*rate),p,v]);Original.smoothSetParam(tap.parameters.get(names[p]),v,now);
    if(p===10)Original.smoothSetParam(lfo.frequency,v,now);else if(p===11)Original.smoothSetParam(gain.gain,v,now);
    await ctx.resume();}
   const out=await rendering;return {rate,initial,events:applied,input,output:Array.from({length:14},(_,p)=>Array.from(out.getChannelData(p)))};
  }finally{URL.revokeObjectURL(url);}
 })()`),label};
}
function compare(test,block){
 const commands=[`modprepare ${test.rate} ${test.initial.join(' ')}`],expected=[null];let frame=0;const count=test.input[0].length;
 function part(end){if(end===frame)return;commands.push(`modrender ${end-frame} ${block} `+Array.from({length:end-frame},(_,i)=>test.input.map(ch=>ch[frame+i]).join(' ')).join(' '));expected.push([frame,end]);frame=end;}
 for(const [frame,p,value]of test.events){part(frame);commands.push(`modtarget ${p} ${Math.fround(value)}`);expected.push(null);}part(count);
 const run=spawnSync(probe,[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:45*1024*1024});assert.equal(run.status,0,run.stderr);
 const lines=run.stdout.trim().split(/\r?\n/);assert.equal(lines.length,expected.length);
 let peak=0,power=0,errorPower=0,parameterError=0,location=[];
 for(let line=0;line<lines.length;++line){const range=expected[line];if(!range){assert.equal(lines[line],'ok');continue;}
  const [begin,end]=range,actual=lines[line].split(' ').slice(1).map(Number);assert.equal(actual.length,(end-begin)*14);
  for(let i=begin;i<end;++i)for(let p=0;p<14;++p){const a=actual[(i-begin)*14+p],b=test.output[p][i],err=a-b;assert(Number.isFinite(a)&&Number.isFinite(b));
   if(p<12){const scaled=Math.abs(err)/Math.max(1,Math.abs(b));parameterError=Math.max(parameterError,scaled);assert(scaled<=1e-6,JSON.stringify({label:test.label,rate:test.rate,block,parameter:names[p],i,a,b,scaled}));}
   else{power+=b*b;errorPower+=err*err;if(Math.abs(err)>peak){peak=Math.abs(err);location=[i,p,a,b];}}
  }
 }
 const relativeRms=Math.sqrt(errorPower/power);assert(power>1,'Nonzero stereo PCM required');
 console.log(JSON.stringify({label:test.label,rate:test.rate,block,peak,relativeRms,parameterError,location}));
 assert(peak<=1e-6&&relativeRms<=1e-5,JSON.stringify({label:test.label,rate:test.rate,block,peak,relativeRms,location}));
 return {peak,parameterError,frames:count};
}
(async()=>{
 try{
  let data;const fixture=option('--fixtures');
  if(fixture){data=JSON.parse(fs.readFileSync(fixture));assert.equal(data.version,1);assert.equal(data.sourceHash,hash,'Unchanged descriptor/factory/automation methods must match');}
  else{
   const {evaluate,browser}=await connect();data={version:1,sourceHash:hash,browser,tests:[]};
   const initial=[...values(hostDefaults),0,.8];
   for(const rate of [8000,22050,32000,44100,48000,96000])data.tests.push(await render(evaluate,rate,initial,[], 'zero-rate center'));
   for(const speed of [.1,.8])data.tests.push(await render(evaluate,48000,[...initial.slice(0,10),speed,1],[], 'static LFO '+speed));
   for(const rate of [8000,22050,32000,44100,48000,96000,192000,384000])data.tests.push(await render(evaluate,rate,[...initial.slice(0,10),5,1],[], 'static LFO 5'));
   const events=[[256,0,.01],[512,1,.9],[768,2,.1],[1024,3,1],[1280,4,19],[1536,5,1],[1792,6,300],[2048,7,1.5],[2304,8,3],[2560,9,1],[2816,10,5],[3072,11,1],
    [4096,10,0],[4224,10,0],[4352,11,0],[4480,11,0],[6144,10,2],[8192,11,.2],[10240,0,.4],[12288,1,0],[14336,2,1],[16384,3,0],[18432,4,0],[20480,5,0],[22528,6,30],[24576,7,0],[26624,8,.25],[28672,9,0]];
   for(const rate of [22050,44100,48000,96000,192000,384000])data.tests.push(await render(evaluate,rate,initial,events,'live/repeated zero-rate and depth gestures'));
   fs.writeFileSync(option('--save')||path.join(root,'build/arp-modulation-browser-fixtures.json'),JSON.stringify(data));
  }
  let maximum=0,parameterError=0,frames=0;for(const test of data.tests)for(const block of [64,128,37]){const result=compare(test,block);maximum=Math.max(maximum,result.peak);parameterError=Math.max(parameterError,result.parameterError);frames+=result.frames;}
  console.log(JSON.stringify({result:'PASS',scope:'Actual AudioParam and stereo auto-pan only; no worklet streaming/Engine/device claim',fixtures:data.tests.length,comparisons:data.tests.length*3,stereoFrames:frames,maximumAbsoluteError:maximum,maximumScaledParameterError:parameterError,browser:data.browser.product}));
 }finally{close();}
})().catch(error=>{console.error(error);process.exitCode=1;});
