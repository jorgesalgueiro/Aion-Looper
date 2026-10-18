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
    // Header and button are interchangeable, including release outside the button.
    await exactlyOne(()=>click('#loop-state-text-0'),'Header starts actual recording');
    await state('recording');await delay(220);
    await exactlyOne(()=>click('#loop-action-0',{outside:true}),'Dragged release finishes once');
    let playing=await state('playing');assert(playing.tracks[0].length>4800,'Real captured PCM committed');
    await evaluate(`(()=>{const e=document.getElementById('native-master');e.value=1;e.dispatchEvent(new Event('input',{bubbles:true}));})()`);
    await wait(s=>s.outputPeak>0.02,'Captured PCM renders through production processor');
    await exactlyOne(()=>click('#loop-state-symbol-0'),'Header stops playback');await state('stopped');
    await exactlyOne(()=>click('#loop-action-0'),'Mouse replays committed loop');await state('playing');
    // Clicking nested controls/waveform must never also toggle transport.
    const beforeWave=await snapshot();await click('#loop-wave-0');await delay(250);
    assert.equal((await snapshot()).lastRequestId,beforeWave.lastRequestId,'Waveform never toggles transport');
    await exactlyOne(()=>click('#loop-0 .loop-track-controls button[aria-pressed]'),'Nested control sends only its own action');
    await exactlyOne(()=>click('#loop-0 .loop-track-controls button[aria-pressed]'),'Unmute without toggling transport');
    await exactlyOne(()=>click('#native-stop-all'),'Stop all');await state('stopped');
    await click('#loop-0 .loop-track-delete');await state('empty');
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
    console.log('PASS: real mouse/header arm, cancel, record, native PCM playback, drag release, nested-control isolation, STATIC queued play/stop and DYNAMIC finish/second tap, auto-play off, rapid record/finish/play; no device opened');
    ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
