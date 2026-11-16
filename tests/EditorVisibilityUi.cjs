// SPDX-License-Identifier: AGPL-3.0-only
// Native window hide/show, production editor/processor; no injected navigation recovery.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const wait=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 assert(process.env.AION_MOUSE_TEST_RUN,'Use TestEditorVisibility.ps1');let target;
 for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(target)break;await wait(100);}assert(target,'native editor target');
 socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
 let serial=0;const pending=new Map(),events=[],errors=[];
 socket.addEventListener('message',e=>{const m=JSON.parse(e.data);if(m.method==='Page.frameNavigated')events.push(m.params.frame.url);if(m.method==='Runtime.exceptionThrown')errors.push(m.params.exceptionDetails);pending.get(m.id)?.(m);});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial,t=setTimeout(()=>{pending.delete(id);reject(Error(method+' timeout'));},12000);pending.set(id,m=>{clearTimeout(t);pending.delete(id);m.error?reject(Error(JSON.stringify(m.error))):resolve(m.result);});socket.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 await call('Runtime.enable');await call('Page.enable');
 const ready=async(blank=false)=>{for(let n=0;n<100;++n){try{if(await evaluate(blank?`location.href==='about:blank'&&!document.querySelector('#loop-action-0')`:`location.href.startsWith('https://juce.backend/')&&!!window.AionNative&&!!document.querySelector('#loop-action-0')`))return;}catch(error){if(!/context|navigat/i.test(error.message))throw error;}await wait(100);}assert.fail(JSON.stringify({page:await evaluate('({url:location.href,html:document.documentElement.outerHTML.slice(0,300)})'),history:await call('Page.getNavigationHistory'),events,errors}));};
 await ready();const rpc=q=>evaluate(`AionNative.request(${JSON.stringify(q)})`);
 for(const [id,value] of [['masterGain',1],['inputMonitor',0],['autoPlayAfterRecord',0],['metronomeVolume',0],['syncMode',0]])assert((await rpc({operation:'setParameter',id,value})).ok);
 const directory=path.resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]}`);let sequence=0;
 const cycle=async(hiddenMs,blank=false)=>{
  await evaluate('window.visibilityMarker=true');
  const id=++sequence,tmp=path.join(directory,'visibility-command.tmp');fs.writeFileSync(tmp,JSON.stringify({id,hiddenMs}));fs.renameSync(tmp,path.join(directory,'visibility-command.json'));
  let acknowledged=false;for(let n=0;n<150;++n){let ack=0;try{ack=Number(fs.readFileSync(path.join(directory,'visibility-ack'),'utf8'));}catch{}if(ack===id){acknowledged=true;break;}await wait(20);}assert(acknowledged,'native show acknowledged');
  await wait(250); // Observe completed asynchronous navigations, not the old pre-hide document.
  await ready(blank);assert.equal(await evaluate('typeof window.visibilityMarker'),'undefined','old document actually unloaded even for zero-ms hides');
  if(!blank){const s=await rpc({operation:'snapshot'});assert(s.audioReady,'native audio remains active');}
  const history=await call('Page.getNavigationHistory');console.log(JSON.stringify({cycle:id,hiddenMs,url:await evaluate('location.href'),historyIndex:history.currentIndex,lastUrls:history.entries.slice(-4).map(e=>e.url),events:events.slice(-4)}));
 };
 const heldOnly=process.argv.includes('--held-stop-only');if(!heldOnly)for(const ms of [0,0,1,10,100,0,1,0])await cycle(ms);
 const until=async(predicate,label,timeout=8000)=>{let s;const end=Date.now()+timeout;while(Date.now()<end){s=await rpc({operation:'snapshot'});if(predicate(s))return s;await wait(50);}assert.fail(label+': '+JSON.stringify({tracks:s.tracks.map(t=>({state:t.state,length:t.length})),lastRequestId:s.lastRequestId,lifecycle:await evaluate('window.holdLifecycle||[]')}));};
 const rect=async selector=>{await evaluate(`document.querySelector(${JSON.stringify(selector)}).scrollIntoView({block:'center',behavior:'instant'})`);await wait(150);return evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)}),r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);};
 const pointer=async(type,p)=>call('Input.dispatchMouseEvent',{type,...p,button:'left',clickCount:1});
 const click=async selector=>{const p=await rect(selector),down=pointer('mousePressed',p);await wait(20);await Promise.all([down,pointer('mouseReleased',p)]);};
 await click('#loop-action-0');await until(s=>s.tracks[0].state==='recording','record');await wait(300);await click('#loop-action-0');await until(s=>s.tracks[0].state==='stopped','finish');await click('#loop-action-0');
 const clip=await until(s=>s.tracks[0].state==='playing'&&s.outputPeak>.02,'play actual PCM');
 if(heldOnly){
  // Retain the preceding clear/deferred-record transition from the broad gate.
  assert((await rpc({operation:'setParameter',id:'autoPlayAfterRecord',value:1})).ok);
  await evaluate(`window.savedHoldPump=AionCommandQueue.prototype.pump;window.pauseHoldQueue=true;AionCommandQueue.prototype.pump=function(){window.pausedHoldQueue=this;if(window.pauseHoldQueue)return;return window.savedHoldPump.call(this);};`);for(let n=0;n<60;++n){if(await evaluate('!!window.pausedHoldQueue'))break;await wait(25);}assert(await evaluate('!!window.pausedHoldQueue'),'capture actual frontend queue');
  await evaluate(`window.pausedHoldQueue.enqueue(()=>AionCommandQueue.wait,{track:0});window.pausedHoldQueue.enqueue(()=>({action:'record',track:0,value:0}),{track:0});`);
  await click('#loop-0 .loop-track-delete');await evaluate(`(async()=>{const queue=window.pausedHoldQueue,pump=window.savedHoldPump;AionCommandQueue.prototype.pump=pump;delete window.pauseHoldQueue;delete window.pausedHoldQueue;delete window.savedHoldPump;await pump.call(queue);})()`);await until(s=>s.tracks[0].state==='empty','preceding DEL');await wait(3200);
  await click('#loop-action-0');await until(s=>s.tracks[0].state==='recording','replacement record');await wait(180);await click('#loop-action-0');await until(s=>s.tracks[0].state==='playing','replacement auto-play');
  let stopRendered=false;for(let n=0;n<180;++n){stopRendered=await evaluate(`document.querySelector('#loop-action-0')?.textContent.trim()==='STOP'`);if(stopRendered)break;await wait(40);}assert(stopRendered,'Deferred hold starts on visible STOP');
  await evaluate(`window.holdLifecycle=[];for(const type of ['blur','focus','pagehide','keydown','keyup'])window.addEventListener(type,e=>{if(type!=='keydown'&&type!=='keyup'||e.code==='Digit1')window.holdLifecycle.push({type,key:e.code,time:performance.now(),focus:document.hasFocus()});},true);`);
  await evaluate(`window.savedOriginPress=AionPerformance.prototype.press;AionPerformance.prototype.press=function(id,key){const state=this.actions.track(id)?.state,result=window.savedOriginPress.call(this,id,key),p=this.presses.get(key);window.holdLifecycle.push({type:'origin',state,initial:p?.initialState,canDelete:p?.canDelete,button:document.querySelector('#loop-action-0')?.textContent,time:performance.now()});return result;};`);
  await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await until(s=>s.tracks[0].state==='stopped','held STOP');
  assert(await evaluate(`window.holdLifecycle.some(e=>e.type==='origin'&&e.initial==='playing'&&e.canDelete===true)`),'Real STOP-origin press captured');
  await evaluate(`window.savedHoldPump=AionCommandQueue.prototype.pump;window.pauseHoldQueue=true;AionCommandQueue.prototype.pump=function(){window.pausedHoldQueue=this;if(window.pauseHoldQueue)return;return window.savedHoldPump.call(this);};`);await wait(2800);
  await evaluate(`window.pausedHoldQueue.enqueue(()=>AionCommandQueue.wait,{track:1});window.pausedHoldQueue.enqueue(()=>({action:'record',track:0,value:0}),{track:0});`);
  await evaluate(`(async()=>{const queue=window.pausedHoldQueue,pump=window.savedHoldPump;AionCommandQueue.prototype.pump=pump;delete window.pauseHoldQueue;delete window.pausedHoldQueue;delete window.savedHoldPump;await pump.call(queue);})()`);await wait(1400);
  await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await until(s=>s.tracks[0].state==='empty','Held STOP bypasses another track finalization wait',1200);await wait(3200);assert.equal((await rpc({operation:'snapshot'})).tracks[0].state,'empty','Held STOP discards a queued replacement recording');
  console.log('PASS isolated held STOP with unchanged deferred-queue timing / '+process.argv[2],await evaluate('JSON.stringify(window.holdLifecycle)'));socket.close();return;
 }
 const stut=await evaluate(`(()=>{const e=[...document.querySelectorAll('#loop-0 button')].find(e=>e.textContent.trim()==='STUT');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
 await pointer('mousePressed',stut);await until(s=>s.tracks[0].stuttering,'held STUT');await cycle(0);
 const released=await until(s=>!s.tracks[0].stuttering&&s.tracks[0].state==='playing'&&s.outputPeak>.02,'hide cancels STUT with continuing PCM');assert.equal(released.tracks[0].waveformId,clip.tracks[0].waveformId);assert.equal(released.tracks[0].length,clip.tracks[0].length);await pointer('mouseReleased',{x:1190,y:790});
 // STOP-origin hold must never delete after focus/document loss, even with no keyup.
 await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await until(s=>s.tracks[0].state==='stopped','held STOP');await cycle(0);await wait(4200);
 const kept=await rpc({operation:'snapshot'});assert.equal(kept.tracks[0].waveformId,clip.tracks[0].waveformId);assert.equal(kept.tracks[0].length,clip.tracks[0].length);assert.equal(kept.tracks[0].state,'stopped');await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await click('#loop-action-0');await until(s=>s.tracks[0].state==='playing'&&s.outputPeak>.02,'retained PCM replays');
 assert((await rpc({operation:'midiConfig',action:'enable',enabled:true})).ok);assert((await rpc({operation:'midiConfig',action:'learn',target:124})).ok);await until(s=>s.midi.learnTarget===124,'learn active');await cycle(10);await until(s=>s.midi.learnTarget===-1,'hide cancels learn');
 const beforeExport=await rpc({operation:'snapshot'}),filename=`Hidden_${process.env.AION_MOUSE_TEST_RUN}.wav`;
 assert((await rpc({operation:'exportLoop',track:0,generation:beforeExport.generation,waveformId:clip.tracks[0].waveformId,filename})).ok);await until(s=>s.exportBusy&&s.exportStatus==='Choose a WAV destination','picker active');await cycle(0);await until(s=>!s.exportBusy,'hide cancels picker');
 fs.writeFileSync(path.join(directory,filename+'.choose'),'');await wait(300);assert(!fs.existsSync(path.join(directory,filename)),'obsolete valid destination cannot save after hide');
 // Explicit allowed blank documents are NOT automatic hide recovery requests.
 fs.writeFileSync(path.join(directory,'navigation-probe-start'),'1');await call('Page.navigate',{url:'about:blank'});await ready(true);await wait(300);await cycle(0,true);await wait(300);
 assert.equal(await evaluate('location.href'),'about:blank');assert.equal(await evaluate(`!!document.getElementById('inputTracksContainer')`),false,'no mixer bootstrap in blank document');
 let evidence;for(let n=0;n<100;++n){try{evidence=JSON.parse(fs.readFileSync(path.join(directory,'navigation-evidence.json'),'utf8'));}catch{}if(evidence?.testRun===process.env.AION_MOUSE_TEST_RUN&&evidence.playing&&!evidence.stuttering&&evidence.outputPeak>.02)break;await wait(20);}assert(evidence?.playing&&!evidence.stuttering&&evidence.outputPeak>.02,'native PCM continues in intentional blank');assert.equal(evidence.testRun,process.env.AION_MOUSE_TEST_RUN,'audio evidence belongs to this host');assert.equal(evidence.waveformId,clip.tracks[0].waveformId);
 await call('Page.navigate',{url:target.url});await ready();await until(s=>s.tracks[0].waveformId===clip.tracks[0].waveformId&&s.tracks[0].state==='playing'&&s.outputPeak>.02,'explicit return retains PCM');
 assert.deepEqual(errors,[]);console.log('PASS native editor visibility / '+process.argv[2]);socket.close();
})().catch(error=>{console.error(error);socket?.close();process.exitCode=1;});
