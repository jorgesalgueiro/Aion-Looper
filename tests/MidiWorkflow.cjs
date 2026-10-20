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
 const click=async(selector,button='left')=>{const r=await evaluate(`(()=>{const e=document.querySelector(${JSON.stringify(selector)});if(e.disabled)throw Error('Disabled MIDI control');e.scrollIntoView({block:'center',behavior:'instant'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2};})()`);const down=call('Input.dispatchMouseEvent',{type:'mousePressed',...r,button,clickCount:1});await delay(20);const up=call('Input.dispatchMouseEvent',{type:'mouseReleased',...r,button,clickCount:1});await Promise.all([down,up]);};
 const inject=async messages=>{const id=++injected,tmp=path.join(directory,'midi-input.tmp');fs.writeFileSync(tmp,JSON.stringify({id,messages}));fs.renameSync(tmp,path.join(directory,'midi-input.json'));for(let n=0;n<150;++n){try{if(Number(fs.readFileSync(path.join(directory,'midi-ack'),'utf8'))===id)return;}catch{}await delay(20);}assert.fail('Synthetic MidiBuffer injection timeout');};
 await call('Runtime.enable');await call('Emulation.setFocusEmulationEnabled',{enabled:true});
 for(let n=0;n<100;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&!!document.getElementById('map-midi-19')`))break;await delay(100);}
 let s=await wait(s=>s.audioReady,'audio');for(const [id,value] of [['syncMode',0],['masterGain',0],['autoPlayAfterRecord',1]])assert((await rpc({operation:'setParameter',id,value})).ok);
 assert((await rpc({operation:'loop',action:'clearAll',track:0,generation:s.generation,expiresAt:Date.now()+3000,value:0})).ok);await wait(s=>s.tracks.every(t=>t.state==='empty'),'clear');
 if(s.midi.enabled)assert((await rpc({operation:'midiConfig',action:'enable',enabled:false})).ok);await wait(s=>!s.midi.enabled,'disabled start');await delay(150);
 await click('#enableMidiBtn');await wait(s=>s.midi.enabled,'native MIDI enabled');
 await inject([[144,36,100,3,17],[128,36,0,3,18]]);await wait(s=>s.tracks[0].state==='recording','default note record');await delay(250);await inject([[144,36,100,3,0],[144,36,0,3,1]]);s=await wait(s=>s.tracks[0].state==='playing'&&s.tracks[0].waveformId,'default note finish');const length=s.tracks[0].length;
 await click('#map-midi-0');await wait(s=>s.midi.learnTarget===0,'learn loop one');await inject([[192,0,0,2,0]]);s=await wait(s=>s.midi.bindings[0]===3&&s.midi.learnTarget===-1,'learn FCB PC zero');assert.equal(s.tracks[0].state,'playing','Learn consumes incoming command');
 await inject([[192,0,0,2,0]]);await wait(s=>s.tracks[0].state==='stopped','PC stops');await delay(4200);s=await snapshot();assert.equal(s.tracks[0].length,length,'Program Change never creates a delete hold');
 await inject([[192,0,0,2,0]]);await wait(s=>s.tracks[0].state==='playing','PC plays');await inject([[144,36,100,3,0],[128,36,0,3,1]]);s=await snapshot();assert.equal(s.tracks[0].state,'playing','Replaced default note is unmapped');
 await click('#map-midi-19');await wait(s=>s.midi.learnTarget===19,'learn loop twenty');await inject([[191,21,127,3,0]]);await wait(s=>s.midi.bindings[19]===(2|(21<<2)),'CC learned on channel 16');
 await click('#map-midi-2');await wait(s=>s.midi.learnTarget===2,'duplicate learn');await inject([[192,0,0,2,0]]);s=await wait(s=>s.midi.learnTarget===-1&&s.midi.status.includes('already assigned'),'duplicate rejected');assert.equal(s.midi.bindings[2],1|(38<<2));
 await click('#map-midi-19','right');await wait(s=>s.midi.bindings[19]===0,'right-click unmaps');
 await click('#map-midi-18');await wait(s=>s.midi.learnTarget===18,'learn before reload');await call('Page.reload');await wait(s=>s.midi.learnTarget===-1&&s.midi.bindings[0]===3,'native reload cancels learn, retains mapping');await delay(150);
 await click('#saveMappingsBtn');await wait(s=>s.midi.status.includes('saved'),'native mappings save');
 await click('#enableMidiBtn');await wait(s=>!s.midi.enabled,'disable');await inject([[192,0,0,2,0]]);s=await snapshot();assert.equal(s.tracks[0].state,'playing','Disabled MIDI leaves audio unchanged');
 assert.deepEqual(errors,[]);assert(!(await evaluate(`!!window.state?.audioContext`)));console.log(`PASS: original MIDI table, raw MidiBuffer note/PC0/CC learning, real mouse clear/save/reload/disable and no PC delete hold at ${process.argv[2]} samples`);ws.close();
})().catch(e=>{console.error(e);socket?.close();process.exitCode=1;});
