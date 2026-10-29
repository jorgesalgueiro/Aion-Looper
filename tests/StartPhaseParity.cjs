// Execute unchanged UIManager.setLoopStartDelay, Loop.restart, AudioGraph and buffer DSP.
// Audio nodes are IO stubs; this compares source PCM, not browser resampling/envelopes.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
(async()=>{
 const root=path.resolve(__dirname,'..');
 const makeBuffer=(data,rate)=>{const channels=data.map(c=>Float32Array.from(c));return {length:channels[0].length,duration:channels[0].length/rate,sampleRate:rate,numberOfChannels:channels.length,getChannelData:c=>channels[c]};};
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
 const queuedSpeedOnly=process.argv[3]==='--queued-early-speed';
 const queuedStopOnly=process.argv[3]==='--queued-stop';
 const queuedRtrgOnly=process.argv[3]==='--queued-early-rtrg';
 const queuedRestartOnly=process.argv[3]==='--queued-early-restart'||queuedRtrgOnly;
 const queuedEarlyOnly=process.argv[3]==='--queued-early-edits';
 const queuedCleanupOnly=process.argv[3]==='--queued-cleanup';
 const queuedLateOnly=process.argv[3]==='--queued-late'||queuedCleanupOnly;
 let cases=0,samples=0,maximum=0,nudges=0,tinyWraps=0,retriggers=0,queuedStarts=0,queuedSilentFrames=0;
 for(const mono of [true,false])for(const sampleRate of [8000,48000,96000])for(const length of [1,17,512,4097,20003]){
  audio.currentTime=0;vm.runInContext('state.syncEnabled=false;state.bpm=120;',context);
  const raw=[Array.from({length},(_,i)=>Math.fround(0.12+0.4*Math.sin(i*0.11))),Array.from({length},(_,i)=>Math.fround(-0.08+0.2*Math.cos(i*0.073)))];
  const buffer=makeBuffer(mono?[raw[0]]:raw,sampleRate);context.condition(buffer);
  let frame=length,phase=0,origin=0,bpm=120,pending=[],excluded=false;
  const commands=[`prepare 81920 ${sampleRate}`,'record 0 0',...raw[0].map((v,i)=>`tick 0 ${v}${mono?'':' '+raw[1][i]}`),'finish 0 0','wait 0 0'];
  audio.currentTime=frame/sampleRate;context.setup(buffer);let playingSource=context.source();const checks=[],metadata=[];
  const render=count=>{for(let n=0;n<count;++n){while(pending.length&&frame>=pending[0].at){const event=pending.shift();phase=event.phase;playingSource=event.source;}
   const src=playingSource,index=Math.floor(phase),next=(index+1)%length,f=Math.fround(phase-index);
   const value=c=>Math.fround(src.getChannelData(c)[index]+Math.fround(src.getChannelData(c)[next]-src.getChannelData(c)[index])*f);
   const expected=excluded?0:(value(0)+value(mono?0:1))*0.5*Math.SQRT1_2;
   checks.push({row:commands.length,expected});commands.push('tick 0 0');phase=(phase+context.rate())%length;frame++;
  }};
  if(queuedStopOnly||queuedLateOnly||queuedEarlyOnly||queuedRestartOnly||queuedSpeedOnly){
   for(const reverse of [false,true])for(const beforeGrid of ((queuedRestartOnly||queuedSpeedOnly)?[true,false]:[true]))for(const speed of (queuedSpeedOnly?[.25,.5,2]:[1])){
    context.stopped();commands.push('stop 0 0');
    // Reapplying an unchanged tempo preserves origin; the production clock
    // does not accumulate rounding from a redundant tempo conversion.
    const nextBpm=137.3;if(bpm!==nextBpm)origin=frame-(frame-origin)*bpm/nextBpm;bpm=nextBpm;
    commands.push(`sync 1 ${bpm} 1 1`);context.configure(1,bpm,origin/sampleRate,1);
    context.phaseSpeed(1);commands.push('speed 0 1');
    if(queuedEarlyOnly||queuedRestartOnly||queuedSpeedOnly)context.setReverse(reverse);else if(reverse)context.toggle();commands.push(`reverse 0 ${reverse?1:0}`);
    context.setDelay(0.217);commands.push('delay 0 0.217');
    audio.currentTime=frame/sampleRate;let grid=context.grid();
    if((queuedRestartOnly||queuedSpeedOnly)&&grid-audio.currentTime<=.100){
     // Some recorded lengths land just before a grid. Wait while STOPPED so
     // every retained case can exercise a genuine 80ms early edit interval.
     const idleUntil=Math.ceil(grid*sampleRate)+1;
     while(frame<idleUntil){checks.push({row:commands.length,expected:0});commands.push('tick 0 0');frame++;queuedSilentFrames++;}
     audio.currentTime=frame/sampleRate;grid=context.grid();
    }
    commands.push('play 0 0');
    if(!queuedLateOnly){await context.scheduledPlay(grid);commands.push('birth 0 0');}
    const target=grid*sampleRate,nearest=Math.round(target);
    const deadline=Math.abs(target-nearest)<=4*Number.EPSILON*Math.max(1,Math.abs(target))?nearest:Math.ceil(target);
    const stopFrame=Math.floor(target-sampleRate*.020);
    assert(stopFrame>frame,'Fixture must allow graph birth before the stop action');
    const silence=until=>{while(frame<until){checks.push({row:commands.length,expected:0});commands.push('tick 0 0');frame++;queuedSilentFrames++;}};
    if(queuedSpeedOnly){
     const original=context.sourceNode(),originalBuffer=context.source(),originalOffset=original.started.offset;
     silence(deadline-Math.ceil(sampleRate*(beforeGrid?.080:.001)));audio.currentTime=frame/sampleRate;
     context.userSpeed(speed);commands.push(`speed 0 ${speed}`);
     assert.equal(context.sourceNode(),original,'Speed updates the existing future source');
     assert.equal(original.started.offset,originalOffset,'Speed metadata reanchor does not change actual source.start offset');
     assert(original.playbackRate.events.some(e=>e.time===audio.currentTime&&Math.fround(e.value)===context.rate()),'Original AudioParam receives current speed before source start');
     silence(deadline);
     pending.push({source:originalBuffer,at:deadline,phase:((originalOffset*sampleRate+(deadline-target)*context.rate())%length+length)%length});
     render(Math.min(length+3,147));queuedStarts++;continue;
    }
    if(queuedRestartOnly){
     const original=context.source(),oldNode=context.sourceNode(),oldOffset=context.offset();
     const editFrame=deadline-Math.ceil(sampleRate*(beforeGrid?.080:(queuedRtrgOnly?.002:.020)));
     assert(editFrame>frame,'Fixture leaves time for early callback and restart edit');
     silence(editFrame);audio.currentTime=frame/sampleRate;
     context.setDelay(.731);commands.push('delay 0 0.731');
     context.setReverse(!reverse);commands.push(`reverse 0 ${reverse?0:1}`);
     // Q is nontrivial at every rate for the longest clip. Tiny-clip nudges
     // retain the existing documented modulo-equivalent native wrap policy.
     if(queuedRtrgOnly){await context.rtrg();metadata.push({row:commands.length,value:context.delayValue()});commands.push('rtrg 0 0');retriggers++;}
     else if(length===20003){context.quantize();commands.push('quantize 0 0');}
     else if(length===17){context.nudge(reverse?-1:1);commands.push(`nudge 0 ${reverse?-1:1}`);}
     else {context.applyPhase();commands.push('apply 0 0');}
     const targetTime=audio.currentTime+(queuedRtrgOnly?.005:.05),restartTarget=targetTime*sampleRate,rounded=Math.round(restartTarget);
     const restartDeadline=Math.abs(restartTarget-rounded)<=4*Number.EPSILON*Math.max(1,Math.abs(restartTarget))?rounded:Math.ceil(restartTarget);
     assert.equal(oldNode.stoppedAt,targetTime,'Original retrigger schedules old source stop at replacement start');
     assert.equal(context.sourceNode().started.time,targetTime,'Original replacement starts at its 5/50ms action deadline');
     const replacement=context.source(),newOffset=context.offset();
     let followingStop=0;
     if(!beforeGrid&&length===20003){
      context.scheduleStop();commands.push('stopgrid 0 0');followingStop=context.stopTime();
      assert(followingStop>grid,'Original scheduled STOP retains a replacement scheduled before it');
     }
     if(beforeGrid){
      assert(restartDeadline<deadline);silence(restartDeadline);
     }else{
      silence(deadline);pending.push({source:original,at:deadline,phase:((oldOffset*sampleRate+(deadline-target)*context.rate())%length+length)%length});
      render(restartDeadline-frame);
     }
     pending.push({source:replacement,at:restartDeadline,phase:((newOffset*sampleRate+(restartDeadline-restartTarget)*context.rate())%length+length)%length});
     assert(context.volumeEvents().some(e=>e.value===0&&e.time===grid),'Unchanged graph retains its zero-volume scheduled start at original grid');
     if(beforeGrid){const gated=deadline-frame;excluded=true;render(gated);excluded=false;queuedSilentFrames+=gated;}
     render(Math.max(deadline+147-frame,147));
     if(followingStop){const stopDeadline=Math.ceil(followingStop*sampleRate);render(stopDeadline-frame);silence(frame+147);}
     queuedStarts++;continue;
    }
    if(queuedEarlyOnly){
     // Start and reverse are metadata edits, not a replacement source. Build
     // first, then edit during the inaudible future-source interval.
     const original=context.source(),initialOffset=context.offset();
     silence(deadline-Math.ceil(sampleRate*.060));audio.currentTime=frame/sampleRate;
     context.setDelay(.731);commands.push('delay 0 0.731');
     context.setReverse(!reverse);commands.push(`reverse 0 ${reverse?0:1}`);
     assert.equal(context.source(),original,'Unchanged metadata edits retain scheduled source buffer');
     assert.equal(context.offset(),initialOffset,'Unchanged metadata edits retain scheduled source offset');
     silence(deadline);
     pending.push({source:original,at:deadline,phase:((initialOffset*sampleRate+(deadline-target)*context.rate())%length+length)%length});
     render(Math.min(length+3,147));queuedStarts++;
     // The next real source must use the newly accepted metadata.
     context.stopped();commands.push('stop 0 0');audio.currentTime=frame/sampleRate;
     const freshGrid=context.grid(),freshTarget=freshGrid*sampleRate;
     const rounded=Math.round(freshTarget),freshDeadline=Math.abs(freshTarget-rounded)<=4*Number.EPSILON*Math.max(1,Math.abs(freshTarget))?rounded:Math.ceil(freshTarget);
     await context.scheduledPlay(freshGrid);commands.push('play 0 0');commands.push('birth 0 0');
     silence(freshDeadline);
     pending.push({source:context.source(),at:freshDeadline,phase:((context.offset()*sampleRate+(freshDeadline-freshTarget)*context.rate())%length+length)%length});
     render(Math.min(length+3,147));queuedStarts++;continue;
    }
    if(queuedLateOnly){
     // Delivery after the grid creates a source now, not in the past. Keep
     // every intervening native sample silent, including post-grid samples.
     silence(deadline+Math.ceil(sampleRate*.023));audio.currentTime=frame/sampleRate;
     // A newly created source takes current controls, not a stale smoothing
     // ramp from the inaudible queued source. Preserve the original deadline.
     if(length===17){context.configure(0,bpm,origin/sampleRate,1);commands.push(`sync 0 ${bpm} 1 1`);}
     if(length===512){const next=211.7;origin=frame-(frame-origin)*bpm/next;bpm=next;
      context.configure(1,bpm,origin/sampleRate,1);commands.push(`sync 1 ${bpm} 1 1`);}
     if(length===4097){const speed=Math.fround(.37);context.phaseSpeed(speed);commands.push(`speed 0 ${speed}`);}
     if(length===20003){context.setDelay(.731);commands.push('delay 0 0.731');}
     await context.scheduledPlay(grid);commands.push('birth 0 0');
     pending.push({source:context.source(),at:frame,phase:((context.offset()*sampleRate)%length+length)%length});
     render(Math.min(length+3,147));queuedStarts++;
     if(queuedCleanupOnly){
      audio.currentTime=frame/sampleRate;const retired=context.cleanup();
      assert(retired.source.stopped&&retired.source.disconnected&&retired.stopped&&retired.detached,'Unchanged teardown stops and disconnects the replacement source');
      commands.push('cleanup 0 0');silence(frame+147);
     }
     continue;
    }
    silence(stopFrame);audio.currentTime=frame/sampleRate;
    context.scheduleStop();commands.push('stopgrid 0 0');
    assert(context.stopTime()>grid,'Original minimum lead schedules STOP at the following grid');
    silence(deadline);
    pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate+(deadline-target)*context.rate())%length+length)%length});
    render(Math.min(length+3,147));queuedStarts++;
   }
  }else{
  for(const mode of [0,1,2]){
   const nextBpm=mode===1?137.3:120;origin=frame-(frame-origin)*bpm/nextBpm;bpm=nextBpm;
   commands.push(`sync ${mode} ${bpm} 4 1`);context.configure(mode,bpm,origin/sampleRate,4);
   if(mode>0){context.toggle();commands.push(`reverse 0 ${mode===1?1:0}`);}
   const speed=mode===2?Math.fround(0.37):1;context.phaseSpeed(speed);commands.push(`speed 0 ${speed}`);
   for(const delay of [0.25,0.333,0.000000001,1,0]){
    audio.currentTime=frame/sampleRate;context.setDelay(delay);commands.push(`delay 0 ${delay}`);render(7); // Input alone must not jump the source.
    audio.currentTime=frame/sampleRate;context.applyPhase();commands.push('apply 0 0');
    const target=(audio.currentTime+0.05)*sampleRate,nearest=Math.round(target),deadline=Math.abs(target-nearest)<=4*Number.EPSILON*Math.max(1,Math.abs(target))?nearest:Math.ceil(target);
    pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate+(deadline-target)*context.rate())%length+length)%length});
    render(deadline-frame+Math.min(length+3,147));
   }
   // Execute the original Q handler, including no-op clips and rapid clicks.
   for(const delay of [0.12499999999999999,0.125,0.24999999999999997,0.17,0.499,1]){
    context.setDelay(delay);commands.push(`delay 0 ${delay}`);
    audio.currentTime=frame/sampleRate;context.quantize();
    metadata.push({row:commands.length,value:context.delayValue()});commands.push('quantize 0 0');
    if(Math.round((length/sampleRate)/(60/bpm/4))>0){
     const target=(audio.currentTime+0.05)*sampleRate,nearest=Math.round(target),deadline=Math.abs(target-nearest)<=4*Number.EPSILON*Math.max(1,Math.abs(target))?nearest:Math.ceil(target);
     pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate+(deadline-target)*context.rate())%length+length)%length});
    }
    render(7);
   }
   if(pending.length)render(pending.at(-1).at-frame+147);
   // Preserve Q coverage above and compare the unchanged original nudge handler.
   for(const [delay,direction] of [[0,-1],[1,1],[0.217,1],[0.993,1],[0.001,-1],[0.5,-1]]){
    context.setDelay(delay);commands.push(`delay 0 ${delay}`);audio.currentTime=frame/sampleRate;
    context.nudge(direction);const raw=context.delayValue();let value=raw;
    if(raw<0||raw>1){
     // Intentional native repair: original handler can leave invalid offsets on
     // tiny clips. Validate the raw reference state, then audition the intended
     // wrapped phase through unchanged setter/restart/AudioGraph methods.
     value=((raw%1)+1)%1;context.setDelay(value);context.applyPhase();tinyWraps++;
    }
    metadata.push({row:commands.length,value});commands.push(`nudge 0 ${direction}`);nudges++;
    const target=(audio.currentTime+0.05)*sampleRate,nearest=Math.round(target),deadline=Math.abs(target-nearest)<=4*Number.EPSILON*Math.max(1,Math.abs(target))?nearest:Math.ceil(target);
    pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate+(deadline-target)*context.rate())%length+length)%length});
    render(7);
   }
   render(pending.at(-1).at-frame+147);
   // Two future sources must both render, even when released within 50ms.
   for(const delay of [0.2,0.8]){
    audio.currentTime=frame/sampleRate;context.setDelay(delay);commands.push(`delay 0 ${delay}`);context.applyPhase();commands.push('apply 0 0');
    const target=(audio.currentTime+0.05)*sampleRate,nearest=Math.round(target),deadline=Math.abs(target-nearest)<=4*Number.EPSILON*Math.max(1,Math.abs(target))?nearest:Math.ceil(target);
    pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate+(deadline-target)*context.rate())%length+length)%length});render(7);
   }
   render(pending.at(-1).at-frame+147);
   // Full user-level Loop.retrigger, distinct from the immediate graph primitive.
   // Starting delay must be overwritten; playing replacement starts at +5ms,
   // whereas a stopped loop must play immediately even with grid sync enabled.
   for(const [tapIndex,stopped] of [false,false,true].entries()){
    context.setDelay(0.731);commands.push('delay 0 0.731');
    if(stopped){context.stopped();commands.push('stop 0 0');}
    audio.currentTime=frame/sampleRate;await context.rtrg();
    metadata.push({row:commands.length,value:context.delayValue()});commands.push('rtrg 0 0');retriggers++;
    const target=(audio.currentTime+(stopped?0:0.005))*sampleRate,nearest=Math.round(target),deadline=Math.abs(target-nearest)<=4*Number.EPSILON*Math.max(1,Math.abs(target))?nearest:Math.ceil(target);
    pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate+(deadline-target)*context.rate())%length+length)%length});
    render(tapIndex===0?1:deadline-frame+Math.min(length+3,147));
   }
  }
  // The original optimized restart bypasses master phase while muted by solo.
  commands.push('solo 1 0');context.phaseSolo(true);excluded=true;context.setDelay(0.217);commands.push('delay 0 0.217');
  audio.currentTime=frame/sampleRate;context.applyPhase();commands.push('apply 0 0');
  const target=(audio.currentTime+0.05)*sampleRate,nearest=Math.round(target),deadline=Math.abs(target-nearest)<=4*Number.EPSILON*Math.max(1,Math.abs(target))?nearest:Math.ceil(target);
  pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate+(deadline-target)*context.rate())%length+length)%length});render(deadline-frame+7);
  audio.currentTime=frame/sampleRate;await context.rtrg();
  metadata.push({row:commands.length,value:context.delayValue()});commands.push('rtrg 0 0');retriggers++;
  {const target=(audio.currentTime+0.005)*sampleRate,deadline=Math.round(target);pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate)%length+length)%length});render(deadline-frame+7);}
  commands.push('solo 1 0');context.phaseSolo(false);excluded=false;render(47);
  // An early queued callback builds the unchanged graph before its grid.
  // Compare the source at the first representable deadline sample, including
  // fractional grids, reverse buffers and a nonzero tempo-preserved origin.
  // Late callback delivery and browser gain envelopes are separate gates.
  for(const mode of [1,2]){
   commands.push('stop 0 0');context.stopped();
   const nextBpm=mode===1?999:731;origin=frame-(frame-origin)*bpm/nextBpm;bpm=nextBpm;
   commands.push(`sync ${mode} ${bpm} 1 1`);context.configure(mode,bpm,origin/sampleRate,1);
   context.phaseSpeed(1);commands.push('speed 0 1');
   if(mode===1){context.toggle();commands.push('reverse 0 1');}
   else {context.toggle();commands.push('reverse 0 0');}
   context.setDelay(0.217);commands.push('delay 0 0.217');
   audio.currentTime=frame/sampleRate;
   const grid=context.grid();assert(grid>audio.currentTime,'Queued source must be scheduled ahead');
   await context.scheduledPlay(grid);commands.push('play 0 0');
   const target=grid*sampleRate,nearest=Math.round(target);
   const deadline=Math.abs(target-nearest)<=4*Number.EPSILON*Math.max(1,Math.abs(target))?nearest:Math.ceil(target);
   while(frame<deadline){checks.push({row:commands.length,expected:0});commands.push('tick 0 0');frame++;queuedSilentFrames++;}
   pending.push({source:context.source(),at:deadline,phase:((context.offset()*sampleRate+(deadline-target)*context.rate())%length+length)%length});
   render(Math.min(length+3,147));queuedStarts++;
  }
  }
  const result=spawnSync(process.argv[2],(queuedLateOnly||queuedEarlyOnly||queuedRestartOnly||queuedSpeedOnly)?['--callback-start']:queuedStopOnly?['--accepted-playback']:[],{input:commands.join('\n')+'\n',encoding:'utf8',maxBuffer:32*1024*1024});assert.equal(result.status,0,result.stderr);const rows=result.stdout.trim().split(/\r?\n/);assert.equal(rows.length,commands.length);
  rows.forEach((r,i)=>assert.equal(Number(r.split(' ')[0]),1,'Rejected '+commands[i]));
  for(const check of metadata)assert(Math.abs(Number(rows[check.row].split(' ')[3])-check.value)<=1e-14,`RTRG/Q/nudge metadata ${mono}/${sampleRate}/${length}, row ${check.row}: ${rows[check.row]} vs ${check.value}`);
  for(const check of checks){const row=rows[check.row].split(' ').map(Number);for(const value of row.slice(6)){const error=Math.abs(value-check.expected);maximum=Math.max(maximum,error);assert(error<=1e-6,`Start phase ${mono}/${sampleRate}/${length}, frame ${row[1]}: ${value} vs ${check.expected}`);}samples++;}
  cases++;
 }
 console.log(JSON.stringify({result:'PASS',cases,queuedStarts,queuedSilentFrames,retriggers,nudges,tinyWraps,samples,maximumAbsoluteError:maximum,tolerance:1e-6,scope:queuedSpeedOnly?'Unchanged UI speed changes on an early validated source at 80/1ms before grid, retained actual source/start offset, current source PCM and pre-grid silence; browser envelopes/resampling excluded':queuedRtrgOnly?'Unchanged early-source RTRG: Start reset/quantization, 5ms replacement before/after grid, reverse PCM, original volume silence gate; fade shapes/resampling excluded':queuedRestartOnly?'Unchanged early-source Apply/Q/nudge replacement at 50ms before/after original grid, reverse/source phase, original volume silence gate and no old-source resurrection; fade shapes/resampling excluded':queuedEarlyOnly?'Unchanged Start/Reverse edits after early source birth preserve scheduled source phase/direction until next play; pre-grid silence; browser envelopes/resampling excluded':queuedCleanupOnly?'Unchanged Loop._teardown/AudioGraph.cleanup stops and disconnects replacement source; native cutoff has no residual tail; original source phase/PCM before cleanup; browser envelopes/resampling excluded':queuedLateOnly?'Queued source is silent through late callback admission, then matches original Loop.play/AudioGraph.build source phase/PCM; browser envelopes/resampling excluded':queuedStopOnly?'Queued source PCM after accepted early graph birth and STOP inside the 30ms minimum lead; original grid/play/build/scheduleStop; envelopes/resampling excluded':'Unchanged user RTRG, UI setter/Q/nudge, Loop.restart and AudioGraph.retrigger source offsets + committed native PCM, 5/50ms timing and early-callback queued grid starts; linear interpolation, browser playback envelopes/resampling excluded'}));
})().catch(e=>{console.error(e);process.exitCode=1;});
