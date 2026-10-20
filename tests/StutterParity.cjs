// Execute unchanged Loop.stutterStart/Stop and reference-conditioned source PCM.
// Audio nodes are IO stubs; this compares source PCM, not browser resampling/envelopes.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const makeBuffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c]};};
 const param=()=>({value:1,setValueAtTime(){},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 const node=()=>({connect(){},disconnect(){},start(time,offset){this.started={time,offset};},stop(){},gain:param(),playbackRate:param()});
 const audio={state:'running',currentTime:0,createBuffer:(count,length,rate)=>makeBuffer(Array.from({length:count},()=>Array(length).fill(0)),rate),createBufferSource:node,createGain:node,createChannelMerger:node,createAnalyser:node};
 const context={window:{},document:{getElementById:()=>null,createElement:()=>({}),head:{appendChild(){}}},effects:{},FACTORY_EFFECTS:{},console,setTimeout:()=>0,clearTimeout(){},audio};vm.createContext(context);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),context);
 const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');vm.runInContext(loops,context);
 const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(loops);assert(match);
 const start=match.index+match[0].indexOf('for ('),brace=loops.indexOf('{',start);let end=brace+1,depth=1;
 for(;depth;end++){assert(end<loops.length);if(loops[end]==='{')depth++;if(loops[end]==='}')depth--;}
 vm.runInContext(`const state={audioContext:audio,syncEnabled:false,loops:[],customEffects:{}};
 UIManager.updateLoop=()=>{}; UIManager.updateStatus=()=>{};state.playingSources={};
 globalThis.condition=b=>{AudioEngine.seamlessLoopCrossfade(b,0.015);const finalBuffer=b;${loops.slice(start,end)}};
 globalThis.setup=b=>{const loop=new Loop(0);loop.audioBuffer=b;loop.duration=b.duration;loop.state='playing';loop.effects.reverse=globalThis.reversed;state.loops=[loop];loop.graph=new AudioGraph(loop);if(!loop.graph.build())throw Error('Reference graph failed');};
 globalThis.toggle=()=>state.loops[0].toggleEffect('reverse');
 globalThis.restart=()=>state.loops[0].graph.retrigger(0,state.loops[0]);
 globalThis.stored=()=>state.loops[0].audioBuffer;
 globalThis.source=()=>state.loops[0].graph.source.buffer;
 globalThis.normalize=()=>state.loops[0].normalize();
 globalThis.edit=async(take,offset,feedback)=>{const finalBuffer=take;${loops.slice(start,end)};AudioEngine.seamlessLoopCrossfade(take,0.008);await AudioEngine.mixBuffersCircular(state.loops[0].audioBuffer,take,offset,feedback);};`,context);

 vm.runInContext(`state.bpm=120;state.bars=1;state.timeSig={num:4};state.masterStartTime=0;state.loopLength=2;
 globalThis.setDelay=value=>UIManager.setLoopStartDelay(0,value);
 globalThis.configure=(mode,bpm,origin,num)=>{state.syncEnabled=mode!==0;state.syncMode=mode===1?'static':mode===2?'dynamic':'off';state.bpm=bpm;state.masterStartTime=origin;state.timeSig.num=num;state.loopLength=60/bpm*num;state.loops[0].originalBpm=120;};
 globalThis.nudge=direction=>UIManager.nudgeLoopStart(0,direction);
 globalThis.quantize=()=>UIManager.quantizeLoopStart(0);
 globalThis.delayValue=()=>state.loops[0].startDelay;
 globalThis.stut=on=>on?state.loops[0].stutterStart():state.loops[0].stutterStop();
 globalThis.progress=()=>state.loops[0].getProgress(audio.currentTime);
 globalThis.region=()=>[state.loops[0].isStuttering,state.loops[0].graph.source.loopStart,state.loops[0].graph.source.loopEnd];
 globalThis.rtrg=()=>state.loops[0].retrigger();
 globalThis.stopped=()=>{state.loops[0].state="stopped";};
 globalThis.applyPhase=()=>state.loops[0].restart(audio.currentTime+0.05);
 globalThis.rate=()=>Math.fround(state.loops[0].effectivePlaybackRate);
 globalThis.offset=()=>state.loops[0].graph.startOffset;
 globalThis.phaseSolo=value=>{state.loops[0].isMutedBySolo=value;};
 globalThis.phaseSpeed=value=>{state.loops[0].playbackRate=value;};
 `,context);

 let cases=0,samples=0,maximum=0,regions=0;
 for(const mono of [true,false])for(const sampleRate of [8000,48000,96000])for(const length of [1,17,512,4097,20003]){
  audio.currentTime=0;vm.runInContext('state.syncEnabled=false;state.bpm=120;',context);
  const raw=[Array.from({length},(_,i)=>Math.fround(0.12+0.4*Math.sin(i*0.11))),Array.from({length},(_,i)=>Math.fround(-0.08+0.2*Math.cos(i*0.073)))];
  const buffer=makeBuffer(mono?[raw[0]]:raw,sampleRate);context.condition(buffer);
  let frame=length,phase=0,origin=0,bpm=120,region=null;
  const commands=[`prepare 81920 ${sampleRate}`,'record 0 0',...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),'finish 0 0','wait 0 0'];
  context.reversed=!mono;if(!mono)commands.splice(1,0,"reverse 0 1");audio.currentTime=frame/sampleRate;context.setup(buffer);const checks=[],metadata=[];
  const render=count=>{for(let n=0;n<count;++n){
   if(region&&phase>=region[1])phase=region[0]+(phase-region[0])%(region[1]-region[0]);else if(phase>=length)phase%=length;
   const src=context.source(),index=Math.floor(phase),f=Math.fround(phase-index);let next=(index+1)%length;
   if(region&&index<region[1]&&index+1>=region[1]){const span=region[1]-region[0];next=Math.floor(Math.max(0,Math.min(length-1,span>=1?phase+1-span:region[0]+(phase+1-region[0])%span)));}
   const value=c=>{const data=src.getChannelData(c);return Math.fround(data[index]+Math.fround(region&&next===index&&index>0?data[index]-data[index-1]:data[next]-data[index])*f);};
   checks.push({row:commands.length,expected:(value(0)+value(mono?0:1))*0.5*Math.SQRT1_2});commands.push('tick 0 0');
   phase+=context.rate();if(region&&phase>=region[1])phase=region[0]+(phase-region[0])%(region[1]-region[0]);else phase%=length;frame++;
  }};
  const change=on=>{audio.currentTime=frame/sampleRate;context.stut(on);commands.push(`stut 0 ${on?1:0}`);
   const r=context.region();metadata.push({row:commands.length,on:!!r[0],start:r[1]*sampleRate,end:r[2]*sampleRate,progress:context.progress()});commands.push('region 0 0');
   region=on?[r[1]*sampleRate,r[2]*sampleRate]:null;regions++;
  };
  for(const mode of [0,1,2]){
   const nextBpm=mode===1?137.3:999;origin=frame-(frame-origin)*bpm/nextBpm;bpm=nextBpm;
   commands.push(`sync ${mode} ${bpm} 4 1`);context.configure(mode,bpm,origin/sampleRate,4);
   for(const speed of [1,Math.fround(0.37),2]){
    context.phaseSpeed(speed);commands.push(`speed 0 ${speed}`);render(37);
    change(true);render(Math.min(length*2+9,1800));change(true);render(17); // repeated down cannot move the region
    change(false);render(Math.min(length+11,237));
    change(true);render(31);change(false);render(17); // next hold uses original graph clock, not the prior fragment's cursor
   }
  }
  const result=spawnSync(process.argv[2],[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(result.status,0,result.stderr);const rows=result.stdout.trim().split(/\r?\n/);assert.equal(rows.length,commands.length);
  rows.forEach((r,i)=>assert.equal(Number(r.split(' ')[0]),1,'Rejected '+commands[i]));
  for(const check of metadata){const row=rows[check.row].split(' ').map(Number);assert.equal(!!row[1],check.on);assert(row[4]===1,'Visual graph clock survives release');const difference=Math.abs(row[5]-check.progress);assert(Math.min(difference,Math.abs(1-difference))<=1e-12,'Unchanged getProgress while held and after release');if(check.on){assert(Math.abs(row[2]-check.start)<=1e-8,`region start ${sampleRate}/${length}: ${row[2]} vs ${check.start}`);assert(Math.abs(row[3]-check.end)<=1e-8,'region end');}}
  for(const check of checks){const row=rows[check.row].split(' ').map(Number);for(const value of row.slice(6)){const error=Math.abs(value-check.expected);maximum=Math.max(maximum,error);assert(error<=1e-6,`Stutter ${mono}/${sampleRate}/${length}, frame ${row[1]}: ${value} vs ${check.expected}`);}samples++;}
  cases++;
 }
 console.log(JSON.stringify({result:'PASS',cases,regions,samples,maximumAbsoluteError:maximum,tolerance:1e-6,scope:'Unchanged Loop.stutterStart/Stop regions and committed source PCM; 1x/fractional/double rates, all sync modes, repeated holds and resumed cursor; browser resampling/envelopes excluded'}));
})().catch(e=>{console.error(e);process.exitCode=1;});
