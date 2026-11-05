// Characterize unchanged queued Undo/Redo after source construction.
// Default is reference-only; --native PROBE is the future, currently failing native PCM gate.
// Audio nodes are IO stubs; this compares source PCM, not browser resampling/envelopes.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const makeBuffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c],copyToChannel:(data,c)=>channels[c].set(data)};};
 const param=()=>({value:1,events:[],setValueAtTime(value,time){this.events.push({value,time});},linearRampToValueAtTime(){},cancelScheduledValues(){}});
 const node=()=>({connect(){},disconnect(){this.disconnected=true;},start(time,offset){this.started={time,offset};},stop(time){this.stopped=true;this.stoppedAt=time;},gain:param(),playbackRate:param()});
 class BufferSource {constructor(){Object.assign(this,node());}}
 const audio={state:'running',currentTime:0,createBuffer:(count,length,rate)=>makeBuffer(Array.from({length:count},()=>Array(length).fill(0)),rate),createBufferSource:()=>new BufferSource(),createGain:node,createChannelMerger:node,createAnalyser:node};
 const context={window:{},document:{getElementById:()=>null,createElement:()=>({}),head:{appendChild(){}}},AudioBufferSourceNode:BufferSource,effects:{},FACTORY_EFFECTS:{},console,setTimeout:()=>0,clearTimeout(){},audio};vm.createContext(context);
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
 `,context);

 vm.runInContext(`state.undoStack=[];state.redoStack=[];UIManager.updateLoopDisplays=()=>{};
 globalThis.seedHistory=()=>{state.undoStack=[];state.redoStack=[];const l=state.loops[0];l.state='stopped';l.graph=null;l.multiplyInstant();};
 globalThis.restore=redo=>state.loops[0][redo?'redo':'undo']();
 globalThis.queue=()=>{state.loops[0].state='queued';};
 globalThis.info=()=>{const l=state.loops[0];return [l.audioBuffer.length,l.state,l.undoStack.length,l.redoStack.length];};`,context);

 vm.runInContext(`globalThis.graph=()=>state.loops[0].graph;
 globalThis.teardown=()=>state.loops[0]._teardown();`,context);
 const nativeIndex=process.argv.indexOf('--native'),probe=nativeIndex<0?null:process.argv[nativeIndex+1];
 if(nativeIndex>=0)assert(probe,'--native requires the current aion_start_probe path');
 const sequenceIndex=process.argv.indexOf('--only-sequence'),onlySequence=sequenceIndex<0?null:process.argv[sequenceIndex+1];
 const sequences=['undo','rapid','apply','apply-chain','rtrg','norm-before','norm-after','cleanup'];
 if(sequenceIndex>=0)assert(sequences.includes(onlySequence),'Unknown --only-sequence');
 let cases=0,sourceCount=0,samples=0,silent=0,observations=0,overlapSamples=0,distinctBufferSamples=0,maximum=0;
 const counts={};
 const ceilSample=v=>{const near=Math.round(v);return Math.abs(v-near)<=4*Number.EPSILON*Math.max(1,Math.abs(v))?near:Math.ceil(v);};
 const data=b=>Array.from({length:b.numberOfChannels},(_,c)=>Float32Array.from(b.getChannelData(c)));
 const equalData=(b,want,label)=>{assert.equal(b.numberOfChannels,want.length,label);for(let c=0;c<want.length;++c)assert.deepEqual(b.getChannelData(c),want[c],label);observations++;};
 for(const mono of [true,false])for(const rate of [8000,48000,96000])for(const size of [1,17,512,4097])for(const reverse of [false,true])for(const nearGrid of (process.argv.includes('--near-grid')?[true]:[false,true]))for(const sequence of (onlySequence?[onlySequence]:sequences)){
  audio.currentTime=0;vm.runInContext('state.syncEnabled=false;state.bpm=120;',context);
  const raw=[Array.from({length:size},(_,i)=>Math.fround(.12+.4*Math.sin(i*.11))),Array.from({length:size},(_,i)=>Math.fround(-.08+.2*Math.cos(i*.073)))];
  const b=makeBuffer(mono?[raw[0]]:raw,rate);context.condition(b);context.setup(b);context.seedHistory();
  const commands=probe?[`prepare 81920 ${rate}`,'record 0 0',...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),'finish 0 0','wait 0 0','stop 0 0','multiply 0 0','wait 0 0']:[];
  let frame=size,bpm=137.3,origin=frame-frame*120/bpm,destroyed=false;
  audio.currentTime=frame/rate;context.configure(1,bpm,origin/rate,1);context.setReverse(reverse);context.setDelay(.217);
  if(probe)commands.push(`sync 1 ${bpm} 1 1`,`reverse 0 ${reverse?1:0}`,'delay 0 .217');
  const sources=[],checks=[],phaseChecks=[];
  let gate=Infinity;
  const remember=()=>{
   const node=context.sourceNode();assert(!sources.some(v=>v.node===node),'Each restart constructs a distinct source');
   const stored=context.stored(),want=data(stored).map(c=>reverse?Float32Array.from(c).reverse():c);
   equalData(node.buffer,want,'Source contains PCM from the stored buffer at construction');
   assert.equal(node.loopEnd,stored.length/rate,'Every node captures its own loop duration');
   sources.push({node,want});sourceCount++;return node;
  };
  const sample=(voice,at)=>{
   const {node,want}=voice,start=node.started.time*rate,stop=node.stoppedAt===undefined?Infinity:node.stoppedAt*rate;
   if(destroyed||at<ceilSample(start)||at>=ceilSample(stop))return null;
   let speed=1,time=-Infinity;for(const e of node.playbackRate.events)if(ceilSample(e.time*rate)<=at&&e.time>=time){time=e.time;speed=Math.fround(e.value);}
   const length=node.buffer.length,phase=((node.started.offset*rate+(at-start)*speed)%length+length)%length,i=Math.floor(phase),next=(i+1)%length,f=Math.fround(phase-i);
   const value=c=>Math.fround(want[c][i]+Math.fround(want[c][next]-want[c][i])*f);
   return [value(0),value(mono?0:1)];
  };
  const render=until=>{for(;frame<until;++frame){let l=0,r=0,live=0;
    for(const voice of sources){const v=sample(voice,frame);if(v){l=Math.fround(l+v[0]);r=Math.fround(r+v[1]);live++;if(voice.node.buffer!==context.stored())distinctBufferSamples++;}}
    const value=frame<gate?0:(l+r)*.5*Math.SQRT1_2;assert(Number.isFinite(value),'Source prediction must remain finite');
    if(frame<gate)silent++;else if(live>1)overlapSamples++;
    if(probe){checks.push({row:commands.length,value});commands.push('tick 0 0');}samples++;
   }audio.currentTime=frame/rate;
  };
  let grid=context.grid();if(grid-audio.currentTime<=.11){render(ceilSample(grid*rate)+1);grid=context.grid();}
  gate=ceilSample(grid*rate);if(probe)commands.push('play 0 0');context.queue();
  render(Math.max(frame,gate-Math.ceil(rate*.1)));await context.scheduledPlay(grid);if(probe)commands.push('birth 0 0');
  const initial=remember(),graph=context.graph(),volume=graph.nodes.volume,analyser=graph.nodes.analyser;
  const volumeEvents=JSON.stringify(context.volumeEvents());assert.equal(initial.started.time,grid);
  render(gate-Math.ceil(rate*(nearGrid?.001:.080)));
  const undoAt=()=>{
   const before=context.sourceNode(),oldBuffer=before.buffer,oldDuration=before.loopEnd,oldStart={...before.started};
   const when=audio.currentTime+.005;context.restore(false);const node=remember();if(probe){phaseChecks.push({row:commands.length,target:ceilSample(when*rate)});commands.push('undo 0 0');}
   assert.equal(node.started.time,when,'Undo requests the original minimum 5ms source replacement');
   assert.equal(before.stoppedAt,when,'Undo stops only the graph current source at its replacement time');
   assert.equal(before.buffer,oldBuffer,'Undo cannot replace an older AudioBufferSourceNode buffer');
   assert.equal(before.loopEnd,oldDuration,'Undo cannot rewrite an older source loop length');assert.deepEqual(before.started,oldStart);
   assert.equal(context.info()[0],size);assert.equal(context.info()[2],0);assert.equal(context.info()[3],1);
   assert.equal(context.graph(),graph);assert.equal(graph.nodes.volume,volume);assert.equal(graph.nodes.analyser,analyser);
   assert.equal(JSON.stringify(context.volumeEvents()),volumeEvents,'History does not move the original post-effect volume grid');observations+=10;return node;
  };
  if(sequence==='norm-before'){context.normalize();if(probe)commands.push('normalize 0 0','wait 0 0');
   if(!reverse)sources[0].want=data(context.stored());}
  let apply,firstApply;
  if(sequence==='apply'||sequence==='apply-chain'){
   context.applyPhase();apply=remember();if(probe)commands.push('apply 0 0');
   assert.equal(apply.started.time,audio.currentTime+.05);assert.equal(initial.stoppedAt,apply.started.time);firstApply=apply;
   render(frame+Math.ceil(rate*.001));
   if(sequence==='apply-chain'){context.applyPhase();apply=remember();if(probe)commands.push('apply 0 0');assert.equal(firstApply.stoppedAt,apply.started.time);render(frame+Math.ceil(rate*.001));}
  }
  if(sequence==='rtrg'){await context.rtrg();const rtrg=remember();if(probe)commands.push('rtrg 0 0');assert.equal(rtrg.started.time,audio.currentTime+.005);render(frame+Math.ceil(rate*.002));}
  const replacement=undoAt();
  if(apply){assert(replacement.started.time<apply.started.time,'Undo overtakes the newest 50ms Apply');assert.equal(initial.stoppedAt,firstApply.started.time,'Undo retains the initial source older independent Apply stop');observations+=2;}
  if(sequence==='rapid'){
   render(frame+Math.ceil(rate*.002));const when=audio.currentTime+.005,old=replacement.buffer;
   context.restore(true);const redo=remember();if(probe){phaseChecks.push({row:commands.length,target:ceilSample(when*rate)});commands.push('redo 0 0');}
   assert.equal(redo.started.time,when);assert.equal(replacement.stoppedAt,when);assert.equal(replacement.buffer,old);assert.equal(context.info()[0],size*2);observations+=4;
   render(frame+Math.ceil(rate*.002));undoAt();
  }
  if(sequence==='norm-after'){
   render(frame+Math.ceil(rate*.001));const current=context.sourceNode(),old=current.buffer,stored=context.stored();context.normalize();if(probe)commands.push('normalize 0 0','wait 0 0');
   assert.equal(context.sourceNode(),current);assert.equal(current.buffer,old,'NORM never creates/replaces an existing source');
   if(!reverse){assert.equal(current.buffer,stored);sources.at(-1).want=data(stored);}else assert.notEqual(current.buffer,stored);
   observations+=3;
  }
  for(const {node,want} of sources)equalData(node.buffer,want,'Every still-owned source retains its own expected PCM');
  render(gate+Math.ceil(rate*.060));
  if(sequence==='cleanup'){
   context.teardown();if(probe)commands.push('cleanup 0 0');destroyed=true;
   for(const {node} of sources)assert(node.stopped&&node.disconnected,'Teardown stops/disconnects every current/retired source');observations+=sources.length;
   render(frame+147);
  }else render(frame+147);
  if(probe){const run=spawnSync(probe,['--callback-start'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);
   const rows=run.stdout.trim().split(/\r?\n/).map(l=>l.split(' ').map(Number));assert.equal(rows.length,commands.length);
   rows.forEach((row,i)=>assert.equal(row[0],1,'Native post-birth source gate rejected '+commands[i]+' '+JSON.stringify({mono,rate,size,reverse,nearGrid,sequence})));
   for(const c of phaseChecks)assert.equal(rows[c.row][4],c.target,'History source replacement deadline must be 5ms');
   for(const c of checks)for(const actual of rows[c.row].slice(6,8)){const error=Math.abs(actual-c.value);maximum=Math.max(maximum,error);assert(error<=1e-6,'Post-birth history PCM '+JSON.stringify({error,actual,expected:c.value,mono,rate,size,reverse,nearGrid,sequence}));}
  }
  counts[sequence]=(counts[sequence]??0)+1;cases++;
 }
 if(!onlySequence)assert(overlapSamples>0,'Corpus includes audible independent source overlap');assert(distinctBufferSamples>0,'Corpus needs distinct stored and source buffers');
 console.log(JSON.stringify({result:'PASS',mode:probe?'native_source_comparison':'reference_only',cases,sourceCount,observations,predictedFrames:samples,preGridSilentFrames:silent,overlapSamples,distinctBufferSamples,counts,maximumAbsoluteError:probe?maximum:null,tolerance:probe?1e-6:null,scope:'Unchanged Loop.undo/redo/restart plus AudioGraph source PCM/length/times, 5ms replacement, original grid, pending Apply/RTRG, NORM and cleanup; source model only, browser envelopes/resampling/hardware excluded. Reference-only pass is not native acceptance.'}));
})().catch(error=>{console.error(error);process.exitCode=1;});
