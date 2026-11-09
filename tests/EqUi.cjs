// SPDX-License-Identifier: AGPL-3.0-only
// Actual CDP pointers/keys into the production editor and hardware-free audio
// processor. This is not OS/driver, listening or Linux WebKit acceptance.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const wait=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
    assert(process.env.AION_MOUSE_TEST_RUN,'Use tools/TestEqUi.ps1 with its owned host');let target;
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
    let injected=0;const directory=path.resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]}`);
    const inject=async messages=>{const id=++injected,tmp=path.join(directory,'midi-input.tmp');fs.writeFileSync(tmp,JSON.stringify({id,messages}));fs.renameSync(tmp,path.join(directory,'midi-input.json'));for(let n=0;n<150;++n){try{if(Number(fs.readFileSync(path.join(directory,'midi-ack'),'utf8'))===id)return;}catch{}await wait(20);}assert.fail('EQ synthetic MidiBuffer injection timeout');};
    const drag=async (x,y,dx,dy)=>{
        const down=call('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',clickCount:1});await wait(15);
        const move=call('Input.dispatchMouseEvent',{type:'mouseMoved',x:x+dx,y:y+dy,button:'left',buttons:1});await wait(30);
        const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',x:x+dx,y:y+dy,button:'left',clickCount:1});await Promise.all([down,move,up]);
    };
    await call('Runtime.enable');await call('Page.enable');await call('Emulation.setFocusEmulationEnabled',{enabled:true});
    await call('Page.bringToFront');
    for(let n=0;n<100;++n){if(await evaluate(`!!document.querySelector('#native-eq-enabled')&&typeof AionNative?.request==='function'&&!!document.querySelector('#loop-0-eq')`))break;await wait(100);}
    await until(s=>s.audioReady&&s.inputAvailable,'native audio active');
    await evaluate(`document.documentElement.style.scrollBehavior='auto'`);
    for(const [id,value]of [['masterGain',1],['inputMonitor',0],['syncMode',0],['autoPlayAfterRecord',0],['metronomeVolume',0]])assert((await rpc({operation:'setParameter',id,value})).ok);
    await click('#loop-action-0');await until(s=>s.tracks[0].state==='recording','record');await wait(250);await click('#loop-action-0');
    await until(s=>s.tracks[0].state==='stopped','committed capture');await click('#loop-action-0');const before=await until(s=>s.tracks[0].state==='playing'&&s.outputPeak>.01,'playback PCM');let s;
    await evaluate(`(()=>{const b=window.__JUCE__.backend,original=b.emitEvent;b.emitEvent=function(name,message){const request=message?.params?.[0];if(name==='__juce__invoke'&&request?.operation==='equalizer'&&request.action==='toggle'){b.emitEvent=original;throw Error('Injected EQ send failure');}return original.call(b,name,message);};})()`);
    await click('#loop-0-eq');
    for(let n=0;n<100;++n){if(await evaluate(`document.querySelector('#native-status').textContent.includes('Injected EQ send failure')`))break;await wait(50);}
    assert(await evaluate(`document.querySelector('#native-status').textContent.includes('Injected EQ send failure')`),'Failed send is visible');
    assert(!(await snapshot()).tracks[0].equalizer.enabled,'Failed send cannot mutate audio');
    await click('#loop-0-eq');await until(s=>s.tracks[0].equalizer.enabled,'original checkbox');
    await wait(100);assert(!await evaluate(`document.querySelector('#native-status').textContent.includes('Injected EQ send failure')`),'Fresh successful gesture clears old send failure');
    for(const [id,value]of [['syncBpm',30],['syncBars',16],['syncMode',1]])assert((await rpc({operation:'setParameter',id,value})).ok);
    await click('#loop-19-eq');await until(s=>s.tracks[19].equalizer.enabled,'empty-track EQ configured before arming');
    await click('#loop-action-19');await until(s=>s.tracks[19].state==='armed','static arm with EQ');
    await click('#loop-action-19');s=await until(s=>s.tracks[19].state==='empty','cancel armed take');assert(s.tracks[19].equalizer.enabled,'CANCEL preserves EQ settings');
    await wait(100);assert(await evaluate(`document.querySelector('#loop-19-eq').checked`),'Original checkbox retains configured EQ after CANCEL');
    for(const [id,value]of [['syncMode',0],['syncBpm',120],['syncBars',2]])assert((await rpc({operation:'setParameter',id,value})).ok);
    await click('#loop-0-eq + span');
    // Original preset select's production change handler; pointer/keyboard graph
    // and original checkbox tests below use actual compositor input events.
    await evaluate(`document.querySelector('#sel_eq').value='Radio';document.querySelector('#sel_eq').dispatchEvent(new Event('change',{bubbles:true}))`);
    s=await until(s=>s.tracks[0].equalizer.preset==='Radio','original preset');await wait(250);
    let r=await rect('#eqCanvas');let x=r.left+Math.log(1500/10)/Math.log(22000/10)*r.width,y=r.top+(24-8)/48*r.height;
    await call('Page.captureScreenshot',{format:'png'}).then(r=>fs.writeFileSync(path.resolve(__dirname,`../build/eq-ui-before-${process.argv[2]}.png`),Buffer.from(r.data,'base64')));
    console.log('EQ point target',JSON.stringify({r,x,y,dom:await evaluate(`(()=>{const e=document.elementFromPoint(${x},${y});return {tag:e?.tagName,id:e?.id,readout:document.querySelector('#native-eq-point').textContent,viewport:[innerWidth,innerHeight]};})()`)}));
    await drag(x,y,25,-12);s=await until(s=>s.tracks[0].equalizer.values.p1Freq>1500&&s.tracks[0].equalizer.values.p1Gain>8,'XY point drag');assert.equal(s.tracks[0].equalizer.preset,'');
    await wait(250);let curve=await rpc({operation:'equalizer',action:'curve',track:0});r=await rect('#eqCanvas');
    x=r.left+Math.log(curve.ranges[0][1]/10)/Math.log(curve.maximum/10)*r.width;y=r.top+(24-s.tracks[0].equalizer.values.p1Gain)/48*r.height;
    const oldQ=s.tracks[0].equalizer.values.p1Q;await drag(x,y,15,0);s=await until(s=>s.tracks[0].equalizer.values.p1Q<oldQ,'horizontal bandwidth handle');
    await call('Input.dispatchKeyEvent',{type:'keyDown',key:'ArrowUp',code:'ArrowUp',windowsVirtualKeyCode:38});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'ArrowUp',code:'ArrowUp',windowsVirtualKeyCode:38});
    const keyboard=await until(v=>v.tracks[0].equalizer.values.p1Gain>s.tracks[0].equalizer.values.p1Gain,'keyboard graph gain');
    assert.equal(keyboard.tracks[0].waveformId,before.tracks[0].waveformId);assert.equal(keyboard.tracks[0].length,before.tracks[0].length);assert.equal(keyboard.tracks[0].undoCount,before.tracks[0].undoCount);
    await call('Page.captureScreenshot',{format:'png'}).then(r=>fs.writeFileSync(path.resolve(__dirname,`../build/eq-ui-${process.argv[2]}.png`),Buffer.from(r.data,'base64')));
    const saved=keyboard.tracks[0].equalizer;await evaluate(`window.eqOldDocument=true`);await call('Page.reload');
    for(let n=0;n<100;++n){if(await evaluate(`!window.eqOldDocument&&document.querySelector('#native-eq-enabled')?.checked===true&&document.querySelectorAll('#loopContainer > div').length===20`))break;await wait(100);}
    await evaluate(`document.documentElement.style.scrollBehavior='auto'`);await wait(200);
    s=await snapshot();assert.deepEqual(s.tracks[0].equalizer,saved,'reload retains native settings');assert.equal(s.tracks[0].state,'playing');assert(s.outputPeak>0);
    await click('#eqP1Gain');
    s=await snapshot();assert((await rpc({operation:'equalizer',track:0,action:'set',values:{p1Gain:6},generation:s.generation,expiresAt:Date.now()+3000})).ok);
    await until(s=>s.tracks[0].equalizer.values.p1Gain===6,'external writer updates focused EQ');await wait(150);
    assert(await evaluate(`document.activeElement===document.querySelector('#eqP1Gain')&&Number(document.querySelector('#eqP1Gain').value)===6`),'Focus alone must not freeze shared GUI/CLI slider state');
    r=await rect('#eqP1Gain');x=r.left+r.width*.625;y=r.y;
    for(const count of [1,2]){const down=call('Input.dispatchMouseEvent',{type:'mousePressed',x,y,button:'left',clickCount:count});await wait(20);const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',x,y,button:'left',clickCount:count});await Promise.all([down,up]);}
    await until(s=>s.tracks[0].equalizer.values.p1Gain===0,'original focused range double-click reset');await wait(100);
    assert(await evaluate(`document.activeElement===document.querySelector('#eqP1Gain')&&Number(document.querySelector('#eqP1Gain').value)===0`),'Focused reset updates actual slider thumb');
    await click('#native-eq-reset');s=await until(s=>s.tracks[0].equalizer.values.p1Gain===0&&s.tracks[0].equalizer.values.p1Freq===1000,'original flat reset');
    await click('#native-eq-enabled');await until(s=>!s.tracks[0].equalizer.enabled,'bypass');
    await click('#loop-0-eq');await until(s=>s.tracks[0].equalizer.enabled,'reenable');
    await click('#eqP1Gain','right');await until(s=>s.midi.learnTarget===97,'EQ right-click CC learn');
    await inject([[176,11,0,3,17],[191,11,127,3,18]]);
    s=await until(s=>s.midi.bindings[97]===(2|(11<<2))&&s.tracks[0].equalizer.values.p1Gain===24,'EQ learns zero and applies immediate same-block expression');await wait(150);
    assert(await evaluate(`Number(document.querySelector('#eqP1Gain').value)===24`),'Learned hardware updates actual focused EQ slider');
    await click('#fx-tabs-container button:nth-child(20)');await until(s=>s.midi.eqTrack===19,'EQ tab routes controller to expanded track 20');
    await inject([[176,11,0,3,3]]);s=await until(s=>s.tracks[19].equalizer.values.p1Gain===-24,'EQ CC follows selected tab');assert.equal(s.tracks[0].equalizer.values.p1Gain,24);
    await click('#fx-tabs-container button:nth-child(1)');await until(s=>s.midi.eqTrack===0,'EQ tab routes back');
    await click('#eqP1Q','right');await until(s=>s.midi.learnTarget===98,'EQ conflicting learn');await inject([[176,11,0,3,3]]);
    await until(s=>s.midi.learnTarget===-1&&s.midi.bindings[98]===0&&s.midi.bindings[97]!==0,'EQ conflict preserves existing binding');
    await click('#eqP1Gain','right');await until(s=>s.midi.learnTarget===97,'EQ learn before Escape');
    await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});
    await until(s=>s.midi.learnTarget===-1,'EQ Escape cancels learn');
    await click('#eqP1Gain','right',8);await until(s=>s.midi.bindings[97]===0,'EQ Shift-right-click removes binding');
    await inject([[176,11,0,3,3]]);assert.equal((await snapshot()).tracks[0].equalizer.values.p1Gain,24,'Cleared EQ binding cannot alter settings');
    await click('#loop-0 .loop-track-delete');s=await until(s=>s.tracks[0].state==='empty'&&!s.tracks[0].equalizer.enabled,'DEL resets effect');
    assert.equal(s.tracks[0].equalizer.values.p1Freq,1000);assert.equal(errors.length,0,JSON.stringify(errors));
    console.log(`PASS actual EQ UI ${process.argv[2]}: send failure/recovery, armed CANCEL, original controls/graph/keyboard/reload, focused slider/reset, MIDI same-block learn/selected-tab/expression/conflict/Escape/clear, bypass/DEL, nonzero PCM, no runtime exceptions`);
    socket.close();
})().catch(error=>{console.error(error);socket?.close();process.exitCode=1;});
