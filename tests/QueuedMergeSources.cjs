// Stored MERGE/NORM: unchanged stored-buffer and source/deadline methods.
// --normalize exercises changed/no-op NORM while scheduled STOP is pending.
// Default is reference-only; --native PROBE checks source PCM and stereo export.
// Audio nodes are IO stubs; this compares source PCM, not browser resampling/envelopes.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..'),inputCapture=false;
 const makeBuffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c],copyToChannel:(data,c)=>channels[c].set(data)};};
 const param=()=>({value:1,events:[],setValueAtTime(value,time){this.events.push({value,time});},setTargetAtTime(value,time,constant){this.events.push({value,time,targetConstant:constant});},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 const node=()=>({connect(){},disconnect(){this.disconnected=true;},start(time,offset){this.started={time,offset};},stop(time){this.stopped=true;this.stoppedAt=time;},gain:param(),playbackRate:param()});
 class BufferSource {constructor(){Object.assign(this,node());}}
 const audio={state:'running',currentTime:0,createBuffer:(count,length,rate)=>makeBuffer(Array.from({length:count},()=>Array(length).fill(0)),rate),createBufferSource:()=>new BufferSource(),createGain:node,createChannelMerger:node,createAnalyser:node};
 const context={window:{},document:{querySelectorAll:()=>[],getElementById:()=>null,createElement:()=>({}),head:{appendChild(){}}},InputManager:{getRecordingNode:()=>inputCapture?node():null},AudioWorkletNode:class {constructor(){this.port={postMessage(){}};}connect(){}disconnect(){}},EffectManager:{updateFxMixTimeUI(){}},Log:{warn(){},error(...args){throw Error(args.join(" "));}},alert:()=>{throw Error("Unexpected varispeed guard");},AudioBufferSourceNode:BufferSource,effects:{},FACTORY_EFFECTS:{},console,setTimeout:()=>0,clearTimeout(){},audio};vm.createContext(context);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/audioEngine.js'),'utf8'),context);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/ui/Modules/scales.js'),'utf8'),context);
 const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');vm.runInContext(loops,context);
 const match=/\/\/ Remove DC Offset\s+for \(let c = 0; c < finalBuffer.numberOfChannels; c\+\+\)/.exec(loops);assert(match);
 const start=match.index+match[0].indexOf('for ('),brace=loops.indexOf('{',start);let end=brace+1,depth=1;
 for(;depth;end++){assert(end<loops.length);if(loops[end]==='{')depth++;if(loops[end]==='}')depth--;}
 vm.runInContext(`const state={audioContext:audio,syncEnabled:false,loops:[],customEffects:{}};
 UIManager.updateLoop=()=>{}; UIManager.updateStatus=()=>{};state.playingSources={};
 globalThis.condition=b=>{AudioEngine.seamlessLoopCrossfade(b,0.015);const finalBuffer=b;${loops.slice(start,end)}};
 globalThis.setup=b=>{const loop=new Loop(0);loop.audioBuffer=b;loop.duration=b.duration;loop.state='playing';state.loops=[loop];loop.graph=new AudioGraph(loop);if(!loop.graph.build())throw Error('Reference graph failed');};
 globalThis.toggle=()=>state.loops[0].toggleEffect('reverse');
 globalThis.setReverse=value=>{if(state.loops[0].effects.reverse!==value)state.loops[0].toggleEffect('reverse');};
 globalThis.restart=()=>state.loops[0].graph.retrigger(0,state.loops[0]);
 globalThis.stored=()=>state.loops[0].audioBuffer;
 globalThis.source=()=>state.loops[0].graph.source.buffer;
 globalThis.sourceNode=()=>state.loops[0].graph.source;
 globalThis.volumeEvents=()=>state.loops[0].graph.nodes.volume.gain.events;
 globalThis.normalize=()=>state.loops[0].normalize();
 globalThis.edit=async(take,offset,feedback)=>{const finalBuffer=take;${loops.slice(start,end)};AudioEngine.seamlessLoopCrossfade(take,0.008);await AudioEngine.mixBuffersCircular(state.loops[0].audioBuffer,take,offset,feedback);};`,context);

 vm.runInContext(`state.bpm=120;state.bars=1;state.timeSig={num:4};state.masterStartTime=0;state.loopLength=2;
 globalThis.setDelay=value=>UIManager.setLoopStartDelay(0,value);
 globalThis.configure=(mode,bpm,origin,num)=>{state.syncEnabled=mode!==0;state.syncMode=mode===1?'static':mode===2?'dynamic':'off';state.bpm=bpm;state.masterStartTime=origin;state.timeSig.num=num;state.loopLength=60/bpm*num;state.loops[0].originalBpm=120;};
 globalThis.nudge=direction=>UIManager.nudgeLoopStart(0,direction);
 globalThis.quantize=()=>UIManager.quantizeLoopStart(0);
 globalThis.delayValue=()=>state.loops[0].startDelay;
 globalThis.rtrg=()=>state.loops[0].retrigger();
 globalThis.stopped=()=>{state.loops[0].state="stopped";};
 globalThis.grid=()=>SyncManager.getNextGridTime();
 globalThis.scheduledPlay=time=>state.loops[0].play(time);
 globalThis.scheduleStop=()=>state.loops[0].scheduleStop();
 globalThis.stopTime=()=>state.loops[0].graph.source.stoppedAt;
 globalThis.cleanup=()=>{const source=state.loops[0].graph.source;state.loops[0]._teardown();return {source,stopped:state.loops[0].state==='stopped',detached:state.loops[0].graph===null};};
 globalThis.applyPhase=()=>state.loops[0].restart(audio.currentTime+0.05);
 globalThis.rate=()=>Math.fround(state.loops[0].effectivePlaybackRate);
 globalThis.offset=()=>state.loops[0].graph.startOffset;
 globalThis.phaseSolo=value=>{state.loops[0].isMutedBySolo=value;};
 globalThis.phaseSpeed=value=>{state.loops[0].playbackRate=value;};
 globalThis.userSpeed=value=>UIManager.setLoopSpeed(0,value);
 globalThis.tempo=value=>{const get=document.getElementById;document.getElementById=id=>id==='bpmInput'?{value}:get(id);try{SyncManager.updateSettings();}finally{document.getElementById=get;}};
 `,context);

 vm.runInContext(`state.undoStack=[];state.redoStack=[];UIManager.updateLoopDisplays=()=>{};
 globalThis.seedHistory=()=>{state.undoStack=[];state.redoStack=[];const l=state.loops[0];l.state='stopped';l.graph=null;l.multiplyInstant();};
 globalThis.restore=redo=>state.loops[0][redo?'redo':'undo']();
 globalThis.queue=()=>{state.loops[0].state='queued';};
 globalThis.info=()=>{const l=state.loops[0];return [l.audioBuffer.length,l.state,l.undoStack.length,l.redoStack.length];};`,context);

 vm.runInContext(`globalThis.direction=()=>state.loops[0].effects.reverse;
 globalThis.graph=()=>state.loops[0].graph;
 globalThis.teardown=()=>state.loops[0]._teardown();
 globalThis.multiply=()=>state.loops[0].multiply();
 globalThis.wav=()=>AudioEngine.bufferToWAV(state.loops[0].audioBuffer);
 globalThis.endCapture=async chunks=>{state.loops[0].multiply();await LoopManager.processMultiplyData(chunks,0);};`,context);

 vm.runInContext(`globalThis.setupMerge=(base,source)=>{
  const dest=new Loop(0),src=new Loop(1);dest.audioBuffer=base;dest.duration=base.duration;dest.state='queued';dest.originalBpm=120;
  src.audioBuffer=source;src.duration=source.duration;src.state='stopped';state.loops=[dest,src];state.undoStack=[];state.redoStack=[];
 };
 globalThis.merge=()=>LoopManager.mergeLoops(0,'1');
 globalThis.stereoWav=(b=state.loops[0].audioBuffer)=>{
  if(b.numberOfChannels===1){const stereo=state.audioContext.createBuffer(2,b.length,b.sampleRate);stereo.copyToChannel(b.getChannelData(0),0);stereo.copyToChannel(b.getChannelData(0),1);b=stereo;}
  return AudioEngine.bufferToWAV(b);
 };`,context);
 const nativeIndex=process.argv.indexOf('--native'),probe=nativeIndex<0?null:process.argv[nativeIndex+1];
 if(nativeIndex>=0)assert(probe,'--native requires aion_start_probe');
 const ceilSample=v=>{const near=Math.round(v);return Math.abs(v-near)<=4*Number.EPSILON*Math.max(1,Math.abs(v))?near:Math.ceil(v);};
 const data=b=>Array.from({length:b.numberOfChannels},(_,c)=>Float32Array.from(b.getChannelData(c)));
 const pendingRtrg=process.argv.includes('--pending-rtrg'),queuedStop=process.argv.includes('--queued-stop-multiply'),multiplying=queuedStop||process.argv.includes('--multiply'),history=process.argv.includes('--history'),normalizing=process.argv.includes('--normalize'),stoppingDestination=pendingRtrg||multiplying||history||normalizing||process.argv.includes('--scheduled-stop');
 vm.runInContext('globalThis.progress=now=>state.loops[0].getProgress(now);',context);
 let cases=0,wavCases=0,samples=0,maximum=0;
 for(const rate of [8000,48000,96000])for(const mono of [true,false])for(const size of [17,512])for(const reverse of [false,true])for(const sequence of (pendingRtrg?['unborn','queued-stop','stopping','apply-stop','multiply-stop','norm-stop','history-stop','stut-stop','rapid','merge-stop']:queuedStop?['same-grid','later-grid','norm-after','apply']:multiplying?['double','undo-redo','norm-after','apply']:history?['undo','rapid','norm-after','apply']:stoppingDestination?['stopping']:['prebirth','constructed','apply','undo-redo']))for(const unity of (normalizing?[false,true]:[false])){
  audio.currentTime=0;audio.sampleRate=rate;
  const raw=n=>[Array.from({length:n},(_,i)=>Math.fround(.12+.4*Math.sin(i*.11))),Array.from({length:n},(_,i)=>Math.fround(-.08+.2*Math.cos(i*.073)))];
  const baseRaw=raw(size),sourceRaw=raw(773);if(mono){baseRaw[1]=baseRaw[0].slice();sourceRaw[1]=sourceRaw[0].slice();}
  const base=makeBuffer(mono?[baseRaw[0]]:baseRaw,rate),source=makeBuffer(mono?[sourceRaw[0]]:sourceRaw,rate);
  context.condition(base);context.condition(source);context.setupMerge(base,source);
  vm.runInContext('globalThis.sourceWav=()=>stereoWav(state.loops[1].audioBuffer);',context);
  const expectedSource=Buffer.from(context.sourceWav());
  const commands=[`prepare 81920 ${rate}`];
  const seed=(id,raw)=>{commands.push(`record ${id} 0`);for(let i=0;i<raw[0].length;++i)commands.push(`tick 0 ${raw[0][i]} ${raw[1][i]}`);commands.push(`finish ${id} 0`,'wait 0 0',`stop ${id} 0`);};
  seed(0,baseRaw);seed(1,sourceRaw);if(history||pendingRtrg&&sequence==='history-stop'){context.seedHistory();commands.push('multiply 0 0','wait 0 0');}if(unity){context.normalize();commands.push('normalize 0 0','wait 0 0');}commands.push('timeline 0 0');
  let frame=0;const voices=[],checks=[],wavs=[],regions=[],rtrgMetadata=[],rejections=new Set();
  const remember=()=>{const node=context.sourceNode();assert(node.started);assert(!voices.some(v=>v.node===node));voices.push({node,volume:context.graph().nodes.volume});};
  const sample=(node,at)=>{
   const start=node.started.time*rate,stop=node.stoppedAt===undefined?Infinity:node.stoppedAt*rate;
   if(at<ceilSample(start)||at>=ceilSample(stop))return null;
   let speed=1,cursor=start,advance=0;
   for(const e of [...node.playbackRate.events].sort((a,b)=>a.time-b.time)){
    const when=e.time*rate;if(when>at)break;if(when>cursor){advance+=(when-cursor)*speed;cursor=when;}speed=Math.fround(e.value);
   }
   advance+=(at-cursor)*speed;const b=node.buffer,length=b.length,phase=((node.started.offset*rate+advance)%length+length)%length,i=Math.floor(phase),next=(i+1)%length,f=Math.fround(phase-i);
   const value=c=>{const v=b.getChannelData(c);return Math.fround(v[i]+Math.fround(v[next]-v[i])*f);};return [value(0),value(mono?0:1)];
  };
  let gate=Infinity;
  const render=until=>{for(;frame<until;++frame){let l=0,r=0;for(const {node,volume} of voices){if(volume.disconnected||node.disconnected)continue;const v=sample(node,frame);if(v){l=Math.fround(l+v[0]);r=Math.fround(r+v[1]);}}
   const want=frame<gate?0:(l+r)*.5*Math.SQRT1_2;checks.push({row:commands.length,want});commands.push('tick 0 0');samples++;
  }audio.currentTime=frame/rate;};
  render(64);context.configure(1,137.3,(frame-frame*120/137.3)/rate,1);context.setReverse(reverse);context.setDelay(.217);
  commands.push('sync 1 137.3 1 1',`reverse 0 ${reverse?1:0}`,'delay 0 .217');const grid=context.grid();gate=ceilSample(grid*rate);commands.push('play 0 0');
  if(sequence!=='prebirth'&&sequence!=='unborn'){await context.scheduledPlay(grid);remember();commands.push('birth 0 0');}
  if(queuedStop&&sequence!=='same-grid')render(gate-Math.ceil(rate*.02));
  if(sequence==='apply'){context.applyPhase();remember();commands.push('apply 0 0');}
  let stopAt=0;
  if(stoppingDestination&&sequence!=='unborn'){if(pendingRtrg&&sequence==='queued-stop')render(gate-Math.ceil(rate*.02));else if(!queuedStop)render(gate+Math.ceil(rate*.012));
   if(pendingRtrg&&sequence==='apply-stop'){context.applyPhase();remember();commands.push('apply 0 0');}
   if(pendingRtrg&&sequence==='stut-stop'){vm.runInContext('state.loops[0].stutterStart();',context);if(!voices.some(v=>v.node===context.sourceNode()))remember();commands.push('stut 0 1');}
   stopAt=ceilSample(context.grid()*rate);context.scheduleStop();
   assert.equal(context.info()[1],'stopping');assert.equal(ceilSample(context.stopTime()*rate),stopAt,'Original source owns scheduled stop');commands.push('stopgrid 0 0');}
  if(pendingRtrg){
   if(sequence==='multiply-stop'){context.multiply();commands.push('multiply 0 1','wait 0 0');}
   if(sequence==='norm-stop'){context.normalize();commands.push('normalize 0 0','wait 0 0');}
   if(sequence==='merge-stop'){await context.merge();commands.push('merge 0 1','wait 0 0');}
   if(sequence==='history-stop'){context.restore(false);context.restore(true);commands.push('undo 0 0','redo 0 0');}
   const oldVolume=sequence==='unborn'?null:context.graph().nodes.volume;
   await context.rtrg();remember();rtrgMetadata.push({row:commands.length,want:context.delayValue()});commands.push('rtrg 0 0');
   assert.equal(context.info()[1],'playing');assert.equal(context.sourceNode().started.time,frame/rate,'Non-playing original RTRG creates an immediate fresh graph');if(oldVolume)assert(oldVolume.disconnected,'Old graph and all of its future sources are detached');
   gate=frame;
   if(sequence==='unborn'){const current=context.sourceNode();await context.scheduledPlay(grid);assert.equal(context.sourceNode(),current,'Late original queue callback is a no-op after immediate play');rejections.add(commands.length);commands.push('birth 0 0');}
   if(sequence==='rapid')for(let n=0;n<2;++n){render(frame+1);await context.rtrg();remember();assert.equal(context.sourceNode().started.time,audio.currentTime+.005,'Following playing tap retains healthy 5ms replacement');rtrgMetadata.push({row:commands.length,want:context.delayValue()});commands.push('rtrg 0 0');}
  }
  else if(multiplying){
   const node=context.sourceNode(),old=node.buffer,started={...node.started},stop=node.stoppedAt;
   const edit=(fn,command)=>{fn();commands.push(command,'wait 0 0');assert.equal(context.info()[1],'stopping');assert.equal(context.sourceNode(),node);assert.equal(node.buffer,old);assert.deepEqual(node.started,started);assert.equal(node.stoppedAt,stop);wavs.push({row:commands.length,want:Buffer.from(context.stereoWav())});commands.push('export 0 0');if(sequence!=='apply'){regions.push({row:commands.length,want:context.progress(frame/rate),clock:!queuedStop||frame>=gate});commands.push('region 0 0');}};
   edit(()=>context.multiply(),'multiply 0 1'); // Input availability cannot turn stopping MULT into capture.
   assert.equal(context.stored().length,size*2);
   render(frame+Math.ceil(rate*.002));
   if(sequence==='undo-redo'){edit(()=>context.restore(false),'undo 0 0');render(frame+Math.ceil(rate*.002));edit(()=>context.restore(true),'redo 0 0');}
   if(sequence==='norm-after')edit(()=>context.normalize(),'normalize 0 0');
   if(sequence==='apply'){render(frame+Math.ceil(rate*.06));regions.push({row:commands.length,want:context.progress(frame/rate)});commands.push('region 0 0');}
   if(queuedStop&&stopAt>gate&&sequence!=='apply'){render(gate+Math.ceil(rate*.005));regions.push({row:commands.length,want:context.progress(frame/rate)});commands.push('region 0 0');}
  }
  else if(history){
   const node=context.sourceNode(),old=node.buffer,started={...node.started},stop=node.stoppedAt;
   const restore=redo=>{context.restore(redo);commands.push(`${redo?'redo':'undo'} 0 0`);assert.equal(context.info()[1],'stopping');assert.equal(context.sourceNode(),node);assert.equal(node.buffer,old);assert.deepEqual(node.started,started);assert.equal(node.stoppedAt,stop);wavs.push({row:commands.length,want:Buffer.from(context.stereoWav())});commands.push('export 0 0');if(sequence!=='apply'){regions.push({row:commands.length,want:context.progress(frame/rate)});commands.push('region 0 0');}};
   restore(false);render(frame+Math.ceil(rate*.002));
   if(sequence==='apply'){render(frame+Math.ceil(rate*.06));regions.push({row:commands.length,want:context.progress(frame/rate)});commands.push('region 0 0');}
   if(sequence==='rapid'){restore(true);render(frame+Math.ceil(rate*.002));restore(false);}
   if(sequence==='norm-after'){context.normalize();commands.push('normalize 0 0','wait 0 0');assert.equal(node.buffer,old,'NORM of restored storage must not edit its old source');wavs.push({row:commands.length,want:Buffer.from(context.stereoWav())});commands.push('export 0 0');}
  }
  else if(normalizing){const node=context.sourceNode(),old=node.buffer;context.normalize();assert.equal(context.sourceNode(),node,'NORM retains original stopping source');assert.equal(node.buffer,old,'NORM does not replace forward/reversed source ownership');commands.push('normalize 0 0','wait 0 0');}
  else {await context.merge();commands.push('merge 0 1','wait 0 0');}wavs.push({row:commands.length,want:Buffer.from(context.stereoWav())});commands.push('export 0 0');
  if(!pendingRtrg)assert.equal(context.info()[2],(normalizing||history)?0:1,'Original action retains expected local history');
  if(sequence==='prebirth'){await context.scheduledPlay(grid);remember();commands.push('birth 0 0');}
  if(sequence==='undo-redo'&&!multiplying){
   context.restore(false);remember();commands.push('undo 0 0');wavs.push({row:commands.length,want:Buffer.from(context.stereoWav())});commands.push('export 0 0');
   render(frame+Math.ceil(rate*.002));context.restore(true);remember();commands.push('redo 0 0');wavs.push({row:commands.length,want:Buffer.from(context.stereoWav())});commands.push('export 0 0');
  }
  const expectedInfo=context.info(),inspectRow=commands.length,inspectFrame=frame;commands.push('inspect 0 0');
  render((pendingRtrg?Math.max(stopAt,ceilSample(grid*rate)):stoppingDestination?stopAt:gate)+Math.ceil(rate*.06));
  if(pendingRtrg&&sequence==='unborn'){const current=context.sourceNode();await context.scheduledPlay(grid);assert.equal(context.sourceNode(),current);rejections.add(commands.length);commands.push('birth 0 0');render(frame+64);}
  const stoppedRow=commands.length;if(stoppingDestination)commands.push('inspect 0 0');wavs.push({row:commands.length,want:expectedSource});commands.push('export 1 0');
  if(probe){
   const run=spawnSync(probe,['--callback-start'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:64*1024*1024});assert.equal(run.status,0,run.stderr);
   const rows=run.stdout.trim().split(/\r?\n/);assert.equal(rows.length,commands.length);
   for(let i=0;i<commands.length;++i)if(!commands[i].startsWith('export '))assert.equal(Number(rows[i].split(' ')[0]),rejections.has(i)?0:1,`${rate}/${mono}/${size}/${reverse}/${sequence}: ${commands[i]}`);
   for(const {row,want} of wavs){assert.deepEqual(Buffer.from(rows[row],'hex'),want,`${rate}/${mono}/${size}/${reverse}/${sequence}: exact stereo export (duplicated mono), history/source preservation`);wavCases++;}
   const meta=rows[inspectRow].split(' ').map(Number);assert.equal(meta[1],history||multiplying||pendingRtrg?expectedInfo[0]:size);assert.equal(meta[5],pendingRtrg?expectedInfo[2]:(normalizing||history)?0:1);assert.equal(meta[6],pendingRtrg?expectedInfo[3]:history?1:0);
   assert(pendingRtrg?meta[2]===2:stoppingDestination?meta[2]===(queuedStop&&inspectFrame<gate?9:10):(meta[2]===9||sequence==='undo-redo'&&meta[2]===2),JSON.stringify({rate,mono,size,reverse,sequence,meta,inspectFrame,gate,stopAt}));
   if(stoppingDestination)assert.equal(Number(rows[stoppedRow].split(' ')[2]),pendingRtrg?2:3,pendingRtrg?'RTRG cancels the old queue/STOP instead of reviving it later':'Native destination stops at its original grid');
   for(const {row,want} of rtrgMetadata)assert(Math.abs(Number(rows[row].split(' ')[3])-want)<=1e-12,'Original RTRG Start reset and sixteenth snap');
   for(const {row,want,clock=true} of regions){const r=rows[row].split(' ').map(Number);assert.equal(r[4],clock?1:0,'Stored replacement retains original visual clock and pre-start suppression');assert(Math.abs(r[5]-want)<=1e-12,'Original history progress must use restored duration/metadata');}
   for(const {row,want} of checks){const actual=rows[row].split(' ').map(Number).slice(-2);for(const value of actual){const error=Math.abs(value-want);maximum=Math.max(maximum,error);assert(error<=1e-6,`${rate}/${mono}/${size}/${reverse}/${sequence} frame ${rows[row]} want ${want}; gate=${gate} grid=${grid} voices=${JSON.stringify(voices.map(v=>v.node.started))}`);}}
  }
  cases++;
 }
 console.log(JSON.stringify({result:'PASS',mode:probe?'native':'reference-only',pendingRtrg,queuedStop,multiplying,history,normalizing,stoppingDestination,cases,wavCases,samples,maximumAbsoluteError:probe?maximum:null,tolerance:1e-6,scope:(pendingRtrg?'Unchanged immediate Loop.retrigger/play/teardown from unborn queues and scheduled STOP, with Apply/MULT/NORM/history/MERGE/STUT/rapid combinations':queuedStop?'Unchanged Loop.multiply/multiplyInstant/normalize/getProgress on a constructed future source with same/next-grid scheduled STOP':multiplying?'Unchanged Loop.multiply/multiplyInstant/undo/redo/normalize/getProgress during scheduled STOP':history?'Unchanged Loop.undo/redo/normalize/getProgress during scheduled STOP':normalizing?'Unchanged Loop.normalize/normalizeBuffer and Loop.scheduleStop':'Unchanged mergeLoops/mixBuffersCircular')+', actual original Loop.play/restart/Undo/Redo source snapshots and gate; node IO stubs/raw source PCM, no browser gain/stop envelopes/resampler or advancing worker-latency claim'}));
})().catch(e=>{console.error(e);process.exitCode=1;});
