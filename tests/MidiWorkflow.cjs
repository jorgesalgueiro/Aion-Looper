// Inject raw packets through the synthetic host's preallocated juce::MidiBuffer.
// Click the production original mapping UI; no product MIDI-injection RPC exists.
const assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path');
const directory=path.resolve(__dirname,`../build/windows/Release/wav-export-${process.argv[2]}`),delay=ms=>new Promise(r=>setTimeout(r,ms));let socket;
(async()=>{
 let target;for(let n=0;n<100;++n){try{target=(await(await fetch('http://127.0.0.1:9235/json')).json()).find(t=>t.type==='page'&&t.url.includes('juce.backend'));}catch{}if(target)break;await delay(150);}assert(target);
 const ws=socket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});let seq=0,injected=0;const pending=new Map(),errors=[];
 ws.addEventListener('message',e=>{const r=JSON.parse(e.data);if(r.method==='Runtime.exceptionThrown')errors.push(r.params.exceptionDetails);pending.get(r.id)?.(r);});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++seq,timer=setTimeout(()=>{pending.delete(id);reject(Error('CDP timeout '+method));},10000);pending.set(id,r=>{pending.delete(id);clearTimeout(timer);r.error?reject(Error(JSON.stringify(r.error))):resolve(r.result);});ws.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 const rpc=request=>evaluate(`AionNative.request(${JSON.stringify(request)})`),snapshot=()=>rpc({operation:'snapshot'});
 const wait=async(predicate,label)=>{let s;for(let n=0;n<180;++n){s=await snapshot();if(predicate(s))return s;await delay(40);}assert.fail(label+': '+JSON.stringify(s.midi));};
 const click=async(selector,button='left',modifiers=0)=>{const r=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});if(e.disabled)throw Error('Disabled MIDI control');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2};})()`);const down=call('Input.dispatchMouseEvent',{type:'mousePressed',...r,button,modifiers,clickCount:1});await delay(20);const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',...r,button,modifiers,clickCount:1});await Promise.all([down,up]);};
 const inject=async messages=>{const id=++injected,tmp=path.join(directory,'midi-input.tmp');fs.writeFileSync(tmp,JSON.stringify({id,messages}));fs.renameSync(tmp,path.join(directory,'midi-input.json'));for(let n=0;n<150;++n){try{if(Number(fs.readFileSync(path.join(directory,'midi-ack'),'utf8'))===id)return;}catch{}await delay(20);}assert.fail('Synthetic MidiBuffer injection timeout');};
 await call('Runtime.enable');await call('Emulation.setFocusEmulationEnabled',{enabled:true});
 for(let n=0;n<100;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&!!document.getElementById('map-midi-19')`))break;await delay(100);}
 let s=await wait(s=>s.audioReady,'audio');for(const [id,value] of [['syncMode',0],['masterGain',0],['autoPlayAfterRecord',1]])assert((await rpc({operation:'setParameter',id,value})).ok);
 assert((await rpc({operation:'loop',action:'clearAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);await wait(s=>s.tracks.every(t=>t.state==='empty'),'clear');
 if(s.midi.enabled)assert((await rpc({operation:'midiConfig',action:'enable',enabled:false})).ok);await wait(s=>!s.midi.enabled,'disabled start');await delay(150);
 await click('#enableMidiBtn');await wait(s=>s.midi.enabled,'native MIDI enabled');
 // Queue two actual pointer clicks together, before waiting on IPC/snapshots.
 const beforeRapidMidiRevision=(await snapshot()).midi.revision;
 const rapidMidi=await evaluate(`(()=>{const e=document.getElementById('enableMidiBtn');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2};})()`);
 await Promise.all(['mousePressed','mouseReleased','mousePressed','mouseReleased'].map(type=>call('Input.dispatchMouseEvent',{type,...rapidMidi,button:'left',clickCount:1})));
 await wait(s=>s.midi.enabled&&s.midi.revision>=beforeRapidMidiRevision+2,'rapid MIDI disable/enable retains both gestures');
 // A captured learn followed by same-block panic must never commit on a later callback.
 for(const panic of [[252,0,0,1,18],[176,120,0,3,18],[176,123,0,3,18]]){
  await click('#map-midi-17');await wait(s=>s.midi.learnTarget===17,'learn before same-block panic');await inject([[192,4,0,2,17],panic]);
  s=await wait(s=>s.midi.learnTarget===-1,'panic cancels captured learn');assert.equal(s.midi.bindings[17],0,'Panic invalidates unacknowledged mapping');await delay(120);s=await snapshot();assert.equal(s.midi.bindings[17],0,'Cancelled mapping never revives on later callbacks');
 }
 await click('#map-midi-17');await wait(s=>s.midi.learnTarget===17,'fresh learn after panic');await inject([[192,5,0,2,17]]);s=await wait(s=>s.midi.bindings[17]===(3|(5<<2))&&s.midi.learnTarget===-1,'fresh learn recovers after panic');assert.equal(s.tracks[17].state,'empty','Fresh learn consumes packet');await click('#map-midi-17','right');await wait(s=>s.midi.bindings[17]===0,'panic fixture cleanup');
 // Keep a real synthetic MidiBuffer note held across original mouse learn/Escape.
 await inject([[144,48,100,3,0],[128,48,0,3,1],[144,37,100,3,2]]);await wait(s=>s.midi.sustain&&s.tracks[1].state==='recording','held SUS recording');
 await click('#loop-vol-slider-19','right');s=await wait(s=>s.midi.learnTarget===52,'learn while holding record');assert.equal(s.tracks[1].state,'recording','Mouse learn preserves held recording');
 await inject([[176,16,0,3,17]]);s=await wait(s=>s.midi.bindings[52]===(2|(16<<2))&&s.midi.learnTarget===-1,'learn acknowledgement while recording');assert.equal(s.tracks[1].state,'recording','Learn acknowledgement preserves held recording');
 await inject([[128,37,0,3,17]]);await wait(s=>s.tracks[1].state==='playing'&&s.tracks[1].waveformId,'NoteOff finishes held recording');
 await inject([[144,37,100,3,17]]);await wait(s=>s.tracks[1].muted,'held SUS mute');
 await click('#loop-pan-slider-19','right');s=await wait(s=>s.midi.learnTarget===72,'learn while held mute');assert(s.tracks[1].muted,'Mouse learn preserves held mute');
 await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});s=await wait(s=>s.midi.learnTarget===-1,'Escape during held mute');assert(s.tracks[1].muted,'Escape preserves held mute');
 await inject([[128,37,0,3,17],[144,48,100,3,18],[128,48,0,3,19]]);await wait(s=>!s.tracks[1].muted&&!s.midi.sustain,'Owned NoteOff restores mute');
 s=await snapshot();assert((await rpc({operation:'loop',action:'clear',track:1,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);await wait(s=>s.tracks[1].state==='empty','held fixture cleanup');
 // A CC pedal release is both a learn candidate and the end of its existing hold.
 await click('#map-midi-18');await wait(s=>s.midi.learnTarget===18,'CC release fixture mapping');await inject([[176,22,127,3,17]]);await wait(s=>s.midi.bindings[18]===(2|(22<<2)),'CC button assigned');
 await inject([[144,48,100,3,0],[128,48,0,3,1],[176,22,127,3,17]]);await wait(s=>s.midi.sustain&&s.tracks[18].state==='recording','held CC SUS recording');
 await click('#loop-vol-slider-18','right');await wait(s=>s.midi.learnTarget===51,'slider learn during held CC');await inject([[176,22,0,3,17]]);
 s=await wait(s=>s.tracks[18].state==='playing'&&s.midi.learnTarget===-1&&s.midi.status.includes('already assigned'),'CC release finishes recording despite conflicting learn');assert.equal(s.midi.bindings[18],2|(22<<2));assert.equal(s.midi.bindings[51],0,'Release cannot steal its existing transport mapping');
 await inject([[176,22,127,3,17]]);await wait(s=>s.tracks[18].muted,'CC rearmed after rejected learn');await click('#loop-pan-slider-18','right');await wait(s=>s.midi.learnTarget===71,'pan learn while CC mute held');await inject([[176,22,0,3,17]]);await wait(s=>!s.tracks[18].muted&&s.midi.learnTarget===-1,'CC release restores mute despite conflicting learn');
 await inject([[144,48,100,3,0],[128,48,0,3,1]]);await wait(s=>!s.midi.sustain,'CC fixture exits SUS');await click('#map-midi-18','right');await wait(s=>s.midi.bindings[18]===0,'CC fixture unmap');
 s=await snapshot();assert((await rpc({operation:'loop',action:'clear',track:18,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);await wait(s=>s.tracks[18].state==='empty','CC release fixture cleanup');
 await inject([[144,36,100,3,17],[128,36,0,3,18]]);await wait(s=>s.tracks[0].state==='recording','default note record');await delay(250);await inject([[144,36,100,3,0],[144,36,0,3,1]]);s=await wait(s=>s.tracks[0].state==='playing'&&s.tracks[0].waveformId,'default note finish');const length=s.tracks[0].length;
 await click('#map-midi-0');await wait(s=>s.midi.learnTarget===0,'learn loop one');await inject([[192,0,0,2,0]]);s=await wait(s=>s.midi.bindings[0]===3&&s.midi.learnTarget===-1,'learn FCB PC zero');assert.equal(s.tracks[0].state,'playing','Learn consumes incoming command');
 await inject([[192,0,0,2,0],[192,0,0,2,1],[192,0,0,2,2]]);await wait(s=>s.tracks[0].state==='stopped','physical triple-PC burst stops once');await delay(4200);s=await snapshot();assert.equal(s.tracks[0].length,length,'Program Change never creates a delete hold');
 await inject([[192,0,0,2,0],[192,0,0,2,1],[192,0,0,2,2]]);await wait(s=>s.tracks[0].state==='playing','physical triple-PC burst plays once');await inject([[144,36,100,3,0],[128,36,0,3,1]]);s=await snapshot();assert.equal(s.tracks[0].state,'playing','Replaced default note is unmapped');
 await click('#map-midi-19');await wait(s=>s.midi.learnTarget===19,'learn loop twenty');await inject([[191,21,127,3,0]]);await wait(s=>s.midi.bindings[19]===(2|(21<<2)),'CC learned on channel 16');
 await click('#map-midi-2');await wait(s=>s.midi.learnTarget===2,'duplicate learn');await inject([[192,0,0,2,0]]);s=await wait(s=>s.midi.learnTarget===-1&&s.midi.status.includes('already assigned'),'duplicate rejected');assert.equal(s.midi.bindings[2],1|(38<<2));
 await click('#map-midi-19','right');await wait(s=>s.midi.bindings[19]===0,'right-click unmaps');
 await click('#map-midi-18');await wait(s=>s.midi.learnTarget===18,'learn before reload');await call('Page.reload');await wait(s=>s.midi.learnTarget===-1&&s.midi.bindings[0]===3,'native reload cancels learn, retains mapping');await delay(150);
 await click('#saveMappingsBtn');await wait(s=>s.midi.status.includes('saved'),'native mappings save');
 await click('#enableMidiBtn');await wait(s=>!s.midi.enabled,'disable');await inject([[192,0,0,2,0]]);s=await snapshot();assert.equal(s.tracks[0].state,'playing','Disabled MIDI leaves audio unchanged');
 // Original slider context-menu gesture: continuous expression and cancellation.
 await click('#loop-vol-slider-2','right');await wait(s=>s.midi.learnTarget===35,'same-block expression learn');await inject([[176,14,0,3,17],[176,14,127,3,18]]);
 s=await wait(s=>s.midi.bindings[35]===(2|(14<<2))&&s.tracks[2].volume===2,'learn burst reaches final value without another pedal movement');assert.equal(s.tracks[2].state,'empty','Expression learn burst never records');
 await click('#loop-vol-slider-0','right');await wait(s=>s.midi.learnTarget===33,'right-click volume learns CC');
 const previousVolume=(await snapshot()).tracks[0].volume;
 await inject([[191,11,0,3,17]]);s=await wait(s=>s.midi.bindings[33]===(2|(11<<2))&&s.midi.learnTarget===-1,'zero learns CC');assert.equal(s.tracks[0].volume,previousVolume,'Learn consumes zero');
 await inject([[176,11,127,3,17]]);s=await wait(s=>s.tracks[0].volume===2,'expression maximum');assert.equal(s.tracks[0].state,'playing');
 await inject([[176,11,64,3,0],[176,11,63,3,17]]);s=await wait(s=>Math.abs(s.tracks[0].volume-126/127)<1e-7,'positive expression values do not debounce');
 await inject([[176,11,0,3,0]]);await wait(s=>s.tracks[0].volume===0,'zero expression is silence');
 await click('#loop-pan-slider-0','right');await wait(s=>s.midi.learnTarget===53,'right-click pan learns');await inject([[176,12,0,3,0]]);await wait(s=>s.midi.bindings[53]===(2|(12<<2)),'pan CC learned');
 for(const value of [0,63,64,126,127]){await inject([[176,12,value,3,17]]);await wait(s=>s.tracks[0].pan===Math.floor(value/127*10),'original pan quantization');}
 await click('#map-midi-2');await wait(s=>s.midi.learnTarget===2,'button learn cannot steal expression');await inject([[176,11,100,3,0]]);s=await wait(s=>s.midi.learnTarget===-1&&s.midi.status.includes('already assigned'),'continuous/button duplicate rejects atomically');assert.equal(s.midi.bindings[33],2|(11<<2));
 await click('#loop-vol-slider-1','right');await wait(s=>s.midi.learnTarget===34,'CC timeout starts');await delay(10300);await wait(s=>s.midi.learnTarget===-1&&s.midi.status.includes('timed out'),'native ten second CC timeout');
 const beforeImmediateCancel=(await snapshot()).midi.revision;
 const immediateLearn=await evaluate(`(()=>{const e=document.getElementById('loop-pan-slider-3');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2};})()`);
 await Promise.all([call('Input.dispatchMouseEvent',{type:'mousePressed',...immediateLearn,button:'right',clickCount:1}),call('Input.dispatchMouseEvent',{type:'mouseReleased',...immediateLearn,button:'right',clickCount:1}),call('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27}),call('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27})]);
 await wait(s=>s.midi.revision>=beforeImmediateCancel+2&&s.midi.learnTarget===-1,'immediate Escape after right-click before a learn snapshot');await inject([[176,15,100,3,17]]);s=await snapshot();assert.equal(s.midi.bindings[56],0,'Cancelled learn cannot assign a later CC');
 await click('#loop-pan-slider-1','right');await wait(s=>s.midi.learnTarget===54,'Escape learn');await evaluate(`document.querySelector('#loop-0 .track-name').focus()`);await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Escape',code:'Escape',windowsVirtualKeyCode:27});await wait(s=>s.midi.learnTarget===-1,'Escape cancels slider learn while loop-name input owns focus');
 await click('#loop-pan-slider-1','right');await wait(s=>s.midi.learnTarget===54,'reload cancels slider learn');await call('Page.reload');await wait(s=>s.midi.learnTarget===-1&&s.midi.bindings[33]===(2|(11<<2))&&s.midi.bindings[53]===(2|(12<<2)),'reload retains expression, cancels learn');await delay(150);
 await click('#loop-pan-slider-0','right',8);await wait(s=>s.midi.bindings[53]===0&&s.midi.bindings[33]===(2|(11<<2)),'Shift right-click clears only the chosen expression');
 await click('#saveMappingsBtn');await click('#enableMidiBtn');await wait(s=>!s.midi.enabled,'disable expression');const savedVolume=(await snapshot()).tracks[0].volume;await inject([[176,11,127,3,0]]);assert.equal((await snapshot()).tracks[0].volume,savedVolume,'Disabled expression leaves value intact');
 assert.deepEqual(errors,[]);assert(!(await evaluate(`!!window.state?.audioContext`)));console.log(`PASS: original MIDI table, raw MidiBuffer note/PC0/CC learning, real mouse clear/save/reload/disable and no PC delete hold at ${process.argv[2]} samples`);ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
