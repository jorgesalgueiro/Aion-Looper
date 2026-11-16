// SPDX-License-Identifier: AGPL-3.0-only
// Actual original factory's preGain/WaveShaper branch in OfflineAudioContext.
// Tone, volume, dry/wet, automation and product integration remain separate.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const {connect,close}=require('./ArpDelayWebAudio.cjs');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
function section(start,end){const a=source.indexOf(start),b=source.indexOf(end,a+start.length);assert(a>=0&&b>a);return source.slice(a,b).trim();}
const factory=section('    _createDistortion(inputNode) {','    static makeDistortionCurve('),curve=section('    static makeDistortionCurve(amount, shape) {','    _createFuzz(');
const sourceHash=crypto.createHash('sha256').update(factory+'\n'+curve).digest('hex');
const args=process.argv.slice(2),option=key=>{const p=args.indexOf(key);return p<0?null:args[p+1];};
for(let i=0;i<args.length;++i){assert(['--probe','--fixtures','--save'].includes(args[i]),'Unknown option '+args[i]);assert(args[++i]);}
const probe=option('--probe')||path.join(root,'build/windows-core/Release/aion_distortion_waveshaper_tests.exe');
// Predeclared Float32 browser error budget. Absolute RMS floor accounts for
// FFT roundoff near silence; the unchanged 1e-6 peak limit applies everywhere.
const peakLimit=1e-6,rmsFloor=1e-7,rmsFactor=1e-5;
const failures=[];
async function render(evaluate,rate,amount,shape,channels,kind,oversample='4x'){
 return {...await evaluate(`(async()=>{
  const count=4096,rate=${rate},channels=${channels},kind=${JSON.stringify(kind)};
  const ctx=new OfflineAudioContext(2,count,rate),state={audioContext:ctx};
  const effects={distortion:{amount:${amount},shape:${shape},tone:4000,volume:.8,mix:1}};
  class AudioGraph {${factory}\n${curve}}
  const buffer=ctx.createBuffer(channels,count,rate),input=[];let seed=123456789;
  for(let ch=0;ch<channels;++ch){const a=buffer.getChannelData(ch);
   for(let i=0;i<count;++i){let v=0;
    if(i<count-1024){
     if(kind==='impulse')v=i===0?.001:0;
     else if(kind==='silence')v=0;
     else if(kind==='quiet')v=1e-4*Math.sin(i*(ch?.113:.071));
     else if(kind==='ramp')v=4*((i%257)/256-.5);
     else if(kind==='random'){seed=(Math.imul(seed,1664525)+1013904223)>>>0;v=(seed/4294967296-.5)*2;}
     else v=.2*Math.sin(i*(ch?.073:.117))+.05*Math.cos(i*(ch?.021:.037));
    }
    a[i]=v;
   }input.push(Array.from(a));
  }
  const source=ctx.createBufferSource();source.buffer=buffer;
  const graph=new AudioGraph();graph.loop={params:{distortion:effects.distortion}};
  const result=graph._createDistortion(source);
  if(${JSON.stringify(oversample)}==='none')result.nodes[1].oversample='none'; // Isolated lookup diagnostic, not the product's 4x mode.
  result.nodes[1].connect(ctx.destination);source.start();
  const out=await ctx.startRendering();return {rate,amount:${amount},shape:${shape},channels,kind,oversample:${JSON.stringify(oversample)},input,pcm:[Array.from(out.getChannelData(0)),Array.from(out.getChannelData(1))]};
 })()`),label:`${kind}/${rate}/${channels}/${amount}/${shape}/${oversample}`};
}
function compare(test,block){
 const n=test.pcm[0].length,header=`${n} ${test.channels} ${block} ${test.amount} ${test.shape}\n`;
 const input=header+Array.from({length:n},(_,i)=>test.input.map(ch=>ch[i]).join(' ')).join('\n')+'\n';
 const result=spawnSync(probe,['probe',test.oversample],{input,encoding:'utf8',maxBuffer:8*1024*1024});assert.equal(result.status,0,result.stderr||result.error?.message);
 const output=result.stdout.trim().split(/\s+/).map(Number);assert.equal(output.length,n*2);
 let peak=0,energy=0,errorEnergy=0,location;
 for(let i=0;i<n;++i)for(let ch=0;ch<2;++ch){const actual=output[i*2+ch],expected=test.pcm[ch][i],error=actual-expected;
  assert(Number.isFinite(actual)&&Number.isFinite(expected));energy+=expected*expected;errorEnergy+=error*error;
  if(Math.abs(error)>peak){peak=Math.abs(error);location=[i,ch,actual,expected];}
 }
 const signalRms=Math.sqrt(energy/(2*n)),errorRms=Math.sqrt(errorEnergy/(2*n)),rmsLimit=rmsFloor+rmsFactor*signalRms;
 console.log(JSON.stringify({label:test.label,block,peak,errorRms,rmsLimit}));
 if(!(peak<=peakLimit&&errorRms<=rmsLimit))failures.push({label:test.label,block,peak,errorRms,rmsLimit,location});
 return peak;
}
(async()=>{try{
 let data;
 if(option('--fixtures')){data=JSON.parse(fs.readFileSync(option('--fixtures')));assert.equal(data.version,1);assert.equal(data.sourceHash,sourceHash);}
 else {
  const {evaluate,browser}=await connect();data={version:1,sourceHash,browser,tests:[]};
  for(const rate of [8000,22050,32000,44100,48000,96000])data.tests.push(await render(evaluate,rate,40,.5,2,'signal'));
  for(const [amount,shape]of [[35,.5],[20,.5],[50,.5],[80,.5],[60,.8],[15,.3],[0,0],[100,1]])data.tests.push(await render(evaluate,48000,amount,shape,2,'random'));
  for(const kind of ['silence','impulse','quiet','ramp'])data.tests.push(await render(evaluate,48000,100,1,2,kind));
  data.tests.push(await render(evaluate,48000,40,.5,1,'signal'));
  for(const kind of ['signal','quiet','ramp'])data.tests.push(await render(evaluate,48000,100,1,2,kind,'none'));
  fs.writeFileSync(option('--save')||path.join(root,'build/distortion-waveshaper-browser-fixtures.json'),JSON.stringify(data));
 }
 let maximum=0;for(const test of data.tests)for(const block of [64,128])maximum=Math.max(maximum,compare(test,block));
 assert.equal(failures.length,0,JSON.stringify(failures)); // Collect every failure, still fail the gate with unchanged limits.
 console.log(JSON.stringify({result:'PASS',cases:data.tests.length*2,maximumAbsoluteError:maximum,peakLimit,rmsFloor,rmsFactor,browser:data.browser.product,scope:'Original preGain/WaveShaper branch only; no tone/mix/automation/product/physical device claim'}));
}finally{close();}})().catch(error=>{console.error(error);process.exitCode=1;});
