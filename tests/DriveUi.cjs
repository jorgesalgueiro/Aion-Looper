// SPDX-License-Identifier: AGPL-3.0-only
// Production WebView2 editor with actual CDP pointer/key events and synthetic
// callback PCM/MidiBuffer. Not a driver, listening or Linux WebKit test.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const wait=ms=>new Promise(r=>setTimeout(r,ms));let socket,phase='connect';const browserEvents=[];
(async()=>{
 assert(process.env.AION_MOUSE_TEST_RUN,'Use TestDriveUi.ps1');let target;
 for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.url.includes('juce.backend'));}catch{}if(target)break;await wait(100);}assert(target);
 socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
 let serial=0;const replies=new Map(),errors=[];
 socket.addEventListener('message',e=>{const r=JSON.parse(e.data);if(r.method==='Runtime.exceptionThrown')errors.push(r.params.exceptionDetails);if(r.method&&/executionContext|frameNavigated|loadEventFired|javascriptDialogOpening|targetCrashed/.test(r.method)){if(browserEvents.length===64)browserEvents.shift();browserEvents.push({phase,method:r.method,params:r.params});}replies.get(r.id)?.(r);});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial,timer=setTimeout(()=>{replies.delete(id);reject(Error(method+' timeout during '+phase+': '+String(params.expression||params.type||'').slice(0,160)));},12000);replies.set(id,r=>{clearTimeout(timer);replies.delete(id);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});socket.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 const rpc=request=>evaluate(`AionNative.request(${JSON.stringify(request)})`),snapshot=()=>rpc({operation:'snapshot'});
 const until=async(predicate,label)=>{phase=label;let s;for(let n=0;n<120;++n){s=await snapshot();if(predicate(s))return s;await wait(50);}assert.fail(label+': '+JSON.stringify({track:s?.tracks?.[0]?.drives,errors}));};
 const rect=async selector=>{phase='pointer target '+selector;
  let r;
  for(let attempt=0;attempt<10;++attempt){
   await evaluate(`(()=>{document.documentElement.style.scrollBehavior='auto';document.querySelector(${JSON.stringify(selector)}).scrollIntoView({block:'center',behavior:'instant'});})()`);await wait(100);
   r=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)}),r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,width:r.width,left:r.x,disabled:!!e.disabled,hit:e.contains(document.elementFromPoint(r.x+r.width/2,r.y+r.height/2)),viewport:innerHeight,scroll:scrollY,ready:document.readyState};})()`);
   if(r.hit)return r;
  }assert.fail('Actual pointer target remains occluded '+selector+' '+JSON.stringify(r));
 };
 const click=async(selector,button='left',modifiers=0,count=1)=>{const r=await rect(selector);assert(!r.disabled&&r.hit,selector+' blocked '+JSON.stringify(r));const down=call('Input.dispatchMouseEvent',{type:'mousePressed',x:r.x,y:r.y,button,modifiers,clickCount:count}),up=call('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.x,y:r.y,button,modifiers,clickCount:count});await Promise.all([down,up]);};
 const key=async(name,code)=>{await call('Input.dispatchKeyEvent',{type:'keyDown',key:name,code:name,windowsVirtualKeyCode:code,nativeVirtualKeyCode:code});await call('Input.dispatchKeyEvent',{type:'keyUp',key:name,code:name,windowsVirtualKeyCode:code,nativeVirtualKeyCode:code});};
 let injected=0;const directory=path.resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]}`);
 const inject=async messages=>{phase='MIDI injection';const id=++injected,tmp=path.join(directory,'midi-input.tmp');fs.writeFileSync(tmp,JSON.stringify({id,messages}));fs.renameSync(tmp,path.join(directory,'midi-input.json'));for(let n=0;n<150;++n){try{if(Number(fs.readFileSync(path.join(directory,'midi-ack'),'utf8'))===id)return;}catch{}await wait(20);}assert.fail('Drive MidiBuffer injection timeout');};
 const ready=async(label='initial bootstrap')=>{phase=label;for(let n=0;n<120;++n){if(await evaluate(`['distortionAmount','overdriveDrive','fuzzGain'].every(id=>{const e=document.getElementById(id);return e&&!e.disabled;})`))return;await wait(100);}assert.fail('Native drive controls not ready');};
 await call('Runtime.enable');await call('Page.enable');await call('Emulation.setFocusEmulationEnabled',{enabled:true});await call('Page.bringToFront');
 // Wait for the real page/bootstrap (and its catalog replies), not merely the
 // document-start bridge function, before issuing test-owned native requests.
 await ready();await until(s=>s.audioReady&&s.inputAvailable,'Audio fixture ready');
 for(const [id,value]of [['masterGain',1],['inputMonitor',0],['syncMode',0],['autoPlayAfterRecord',0],['metronomeVolume',0]])assert((await rpc({operation:'setParameter',id,value})).ok);
 await click('#loop-action-0');await until(s=>s.tracks[0].state==='recording','Record');await wait(250);await click('#loop-action-0');await until(s=>s.tracks[0].state==='stopped','Finish');await click('#loop-action-0');
 const before=await until(s=>s.tracks[0].state==='playing'&&s.outputPeak>.01,'Nonzero playback');
 await evaluate(`(()=>{const b=window.__JUCE__.backend,original=b.emitEvent;b.emitEvent=function(name,message){const r=message?.params?.[0];if(name==='__juce__invoke'&&r?.operation==='drive'&&r.action==='toggle'){b.emitEvent=original;throw Error('Injected drive send failure');}return original.call(b,name,message);};})()`);
 await click('#loop-0-distortion');for(let n=0;n<100;++n){if(await evaluate(`document.querySelector('#native-status').textContent.includes('Injected drive send failure')`))break;await wait(50);}
 assert(!(await snapshot()).tracks[0].drives.distortion.enabled,'Rejected send cannot enable DSP');
 const specs=[['distortion','tube-warmth','amount'],['overdrive','American Sparkle','drive'],['fuzz','Big Muff','gain']];
 for(const [name,preset,param]of specs){
  await click(`#loop-0-${name}`);await until(s=>s.tracks[0].drives[name].enabled,name+' enabled');
  await click(`#sel_${name}`);await key('Home',36);await key('ArrowDown',40);await key('ArrowDown',40);await key('Enter',13);
  await until(s=>s.tracks[0].drives[name].preset===preset,name+' original preset keyboard selection');
  const id=name+param[0].toUpperCase()+param.slice(1),r=await rect('#'+id);
  const down=call('Input.dispatchMouseEvent',{type:'mousePressed',x:r.left+r.width*.6,y:r.y,button:'left',clickCount:1});
  const move=call('Input.dispatchMouseEvent',{type:'mouseMoved',x:r.left+r.width*.7,y:r.y,button:'left',buttons:1});const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',x:r.left+r.width*.7,y:r.y,button:'left',clickCount:1});await Promise.all([down,move,up]);
  const s=await until(s=>!s.tracks[0].drives[name].preset,name+' live slider');assert.equal(s.tracks[0].waveformId,before.tracks[0].waveformId);assert.equal(s.tracks[0].state,'playing');
 }
 const parameters=[...['amount','shape','tone','volume','mix'].map(p=>['distortion',p]),...['drive','tone','volume','mix'].map(p=>['overdrive',p]),...['gain','bias','tone','volume','mix'].map(p=>['fuzz',p])];
 for(const [i,[name,param]]of parameters.entries()){
  const id=name+param[0].toUpperCase()+param.slice(1);await click('#'+id,'right');await until(s=>s.midi.learnTarget===133+i,'Native drive CC learn');
  await inject([[176,11+i,0,3,17],[191,11+i,127,3,18]]);const expected=Number(await evaluate(`document.getElementById(${JSON.stringify(id)}).max`));
  await until(s=>s.tracks[0].drives[name].values[param]===expected,'Same-block learned drive CC');
  for(let n=0;n<100&&Number(await evaluate(`document.getElementById(${JSON.stringify(id)}).value`))!==expected;++n)await wait(20);
  assert.equal(Number(await evaluate(`document.getElementById(${JSON.stringify(id)}).value`)),expected,'Focused learned slider follows hardware');
 }
 await call('Page.reload');await wait(100);await ready('editor reload');let s=await snapshot();assert.equal(s.tracks[0].waveformId,before.tracks[0].waveformId);
 assert.equal(s.tracks[0].drives.fuzz.values.mix,1,'Editor reopening sync');
 for(const [name]of specs){if(s.tracks[0].drives[name].enabled){await click(`#loop-0-${name}`);s=await until(s=>!s.tracks[0].drives[name].enabled,'Bypass '+name);}}
 await until(s=>s.outputPeak>.01,'Direct bypass remains audible');
 await click('#loop-0 .loop-track-delete');s=await until(s=>s.tracks[0].state==='empty','DEL');for(const [name]of specs)assert(!s.tracks[0].drives[name].enabled);
 assert.equal(errors.length,0,JSON.stringify(errors));console.log(`PASS actual T/O/F UI ${process.argv[2]}: pointers, presets, sliders, failed-send recovery, 14 MIDI controls, reload/bypass/DEL and nonzero PCM`);socket.close();
})().catch(e=>{console.error(e);console.error('Browser lifecycle at failure: '+JSON.stringify(browserEvents));socket?.close();process.exitCode=1;});
