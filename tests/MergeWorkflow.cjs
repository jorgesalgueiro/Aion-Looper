// Actual GO/Undo/Redo mouse events and production processor/worker/PCM. Only
// file destination selection uses the existing deterministic test-host picker.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const directory=path.resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]}`);
const delay=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 let target;for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.type==='page'&&t.url.includes('juce.backend'));}catch{}if(target)break;await delay(150);}assert(target);
 const ws=socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});
 let seq=0,acceptSourceClear=false;const pending=new Map(),errors=[],dialogs=[];
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++seq,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout '+method));},10000);pending.set(id,r=>{pending.delete(id);clearTimeout(timer);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});ws.send(JSON.stringify({id,method,params}));});
 ws.addEventListener('message',e=>{const r=JSON.parse(e.data);if(r.method==='Runtime.exceptionThrown')errors.push(r.params.exceptionDetails);if(r.method==='Page.javascriptDialogOpening'){dialogs.push(r.params.message);call('Page.handleJavaScriptDialog',{accept:acceptSourceClear}).catch(e=>errors.push(e.message));}pending.get(r.id)?.(r);});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 const rpc=request=>evaluate(`AionNative.request(${JSON.stringify(request)})`),snapshot=()=>rpc({operation:'snapshot'});
 const wait=async(predicate,label)=>{let s;for(let n=0;n<180;++n){s=await snapshot();if(predicate(s))return s;await delay(40);}assert.fail(label+': '+JSON.stringify({tracks:s.tracks.slice(0,3).map(({wavePeaks,...t})=>t),input:await evaluate('window.mergeInputEvidence')}));};
 const click=async selector=>{const r=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});if(e.disabled)throw Error('Disabled: '+${JSON.stringify(selector)});e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);const down=call('Input.dispatchMouseEvent',{type:'mousePressed',...r,button:'left',clickCount:1});await delay(20);const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',...r,button:'left',clickCount:1});await Promise.all([down,up]);};
 const save=async(id,label)=>{let s=await snapshot();const filename=`Merge_${label}_${Date.now()}.wav`;assert((await rpc({operation:'exportLoop',track:id,generation:s.generation,waveformId:s.tracks[id].waveformId,filename})).ok);await wait(s=>s.exportStatus==='Choose a WAV destination','export ready');fs.writeFileSync(path.join(directory,filename+'.choose'),'');s=await wait(s=>!s.exportBusy,'export complete');assert.equal(s.exportStatus,'WAV saved');return fs.readFileSync(path.join(directory,filename));};
 await call('Runtime.enable');await call('Page.enable');await call('Emulation.setFocusEmulationEnabled',{enabled:true});
 for(let n=0;n<100;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&document.querySelectorAll('#loopContainer > div').length===20`))break;await delay(100);}
 let s=await wait(s=>s.audioReady,'ready');
 for(const [id,value] of [['syncMode',0],['metronomeEnabled',0],['masterGain',0],['autoPlayAfterRecord',1]])assert((await rpc({operation:'setParameter',id,value})).ok);
 assert((await rpc({operation:'loop',action:'clearAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);await wait(s=>s.tracks.every(t=>t.state==='empty'),'clear');await delay(200);
 for(const id of [0,1]){await click(`#loop-action-${id}`);await wait(s=>s.tracks[id].state==='recording','record');await delay(250+id*100);await click(`#loop-action-${id}`);await wait(s=>s.tracks[id].state==='playing'&&s.tracks[id].waveformId,'commit');await delay(150);}
 const before=await save(0,'Before'),source=await save(1,'Source');s=await snapshot();const length=s.tracks[0].length,sourceMeta=s.tracks[1];
 const go=async(dest,src)=>{await evaluate(`(()=>{const s=document.getElementById('merge-src-${dest}');s.value='${src}';s.dispatchEvent(new Event('change',{bubbles:true}));})()`);await delay(150);await click(`#loop-${dest} button[title="Merge selected source into this loop"]`);};
 await go(0,1);s=await wait(s=>s.tracks[0].mergeSerial>0&&s.tracks[0].state==='playing'&&dialogs.length===1,'mix and decline prompt');
 assert.equal(dialogs[0],'Merge successful. Clear Source Loop 2?');assert.equal(s.tracks[0].length,length);assert.equal(s.tracks[0].undoCount,1);assert.equal(s.nextUndo,0);assert.equal(s.tracks[1].waveformId,sourceMeta.waveformId);
 const mixedSerial=s.tracks[0].mergeSerial;
 const mixed=await save(0,'Mixed');assert.notDeepEqual(mixed,before);assert.deepEqual(await save(1,'SourceUnchanged'),source);
 await click('#loop-0 button[title="UNDO"]');await wait(s=>s.tracks[0].undoCount===0&&s.tracks[0].redoCount===1,'undo merge');assert.deepEqual(await save(0,'Undo'),before);
 await click('#loop-0 button[title="REDO"]');await wait(s=>s.tracks[0].undoCount===1&&s.tracks[0].redoCount===0,'redo merge');assert.deepEqual(await save(0,'Redo'),mixed);
 const undoneConfirmation=await rpc({operation:'loop',action:'clearMergedSource',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:mixedSerial});assert(undoneConfirmation.ok);
 s=await wait(s=>s.lastRequestId===undoneConfirmation.requestId,'Undo invalidates old completion');assert(s.rejected&&s.tracks[1].waveformId===sourceMeta.waveformId,'Undo/Redo cannot leave an old destructive confirmation valid');
 // Destination-side queued MERGE retains the original source grid/phase.
 for(const graph of [false,true]){
  assert((await rpc({operation:'loop',action:'stopAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  await wait(s=>s.tracks[0].state==='stopped'&&s.tracks[1].state==='stopped','stop before queued destination');
  const beforeBytes=await save(0,'QueuedDestBefore'+graph);
  for(const [id,value] of [['syncBars',64],['syncBpm',120],['syncMode',1],['masterGain',1]])assert((await rpc({operation:'setParameter',id,value})).ok);
  assert((await rpc({operation:'loop',action:'reverse',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:graph?1:0})).ok);
  await click('#loop-action-0');await wait(s=>s.tracks[0].state==='queued','destination queued');
  if(graph){fs.writeFileSync(path.join(directory,'early-source-start'),'1');await wait(s=>s.tracks[0].queuedSourceReady,'queued destination reversed source constructed');}
  const before=await snapshot(),count=dialogs.length;acceptSourceClear=false;await go(0,1);
  s=await wait(s=>s.tracks[0].mergeSerial&&dialogs.length===count+1,'queued destination MERGE commit/decline');
  assert.equal(s.tracks[0].state,'queued');assert.equal(s.tracks[0].queuedSourceReady,graph);assert.equal(s.tracks[0].scheduledFrame,before.tracks[0].scheduledFrame);
  assert.equal(s.tracks[0].length,before.tracks[0].length);assert.equal(s.tracks[0].undoCount,before.tracks[0].undoCount+1);assert.equal(s.outputPeak,0,'Queued destination remains inaudible before original grid');
  assert.equal(s.tracks[1].waveformId,sourceMeta.waveformId);assert.deepEqual(await save(1,'QueuedDestSource'+graph),source);
  const mixedBytes=await save(0,'QueuedDestMixed'+graph);assert.notDeepEqual(mixedBytes,beforeBytes);
  await click('#loop-0 button[title="UNDO"]');await wait(s=>s.tracks[0].waveformId===before.tracks[0].waveformId,'queued destination Undo identity');
  assert.deepEqual(await save(0,'QueuedDestUndo'+graph),beforeBytes);
  await click('#loop-0 button[title="REDO"]');await wait(s=>s.tracks[0].undoCount===before.tracks[0].undoCount+1&&s.tracks[0].mergeSerial===0,'queued destination Redo invalidates confirmation');
  assert.deepEqual(await save(0,'QueuedDestRedo'+graph),mixedBytes);
  s=await snapshot();assert.equal(s.outputPeak,0,'History replacements retain original closed volume gate');
  assert((await rpc({operation:'loop',action:'stopAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  await wait(s=>s.tracks[0].state==='stopped','queued destination stop');
  for(const [id,value] of [['masterGain',0],['syncMode',0]])assert((await rpc({operation:'setParameter',id,value})).ok);
 }
 const cloneDialogCount=dialogs.length;
 acceptSourceClear=true;await go(2,1);s=await wait(s=>s.tracks[2].mergeSerial>0&&s.tracks[2].state==='stopped'&&s.tracks[1].state==='empty'&&dialogs.length===cloneDialogCount+1,'clone and accept source clear');
 assert.equal(dialogs[cloneDialogCount],'Merge successful. Clear Source Loop 2?');assert.equal(s.tracks[2].length,sourceMeta.length);assert.equal(s.tracks[2].originalBpm,sourceMeta.originalBpm);assert.equal(s.tracks[2].dynamicRecorded,sourceMeta.dynamicRecorded);assert.equal(s.tracks[2].undoCount,0);
 assert.deepEqual(await save(2,'Clone'),source,'Clone keeps exact stereo PCM after clearing source');
 await delay(150);assert(!(await evaluate(`!!document.querySelector('#merge-src-0 option[value="1"]')`)),'Cleared source is removed from original selector');
 // Source-side queued MERGE must preserve the original pending PLAY/graph.
 // The existing host hook constructs an early graph through production code.
 for(const graph of [false,true]){
  if(graph){acceptSourceClear=false;const count=dialogs.length;await go(0,2);await wait(s=>s.tracks[0].state==='stopped'&&s.tracks[0].mergeSerial&&dialogs.length===count+1,'restore source for constructed graph');}
  assert((await rpc({operation:'loop',action:'stopImmediate',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  await wait(s=>s.tracks[0].state==='stopped','queued source starts stopped');
  const expected=await save(0,'QueuedSource'+graph);
  for(const [id,value] of [['syncBars',64],['syncBpm',120],['syncMode',1]])assert((await rpc({operation:'setParameter',id,value})).ok);
  if(graph)assert((await rpc({operation:'loop',action:'reverse',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:1})).ok);
  await click('#loop-action-0');await wait(s=>s.tracks[0].state==='queued','source waits on original grid');
  if(graph){fs.writeFileSync(path.join(directory,'early-source-start'),'1');await wait(s=>s.tracks[0].queuedSourceReady,'constructed reversed queued source');}
  const queued=await snapshot(),count=dialogs.length;acceptSourceClear=false;
  await go(4,0);s=await wait(s=>s.tracks[4].mergeSerial&&dialogs.length===count+1,'queued source clone and decline');
  assert.equal(s.tracks[0].state,'queued');assert.equal(s.tracks[0].scheduledFrame,queued.tracks[0].scheduledFrame);
  assert.equal(s.tracks[0].waveformId,queued.tracks[0].waveformId);assert.equal(s.tracks[0].queuedSourceReady,graph);
  assert.deepEqual(await save(4,'QueuedClone'+graph),expected,'Queued source clones exact stored stereo WAV');
  assert.deepEqual(await save(0,'QueuedUnchanged'+graph),expected,'MERGE preserves queued source stored PCM');
  acceptSourceClear=true;await go(5,0);s=await wait(s=>s.tracks[5].mergeSerial&&s.tracks[0].state==='empty'&&dialogs.length===count+2,'confirm queued source clear');
  assert.equal(s.tracks[5].state,'stopped');assert.deepEqual(await save(5,'QueuedConfirmed'+graph),expected);
  // Reuse the original empty clone destinations on the second pass.
  for(const track of [4,5])assert((await rpc({operation:'loop',action:'clear',track,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  await wait(s=>s.tracks[4].state==='empty'&&s.tracks[5].state==='empty','queued clone cleanup');
  assert((await rpc({operation:'setParameter',id:'syncMode',value:0})).ok);
 }
 acceptSourceClear=false;const restoreCount=dialogs.length;await go(0,2);await wait(s=>s.tracks[0].state==='stopped'&&s.tracks[0].mergeSerial&&dialogs.length===restoreCount+1,'restore panic destination');
 await evaluate(`(()=>{window.mergeInputEvidence=[];document.addEventListener('pointerdown',e=>{window.mergeInputEvidence.push({pointer:e.target.id,text:e.target.textContent});if(window.mergeInputEvidence.length>64)window.mergeInputEvidence.shift();},true);})()`);
 // Scheduled-stop source/destination/both keep original GO/confirmation and
 // committed PCM semantics while the existing audio stop deadline remains live.
 for(const kind of ['source','destination','both'])for(const reverse of [false,true]){
  assert((await rpc({operation:'loop',action:'stopAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  await wait(s=>s.tracks[0].state==='stopped'&&s.tracks[2].state==='stopped','scheduled MERGE initial stop');
  assert((await rpc({operation:'setParameter',id:'syncMode',value:0})).ok);
  for(const track of [0,2])assert((await rpc({operation:'loop',action:'reverse',track,generation:s.generation,expiresAt:Date.now()+3000,value:reverse?1:0})).ok);
  if(kind!=='destination')assert((await rpc({operation:'loop',action:'play',track:2,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  if(kind!=='source')assert((await rpc({operation:'loop',action:'play',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  await wait(s=>(kind==='destination'||s.tracks[2].state==='playing')&&(kind==='source'||s.tracks[0].state==='playing'),'scheduled MERGE playback');
  for(const [id,value] of [['syncBars',64],['syncBpm',120],['syncMode',1]])assert((await rpc({operation:'setParameter',id,value})).ok);
  // Native command acceptance precedes frontend telemetry. Wait for the actual
  // rendered STOP control, not merely a direct native snapshot, before pressing.
  const stopPointer=async track=>{let rendered=false;for(let n=0;n<180;++n){rendered=await evaluate(`document.getElementById('loop-action-${track}').textContent==='STOP'&&[...document.querySelectorAll('input[name=syncMode]')][1].checked&&Number(document.getElementById('numBars').value)===64&&Number(document.getElementById('bpmInput').value)===120`);if(rendered)break;await delay(40);}assert(rendered,'Scheduled STOP is rendered before pointer input');await click(`#loop-action-${track}`);};
  if(kind!=='destination'){await stopPointer(2);await wait(s=>s.tracks[2].state==='stopping','source scheduled STOP pointer');}
  if(kind!=='source'){await stopPointer(0);await wait(s=>s.tracks[0].state==='stopping','destination scheduled STOP pointer');}
  const prior=await snapshot(),priorBytes=await save(0,'ScheduledBefore'+kind+reverse),sourceBytes=await save(2,'ScheduledSource'+kind+reverse),count=dialogs.length;
  acceptSourceClear=false;await go(0,2);s=await wait(s=>s.tracks[0].mergeSerial&&dialogs.length===count+1,'scheduled MERGE actual GO');
  for(const track of [0,2]){assert.equal(s.tracks[track].state,prior.tracks[track].state);assert.equal(s.tracks[track].scheduledFrame,prior.tracks[track].scheduledFrame);assert.equal(s.tracks[track].phaseApplyFrame,prior.tracks[track].phaseApplyFrame,'MERGE preserves accepted phase deadline');assert.equal(s.tracks[track].reverse,prior.tracks[track].reverse);if(track===2)assert.equal(s.tracks[track].waveformId,prior.tracks[track].waveformId,'MERGE preserves stopping source identity');}
  assert.equal(s.tracks[0].length,prior.tracks[0].length);assert.equal(s.tracks[0].undoCount,Math.min(5,prior.tracks[0].undoCount+1));
  const mergedBytes=await save(0,'ScheduledMerged'+kind+reverse);assert.deepEqual(await save(2,'ScheduledUnchanged'+kind+reverse),sourceBytes);
  if(kind==='source'&&!reverse){
   acceptSourceClear=true;const confirmations=dialogs.length;await go(6,2);s=await wait(s=>s.tracks[6].mergeSerial&&s.tracks[2].state==='empty'&&dialogs.length===confirmations+1,'confirm actual stopping source clear');
   assert.deepEqual(await save(6,'ScheduledConfirmed'),sourceBytes);
   acceptSourceClear=false;const restored=dialogs.length;await go(2,6);await wait(s=>s.tracks[2].state==='stopped'&&dialogs.length===restored+1,'restore source after confirmed scheduled clear');
  }
  assert((await rpc({operation:'loop',action:'stopAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  await wait(s=>s.tracks[0].state==='stopped','stop before scheduled merge history');
  for(let n=0;n<180&&await evaluate(`document.querySelector('#loop-0 button[title="UNDO"]').disabled`);++n)await delay(40);
  await click('#loop-0 button[title="UNDO"]');await wait(s=>s.tracks[0].waveformId===prior.tracks[0].waveformId,'post-stop Undo identity');assert.deepEqual(await save(0,'ScheduledUndo'+kind+reverse),priorBytes);
  for(let n=0;n<180&&await evaluate(`document.querySelector('#loop-0 button[title="REDO"]').disabled`);++n)await delay(40);
  await click('#loop-0 button[title="REDO"]');await wait(s=>s.tracks[0].waveformId!==prior.tracks[0].waveformId&&s.tracks[0].redoCount===0,'post-stop Redo identity');assert.deepEqual(await save(0,'ScheduledRedo'+kind+reverse),mergedBytes);
  assert((await rpc({operation:'setParameter',id:'syncMode',value:0})).ok);
 }
 // Panic invalidates both a pending and an already published completion; an old
 // clear-source confirmation must not outlive it, even if the worker wins first.
 const dialogCount=dialogs.length;
 // Pause only between real callbacks after this job enters finalizing. Awaiting
 // two CDP roundtrips can otherwise deliver STOP after the valid 50ms prompt.
 fs.writeFileSync(path.join(directory,'pause-after-finalizing'),'1');
 for(let n=0;n<100&&fs.existsSync(path.join(directory,'pause-after-finalizing'));++n)await delay(10);
 assert(!fs.existsSync(path.join(directory,'pause-after-finalizing')),'Owned host arms MERGE finalization pause');
 assert((await rpc({operation:'loop',action:'merge',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:2})).ok);
 for(let n=0;n<200&&!fs.existsSync(path.join(directory,'audio-paused'));++n)await delay(10);
 assert(fs.existsSync(path.join(directory,'audio-paused')),'Audio owner pauses actual MERGE before result acceptance');
 await wait(s=>s.tracks[0].state==='finalizing','Actual MERGE worker remains pending');
 assert((await rpc({operation:'loop',action:'stopAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
 fs.writeFileSync(path.join(directory,'resume-audio'),'1');
 s=await wait(s=>s.tracks[0].state==='stopped'&&s.tracks[0].mergeSerial===0,'panic cancels completion');await delay(250);assert.equal(dialogs.length,dialogCount);assert.equal(s.tracks[2].length,sourceMeta.length);
 acceptSourceClear=false;await go(3,2);s=await wait(s=>s.tracks[3].mergeSerial>0&&dialogs.length===dialogCount+1,'clone for active-source guard');const serial=s.tracks[3].mergeSerial;
 for(const action of ['play','overdub']){
  assert((await rpc({operation:'loop',action,track:2,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
  await wait(s=>s.tracks[2].state===(action==='play'?'playing':'overdubbing'),'source '+action);
 }
 const stale=await rpc({operation:'loop',action:'clearMergedSource',track:3,generation:s.generation,expiresAt:Date.now()+3000,value:serial});assert(stale.ok);
 s=await wait(s=>s.lastRequestId===stale.requestId,'guard acknowledgement');assert(s.rejected&&s.tracks[2].state==='overdubbing','Stale confirmation cannot erase new source capture');
 for(const value of [-1,20,0.5])assert(!(await rpc({operation:'loop',action:'merge',track:0,generation:s.generation,expiresAt:Date.now()+3000,value})).ok,'Bridge rejects invalid merge source');
 assert.deepEqual(errors,[]);console.log(`PASS: original MERGE/GO, actual confirmation decline/accept, clone/mix/source preservation, Undo/Redo, panic/active-capture guards and exact saved stereo PCM at ${process.argv[2]} samples`);ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
