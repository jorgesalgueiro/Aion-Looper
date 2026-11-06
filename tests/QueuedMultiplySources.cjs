// MULT on a constructed, grid-gated source: unchanged JS methods.
// Default is no-input doubling; --input-capture exercises the original recorder handlers.
// Default is explicitly reference-only; --native PROBE also checks native source PCM/deadlines.
// Audio nodes are IO stubs; this compares source PCM, not browser resampling/envelopes.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..'),inputCapture=process.argv.includes('--input-capture');
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
 const nativeIndex=process.argv.indexOf('--native'),probe=nativeIndex<0?null:process.argv[nativeIndex+1];
 if(nativeIndex>=0)assert(probe,'--native requires the current aion_start_probe path');
 const sequenceIndex=process.argv.indexOf('--only-sequence'),onlySequence=sequenceIndex<0?null:process.argv[sequenceIndex+1];
 const controls=process.argv.includes('--controls');
 const workerMetadata=process.argv.includes('--worker-metadata');
 const workerTempo=process.argv.includes('--worker-tempo');
 const sourceTempo=process.argv.includes('--source-tempo'),tempoControl=workerTempo||sourceTempo;
 const sequences=inputCapture?['capture-short','capture-append','capture-grid','capture-pending','capture-gated-playing']:sourceTempo?['source-tempo']:workerTempo?['worker-tempo']:workerMetadata?['worker-reverse','worker-start','worker-both']:controls?['metadata','control-apply','speed-before','speed-tie']:['double','rapid','apply-chain','norm-history'];
 if(sequenceIndex>=0)assert(sequences.includes(onlySequence),'Unknown --only-sequence');
 let cases=0,wavCases=0,sourceCount=0,samples=0,silent=0,observations=0,overlapSamples=0,distinctBufferSamples=0,maximum=0;
 const counts={};
 const ceilSample=v=>{const near=Math.round(v);return Math.abs(v-near)<=4*Number.EPSILON*Math.max(1,Math.abs(v))?near:Math.ceil(v);};
 const data=b=>Array.from({length:b.numberOfChannels},(_,c)=>Float32Array.from(b.getChannelData(c)));
 const equalData=(b,want,label)=>{assert.equal(b.numberOfChannels,want.length,label);for(let c=0;c<want.length;++c)assert.deepEqual(b.getChannelData(c),want[c],label);observations++;};
 for(const mono of [true,false])for(const rate of [8000,48000,96000])for(const size of (inputCapture?[17,512]:[1,17,512,4097]))for(const reverse of [false,true])for(const nearGrid of (process.argv.includes('--near-grid')?[true]:[false,true]))for(const nextTempo of (tempoControl?[93.75,211.7]:[null]))for(const sequence of (onlySequence?[onlySequence]:sequences)){
  const overlapBefore=overlapSamples;audio.currentTime=0;audio.sampleRate=rate;vm.runInContext('state.syncEnabled=false;state.bpm=120;',context);
  const raw=[Array.from({length:size},(_,i)=>Math.fround(.12+.4*Math.sin(i*.11))),Array.from({length:size},(_,i)=>Math.fround(-.08+.2*Math.cos(i*.073)))];
  const b=makeBuffer(mono?[raw[0]]:raw,rate);context.condition(b);context.setup(b);context.seedHistory();
  const commands=probe?[`prepare 81920 ${rate}`,'record 0 0',...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),'finish 0 0','wait 0 0','stop 0 0','multiply 0 0','wait 0 0']:[];
  let frame=size,bpm=137.3,origin=frame-frame*120/bpm,destroyed=false;
  const syncMode=inputCapture&&sequence==='capture-gated-playing'&&nearGrid?2:1;audio.currentTime=frame/rate;context.configure(syncMode,bpm,origin/rate,1);context.setReverse(reverse);context.setDelay(.217);
  if(probe)commands.push(`sync ${syncMode} ${bpm} 1 1`,`reverse 0 ${reverse?1:0}`,'delay 0 .217');
  const sources=[],checks=[],phaseChecks=[],wavChecks=[];
  let gate=Infinity;
  const remember=()=>{
   const node=context.sourceNode();assert(!sources.some(v=>v.node===node),'Each restart constructs a distinct source');
   const stored=context.stored(),want=data(stored).map(c=>context.direction()?Float32Array.from(c).reverse():c);
   equalData(node.buffer,want,'Source contains PCM from the stored buffer at construction');
   assert.equal(node.loopEnd,stored.length/rate,'Every node captures its own loop duration');
   sources.push({node,want});sourceCount++;return node;
  };
  const sample=(voice,at)=>{
   const {node,want}=voice,start=node.started.time*rate,stop=node.stoppedAt===undefined?Infinity:node.stoppedAt*rate;
   if(destroyed||at<ceilSample(start)||at>=ceilSample(stop))return null;
   // Integrate the source's own AudioParam timeline. Later writes at one
   // timestamp replace earlier ones; writes before start do not erase its
   // already scheduled initial-rate event at the future start timestamp.
   const events=[...node.playbackRate.events].sort((a,b)=>a.time-b.time);
   let speed=1,cursor=start,advance=0;
   for(const e of events){const when=e.time*rate;if(when>at)break;
    if(when>cursor){advance+=(when-cursor)*speed;cursor=when;}speed=Math.fround(e.value);
   }
   advance+=(at-cursor)*speed;
   const length=node.buffer.length,phase=((node.started.offset*rate+advance)%length+length)%length,i=Math.floor(phase),next=(i+1)%length,f=Math.fround(phase-i);
   const value=c=>Math.fround(want[c][i]+Math.fround(want[c][next]-want[c][i])*f);
   return [value(0),value(mono?0:1)];
  };
  const render=(until,take=null)=>{const captureStart=frame;for(;frame<until;++frame){let l=0,r=0,live=0;
    for(const voice of sources){const v=sample(voice,frame);if(v){l=Math.fround(l+v[0]);r=Math.fround(r+v[1]);live++;if(voice.node.buffer!==context.stored())distinctBufferSamples++;}}
    const value=frame<gate?0:(l+r)*.5*Math.SQRT1_2;assert(Number.isFinite(value),'Source prediction must remain finite');
    if(frame<gate)silent++;else if(live>1)overlapSamples++;
    if(probe){checks.push({row:commands.length,value});commands.push(take?`tick 0 ${take[0][frame-captureStart]} ${take[mono?0:1][frame-captureStart]}`:'tick 0 0');}samples++;
   }audio.currentTime=frame/rate;
  };
  let grid=context.grid();if(grid-audio.currentTime<=.11){render(ceilSample(grid*rate)+1);grid=context.grid();}
  gate=ceilSample(grid*rate);if(probe)commands.push('play 0 0');context.queue();
  render(Math.max(frame,gate-Math.ceil(rate*.1)));await context.scheduledPlay(grid);if(probe)commands.push('birth 0 0');
  const initial=remember(),graph=context.graph(),volume=graph.nodes.volume,analyser=graph.nodes.analyser;
  const volumeEvents=JSON.stringify(context.volumeEvents());assert.equal(initial.started.time,grid);
  render(gate-Math.ceil(rate*(nearGrid?(sequence==='capture-gated-playing'?.010:.001):.080)));
  const infoChecks=[];
  const inspect=()=>{if(probe){infoChecks.push({row:commands.length,want:context.info()});commands.push('inspect 0 0');}};
  const doubleAt=()=>{
   const before=context.sourceNode(),old=before.buffer,duration=before.loopEnd,start={...before.started},length=context.stored().length;
   const when=audio.currentTime+.005;context.multiply();const node=remember();
   if(probe)commands.push('multiply 0 0');
   if(workerMetadata){
    // JS doubling/restart is synchronous. Its following metadata writes cannot
    // change the constructed source. Native writes are deliberately delivered
    // before beginBlock/wait accepts the asynchronous worker result.
    if(sequence!=='worker-start'){context.setReverse(!reverse);if(probe)commands.push(`reverse 0 ${reverse?0:1}`);}
    if(sequence!=='worker-reverse'){context.setDelay(.731);if(probe)commands.push('delay 0 .731');}
   }
   if(tempoControl){
    const previousEvents=initial.playbackRate.events.length,scheduled=node.playbackRate.events.find(e=>e.time===when).value;
    context.tempo(nextTempo);if(probe&&workerTempo)commands.push(`sync 1 ${nextTempo} 1 1`);
    assert.equal(initial.playbackRate.events.length,previousEvents,'Tempo targets only the newest source AudioParam');
    const target=node.playbackRate.events.at(-1);assert.equal(target.targetConstant,.05);assert.equal(target.time,audio.currentTime);
    assert(target.time<node.started.time,'This corpus tests tempo before the initial scheduled rate, not its live exponential ramp');
    assert.equal(node.playbackRate.events.find(e=>e.time===when).value,scheduled,'Future initial-rate event survives earlier tempo target');
   }
   if(probe){phaseChecks.push({row:commands.length,target:ceilSample(when*rate)});commands.push('wait 0 0');}
   if(probe&&sourceTempo)commands.push(`sync 1 ${nextTempo} 1 1`);
   assert.equal(node.started.time,when,'No-input MULT requests minimum 5ms replacement');assert.equal(node.buffer.length,length*2);
   assert.equal(before.stoppedAt,when);assert.equal(before.buffer,old);assert.equal(before.loopEnd,duration);assert.deepEqual(before.started,start);
   assert.equal(context.graph(),graph);assert.equal(graph.nodes.volume,volume);assert.equal(graph.nodes.analyser,analyser);
   assert.equal(JSON.stringify(context.volumeEvents()),volumeEvents,'MULT retains the original post-effect grid gate');observations+=10;inspect();return node;
  };
  if(inputCapture&&sequence==='capture-gated-playing'){context.rtrg();remember();if(probe)commands.push('rtrg 0 0');render(frame+Math.ceil(rate*.006));assert(frame<gate,'Early replacement is still volume gated');if(probe){infoChecks.push({row:commands.length,want:context.info(),state:2});commands.push('inspect 0 0');}}
  if(inputCapture&&sequence==='capture-pending'){context.applyPhase();remember();if(probe)commands.push('apply 0 0');render(frame+Math.ceil(rate*.001));}
  if(sequence==='apply-chain'){
   context.applyPhase();const first=remember();if(probe)commands.push('apply 0 0');render(frame+Math.ceil(rate*.001));
   context.applyPhase();const last=remember();if(probe)commands.push('apply 0 0');assert.equal(first.stoppedAt,last.started.time);render(frame+Math.ceil(rate*.001));
  }
  if(inputCapture){
   const before=context.stored(),oldNode=context.sourceNode(),history=context.info()[2];
   context.multiply();if(probe)commands.push('multiply 0 1');
   assert.equal(context.info()[1],'multiplying');assert.equal(context.sourceNode(),oldNode);
   const length=sequence==='capture-short'?127:sequence==='capture-append'||sequence==='capture-pending'?513:Math.max(128,gate-frame+257);
   const take=[Array.from({length},(_,i)=>Math.fround(.1+.2*Math.sin(i*.079))),Array.from({length},(_,i)=>Math.fround(-.07+.3*Math.cos(i*.053)))];
   render(frame+length,take);assert.equal(context.info()[1],'multiplying','Capture does not end at the graph grid');
   if(probe){infoChecks.push({row:commands.length,want:[before.length,'multiplying',history,0],state:7});commands.push('inspect 0 0');}
   const chunks=[];for(let i=0;i<length;i+=64)chunks.push((mono?[take[0]]:take).map(c=>Float32Array.from(c.slice(i,i+64))));
   const when=audio.currentTime+.005;await context.endCapture(chunks);if(probe)commands.push('multiply 0 1');
   assert.equal(context.info()[1],'playing');assert.equal(context.graph(),graph);assert.equal(JSON.stringify(context.volumeEvents()),volumeEvents);
   if(length<128){assert.equal(context.stored(),before);assert.equal(context.sourceNode(),oldNode);assert.equal(context.info()[2],history);if(probe)commands.push('wait 0 0');}
   else {const replacement=remember();assert.equal(replacement.started.time,when);assert.equal(oldNode.stoppedAt,when);assert.equal(context.stored().length,before.length+length);
    assert.equal(context.info()[2],history+1);assert.equal(context.info()[3],0);
    if(probe){phaseChecks.push({row:commands.length,target:ceilSample(when*rate)});commands.push('wait 0 0');}}
   inspect();
   if(!mono){const expected=Buffer.from(context.wav()).toString('hex');if(probe){wavChecks.push({row:commands.length,expected});commands.push('export 0 0');}wavCases++;}
  }else {const doubled=doubleAt();
  if(tempoControl){
   render(frame+Math.ceil(rate*.010));const when=audio.currentTime+.05;
   context.applyPhase();const applied=remember();assert.equal(applied.started.time,when);
   if(probe){phaseChecks.push({row:commands.length,target:ceilSample(when*rate)});commands.push('apply 0 0');}
  }
  if(controls){
   const replacementAt=ceilSample(doubled.started.time*rate);
   render(sequence==='speed-tie'?replacementAt:frame+Math.ceil(rate*.001));
   const oldDirection=context.direction(),offset=doubled.started.offset,events=initial.playbackRate.events.length;
   context.setReverse(!oldDirection);context.setDelay(.731);
   if(probe)commands.push(`reverse 0 ${oldDirection?0:1}`,'delay 0 .731');
   equalData(doubled.buffer,sources.at(-1).want,'Metadata edits cannot rewrite the already constructed MULT source');
   assert.equal(doubled.started.offset,offset,'Start metadata cannot move a constructed source');
   if(sequence==='speed-before'||sequence==='speed-tie'||sequence==='control-apply'){
    context.userSpeed(.5);if(probe)commands.push('speed 0 .5');
    assert.equal(initial.playbackRate.events.length,events,'Speed writes target only the newest source');
    if(sequence==='speed-before')assert(context.rate()!==Math.fround(doubled.playbackRate.events[0].value),'Future initial-rate event differs from newer control');
   }
   if(sequence==='control-apply'){
    const when=audio.currentTime+.05;context.applyPhase();const applied=remember();
    assert.equal(applied.started.time,when);assert.equal(doubled.stoppedAt,when);
    if(probe){phaseChecks.push({row:commands.length,target:ceilSample(when*rate)});commands.push('apply 0 0');}
   }
  }
  if(sequence==='rapid'){render(frame+Math.ceil(rate*.002));doubleAt();}
  if(sequence==='norm-history'){
   render(frame+Math.ceil(rate*.002));const when=audio.currentTime+.005;context.restore(false);remember();
   if(probe){phaseChecks.push({row:commands.length,target:ceilSample(when*rate)});commands.push('undo 0 0');}inspect();
   render(frame+Math.ceil(rate*.001));const stored=context.stored(),current=context.sourceNode();context.normalize();
   if(probe)commands.push('normalize 0 0','wait 0 0');if(!reverse){assert.equal(current.buffer,stored);sources.at(-1).want=data(stored);}
  }
  }
  for(const {node,want} of sources)equalData(node.buffer,want,'Old source PCM survives MULT/history/NORM');
  render((inputCapture?Math.max(gate,frame):gate)+Math.ceil(rate*.060));
  if(sequence==='double'){
   context.teardown();if(probe)commands.push('cleanup 0 0');destroyed=true;
   for(const {node} of sources)assert(node.stopped&&node.disconnected,'Teardown disconnects every source');observations+=sources.length;
  }
  render(frame+147);
  if(probe){const run=spawnSync(probe,['--callback-start'],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(run.status,0,run.stderr);
   const rawRows=run.stdout.trim().split(/\r?\n/),rows=rawRows.map((l,i)=>wavChecks.some(c=>c.row===i)?[1]:l.split(' ').map(Number));assert.equal(rows.length,commands.length);
   for(const c of wavChecks)assert.equal(rawRows[c.row],c.expected,'Exact unchanged stereo WAV after capture '+JSON.stringify({rate,size,reverse,nearGrid,sequence}));
   rows.forEach((row,i)=>assert.equal(row[0],1,'Native post-birth MULT rejected '+commands[i]+' '+JSON.stringify({mono,rate,size,reverse,nearGrid,sequence})));
   for(const c of phaseChecks)assert.equal(rows[c.row][4],c.target,'Original source replacement deadline '+JSON.stringify({mono,rate,size,reverse,nearGrid,sequence,command:commands[c.row]}));
   for(const {row,want} of infoChecks){assert.equal(rows[row][1],want[0]);assert.equal(rows[row][5],want[2]);assert.equal(rows[row][6],want[3]);const check=infoChecks.find(c=>c.row===row);if(check.state!==undefined)assert.equal(rows[row][2],check.state);}
   for(const c of checks)for(const actual of rows[c.row].slice(6,8)){const error=Math.abs(actual-c.value);maximum=Math.max(maximum,error);assert(error<=1e-6,'Post-birth MULT PCM '+JSON.stringify({error,actual,expected:c.value,mono,rate,size,reverse,nearGrid,sequence}));}
  }
  if(inputCapture&&sequence!=='capture-pending')assert.equal(overlapSamples,overlapBefore,'Ordinary capture sources do not overlap');
  counts[sequence]=(counts[sequence]??0)+1;cases++;
 }
 if(!onlySequence){
  if(inputCapture)assert(overlapSamples>0,'Pending pre-capture Apply retains the independently stopped older source');
  else if(controls||workerMetadata||tempoControl)assert.equal(overlapSamples,0,'Sequential MULT/Apply control sources stop at the next source birth without overlap');
  else assert(overlapSamples>0,'Corpus includes audible independently stopped overlap');
 }
 assert(distinctBufferSamples>0);
 console.log(JSON.stringify({result:'PASS',mode:probe?'native_source_comparison':'reference_only',cases,wavCases,sourceCount,observations,predictedFrames:samples,preGridSilentFrames:silent,overlapSamples,distinctBufferSamples,counts,maximumAbsoluteError:probe?maximum:null,tolerance:probe?1e-6:null,scope:inputCapture?'Unchanged startMultiply/stopMultiply/processMultiplyData on a constructed grid-gated graph, actual captured chunks, short/no-op, append across grid and pending Start overlap; source PCM and original gate, recorder IO stubbed, no browser/hardware/worker-latency claim':sourceTempo?'Unchanged tempo target after MULT source construction, before its initial-rate event; old and replacement source PCM, current-metadata Apply and original grid; no live exponential-ramp claim':workerTempo?'Unchanged SyncManager.updateSettings during native no-input MULT worker finalization, before the replacement initial-rate event; source snapshots, grid and PCM; no advancing worker clock, live exponential ramp or hardware claim':workerMetadata?'Unchanged synchronous no-input MULT then Reverse/Start metadata versus native commands before worker-result acceptance; source snapshots, exact 5ms deadline/grid and PCM, no advancing worker clock or tempo/speed claim':controls?'Unchanged no-input MULT followed by Reverse/Start metadata, Apply and speed before/at the 5ms source deadline; captured buffers/direction/offset/rate and grid; source model only, delayed worker and tempo excluded':'Unchanged no-input MULT/startMultiply/multiplyInstant, source buffers/lengths/5ms stops, original grid, rapid doubles, pending Applies, Undo/NORM and cleanup; source model, browser rendering and worker latency excluded'}));
})().catch(error=>{console.error(error);process.exitCode=1;});
