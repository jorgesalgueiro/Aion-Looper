// SPDX-License-Identifier: AGPL-3.0-only
// Actual editor/CDP gestures for the preserved B panel; no hardware claim.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),wait=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 assert(process.env.AION_MOUSE_TEST_RUN,'Use tools/TestReverbUi.ps1');let target;
 for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9236/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(target)break;await wait(100);}assert(target);
 socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
 let serial=0;const replies=new Map(),errors=[],domFrames=new Set();socket.addEventListener('message',e=>{const r=JSON.parse(e.data);if(r.method==='Runtime.exceptionThrown')errors.push(r.params.exceptionDetails);if(r.method==='Page.lifecycleEvent'&&r.params.name==='DOMContentLoaded')domFrames.add(r.params.frameId);replies.get(r.id)?.(r);});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial,timer=setTimeout(()=>{replies.delete(id);reject(Error(method+' timeout'));},15000);replies.set(id,r=>{clearTimeout(timer);replies.delete(id);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});socket.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{let r;try{r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});}catch(error){throw Error(error.message+' evaluating '+expression.slice(0,220));}assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 const rpc=r=>evaluate(`AionNative.request(${JSON.stringify(r)})`),snapshot=()=>rpc({operation:'snapshot'});
 const until=async(predicate,label)=>{let s;for(let n=0;n<240;++n){s=await snapshot();if(predicate(s))return s;await wait(50);}assert.fail(label+': '+JSON.stringify({reverb:s?.tracks[0]?.reverb,busy:s?.tracks[0]?.reverbBusy,pending:s?.tracks[0]?.reverbPending,ready:s?.tracks[0]?.reverbReady,errors}));};
 const click=async (selector,button='left')=>{let r;for(let n=0;n<30;++n){if(!await evaluate('!!document.querySelector('+JSON.stringify(selector)+')')){await wait(150);continue;}await evaluate(`document.querySelector(${JSON.stringify(selector)}).scrollIntoView({block:'center',behavior:'instant'})`);await wait(150);r=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)}),r=e.getBoundingClientRect(),hit=document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);return {x:r.x+r.width/2,y:r.y+r.height/2,disabled:e.disabled,hit:e.contains(hit),cover:hit?.outerHTML.slice(0,180)};})()`);if(!r.disabled&&r.hit)break;}assert(!r.disabled&&r.hit,selector+' blocked '+JSON.stringify(r));const down=call('Input.dispatchMouseEvent',{type:'mousePressed',x:r.x,y:r.y,button,clickCount:1});await wait(20);const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x,y:r.y,button,clickCount:1});await Promise.all([down,up]);};
 const key=async(key,code,vk)=>{await call('Input.dispatchKeyEvent',{type:'keyDown',key,code,windowsVirtualKeyCode:vk});await call('Input.dispatchKeyEvent',{type:'keyUp',key,code,windowsVirtualKeyCode:vk});};
 const focus=async selector=>{for(let n=0;n<120;++n){if(await evaluate(`!!document.querySelector(${JSON.stringify(selector)})&&!document.querySelector(${JSON.stringify(selector)}).disabled`))break;await wait(20);}const state=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});return {exists:!!e,disabled:e?.disabled??null,panel:!!document.querySelector('#native-reverb-panel'),controls:document.querySelectorAll('#native-reverb-controls input').length,title:document.title,ready:!!document.querySelector('#native-status')};})()`);assert(state.exists,selector+' missing '+JSON.stringify(state));await evaluate(`document.querySelector(${JSON.stringify(selector)}).focus()`);assert(await evaluate(`document.activeElement===document.querySelector(${JSON.stringify(selector)})`),selector+' focus '+JSON.stringify(state));};
 await call('Page.enable');const frame=(await call('Page.getFrameTree')).frameTree.frame.id;await call('Page.setLifecycleEventsEnabled',{enabled:true});for(let n=0;n<100&&!domFrames.has(frame);++n)await wait(100);assert(domFrames.has(frame),'native document DOMContentLoaded');
 await call('Runtime.enable');let editorReady=false;for(let n=0;n<200;++n){editorReady=await evaluate(`!!document.querySelector('#native-reverb-enabled')&&!!document.querySelector('#loop-action-0')&&typeof window.AionNative?.request==='function'`);if(editorReady)break;await wait(100);}assert(editorReady,'native Reverb editor initialized');await call('Emulation.setFocusEmulationEnabled',{enabled:true});
 await until(s=>s.audioReady&&s.inputAvailable,'audio active');
 assert(await evaluate(`(()=>{const s=document.querySelector('#sel_reverb');return s&&s.options.length===18&&s.options[1].value==='small'&&s.options[17].value==='shimmer-hall';})()`),'preserved B preset catalogue');
 for(const [id,value] of [['masterGain',1],['inputMonitor',0],['syncMode',0],['autoPlayAfterRecord',0],['metronomeVolume',0]])assert((await rpc({operation:'setParameter',id,value})).ok);
 await click('#loop-action-0');await until(s=>s.tracks[0].state==='recording','record');await wait(300);await click('#loop-action-0');await until(s=>s.tracks[0].state==='stopped','capture');await click('#loop-action-0');
 const take=await until(s=>s.tracks[0].state==='playing'&&s.outputPeak>.01,'nonzero playback');
 await click('#loop-0-reverb');await until(s=>s.tracks[0].reverb.enabled&&s.tracks[0].reverbReady&&s.outputPeak>.001,'async B row enable');await click('#loop-0-reverb + span');
 assert(await evaluate(`document.querySelector('#native-reverb-title').textContent.includes('Loop 1')`));
 const controls=[['reverbVolume','volume',.1],['reverbMix','mix',0],['reverbDuration','duration',.1],['reverbDecay','decay',.1],['reverbDamp','damp',500],['reverbPredelay','predelay',0]];
 for(const [id,name,min] of controls){await focus('#'+id);await key('Home','Home',36);await key('Tab','Tab',9);await until(s=>s.tracks[0].reverb.values[name]===min&&s.tracks[0].reverb.preset===''&&s.tracks[0].reverbReady,'trusted '+name+' range');}
 assert(await evaluate(`document.querySelector('#reverbDampVal').textContent==='500.00'`),'original value display');
 await focus('#sel_reverb');await key('End','End',35);await key('Tab','Tab',9);
 await until(s=>s.tracks[0].reverb.preset==='shimmer-hall'&&s.tracks[0].reverb.room==='cathedral'&&s.tracks[0].reverbReady,'original preset selector');
 let preset=await snapshot();assert.deepEqual(preset.tracks[0].reverb.values,{volume:1.3,mix:.65,duration:3.5,decay:2,damp:8000,predelay:.05});
 await click('#native-reverb-reset');await until(s=>s.tracks[0].reverbReady&&s.tracks[0].reverb.enabled&&s.tracks[0].reverb.preset===''&&s.tracks[0].reverb.values.volume===1&&s.tracks[0].reverb.values.mix===.15&&s.tracks[0].reverb.values.duration===1.2&&s.tracks[0].reverb.values.decay===1.8&&s.tracks[0].reverb.values.damp===20000&&s.tracks[0].reverb.values.predelay===0,'Reset preserves enabled');
 const precise=.12345678901234566,s=await snapshot();const receipt=await rpc({operation:'reverb',track:0,action:'set',parameter:'volume',value:precise,generation:s.generation,expiresAt:Date.now()+3000});assert(receipt.ok);
 await until(s=>s.tracks[0].reverbReady&&s.tracks[0].reverbRequestId===receipt.requestId&&s.tracks[0].reverb.values.volume===precise,'exact precision over real bridge');
 await click('#native-reverb-enabled');await until(s=>!s.tracks[0].reverb.enabled&&s.tracks[0].reverbReady,'panel bypass');await click('#native-reverb-enabled');await until(s=>s.tracks[0].reverb.enabled&&s.tracks[0].reverbReady,'panel re-enable');
 const beforeReload=await snapshot();assert.equal(beforeReload.tracks[0].waveformId,take.tracks[0].waveformId);assert.equal(beforeReload.tracks[0].state,'playing');
 await call('Page.reload',{ignoreCache:true});await wait(500);await until(s=>JSON.stringify(s.tracks[0].reverb)===JSON.stringify(beforeReload.tracks[0].reverb)&&s.outputPeak>.001,'reload retains native B settings and playback');
 for(let n=0;n<120;++n){if(await evaluate(`document.querySelector('#reverbVolumeVal').textContent==='0.12'`))break;await wait(50);}
 assert(await evaluate(`document.querySelector('#reverbVolumeVal').textContent==='0.12'`),'reloaded control shows the authoritative native value');
 assert(await evaluate(`(()=>{const b=[...document.querySelectorAll('.main-tab-btn')].find(e=>e.textContent.trim()==='LIVE PERFORMANCE');b.click();const t=document.querySelector('#live-loop-0-reverb');return t&&!t.disabled;})()`),'LIVE B toggle bound');
 await click('#live-loop-0-reverb');await until(s=>!s.tracks[0].reverb.enabled&&s.tracks[0].reverbReady,'LIVE bypass');
 await click('#live-loop-0-reverb');await until(s=>s.tracks[0].reverb.enabled&&s.tracks[0].reverbReady,'LIVE re-enable');
 await evaluate(`[...document.querySelectorAll('.main-tab-btn')].find(e=>e.textContent.trim()==='LOOP TRACKS').click()`);
 const pcmDirectory=path.resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]}`);
 const waitAck=async(kind,id)=>{const file=path.join(pcmDirectory,kind);for(let n=0;n<500;++n){try{if(fs.readFileSync(file,'utf8').trim()===String(id))return;}catch{}await wait(20);}assert.fail(kind+' timeout');};
 const statePreset=async preset=>{const s=await snapshot();const reply=await rpc({operation:'reverb',track:0,action:'preset',preset,generation:s.generation,expiresAt:Date.now()+3000});assert(reply.ok);};
 await statePreset('dark-cave');await until(s=>s.tracks[0].reverb.preset==='dark-cave'&&s.tracks[0].reverbReady,'dark-cave before host state save');
 const stateId=Date.now();
 const saveRequest=path.join(pcmDirectory,'state-save'),saveAck=path.join(pcmDirectory,'state-save-ack');
 fs.rmSync(saveAck,{force:true});fs.writeFileSync(saveRequest,String(stateId));await waitAck('state-save-ack',stateId);
 assert(fs.existsSync(path.join(pcmDirectory,`state-saved-${stateId}.bin`)),'host state saved through production getStateInformation');
 await statePreset('small');await until(s=>s.tracks[0].reverb.preset==='small'&&s.tracks[0].reverbReady,'small before host state load');
 const beforeLoadWaveform=(await snapshot()).tracks[0].waveformId;
 const loadRequest=path.join(pcmDirectory,'state-load'),loadAck=path.join(pcmDirectory,'state-load-ack');
 fs.rmSync(loadAck,{force:true});fs.writeFileSync(loadRequest,String(stateId));await waitAck('state-load-ack',stateId);
 const restored=await until(s=>s.tracks[0].reverb.preset==='dark-cave'&&s.tracks[0].reverbReady,'host state restores B');
 assert.equal(restored.tracks[0].waveformId,beforeLoadWaveform,'host state load preserves session PCM');
 for(let n=0;n<120;++n){if(await evaluate(`document.querySelector('#sel_reverb').value==='dark-cave'`))break;await wait(50);}
 assert(await evaluate(`document.querySelector('#sel_reverb').value==='dark-cave'`),'B panel follows host state load');
 assert(await evaluate(`document.querySelector('#reverbVolumeVal').textContent==='1.15'`),'B slider follows host state load');
 assert(await evaluate(`document.querySelector('#loop-0-reverb').checked`),'B row toggle follows host state load');
 await wait(200);await click('#loop-0-reverb');const end=await until(s=>!s.tracks[0].reverb.enabled&&s.tracks[0].reverbReady,'bypass after reload');assert.equal(end.tracks[0].waveformId,take.tracks[0].waveformId);assert(!await evaluate(`!!window.state?.audioContext`));assert.deepEqual(errors,[]);
 console.log('PASS actual Reverb UI '+process.argv[2]+': B row/panel enable, preset catalogue, six original ranges, Custom-on-edit, preset/room, Reset, exact precision, panel bypass, reload, LIVE binding, host state save/load restore and unchanged native nonzero PCM/clip');
})().catch(error=>{console.error(error);process.exitCode=1;}).finally(()=>socket?.close());
