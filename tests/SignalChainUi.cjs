// SPDX-License-Identifier: AGPL-3.0-only
// Actual CDP pointers/keys into the production editor and hardware-free audio
// processor. This is not OS/driver, listening or Linux WebKit acceptance.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const wait=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
    assert(process.env.AION_MOUSE_TEST_RUN,'Use tools/TestSignalChainUi.ps1 with its owned host');let target;
    for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(target)break;await wait(100);}
    assert(target);socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
    let serial=0;const replies=new Map(),errors=[];
    socket.addEventListener('message',e=>{const r=JSON.parse(e.data);if(r.method==='Runtime.exceptionThrown')errors.push(r.params.exceptionDetails);replies.get(r.id)?.(r);});
    const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial,timer=setTimeout(()=>{replies.delete(id);reject(Error(method+' timeout'));},12000);replies.set(id,r=>{clearTimeout(timer);replies.delete(id);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});socket.send(JSON.stringify({id,method,params}));});
    const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
    const rpc=request=>evaluate(`AionNative.request(${JSON.stringify(request)})`),snapshot=()=>rpc({operation:'snapshot'});
    const until=async(predicate,label)=>{let s;for(let n=0;n<120;++n){s=await snapshot();if(predicate(s))return s;await wait(50);}const t={...s?.tracks[0]};delete t.wavePeaks;assert.fail(label+': '+JSON.stringify({frame:s?.frame,request:s?.lastRequestId,track:t,errors}));};
    const rect=async selector=>{
        await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});if(!e)throw Error('Missing '+${JSON.stringify(selector)});e.scrollIntoView({block:'center',behavior:'instant'});})()`);
        await wait(150);
        return evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)}),r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,left:r.x,top:r.y,width:r.width,height:r.height,disabled:!!e.disabled,hit:e.contains(document.elementFromPoint(r.x+r.width/2,r.y+r.height/2))};})()`);
    };
    const click=async (selector,button='left',modifiers=0)=>{const r=await rect(selector);assert(!r.disabled&&r.hit,selector+' blocked '+JSON.stringify(r));const down=call('Input.dispatchMouseEvent',{type:'mousePressed',x:r.x,y:r.y,button,modifiers,clickCount:1});await wait(20);const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x,y:r.y,button,modifiers,clickCount:1});await Promise.all([down,up]);};

    await call('Runtime.enable');await call('Page.enable');await call('Emulation.setFocusEmulationEnabled',{enabled:true});await call('Page.bringToFront');
    const field='#loop-0 [aria-label="Loop FX Chain"]',last='#loop-19 [aria-label="Loop FX Chain"]';
    for(let n=0;n<100;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&!!document.querySelector(${JSON.stringify(field)})`))break;await wait(100);}
    await until(s=>s.audioReady&&s.inputAvailable,'native ready');
    await evaluate(`document.documentElement.style.scrollBehavior='auto'`);
    const key=async(key,code,vk,modifiers=0)=>{await call('Input.dispatchKeyEvent',{type:'keyDown',key,code,windowsVirtualKeyCode:vk,modifiers});await call('Input.dispatchKeyEvent',{type:'keyUp',key,code,windowsVirtualKeyCode:vk,modifiers});};
    const enter=async(selector,text)=>{await click(selector);await key('a','KeyA',65,2);await call('Input.insertText',{text});await key('Tab','Tab',9);};
    for(const [id,value]of [['masterGain',1],['inputMonitor',0],['syncMode',0],['autoPlayAfterRecord',0],['metronomeVolume',0]])assert((await rpc({operation:'setParameter',id,value})).ok);
    await enter(last,' c q a c ! ');await until(s=>s.tracks[19].signalChain==='CQA','expanded-track original field');await wait(120);
    assert.equal(await evaluate(`document.querySelector(${JSON.stringify(last)}).value`),'CQA');
    assert.equal((await snapshot()).tracks[19].state,'empty','typing chain cannot trigger performance keys');
    await click('#loop-action-0');await until(s=>s.tracks[0].state==='recording','capture');await wait(250);await click('#loop-action-0');await until(s=>s.tracks[0].state==='stopped','finish');await click('#loop-action-0');
    let before=await until(s=>s.tracks[0].state==='playing'&&s.outputPeak>.01,'nonzero playback');
    await click('#loop-0-compressor');await until(s=>s.tracks[0].compressor.enabled,'C enabled');
    await click('#loop-0-eq');await until(s=>s.tracks[0].equalizer.enabled,'Q enabled');
    for(const order of ['CQ','QC','A','CQ']){
        await enter(field,order);const s=await until(s=>s.tracks[0].signalChain===order,'live chain '+order);
        assert.equal(s.tracks[0].state,'playing');assert.equal(s.tracks[0].length,before.tracks[0].length);assert.equal(s.tracks[0].waveformId,before.tracks[0].waveformId);assert.equal(s.tracks[0].undoCount,before.tracks[0].undoCount);assert(s.outputPeak>0);
        await wait(120);assert.deepEqual(await evaluate(`[...document.querySelector('#loop-fx-container-0').children].map(span=>span.querySelector('input').id.split('-').at(-1))`),['reverse',...order].map(c=>({Q:'eq',C:'compressor',A:'arpDelay',reverse:'reverse'})[c]),'visible original effect controls follow the chain, including omission');
    }
    await evaluate(`(()=>{const b=window.__JUCE__.backend,old=b.emitEvent;b.emitEvent=function(name,message){if(message?.params?.[0]?.operation==='signalChain'){b.emitEvent=old;throw Error('Injected chain send failure');}return old.call(b,name,message);};})()`);
    await enter(field,'Q');for(let i=0;i<80;++i){if(await evaluate(`document.querySelector('#native-status').textContent.includes('Injected chain send failure')`))break;await wait(50);}
    assert(await evaluate(`document.querySelector('#native-status').textContent.includes('Injected chain send failure')`));assert.equal((await snapshot()).tracks[0].signalChain,'CQ');
    await enter(field,'QC');await until(s=>s.tracks[0].signalChain==='QC','send failure recovery');
    await click(field);await key('a','KeyA',65,2);await call('Input.insertText',{text:'CQ'});await wait(250);assert.equal(await evaluate(`document.querySelector(${JSON.stringify(field)}).value`),'CQ','snapshots cannot erase uncommitted typing');assert.equal((await snapshot()).tracks[0].signalChain,'QC');await key('Tab','Tab',9);await until(s=>s.tracks[0].signalChain==='CQ','draft commit');
    await evaluate('window.chainOld=true');await call('Page.reload');for(let n=0;n<100;++n){if(await evaluate(`!window.chainOld&&document.querySelector(${JSON.stringify(field)})?.value==='CQ'`))break;await wait(100);}
    assert.equal(await evaluate(`document.querySelector(${JSON.stringify(field)}).value`),'CQ','reopened editor shows native chain');assert.equal((await snapshot()).tracks[19].signalChain,'CQA');
    assert(await evaluate(`document.querySelector('#loop-0 select[aria-label="Apply FX Preset"]').disabled`),'chain presets remain explicitly unavailable');
    await click('.main-tab-btn:nth-child(4)');await wait(150);
    assert.deepEqual(await evaluate(`[...document.querySelector('#live-loop-fx-container-0').children].map(span=>span.querySelector('input').id.split('-').at(-1))`),['reverse','compressor','eq'],'LIVE card reflects persisted order');
    let live=await snapshot();assert((await rpc({operation:'signalChain',action:'set',track:0,order:'A',generation:live.generation,expiresAt:Date.now()+3000})).ok);await until(s=>s.tracks[0].signalChain==='A','external writer updates LIVE order');await wait(150);
    assert.deepEqual(await evaluate(`[...document.querySelector('#live-loop-fx-container-0').children].map(span=>span.querySelector('input').id.split('-').at(-1))`),['reverse','arpDelay'],'LIVE card removes omitted effects');
    live=await snapshot();assert((await rpc({operation:'signalChain',action:'set',track:0,order:'CQ',generation:live.generation,expiresAt:Date.now()+3000})).ok);await until(s=>s.tracks[0].signalChain==='CQ','LIVE reinsertion');await wait(100);
    await click('#live-loop-0-compressor');await until(s=>!s.tracks[0].compressor.enabled,'reinserted LIVE toggle retains listener');
    await click('.main-tab-btn:nth-child(1)');await wait(100);

    await click('#loop-0 .loop-track-delete');await until(s=>s.tracks[0].state==='empty'&&s.tracks[0].signalChain==='QCATFODBVKZG','DEL resets chain');await wait(150);
    assert.equal(await evaluate(`document.querySelector(${JSON.stringify(field)}).value`),'QCATFODBVKZG');assert.equal(errors.length,0,JSON.stringify(errors));
    console.log(`PASS actual chain UI ${process.argv[2]}: original text field, typing/normalization, expanded track, live order/omission, send failure/recovery, draft ownership, reopen/DEL and nonzero PCM`);socket.close();
})().catch(error=>{console.error(error);socket?.close();process.exitCode=1;});
