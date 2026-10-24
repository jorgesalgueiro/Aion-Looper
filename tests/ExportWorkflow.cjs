// Production editor/processor/worker with synthetic PCM and a host-owned test
// destination picker. The operating-system file dialog itself is a separate gate.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const directory=path.resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]}`);
const delay=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 let target;for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.type==='page'&&t.url.includes('juce.backend'));}catch{}if(target)break;await delay(150);}assert(target,'Synthetic host CDP page');
 const ws=socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});
 let onPaused=null;let seq=0;const pending=new Map(),errors=[];
 ws.addEventListener('message',e=>{const r=JSON.parse(e.data);if(r.method==='Debugger.paused')onPaused?.(r.params).catch(error=>{errors.push(String(error));call('Debugger.resume').catch(()=>{});});if(r.method==='Runtime.exceptionThrown')errors.push(r.params.exceptionDetails);pending.get(r.id)?.(r);});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++seq,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout '+method));},10000);pending.set(id,r=>{pending.delete(id);clearTimeout(timer);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});ws.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 const rpc=async request=>evaluate(`AionNative.request(${JSON.stringify(request)})`),snapshot=()=>rpc({operation:'snapshot'});
 const wait=async(predicate,label)=>{let s;for(let n=0;n<150;++n){s=await snapshot();if(predicate(s))return s;await delay(40);}assert.fail(label+': '+JSON.stringify({state:s.tracks[0].state,exportBusy:s.exportBusy,exportStatus:s.exportStatus}));};
 const choose=filename=>fs.writeFileSync(path.join(directory,filename+'.choose'),'');
 const click=async selector=>{const r=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);const down=call('Input.dispatchMouseEvent',{type:'mousePressed',...r,button:'left',clickCount:1});await delay(20);const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',...r,button:'left',clickCount:1});await Promise.all([down,up]);};
 await call('Runtime.enable');await call('Emulation.setFocusEmulationEnabled',{enabled:true});
 for(let n=0;n<100;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&document.querySelectorAll('#loopContainer > div').length===20`))break;await delay(100);}
 let s=await wait(s=>s.audioReady&&s.inputAvailable,'Synthetic audio ready');
 for(const [id,value] of [['syncMode',0],['metronomeEnabled',0],['masterGain',0],['autoPlayAfterRecord',1]])assert((await rpc({operation:'setParameter',id,value})).ok);
 assert((await rpc({operation:'loop',action:'clearAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
 await wait(s=>s.tracks.every(t=>t.state==='empty'),'clear export fixture');await delay(150);
 const noOpBefore=(await snapshot()).lastRequestId;await click('#loop-wave-0');await delay(200);s=await snapshot();assert(!s.exportBusy);assert.equal(s.lastRequestId,noOpBefore,'Empty waveform click does not record');
 await click('#loop-action-0');await wait(s=>s.tracks[0].state==='recording','record');await delay(250);await click('#loop-action-0');s=await wait(s=>s.tracks[0].state==='playing'&&s.tracks[0].waveformId>0,'commit');
 const identity=s.tracks[0].waveformId,length=s.tracks[0].length,requestId=s.lastRequestId;
 await evaluate(`(async()=>{const end=Date.now()+7000;while(Date.now()<end){if(document.getElementById('loop-0').classList.contains('playing')&&!document.getElementById('loop-action-0').disabled)return;await new Promise(r=>setTimeout(r,20));}throw Error('Committed clip must reach the rendered controller before waveform input');})()`);
 // Export must use the name typed in this task, before the next state poll.
 await evaluate(`(()=>{const e=document.querySelector('#loop-0 .track-name');e.value='Fresh ✨';e.dispatchEvent(new Event('input',{bubbles:true}));document.getElementById('loop-wave-0').click();})()`);
 const fresh=await wait(s=>s.exportBusy,'fresh name export');assert(fresh.exportFilename.startsWith('Fresh___'),'Latest typed UTF-16 name, not stale telemetry');choose(fresh.exportFilename);await wait(s=>!s.exportBusy,'fresh name saved');
 for(const name of ['Bass / 1','Cancel','Fail','AfterCancel']){
  assert((await rpc({operation:'renameLoop',track:0,name})).ok);await wait(s=>s.tracks[0].name===name,'name');await delay(120);
  await click('#loop-wave-0');const started=await wait(s=>s.exportBusy,'mouse export');
  assert((await evaluate(`document.getElementById('loop-wave-0').title`)).includes('Export WAV'));
  const duplicate=await rpc({operation:'exportLoop',track:0,generation:s.generation,waveformId:identity,filename:'Duplicate.wav'});assert(!duplicate.ok&&duplicate.exportBusy,'Second export rejected while chooser active');
  await click('#loop-wave-0');assert(!(await evaluate(`document.getElementById('native-status').textContent`)).includes('already in progress'),'Repeated waveform click preserves the active export status');
  choose(started.exportFilename);
  const done=await wait(s=>!s.exportBusy,'export completion');assert.equal(done.lastRequestId,requestId,'Waveform click must not trigger transport');assert.equal(done.tracks[0].waveformId,identity);assert.equal(done.tracks[0].undoCount,0);
  const file=path.join(directory,started.exportFilename);
  if(name==='Cancel'){assert.equal(done.exportStatus,'WAV export cancelled');assert(!fs.existsSync(file));}
  else if(name==='Fail'){assert(done.exportStatus.includes('failed'));assert(!fs.existsSync(file));}
  else {assert.equal(done.exportStatus,'WAV saved');const wav=fs.readFileSync(file);assert.equal(wav.length,44+length*4);assert.equal(wav.toString('ascii',0,4),'RIFF');assert.equal(wav.readUInt16LE(22),2);assert.equal(wav.readUInt32LE(24),48000);assert.equal(wav.readUInt16LE(34),16);assert(wav.subarray(44).some(v=>v!==0),'Committed native stereo PCM saved');}
 }
 // A canceled picker may deliver its callback after the next export is already
 // choosing a destination. It must neither write the old snapshot nor cancel,
 // finish or replace the new request's status. Use an ordinary filename so the
 // fixture supplies a real destination, rather than its Cancel_* shortcut.
 const obsolete=`OldPicker_${Date.now()}.wav`,replacement=`NewPicker_${Date.now()}.wav`;
 assert((await rpc({operation:'exportLoop',track:0,generation:s.generation,waveformId:identity,filename:obsolete})).ok);
 await wait(s=>s.exportStatus==='Choose a WAV destination','old picker ready');
 assert((await rpc({operation:'cancelExport'})).ok);
 await wait(s=>!s.exportBusy&&s.exportStatus==='WAV export cancelled','explicit picker cancellation');
 assert((await rpc({operation:'exportLoop',track:0,generation:s.generation,waveformId:identity,filename:replacement})).ok);
 await wait(s=>s.exportBusy&&s.exportFilename===replacement&&s.exportStatus==='Choose a WAV destination','replacement picker ready');
 choose(obsolete);await delay(300);s=await snapshot();
 assert(!fs.existsSync(path.join(directory,obsolete)),'Canceled picker cannot save through a late callback');
 assert(s.exportBusy&&s.exportFilename===replacement&&s.exportStatus==='Choose a WAV destination','Old callback cannot finish or alter the replacement picker');
 assert.equal(s.tracks[0].waveformId,identity);assert.equal(s.lastRequestId,requestId);
 choose(replacement);s=await wait(s=>!s.exportBusy,'replacement saved');
 assert.equal(s.exportStatus,'WAV saved');
 assert.deepEqual(fs.readFileSync(path.join(directory,replacement)),fs.readFileSync(path.join(directory,fresh.exportFilename)),'Replacement keeps the exact stored snapshot');
 // Reload cancels an unchosen destination; its old async callback must not
 // write a file or hijack the next export after a fresh editor bootstrap.
 const abandoned=`Abandoned_${Date.now()}.wav`;assert((await rpc({operation:'exportLoop',track:0,generation:s.generation,waveformId:identity,filename:abandoned})).ok);
 await wait(s=>s.exportStatus==='Choose a WAV destination','reload picker');
 // Disable script execution across navigation: neither unload RPC nor bootstrap
 // may be responsible for this cancellation. Release the old picker callback
 // before reenabling JS and verify the native navigation boundary kept it inert.
 await call('Emulation.setScriptExecutionDisabled',{value:true});await call('Page.reload');choose(abandoned);await delay(600);assert(!fs.existsSync(path.join(directory,abandoned)),'Native navigation alone cancels the late picker');
 // Pause the actual production controller before its DOMContentLoaded body.
 // Hold its real startup cleanup packet, then deliver it after a new picker.
 // This delays IPC only: the native audio callback and WAV worker keep running.
 await call('Emulation.setScriptExecutionDisabled',{value:false});await call('Debugger.enable');
 let phase='scan',hookInstalled=false,controllerBreakpoint;
 const instrumentation=await call('Debugger.setInstrumentationBreakpoint',{instrumentation:'beforeScriptExecution'});
 onPaused=async paused=>{
  const frame=paused.callFrames[0];
  if(phase==='scan'){
   const {scriptSource}=await call('Debugger.getScriptSource',{scriptId:frame.location.scriptId});
   const at=scriptSource.indexOf('const rpc = window.AionNative?.request;');
   if(at>=0){
    controllerBreakpoint=await call('Debugger.setBreakpoint',{location:{scriptId:frame.location.scriptId,lineNumber:scriptSource.slice(0,at).split('\n').length-1}});
    phase='hook';await call('Debugger.removeBreakpoint',{breakpointId:instrumentation.breakpointId});
   }
  }else if(phase==='hook'){
   const result=await call('Debugger.evaluateOnCallFrame',{callFrameId:frame.callFrameId,expression:`(()=>{
    const backend=window.__JUCE__.backend,emit=backend.emitEvent;window.delayedExportCleanup=[];
    backend.emitEvent=function(type,message){if(type==='__juce__invoke'&&message?.params?.[0]?.operation==='cancelExport'){window.delayedExportCleanup.push([type,message]);return;}return Reflect.apply(emit,this,[type,message]);};
    window.releaseExportCleanup=()=>{const packets=window.delayedExportCleanup.splice(0);for(const args of packets)Reflect.apply(emit,backend,args);return packets.length;};
   })()`,returnByValue:true});
   assert(!result.exceptionDetails,JSON.stringify(result.exceptionDetails));hookInstalled=true;phase='done';
   await call('Debugger.removeBreakpoint',{breakpointId:controllerBreakpoint.breakpointId});
  }
  await call('Debugger.resume');
 };
 await call('Page.reload');await delay(600);await wait(s=>!s.exportBusy&&s.exportStatus==='WAV export cancelled','reload cancels chooser');
 await evaluate(`(async()=>{const end=Date.now()+7000;while(Date.now()<end){const row=document.getElementById('loop-0'),norm=[...(row?.querySelectorAll('button')||[])].find(e=>e.textContent.trim()==='NORM');if(row?.classList.contains('playing')&&norm&&!norm.disabled)return;await new Promise(r=>setTimeout(r,20));}throw Error('Reloaded original NORM control must be enabled before the edit/export gesture');})()`);
 assert(hookInstalled,'Delayed cleanup hook installed in the actual production controller');await call('Debugger.disable');onPaused=null;
 // Only the dedicated native picker supplies paths; IPC rejects traversal and
 // invalid identities instead of granting the WebView arbitrary filesystem IO.
 for(const patch of [{filename:'../escape.wav'},{filename:'C:\\escape.wav'},{waveformId:NaN},{waveformId:0},{track:20},{generation:s.generation+1}]){
  assert(!(await rpc({operation:'exportLoop',track:0,generation:s.generation,waveformId:identity,filename:'Valid.wav',...patch})).ok,'Invalid export rejected');
 }
 // A rapid NORM then waveform click exports the newly committed samples rather
 // than bypassing the edit through the independent audio-to-worker mailbox.
 await evaluate(`(()=>{[...document.querySelectorAll('#loop-0 button')].find(e=>e.textContent.trim()==='NORM').click();document.getElementById('loop-wave-0').click();})()`);
 const normalized=await wait(s=>s.exportBusy&&s.tracks[0].waveformId!==identity,'ordered NORM export');
 await wait(s=>s.exportBusy&&s.exportStatus==='Choose a WAV destination','new NORM picker before delayed cleanup');
 const delayedCleanup=await evaluate('window.releaseExportCleanup()');console.log('DIAGNOSTIC: delayed new-document export cleanup packets: '+delayedCleanup);
 await delay(200);const afterCleanup=await snapshot();
 assert(afterCleanup.exportBusy&&afterCleanup.exportStatus==='Choose a WAV destination','Obsolete frontend startup cleanup must not cancel a new export: '+JSON.stringify({delayedCleanup,status:afterCleanup.exportStatus,busy:afterCleanup.exportBusy}));
 assert.equal(afterCleanup.exportFilename,normalized.exportFilename);assert.equal(afterCleanup.tracks[0].waveformId,normalized.tracks[0].waveformId);
 assert.equal(delayedCleanup,0,'Navigation owns cleanup; the new document must issue no unscoped export cancellation');
 choose(normalized.exportFilename);await wait(s=>!s.exportBusy,'normalized save');
 const normalizedWav=fs.readFileSync(path.join(directory,normalized.exportFilename));let normalizedPeak=0;for(let i=44;i<normalizedWav.length;i+=2)normalizedPeak=Math.max(normalizedPeak,Math.abs(normalizedWav.readInt16LE(i))/32768);assert(normalizedPeak>0.8,'WAV contains the normalized take, not the earlier <0.3 peak');
 const currentIdentity=normalized.tracks[0].waveformId;
 // Explicit overwrite uses the same production temporary-file writer.
 const overwrite=path.join(directory,'Overwrite.wav');fs.writeFileSync(overwrite,'old-data');
 assert((await rpc({operation:'exportLoop',track:0,generation:s.generation,waveformId:currentIdentity,filename:'Overwrite.wav'})).ok);
 choose('Overwrite.wav');
 await wait(s=>!s.exportBusy,'overwrite');assert.equal(fs.readFileSync(overwrite).toString('ascii',0,4),'RIFF');
 // Deleting after the clip has been pinned must not corrupt the saved snapshot.
 assert((await rpc({operation:'exportLoop',track:0,generation:s.generation,waveformId:currentIdentity,filename:'Pinned.wav'})).ok);
 await wait(s=>s.exportStatus==='Choose a WAV destination','pinned before DEL');
 assert((await rpc({operation:'loop',action:'clear',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);
 await wait(s=>s.tracks[0].state==='empty','DEL before choosing');choose('Pinned.wav');
 const cleared=await wait(s=>!s.exportBusy&&s.tracks[0].state==='empty','delete while picker pending');assert.equal(cleared.exportStatus,'WAV saved');assert.deepEqual(fs.readFileSync(path.join(directory,'Pinned.wav')),fs.readFileSync(overwrite));
 assert.deepEqual(errors,[],'No native editor runtime errors');
 console.log(`PASS: real waveform mouse/IPC export, stored stereo PCM, cancel/replacement/late-callback isolation/failure/recovery/overwrite/DEL and transport isolation at ${process.argv[2]} samples; host picker substitutes only destination selection`);ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
