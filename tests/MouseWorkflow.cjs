// Run only against aion_mouse_host, never against a hardware Standalone session.
// No snapshot fixtures: all PCM, state transitions and acknowledgements are real.
const assert = require('node:assert/strict');
const delay = ms => new Promise(r => setTimeout(r, ms));
let socket;
(async () => {
    let target;
    for (let n=0;n<100;++n) {
        try { target=(await (await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.type==='page'&&t.url.includes('juce.backend')); } catch {}
        if(target)break;await delay(150);
    }
    assert(target,'Start the synthetic mouse host on CDP port 9235');
    const ws=socket=new WebSocket(target.webSocketDebuggerUrl);
    await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});
    let seq=0;const pending=new Map();
    ws.addEventListener('message',e=>{const reply=JSON.parse(e.data);pending.get(reply.id)?.(reply);});
    const call=(method,params={})=>new Promise((resolve,reject)=>{
        const id=++seq,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout: '+method));},10000);
        pending.set(id,reply=>{clearTimeout(timer);pending.delete(id);reply.error?reject(Error(JSON.stringify(reply.error))):resolve(reply.result);});
        ws.send(JSON.stringify({id,method,params}));
    });
    const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
    const snapshot=()=>evaluate(`AionNative.request({operation:'snapshot'})`);
    const wait=async (predicate,label,timeout=7000)=>{const end=Date.now()+timeout;let value;while(Date.now()<end){value=await snapshot();if(predicate(value)){await delay(100);return value;}await delay(35);}assert.fail(label+': '+JSON.stringify(value));};
    const state=(wanted,id=0)=>wait(s=>s.tracks[id].state===wanted,`Loop ${id+1} -> ${wanted}`);
    const click=async (selector,{outside=false,hold=20}={})=>{
        const rect=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});if(!e)throw Error('Missing target');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,disabled:!!e.disabled,hit:e.contains(document.elementFromPoint(r.x+r.width/2,r.y+r.height/2))};})()`);
        assert(!rect.disabled&&rect.hit,'Mouse target blocked: '+selector+' '+JSON.stringify(rect));
        await call('Input.dispatchMouseEvent',{type:'mousePressed',x:rect.x,y:rect.y,button:'left',clickCount:1});
        await delay(hold);
        if(outside)await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:1190,y:790,button:'left',buttons:1});
        await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:outside?1190:rect.x,y:outside?790:rect.y,button:'left',clickCount:1});
    };
    const pauseDispatch=()=>evaluate(`window.savedHoldPump=AionCommandQueue.prototype.pump;window.pauseHoldQueue=true;AionCommandQueue.prototype.pump=function(){window.pausedHoldQueue=this;if(window.pauseHoldQueue)return;return window.savedHoldPump.call(this);};`);
    const resumeDispatch=()=>evaluate(`(async()=>{const queue=window.pausedHoldQueue,pump=window.savedHoldPump;AionCommandQueue.prototype.pump=pump;delete window.pauseHoldQueue;delete window.pausedHoldQueue;delete window.savedHoldPump;await pump.call(queue);})()`);
    const parameter=async (selector,value)=>{
        await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});e.value=${JSON.stringify(value)};e.dispatchEvent(new Event('change',{bubbles:true}));})()`);
    };
    const mode=async value=>{await click(`label:has(input[name=syncMode][value=${value}])`);await wait(s=>s.syncMode===['off','static','dynamic'].indexOf(value),'sync mode '+value);};
    const exactlyOne=async (gesture,label)=>{const before=await snapshot();await gesture();const after=await wait(s=>s.lastRequestId>before.lastRequestId,label);assert.equal(after.lastRequestId,before.lastRequestId+1,label);assert(!after.rejected,label+' rejected');await delay(160);assert.equal((await snapshot()).lastRequestId,after.lastRequestId,label+' duplicated');return after;};
    await call('Emulation.setFocusEmulationEnabled',{enabled:true});
    for(let n=0;n<100;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&document.querySelectorAll('#loopContainer > div').length===20`))break;await delay(100);}
    await wait(s=>s.audioReady&&s.inputAvailable&&s.inputPeak>0.1,'Synthetic native input active');
    await evaluate(`window.mouseErrors=[];window.addEventListener('error',e=>window.mouseErrors.push(e.message));document.documentElement.style.scrollBehavior='auto'`);
    await parameter('#bpmInput',120);await parameter('#numBars',1);await parameter('#timeSigNum',4);
    await click('#countInVisual');
    await mode('off');
    // The original click bypasses MASTER, but never loop capture. No device is open.
    const metroBefore=await snapshot();assert.equal(metroBefore.masterGain,0);
    await evaluate(`document.getElementById('metronomeVol').value='0.35';document.getElementById('metronomeVol').dispatchEvent(new Event('input',{bubbles:true}));`);
    await click('#metronomeBtn');
    const metroSound=await wait(s=>s.metronomeEnabled&&Math.abs(s.metronomeVolume-0.35)<0.001&&s.outputPeak>0.005,'Native metronome audible with MASTER muted');
    assert.equal(metroSound.masterPeak,0,'Native MASTER meter excludes the monitor click');
    for(let n=0;n<25;++n){assert((await evaluate(`document.getElementById('master-ascii-vu').textContent`)).includes('-inf'),'Original MASTER display excludes the audible click');await delay(25);}

    assert.equal((await snapshot()).lastRequestId,metroBefore.lastRequestId,'Metronome is independent of loop commands');
    await click('#native-stop-all');await wait(s=>s.metronomeEnabled&&s.outputPeak>0.005,'STOP ALL preserves the original metronome');
    await evaluate(`document.activeElement?.blur()`);
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'KeyN',key:'n',windowsVirtualKeyCode:78});
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'KeyN',key:'n',windowsVirtualKeyCode:78});
    await wait(s=>!s.metronomeEnabled&&s.outputPeak===0,'N stops native metronome without a click tail');
    assert.equal((await snapshot()).lastRequestId,metroBefore.lastRequestId+1,'Only STOP ALL dispatches an audio transport command');
    await evaluate(`document.getElementById('metronomeVol').value='0.5';document.getElementById('metronomeVol').dispatchEvent(new Event('input',{bubbles:true}));`);
    // Actual original SOUND checkbox and real output, with no physical device.
    await parameter('#bpmInput',240);await mode('static');
    await click('#countInAudio');await wait(s=>s.countInAudio&&!s.countInVisual,'SOUND-only lead-in');
    for(const cancel of ['#loop-action-0','#native-stop-all']){
        await click('#loop-action-0');await state('armed');
        const heard=await wait(s=>s.tracks[0].state==='armed'&&s.outputPeak>0.01,'Audible native SOUND before capture');
        assert.equal(heard.masterPeak,0,'SOUND excluded from MASTER meter');assert.equal(heard.countInFlash,0,'SOUND does not enable VISUAL');
        await click(cancel);await state('empty');
        await delay(150);for(let n=0;n<20;++n){const stopped=await snapshot();assert.equal(stopped.outputPeak,0,'Cancel leaves no delayed SOUND');assert.equal(stopped.countInFlash,0);assert.equal(stopped.tracks[0].state,'empty');await delay(50);}
    }
    await click('#countInAudio');await wait(s=>!s.countInAudio,'SOUND disabled');
    await parameter('#bpmInput',120);await mode('off');
    // Header and button are interchangeable, including release outside the button.
    await exactlyOne(()=>click('#loop-state-text-0'),'Header starts actual recording');
    await state('recording');await delay(220);
    await exactlyOne(()=>click('#loop-action-0',{outside:true}),'Dragged release finishes once');
    let playing=await state('playing');assert(playing.tracks[0].length>4800,'Real captured PCM committed');
    await evaluate(`(()=>{const e=document.getElementById('native-master');e.value=1;e.dispatchEvent(new Event('input',{bubbles:true}));})()`);
    await wait(s=>s.outputPeak>0.02,'Captured PCM renders through production processor');
    // Drag the original Start slider using actual pointer input, then release.
    const beforePhase=await snapshot();
    const phaseRect=await evaluate(`(()=>{const e=document.getElementById('loop-start-slider-0');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return {x:r.x,y:r.y+r.height/2,w:r.width};})()`);
    await call('Input.dispatchMouseEvent',{type:'mousePressed',x:phaseRect.x+phaseRect.w*0.2,y:phaseRect.y,button:'left',clickCount:1});
    await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:phaseRect.x+phaseRect.w*0.7,y:phaseRect.y,buttons:1,button:'left'});
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:phaseRect.x+phaseRect.w*0.7,y:phaseRect.y,button:'left',clickCount:1});
    const phased=await wait(s=>s.tracks[0].startDelay>0.5&&s.tracks[0].phaseApplyFrame===0,'Mouse Start phase applied');
    assert.equal(phased.tracks[0].state,'playing');assert.equal(phased.tracks[0].length,beforePhase.tracks[0].length);assert(!phased.rejected);
    assert.equal(await evaluate(`document.getElementById('loop-delay-display-0').textContent`),Math.floor(phased.tracks[0].startDelay*(phased.tracks[0].length/phased.sampleRate*1000))+'ms');
    await wait(s=>s.outputPeak>0.02,'Phase-shifted PCM keeps playing');
    const divisions=Math.round((phased.tracks[0].length/phased.sampleRate)/(60/phased.syncBpm/4));
    const snapped=divisions>0?Math.round(phased.tracks[0].startDelay*divisions)/divisions:phased.tracks[0].startDelay;
    await exactlyOne(()=>click('#loop-0 .btn-purple[title="Quantize Start to 16th Note"]'),'Original Q button sends exactly one command');
    const quantized=await wait(s=>Math.abs(s.tracks[0].startDelay-snapped)<1e-14&&s.tracks[0].phaseApplyFrame===0,'Mouse Q phase applied');
    assert.equal(quantized.tracks[0].length,phased.tracks[0].length);assert.equal(quantized.tracks[0].state,'playing');assert(!quantized.rejected);
    await wait(s=>s.outputPeak>0.02,'Quantized PCM keeps playing');
    for(const direction of [-1,1]){
        const before=await snapshot();let expected=before.tracks[0].startDelay+direction*((60/before.syncBpm/16)/(before.tracks[0].length/before.sampleRate));
        if(expected<0)expected+=1;if(expected>1)expected-=1;if(expected<0||expected>1)expected=((expected%1)+1)%1;
        await exactlyOne(()=>click(`#loop-0 [aria-label="Nudge loop start ${direction<0?'left':'right'}"]`),'Original nudge sends one command');
        const nudged=await wait(s=>Math.abs(s.tracks[0].startDelay-expected)<1e-14&&s.tracks[0].phaseApplyFrame===0,'Mouse nudge applied');
        assert.equal(nudged.tracks[0].state,'playing');assert.equal(nudged.tracks[0].length,before.tracks[0].length);assert(!nudged.rejected);
        await wait(s=>s.outputPeak>0.02,'Nudged PCM keeps playing');
    }
    await evaluate(`(()=>{const e=document.getElementById('loop-start-slider-0');e.value=0;e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}));})()`);
    await wait(s=>s.tracks[0].startDelay===0&&s.tracks[0].phaseApplyFrame===0,'Restore zero phase');

    const namedPcm=await snapshot();
    await click('#loop-0 .track-name');await call('Input.insertText',{text:'Guitarra Á 1 + 😀'});
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Enter',key:'Enter',windowsVirtualKeyCode:13});
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Enter',key:'Enter',windowsVirtualKeyCode:13});
    const renamed=await wait(s=>s.tracks[0].name==='Guitarra Á 1 + 😀','Rename real playing PCM');
    assert.equal(renamed.lastRequestId,namedPcm.lastRequestId,'Name input cannot trigger loop keyboard shortcuts');
    assert.equal(renamed.tracks[0].length,namedPcm.tracks[0].length);assert.equal(renamed.tracks[0].state,'playing');
    await wait(s=>s.outputPeak>0.02,'Renamed PCM keeps playing');

    // Pause only frontend dispatch, preserving real core PCM and audio callbacks.
    // A deletion queued at 4s must be discarded if focus is then lost.
    const beforeBlur=await snapshot();
    await pauseDispatch();
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await delay(4200);
    await evaluate(`window.dispatchEvent(new Event('blur'))`);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});
    await resumeDispatch();
    // The short STOP has expired after the queue's existing 3s limit. Retain
    // this case: the uncancelled deletion used to erase the still-playing clip.
    const afterBlur=await state('playing');assert.equal(afterBlur.tracks[0].length,beforeBlur.tracks[0].length,'Blur invalidates queued deletion, preserving actual PCM');
    await delay(200);assert.equal((await snapshot()).lastRequestId,beforeBlur.lastRequestId,'Expired short STOP and cancelled hold send no commands');
    await wait(s=>s.outputPeak>0.02,'PCM retained after cancelling queued deletion');
    // Also cover an acknowledged STOP: only hold commands wait behind dispatch.
    const beforeAckBlur=await snapshot();
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await state('stopped');
    await pauseDispatch();await delay(4200);await evaluate(`window.dispatchEvent(new Event('blur'))`);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await resumeDispatch();
    const acknowledged=await state('stopped');assert.equal(acknowledged.tracks[0].length,beforeAckBlur.tracks[0].length,'Cancelled queued delete preserves an already-stopped clip');
    assert.equal(acknowledged.lastRequestId,beforeAckBlur.lastRequestId+1,'Acknowledged short STOP remains the only command');
    await click('#loop-action-0');await state('playing');await wait(s=>s.outputPeak>0.02,'Stopped PCM still replays after cancellation');
    // Clear/re-record while the physical key remains held: an old STOP cannot
    // delete the replacement take when its original 4s deadline arrives.
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    assert.equal((await snapshot()).tracks[0].name,'','DEL clears the name with the real loop');
    await click('#loop-action-0');await state('recording');await delay(180);await click('#loop-action-0');
    const replacement=await state('playing');await delay(4200);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});
    const protectedTake=await state('playing');assert.equal(protectedTake.tracks[0].length,replacement.tracks[0].length,'DEL cancels stale holds before a replacement take');
    await wait(s=>s.outputPeak>0.02,'Replacement PCM survives the previous gesture deadline');
    // Seed a finalization wait in the actual frontend FIFO, without changing
    // snapshots or PCM. DEL must cancel unsent actions and clear immediately.
    await evaluate(`window.savedClearPump=AionCommandQueue.prototype.pump;AionCommandQueue.prototype.pump=function(){window.clearTestQueue=this;return window.savedClearPump.call(this);};`);
    for(let n=0;n<60;++n){if(await evaluate(`!!window.clearTestQueue`))break;await delay(25);}
    await evaluate(`AionCommandQueue.prototype.pump=window.savedClearPump;window.clearTestQueue.enqueue(()=>AionCommandQueue.wait,{track:0});window.clearTestQueue.enqueue(()=>({action:'record',track:0,value:0}),{track:0});delete window.savedClearPump;delete window.clearTestQueue;`);
    const beforeDeferredClear=await snapshot();await click('#loop-0 .loop-track-delete');
    const immediatelyCleared=await wait(s=>s.tracks[0].state==='empty','DEL bypasses finalization wait',1200);
    assert.equal(immediatelyCleared.lastRequestId,beforeDeferredClear.lastRequestId+1,'Only explicit DEL dispatches');
    await delay(3200);assert.equal((await snapshot()).tracks[0].state,'empty','Cancelled deferred record cannot resurrect the deleted loop');
    await click('#loop-action-0');await state('recording');await delay(180);await click('#loop-action-0');await state('playing');
    // The 4s STOP gesture must clear just like DEL, even behind another track's
    // finalization wait, and discard an unsent record belonging to this track.
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await state('stopped');
    await pauseDispatch();await delay(2800);
    await evaluate(`window.pausedHoldQueue.enqueue(()=>AionCommandQueue.wait,{track:1});window.pausedHoldQueue.enqueue(()=>({action:'record',track:0,value:0}),{track:0});`);
    await resumeDispatch();await delay(1400);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});
    await wait(s=>s.tracks[0].state==='empty','Held STOP bypasses another track finalization wait',1200);
    await delay(3200);assert.equal((await snapshot()).tracks[0].state,'empty','Held STOP discards a queued replacement recording');
    await click('#loop-action-0');await state('recording');await delay(180);await click('#loop-action-0');await state('playing');
    const beforeHold=await snapshot();
    await click('#loop-action-0',{hold:3800});
    const kept=await state('stopped');assert.equal(kept.tracks[0].length,beforeHold.tracks[0].length,'Releasing STOP before 4s keeps PCM');
    await delay(400);assert.equal((await snapshot()).tracks[0].length,kept.tracks[0].length,'Released hold cannot delete later');
    await click('#loop-action-0');await state('playing');
    await click('#loop-action-0',{hold:4200});
    const heldStop=await state('empty');
    assert.equal(heldStop.tracks[0].length,0,'Holding STOP intentionally deletes a bad loop');
    assert.equal(heldStop.tracks[0].undoCount,0,'Quick delete clears local undo history');
    await click('#loop-action-0');await state('recording');await delay(220);
    await click('#loop-action-0');await state('playing');
    await exactlyOne(()=>click('#loop-state-symbol-0'),'Header stops playback');await state('stopped');
    await exactlyOne(()=>click('#loop-action-0'),'Mouse replays committed loop');await state('playing');
    // Clicking nested controls/waveform must never also toggle transport.
    const beforeWave=await snapshot();await click('#loop-wave-0');await delay(250);
    assert.equal((await snapshot()).lastRequestId,beforeWave.lastRequestId,'Waveform never toggles transport');
    await exactlyOne(()=>click('#loop-0 .loop-track-controls button[aria-pressed]'),'Nested control sends only its own action');
    await exactlyOne(()=>click('#loop-0 .loop-track-controls button[aria-pressed]'),'Unmute without toggling transport');
    await exactlyOne(()=>click('#native-stop-all'),'Stop all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    // FINISH may commit and auto-play before 600ms; the same hold must not stop it.
    await click('#loop-action-0');await state('recording');await delay(180);
    await click('#loop-action-0',{hold:4200});playing=await state('playing');
    const finishedLength=playing.tracks[0].length;assert(finishedLength>4800,'Held FINISH commits PCM');
    await wait(s=>s.outputPeak>0.02,'Held FINISH preserves auto-play');
    await call('Input.dispatchKeyEvent',{type:'keyDown',code:'Digit1',key:'1',windowsVirtualKeyCode:49});await delay(4200);
    await call('Input.dispatchKeyEvent',{type:'keyUp',code:'Digit1',key:'1',windowsVirtualKeyCode:49});
    const heldKeyStop=await state('empty');assert.equal(heldKeyStop.tracks[0].length,0,'Held keyboard STOP also quick-deletes the bad loop');
    // STATIC: arm cancellation, automatic capture, exact length, queued play cancellation.
    await mode('static');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early static grid phase');
    await exactlyOne(()=>click('#loop-action-0'),'Mouse arms static capture');await state('armed');
    await exactlyOne(()=>click('#loop-action-0'),'Mouse cancels arm');await state('empty');
    await exactlyOne(()=>click('#loop-state-text-0'),'Header arms static capture');
    await state('recording');playing=await state('playing');
    assert.equal(playing.tracks[0].length,96000,'Static capture has exact two-second grid PCM');
    await wait(s=>s.outputPeak>0.02,'Static PCM playback');
    await exactlyOne(()=>click('#native-stop-all'),'Stop static playback');await state('stopped');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early queued-play grid phase');
    await exactlyOne(()=>click('#loop-action-0'),'Queue static play');await state('queued');
    await exactlyOne(()=>click('#loop-state-text-0'),'Header cancels queued play');await state('stopped');
    await delay(2100);assert.equal((await snapshot()).tracks[0].state,'stopped','Cancelled play stays cancelled across grid');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early held queued-cancel phase');
    await click('#loop-action-0');await state('queued');
    await click('#loop-action-0',{hold:4200});
    const heldCancel=await state('stopped');assert.equal(heldCancel.tracks[0].length,96000,'Holding queued CANCEL preserves PCM');
    await exactlyOne(()=>click('#loop-action-0'),'Queue playback again');await state('playing');
    await exactlyOne(()=>click('#loop-action-0'),'Queue quantized stop');await state('stopping');
    await exactlyOne(()=>click('#loop-action-0'),'Second mouse stop cuts now');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    // DYNAMIC: starts on mouse down, first finish schedules, second finishes immediately.
    await mode('dynamic');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early dynamic grid phase');
    await exactlyOne(()=>click('#loop-action-0'),'Dynamic mouse starts recording');await state('recording');
    await delay(200);await click('#loop-action-0');
    await wait(s=>s.tracks[0].stopPending,'Dynamic finish deadline');
    playing=await state('playing');assert(playing.tracks[0].length>0&&playing.tracks[0].dynamicRecorded,'Dynamic PCM committed');
    await wait(s=>s.outputPeak>0.02,'Dynamic PCM playback');
    await click('#native-stop-all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    await wait(s=>s.gridPosition>0.03&&s.gridPosition<0.2,'Early dynamic second-tap phase');
    await click('#loop-action-0');await state('recording');await delay(150);
    await click('#loop-action-0');await wait(s=>s.tracks[0].stopPending,'Dynamic scheduled finish before second tap');
    await click('#loop-action-0');playing=await state('playing');
    assert(playing.tracks[0].length<96000,'Second finish mouse tap commits before the bar deadline');
    await click('#native-stop-all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    // A short fixed take can finish automatically while RECORD is still held.
    await parameter('#bpmInput',600);await parameter('#timeSigNum',1);
    await mode('static');await wait(s=>s.syncBpm===600&&s.syncNum===1,'Fast static grid');
    await click('#loop-action-0',{hold:4200});
    const heldRecord=await state('playing');assert.equal(heldRecord.tracks[0].length,4800,'Held RECORD does not stop an auto-completed take');
    await click('#native-stop-all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    await parameter('#bpmInput',120);await parameter('#timeSigNum',4);
    await mode('off');
    await click('#autoPlayAfterRecord');await wait(s=>!s.autoPlayAfterRecord,'Auto-play off');
    await click('#loop-action-0');await state('recording');await delay(150);await click('#loop-action-0');await state('stopped');
    await click('#loop-action-0');await state('playing');
    await wait(s=>s.outputPeak>0.02,'Explicit play with auto-play disabled');
    await click('#native-stop-all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
    const rapidBefore=await snapshot();
    await click('#loop-action-0');await click('#loop-action-0');await click('#loop-action-0');
    const rapidAfter=await state('playing');
    assert.equal(rapidAfter.lastRequestId,rapidBefore.lastRequestId+3,'Rapid record/finish/play resolves against acknowledged native state');
    assert(rapidAfter.tracks[0].length>0&&!rapidAfter.rejected,'Rapid mouse sequence commits real audio');
    await delay(750);assert.equal((await snapshot()).lastRequestId,rapidAfter.lastRequestId,'Released mouse gestures leave no delayed hold');
    assert.deepEqual(await evaluate('window.mouseErrors'),[],'No browser gesture exceptions');
    console.log('PASS: real mouse/header arm, cancel, record, native PCM playback, Start drag/Q/nudges, nested-control isolation, STATIC queued play/stop and DYNAMIC finish/second tap, auto-play off, rapid record/finish/play, held STOP quick-deletes; FINISH/CANCEL/RECORD preserve PCM; no device opened');
    ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
