const fs=require('node:fs');
const path=require('node:path');
const output=path.resolve(__dirname,'../build');
fs.mkdirSync(output,{recursive:true});
const assert=require('node:assert/strict');
let debugSocket;
const delay=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 let target;
 for(let n=0;n<80;++n){try{const list=await(await fetch('http://127.0.0.1:9234/json')).json();target=list.find(t=>t.type==='page'&&t.url.includes('juce.backend'));if(target)break;}catch{}await delay(250);}
 assert(target,'Native WebView2 page not found');
 const ws=debugSocket=new WebSocket(target.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});
 let seq=0;const requests=new Map();
 ws.addEventListener('message',event=>{const reply=JSON.parse(event.data);if(reply.id&&requests.has(reply.id)){requests.get(reply.id)(reply);requests.delete(reply.id);}});
 const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++seq;const timer=setTimeout(()=>{requests.delete(id);reject(Error('CDP timeout '+method));},10000);requests.set(id,reply=>{clearTimeout(timer);reply.error?reject(Error(JSON.stringify(reply.error))):resolve(reply.result);});ws.send(JSON.stringify({id,method,params}));});
 const evaluate=async expression=>{const r=await call('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r.exceptionDetails));return r.result.value;};
 await call('Runtime.enable');
 await call('Emulation.setFocusEmulationEnabled',{enabled:true});
 let info;
 for(let n=0;n<60;++n){info=await evaluate(`({cards:document.querySelectorAll('#loopContainer > div').length,status:document.getElementById('native-status')?.textContent,native:typeof window.AionNative?.request})`);if(info.cards===20)break;await delay(150);}
 assert.equal(info.cards,20,JSON.stringify(info));assert.equal(info.native,'function');
 const manual=await evaluate(`['en','es','pt'].map(language=>({language,text:document.getElementById('native-hold-manual-'+language)?.textContent,parent:document.getElementById('native-hold-manual-'+language)?.parentElement.id}))`);
 assert(manual.every(m=>m.text?.includes('4 s')&&m.text.includes('600 ms')&&m.text.includes('Undo/Redo')&&m.text.includes('FINISH')&&m.parent==='manual-'+m.language),'Quick-delete instructions must be in all three original manual panels: '+JSON.stringify(manual));
 const visible=await evaluate(`({overlay:!!document.getElementById('startOverlay'),text:document.getElementById('native-status').textContent,mainVisible:document.getElementById('native-master').getBoundingClientRect().height>0})`);
 assert(!visible.overlay&&visible.mainVisible,JSON.stringify(visible));
 assert(await evaluate(`!document.getElementById('loopContainer').textContent.includes('undefined')`),'Missing renderer field');assert(!visible.text.includes('\u00c3'),'Garbled native status text');
 const snapshot=await evaluate(`AionNative.request({operation:'snapshot'})`);assert(snapshot.ok&&snapshot.tracks.length===20);
 assert(snapshot.tracks.every(t=>t.state==='empty'),'Smoke test must not capture audio');
 assert(await evaluate(`document.getElementById('btnGlobalUndo').disabled&&document.getElementById('btnGlobalRedo').disabled&&[...document.querySelectorAll('#loopContainer button')].filter(b=>['UNDO','REDO'].includes(b.textContent.trim())).every(b=>b.disabled)`),'Empty history controls are disabled');
 const layout=await evaluate(`({extraPanel:!!document.getElementById('native-mode'),masterInOriginal:!!document.querySelector('#mod-master #native-master'),inputInOriginal:!!document.querySelector('#mod-input #native-monitor'),stopInOriginal:!!document.querySelector('#mod-menu #native-stop-all'),syncMode:document.querySelector('input[name=syncMode]:checked').value,syncEnabled:[...document.querySelectorAll('input[name=syncMode]')].every(e=>!e.disabled)})`);
 assert(!layout.extraPanel&&layout.masterInOriginal&&layout.inputInOriginal&&layout.stopInOriginal&&layout.syncEnabled&&layout.syncMode===['off','static','dynamic'][snapshot.syncMode],JSON.stringify(layout));
 // Exercise the original sync controls through APVTS and coherent audio snapshots.
 const timingBefore=await evaluate(`AionNative.request({operation:'snapshot'})`);
 for(const [id,value,property] of [['bpmInput',137.5,'syncBpm'],['numBars',1,'syncBars'],['timeSigNum',3,'syncNum'],['timeSigDen',8,'syncDen']]){
   await evaluate(`document.getElementById('${id}').value='${value}';document.getElementById('${id}').dispatchEvent(new Event('change'))`);
   let timed;for(let n=0;n<60;++n){timed=await evaluate(`AionNative.request({operation:'snapshot'})`);if(timed[property]===value)break;await delay(50);}assert.equal(timed[property],value,'Original timing field reaches native owner: '+id);
 }
 for(const mode of ['off','static','dynamic']){
   await evaluate(`document.querySelector('input[name=syncMode][value=${mode}]').click()`);
   let timed;for(let n=0;n<60;++n){timed=await evaluate(`AionNative.request({operation:'snapshot'})`);if(timed.syncMode===['off','static','dynamic'].indexOf(mode))break;await delay(50);}assert.equal(timed.syncMode,['off','static','dynamic'].indexOf(mode));await delay(100);assert.equal(await evaluate(`document.getElementById('markers').children.length`),mode==='off'?0:3,'Original shared-grid beat markers');if(mode==='off')assert.equal(await evaluate(`document.getElementById('playhead').style.left`),'0%','OFF keeps the original timeline playhead at zero');
 }
 for(const id of ['syncBpm','syncBars','syncNum','syncDen','syncMode','countInVisual'])assert((await evaluate(`AionNative.request({operation:'setParameter',id:'${id}',value:${JSON.stringify(timingBefore[id])=== 'true'?1:JSON.stringify(timingBefore[id])==='false'?0:JSON.stringify(timingBefore[id])}})`)).ok);
 assert(!(await evaluate(`AionNative.request({operation:'setParameter',id:'syncBpm',value:0})`)).ok);
 assert(!(await evaluate(`AionNative.request({operation:'setParameter',id:'syncBars',value:1.5})`)).ok);
 await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{key:'+',code:'Equal',bubbles:true}))`);
 assert(await evaluate(`document.body.classList.contains('mode-overdub')`));
 await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{key:'-',code:'Minus',bubbles:true}))`);
 assert(await evaluate(`document.body.classList.contains('mode-substitute')&&!document.body.classList.contains('mode-overdub')`));
 await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{key:'-',code:'Minus',bubbles:true}))`);
 await evaluate(`document.querySelectorAll('.main-tab-btn')[1].click()`);
 assert(await evaluate(`document.getElementById('ws-drone').classList.contains('active')`));
 await evaluate(`document.querySelectorAll('.main-tab-btn')[0].click();document.querySelectorAll('.manual-tab-btn')[2].click()`);
 assert(await evaluate(`document.getElementById('manual-pt').classList.contains('active')`));
 await evaluate(`document.querySelectorAll('.manual-tab-btn')[0].click();document.getElementById('autoPlayAfterRecord').click()`);
 assert.equal((await evaluate(`AionNative.request({operation:'snapshot'})`)).autoPlayAfterRecord,!snapshot.autoPlayAfterRecord);
 await evaluate(`document.getElementById('autoPlayAfterRecord').click()`);
 if(snapshot.audioReady){
   await evaluate(`document.getElementById('loop-vol-slider-0').value='0.6';document.getElementById('loop-vol-slider-0').dispatchEvent(new Event('input'));document.getElementById('loop-vol-slider-0').value='0.7';document.getElementById('loop-vol-slider-0').dispatchEvent(new Event('input'));`);
   let edited;
   for(let n=0;n<60;++n){edited=await evaluate(`AionNative.request({operation:'snapshot'})`);if(Math.abs(edited.tracks[0].volume-0.7)<0.001)break;await delay(50);}
   assert(Math.abs(edited.tracks[0].volume-0.7)<0.001,'Final coalesced slider value reaches audio owner');
   await evaluate(`document.getElementById('loop-vol-slider-0').value='1';document.getElementById('loop-vol-slider-0').dispatchEvent(new Event('input'));`);
   for(let n=0;n<60;++n){edited=await evaluate(`AionNative.request({operation:'snapshot'})`);if(edited.tracks[0].volume===1)break;await delay(50);}
   for(let n=0;n<60;++n){if(await evaluate(`![...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='SOLO').disabled`))break;await delay(50);}
   for(const [name,value,field] of [['pan',10,'pan'],['speed',0.5,'playbackRate'],['pan',5,'pan'],['speed',1,'playbackRate']]){
     await evaluate(`document.getElementById('loop-${name}-slider-0').value='${value}';document.getElementById('loop-${name}-slider-0').dispatchEvent(new Event('input'))`);
     let changed;
     for(let n=0;n<60;++n){changed=await evaluate(`AionNative.request({operation:'snapshot'})`);if(changed.tracks[0][field]===value)break;await delay(50);}
     assert.equal(changed.tracks[0][field],value,'Original '+name+' slider reaches audio owner');
   }
   for(let n=0;n<60;++n){if(await evaluate(`![...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='SOLO').disabled`))break;await delay(50);}
   await evaluate(`[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='SOLO').click()`);
   for(let n=0;n<60;++n){edited=await evaluate(`AionNative.request({operation:'snapshot'})`);if(edited.tracks[0].solo)break;await delay(50);}
   assert(edited.tracks[0].solo,'Original SOLO reaches native engine');
   await delay(100);
   for(let n=0;n<60;++n){if(await evaluate(`![...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='SOLO').disabled`))break;await delay(50);}
   await evaluate(`[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='SOLO').click()`);
   for(let n=0;n<60;++n){edited=await evaluate(`AionNative.request({operation:'snapshot'})`);if(!edited.tracks[0].solo)break;await delay(50);}
   assert(!edited.tracks[0].solo,'Original SOLO toggles off');
 }

 if(snapshot.audioReady){
   // Original Rvers checkbox changes requested direction even before a take exists.
   const beforeReverse=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await evaluate(`document.getElementById('loop-0-reverse').click()`);
   let reverseAck;
   for(let n=0;n<60;++n){reverseAck=await evaluate(`AionNative.request({operation:'snapshot'})`);if(reverseAck.lastRequestId>beforeReverse.lastRequestId&&reverseAck.tracks[0].reverse)break;await delay(50);}
   assert(reverseAck.tracks[0].reverse&&!reverseAck.rejected,'Original Rvers reaches native owner');
   for(let n=0;n<40;++n){if(await evaluate(`document.getElementById('loop-0-reverse').checked`))break;await delay(50);}
   assert(await evaluate(`document.getElementById('loop-0-reverse').checked`),'Reverse checkbox reflects acknowledged native state');
   await evaluate(`document.getElementById('loop-0-reverse').click()`);
   for(let n=0;n<60;++n){reverseAck=await evaluate(`AionNative.request({operation:'snapshot'})`);if(!reverseAck.tracks[0].reverse)break;await delay(50);}
   assert(!reverseAck.tracks[0].reverse,'Original Rvers resets to forward');
   const invalidReverse=await evaluate(`AionNative.request({operation:'loop',action:'reverse',track:0,value:0.5,generation:${snapshot.generation},expiresAt:Date.now()+3000})`);
   assert(!invalidReverse.ok,'Reverse IPC rejects nonboolean values');
   // Two same-turn clicks used to lose the second gesture during RPC acknowledgement.
   await delay(150);
   const beforeBurst=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await evaluate(`(()=>{const mute=[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='MUTE');mute.click();mute.click();})()`);
   let afterBurst;
   for(let n=0;n<80;++n){afterBurst=await evaluate(`AionNative.request({operation:'snapshot'})`);if(afterBurst.lastRequestId>=beforeBurst.lastRequestId+2&&!afterBurst.tracks[0].muted)break;await delay(50);}
   assert(afterBurst.lastRequestId>=beforeBurst.lastRequestId+2&&!afterBurst.tracks[0].muted,'Both rapid MUTE gestures reach native audio in order');
   for(let n=0;n<60;++n){if(await evaluate(`![...document.querySelectorAll('#mod-menu button')].find(b=>b.textContent.trim()==='DEL ALL').disabled`))break;await delay(50);}
   const before=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await evaluate(`window.savedConfirm=window.confirm;window.confirmCalled=0;window.confirm=()=>{window.confirmCalled++;return false;};[...document.querySelectorAll('#mod-menu button')].find(b=>b.textContent.trim()==='DEL ALL').click();window.confirm=window.savedConfirm;delete window.savedConfirm;`);
   assert.equal(await evaluate(`window.confirmCalled`),1,'Original confirmation must be shown before cancellation');
   await evaluate(`delete window.confirmCalled`);
   await delay(150);
   const cancelled=await evaluate(`AionNative.request({operation:'snapshot'})`);assert.equal(cancelled.lastRequestId,before.lastRequestId,'DEL ALL cancellation must not send a command');
 }
 // Presentation-only saving fixture: no microphone capture or test PCM enters the app.
 await evaluate(`window.savedBackendEmit=__JUCE__.backend.emitByBackend;__JUCE__.backend.emitByBackend=function(event,payload){if(event==='__juce__complete'){const message=JSON.parse(payload);if(message.result?.tracks)message.result.tracks[0].state='finalizing';payload=JSON.stringify(message);}return window.savedBackendEmit.call(this,event,payload);}`);
 for(let n=0;n<40;++n){if(await evaluate(`document.getElementById('loop-action-0').textContent==='SAVING'`))break;await delay(50);}
 const saving=await evaluate(`({caption:document.getElementById('loop-action-0').textContent,disabled:document.getElementById('loop-action-0').disabled,undefined:document.getElementById('loopContainer').textContent.includes('undefined'),fixture:window.fixtureInstalled})`);
 assert(saving.caption==='SAVING'&&saving.disabled&&!saving.undefined,JSON.stringify(saving));
 await evaluate(`__JUCE__.backend.emitByBackend=window.savedBackendEmit;delete window.savedBackendEmit`);
 for(let n=0;n<40;++n){if(await evaluate(`document.getElementById('loop-action-0').textContent==='RECORD'`))break;await delay(50);}
 assert(await evaluate(`[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='NORM').disabled`),'Empty loop normalization stays disabled');
 if(snapshot.audioReady){
   // Present a clip only in snapshots to verify binding/IPC; the real core remains empty.
   await evaluate(`window.savedBackendEmit=__JUCE__.backend.emitByBackend;__JUCE__.backend.emitByBackend=function(event,payload){if(event==='__juce__complete'){const message=JSON.parse(payload);if(message.result?.tracks){message.result.tracks[0].state='playing';message.result.tracks[0].length=512;}payload=JSON.stringify(message);}return window.savedBackendEmit.call(this,event,payload);}`);
   for(let n=0;n<40;++n){if(await evaluate(`![...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='NORM').disabled`))break;await delay(50);}
   const beforeNorm=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await evaluate(`[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='NORM').click()`);
   let normAck;
   for(let n=0;n<40;++n){normAck=await evaluate(`AionNative.request({operation:'snapshot'})`);if(normAck.lastRequestId>beforeNorm.lastRequestId)break;await delay(50);}
   assert(normAck.lastRequestId>beforeNorm.lastRequestId&&normAck.rejected,'NORM binding reaches audio owner, which rejects the fixture-only clip');
   assert.equal(typeof normAck.inputAvailable,'boolean','Native backend reports input routing availability');
   for(const gesture of [`[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='MULT').click()`,`document.dispatchEvent(new KeyboardEvent('keydown',{code:'Backslash',key:String.fromCharCode(92),bubbles:true}))`]){
     for(let n=0;n<40;++n){if(await evaluate(`![...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='MULT').disabled`))break;await delay(50);}
     const before=await evaluate(`AionNative.request({operation:'snapshot'})`);await evaluate(gesture);let ack;
     for(let n=0;n<40;++n){ack=await evaluate(`AionNative.request({operation:'snapshot'})`);if(ack.lastRequestId>before.lastRequestId)break;await delay(50);}
     assert(ack.lastRequestId>before.lastRequestId&&ack.rejected,'MULT button/shortcut reaches audio owner, which rejects a snapshot-only clip');
   }
   await evaluate(`__JUCE__.backend.emitByBackend=function(event,payload){if(event==='__juce__complete'){const message=JSON.parse(payload);if(message.result?.tracks){message.result.tracks[0].state='multiplying';message.result.tracks[0].length=512;}payload=JSON.stringify(message);}return window.savedBackendEmit.call(this,event,payload);}`);
   for(let n=0;n<40;++n){if(await evaluate(`document.getElementById('loop-action-0').textContent==='FINISH'&&document.body.classList.contains('is-multiplying')`))break;await delay(50);}
   assert(await evaluate(`document.getElementById('loop-action-0').textContent==='FINISH'&&!document.getElementById('loop-action-0').disabled&&document.getElementById('loop-state-symbol-0').textContent==='(M)'&&document.getElementById('loop-speed-slider-0').disabled&&[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='RTRG').disabled&&![...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='MULT').disabled`),'Original Multiply state/FINISH/speed presentation');

   await evaluate(`__JUCE__.backend.emitByBackend=window.savedBackendEmit;delete window.savedBackendEmit`);
 }
 if(snapshot.audioReady){
   // These snapshots verify presentation/routing only; no microphone recording.
   await evaluate(`window.savedBackendEmit=__JUCE__.backend.emitByBackend;window.transportFixture='armed';__JUCE__.backend.emitByBackend=function(event,payload){if(event==='__juce__complete'){const message=JSON.parse(payload);if(message.result?.tracks){Object.assign(message.result.tracks[0],{state:window.transportFixture,length:window.transportFixture==='armed'?0:512,scheduledFrame:message.result.syncFrame+message.result.sampleRate});}payload=JSON.stringify(message);}return window.savedBackendEmit.call(this,event,payload);}`);
   for(const state of ['armed','queued','stopping']){
     await evaluate(`window.transportFixture='${state}'`);
     const caption=state==='stopping'?'STOP NOW':'CANCEL';
     for(let n=0;n<40;++n){if(await evaluate(`document.getElementById('loop-action-0').textContent==='${caption}'`))break;await delay(50);}
     assert(await evaluate(`document.getElementById('loop-action-0').textContent==='${caption}'&&!document.getElementById('loop-action-0').disabled`),'Original '+state+' main control caption');
   }
   await evaluate(`window.transportFixture='armed';document.getElementById('loop-action-0').scrollIntoView({behavior:'instant',block:'center'})`);await delay(200);
   const rect=await evaluate(`(()=>{const r=document.getElementById('loop-action-0').getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`);
   await evaluate(`window.pointerTrace=[];window.tracePointer=e=>window.pointerTrace.push({type:e.type,target:e.target.id,id:e.pointerId,buttons:e.buttons,button:e.button,primary:e.isPrimary,at:performance.now()});window.oldPress=AionPerformance.prototype.press;AionPerformance.prototype.press=function(id,key){window.pointerTrace.push({type:'press',key,held:this.presses.has(key),state:this.actions.track(id)?.state});if(!window.testController){window.testController=this;window.oldAction=this.actions.action;window.oldHold=this.actions.hold;this.actions.action=id=>{window.pointerTrace.push({type:'action',state:this.actions.track(id)?.state});return window.oldAction(id);};this.actions.hold=(id,initialState,valid)=>{window.pointerTrace.push({type:'hold',state:this.actions.track(id)?.state,initialState});return window.oldHold(id,initialState,valid);};}return window.oldPress.call(this,id,key);};window.traceError=e=>window.pointerTrace.push({type:'error',message:e.message});window.addEventListener('error',window.traceError);for(const type of ['pointerdown','pointerup','pointercancel','lostpointercapture'])window.addEventListener(type,window.tracePointer,true);`);
   assert(await evaluate(`document.elementFromPoint(${rect.x},${rect.y})?.id==='loop-action-0'`),'Mouse cancel target is the visible main button');
   const beforeMouse=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await call('Input.dispatchMouseEvent',{type:'mouseMoved',x:rect.x,y:rect.y});
   await call('Input.dispatchMouseEvent',{type:'mousePressed',x:rect.x,y:rect.y,button:'left',clickCount:1});
   await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:rect.x,y:rect.y,button:'left',clickCount:1});
   let afterMouse;for(let n=0;n<80;++n){afterMouse=await evaluate(`AionNative.request({operation:'snapshot'})`);if(afterMouse.lastRequestId>beforeMouse.lastRequestId)break;await delay(50);}
   assert.equal(afterMouse.lastRequestId,beforeMouse.lastRequestId+1,'Mouse down/up/click sends exactly one cancel command: '+JSON.stringify(await evaluate(`({events:window.pointerTrace,status:document.getElementById('native-status').textContent,disabled:document.getElementById('loop-action-0').disabled,caption:document.getElementById('loop-action-0').textContent})`)));
   const beforeKey=afterMouse;
   await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{code:'Digit1',key:'1',bubbles:true}));document.dispatchEvent(new KeyboardEvent('keyup',{code:'Digit1',key:'1',bubbles:true}));`);
   let afterKey;for(let n=0;n<80;++n){afterKey=await evaluate(`AionNative.request({operation:'snapshot'})`);if(afterKey.lastRequestId>beforeKey.lastRequestId)break;await delay(50);}assert.equal(afterKey.lastRequestId,beforeKey.lastRequestId+1,'Keyboard and mouse use the same cancel route');
   await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{code:'Digit1',key:'1',bubbles:true}));document.dispatchEvent(new KeyboardEvent('keyup',{code:'Digit1',key:'1',bubbles:true}));`);
   let afterRepeat;for(let n=0;n<80;++n){afterRepeat=await evaluate(`AionNative.request({operation:'snapshot'})`);if(afterRepeat.lastRequestId>afterKey.lastRequestId)break;await delay(50);}assert.equal(afterRepeat.lastRequestId,afterKey.lastRequestId+1,'A released number key can be pressed again; no stranded key state');
   await evaluate(`window.transportFixture='playing'`);
   for(let n=0;n<40;++n){if(await evaluate(`document.getElementById('loop-action-0').textContent==='STOP'`))break;await delay(50);}
   Object.assign(rect,await evaluate(`(()=>{const r=document.getElementById('loop-action-0').getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2};})()`));
   assert(await evaluate(`document.elementFromPoint(${rect.x},${rect.y})?.id==='loop-action-0'`),'Mouse hold target is the visible original main button');
   const beforeHold=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await call('Input.dispatchMouseEvent',{type:'mousePressed',x:rect.x,y:rect.y,button:'left',clickCount:1});await delay(800);
   await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:rect.x,y:rect.y,button:'left',clickCount:1});await delay(250);
   let afterHold;for(let n=0;n<120;++n){afterHold=await evaluate(`AionNative.request({operation:'snapshot'})`);if(afterHold.lastRequestId>=beforeHold.lastRequestId+2)break;await delay(50);}assert.equal(afterHold.lastRequestId,beforeHold.lastRequestId+2,'Mouse hold sends short stop then immediate panic, with no release duplicate: '+JSON.stringify(await evaluate(`({events:window.pointerTrace,status:document.getElementById('native-status').textContent,disabled:document.getElementById('loop-action-0').disabled,caption:document.getElementById('loop-action-0').textContent})`)));
   await delay(200);assert.equal((await evaluate(`AionNative.request({operation:'snapshot'})`)).lastRequestId,afterHold.lastRequestId,'Release never duplicates the held action');
   const beforeKeyHold=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{code:'Digit1',key:'1',bubbles:true}));document.dispatchEvent(new KeyboardEvent('keydown',{code:'Digit1',key:'1',repeat:true,bubbles:true}));`);await delay(800);
   await evaluate(`document.dispatchEvent(new KeyboardEvent('keyup',{code:'Digit1',key:'1',bubbles:true}))`);
   let afterKeyHold;for(let n=0;n<120;++n){afterKeyHold=await evaluate(`AionNative.request({operation:'snapshot'})`);if(afterKeyHold.lastRequestId>=beforeKeyHold.lastRequestId+2)break;await delay(50);}assert.equal(afterKeyHold.lastRequestId,beforeKeyHold.lastRequestId+2,'Keyboard hold cuts now once and ignores auto-repeat');
   assert(await evaluate(`!window.pointerTrace.some(e=>e.type==='error')`),'Native press/release/hold produces no browser exceptions');
   // Inject only a generation notification, not an audio reset or fake PCM.
   const epochBefore=await evaluate(`window.testController.epoch`);
   await evaluate(`window.generationAction=window.testController.actions.action;window.testController.actions.action=()=>{};window.testController.press(0,'generation-check');window.generationHoldValid=window.testController.presses.get('generation-check').valid;window.testController.actions.action=window.generationAction;delete window.generationAction;window.generationEmit=__JUCE__.backend.emitByBackend;__JUCE__.backend.emitByBackend=function(event,payload){if(event==='__juce__complete'){const message=JSON.parse(payload);if(message.result?.tracks)message.result.generation++;payload=JSON.stringify(message);}return window.generationEmit.call(this,event,payload);}`);
   for(let n=0;n<60;++n){if(await evaluate(`window.testController.presses.size===0&&!window.generationHoldValid()`))break;await delay(50);}
   assert(await evaluate(`window.testController.epoch>${epochBefore}&&window.testController.presses.size===0&&!window.generationHoldValid()`),'Audio-generation notification cancels held timers and invalidates their queued token');
   await evaluate(`__JUCE__.backend.emitByBackend=window.generationEmit;delete window.generationEmit;delete window.generationHoldValid`);

   await evaluate(`for(const type of ['pointerdown','pointerup','pointercancel','lostpointercapture'])window.removeEventListener(type,window.tracePointer,true);AionPerformance.prototype.press=window.oldPress;if(window.testController){window.testController.actions.action=window.oldAction;window.testController.actions.hold=window.oldHold;}window.removeEventListener('error',window.traceError);delete window.oldPress;delete window.oldAction;delete window.oldHold;delete window.testController;delete window.traceError;delete window.tracePointer;delete window.pointerTrace`);
   await evaluate(`__JUCE__.backend.emitByBackend=window.savedBackendEmit;delete window.savedBackendEmit;delete window.transportFixture`);
 }
 if(snapshot.audioReady){
   await delay(150);
   const beforeEmptyHistory=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',ctrlKey:true,bubbles:true}));document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'Z',ctrlKey:true,shiftKey:true,bubbles:true}));`);
   await delay(350);
   const afterEmptyHistory=await evaluate(`AionNative.request({operation:'snapshot'})`);
   assert.equal(afterEmptyHistory.lastRequestId,beforeEmptyHistory.lastRequestId,'Empty Ctrl+Z/Ctrl+Shift+Z are silent no-ops like the original JS');
   // History presentation/routing only: no PCM is inserted or microphone captured.
   await evaluate(`window.savedBackendEmit=__JUCE__.backend.emitByBackend;__JUCE__.backend.emitByBackend=function(event,payload){if(event==='__juce__complete'){const message=JSON.parse(payload);if(message.result?.tracks){Object.assign(message.result.tracks[0],{state:'playing',length:512,undoCount:1,redoCount:1});message.result.nextUndo=0;message.result.nextRedo=0;}payload=JSON.stringify(message);}return window.savedBackendEmit.call(this,event,payload);}`);
   for(let n=0;n<40;++n){if(await evaluate(`!document.getElementById('btnGlobalUndo').disabled&&document.getElementById('btnGlobalUndo').textContent==='UNDO L1'`))break;await delay(50);}
   assert(await evaluate(`!document.getElementById('btnGlobalUndo').disabled&&!document.getElementById('btnGlobalRedo').disabled&&document.getElementById('btnGlobalRedo').textContent==='REDO L1'`),'Original global history labels and availability');
   const gestures=[`[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='UNDO').click()`,`[...document.querySelectorAll('#loop-0 button')].find(b=>b.textContent==='REDO').click()`,`document.getElementById('btnGlobalUndo').click()`,`document.getElementById('btnGlobalRedo').click()`,`document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',ctrlKey:true,bubbles:true}))`,`document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'Z',metaKey:true,shiftKey:true,bubbles:true}))`];
   for(const gesture of gestures){const before=await evaluate(`AionNative.request({operation:'snapshot'})`);await evaluate(gesture);let ack;for(let n=0;n<60;++n){ack=await evaluate(`AionNative.request({operation:'snapshot'})`);if(ack.lastRequestId>before.lastRequestId)break;await delay(50);}assert(ack.lastRequestId>before.lastRequestId&&ack.rejected,'History gesture reaches real owner, which rejects snapshot-only history');}
   await evaluate(`__JUCE__.backend.emitByBackend=window.savedBackendEmit;delete window.savedBackendEmit`);
   for(let n=0;n<40;++n){if(await evaluate(`document.getElementById('btnGlobalUndo').disabled&&document.getElementById('btnGlobalRedo').disabled`))break;await delay(50);}
   assert(await evaluate(`document.getElementById('btnGlobalUndo').disabled&&document.getElementById('btnGlobalRedo').disabled`),'History fixture restores empty native state');
   const beforeRepeat=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await evaluate(`window.historyFixtureBase=${beforeRepeat.lastRequestId};window.savedBackendEmit=__JUCE__.backend.emitByBackend;__JUCE__.backend.emitByBackend=function(event,payload){if(event==='__juce__complete'){const message=JSON.parse(payload);if(message.result?.tracks){const done=message.result.lastRequestId>window.historyFixtureBase;Object.assign(message.result.tracks[0],{state:'playing',length:512,undoCount:done?0:1,redoCount:done?1:0});message.result.nextUndo=done?-1:0;message.result.nextRedo=done?0:-1;if(done)message.result.rejected=false;}payload=JSON.stringify(message);}return window.savedBackendEmit.call(this,event,payload);}`);
   for(let n=0;n<40;++n){if(await evaluate(`!document.getElementById('btnGlobalUndo').disabled`))break;await delay(50);}
   await evaluate(`document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',ctrlKey:true,bubbles:true}));document.dispatchEvent(new KeyboardEvent('keydown',{code:'KeyZ',key:'z',ctrlKey:true,bubbles:true}));`);
   for(let n=0;n<40;++n){if((await evaluate(`AionNative.request({operation:'snapshot'})`)).lastRequestId>beforeRepeat.lastRequestId)break;await delay(50);}
   await delay(300);
   assert.equal((await evaluate(`AionNative.request({operation:'snapshot'})`)).lastRequestId,beforeRepeat.lastRequestId+1,'Repeated Undo stops at the newly acknowledged empty depth');
   await evaluate(`__JUCE__.backend.emitByBackend=window.savedBackendEmit;delete window.savedBackendEmit;delete window.historyFixtureBase`);

 }
 const invalid=await evaluate(`AionNative.request({operation:'setParameter',id:'masterGain',value:3})`);assert(!invalid.ok);
 const muted=await evaluate(`AionNative.request({operation:'setParameter',id:'masterGain',value:0})`);assert(muted.ok);
 if(!snapshot.inputAvailable){const missing=await evaluate(`AionNative.request({operation:'loop',action:'record',track:0,generation:${snapshot.generation||1},expiresAt:Date.now()+3000})`);assert(!missing.ok&&missing.error.includes('No native input'),'Missing input fails clearly without silently recording zeros');}
 const expired=await evaluate(`AionNative.request({operation:'loop',action:'record',track:0,generation:${snapshot.generation||1},expiresAt:Date.now()-1000})`);assert(!expired.ok&&expired.error.includes('expired'));
 if(snapshot.audioReady){
   const queued=await evaluate(`AionNative.request({operation:'loop',action:'play',track:0,generation:${snapshot.generation},expiresAt:Date.now()+3000})`);assert(queued.ok&&queued.queued);
   let ack;
   for(let n=0;n<40;++n){ack=await evaluate(`AionNative.request({operation:'snapshot'})`);if(ack.lastRequestId===queued.requestId)break;await delay(50);}
   assert.equal(ack.lastRequestId,queued.requestId);assert(ack.rejected,'Empty-track play must be rejected by the audio owner');
 }
 if(snapshot.audioReady) {
   const beforeStop=await evaluate(`AionNative.request({operation:'snapshot'})`);
   await evaluate(`document.getElementById('native-stop-all').click()`);
   let stopped;
   for(let n=0;n<60;++n){stopped=await evaluate(`AionNative.request({operation:'snapshot'})`);if(stopped.lastRequestId>beforeStop.lastRequestId&&!stopped.rejected)break;await delay(50);}
   assert(stopped.lastRequestId>beforeStop.lastRequestId&&!stopped.rejected&&stopped.tracks.every(t=>t.state==='empty'),JSON.stringify(stopped));
 }
 // Labels use the original input and stay in C++ across a browser/editor reload.
 const label='Voz 1 + baixo Á 😀 <tag> " &';
 await evaluate(`(()=>{const e=document.querySelector('#loop-0 .track-name');if(e.disabled)throw Error('Name field is disabled');e.focus();})()`);
 const beforeName=await evaluate(`AionNative.request({operation:'snapshot'})`);
 await call('Input.insertText',{text:label});await delay(200);
 await call('Input.dispatchKeyEvent',{type:'keyDown',key:'1',code:'Digit1',windowsVirtualKeyCode:49});
 await call('Input.dispatchKeyEvent',{type:'keyUp',key:'1',code:'Digit1',windowsVirtualKeyCode:49});
 let named=await evaluate(`AionNative.request({operation:'snapshot'})`);
 assert.equal(named.tracks[0].name,label,'Original name input reaches C++');
 assert.equal(named.lastRequestId,beforeName.lastRequestId,'Typing names never sends audio transport commands');
 await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
 await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
 assert(await evaluate(`document.activeElement!==document.querySelector('#loop-0 .track-name')`),'Enter blurs the original input');
 for(const request of [{track:-1,name:'bad'},{track:20,name:'bad'},{track:0.5,name:'bad'},{track:0,name:7},{track:0,name:'a'.repeat(4097)},{track:0,name:'😀'.repeat(1025)}]) {
  const bad=await evaluate(`AionNative.request(${JSON.stringify({operation:'renameLoop',...request})})`);assert(!bad.ok,'Invalid label accepted');
 }
 assert.equal((await evaluate(`AionNative.request({operation:'snapshot'})`)).tracks[0].name,label,'Invalid requests preserve the prior name');
 for(const boundary of ['a'.repeat(4096),'😀'.repeat(1024)]) {
  assert((await evaluate(`AionNative.request(${JSON.stringify({operation:'renameLoop',track:0,name:boundary})})`)).ok,'Exact UTF-8 limit is accepted');
  assert.equal((await evaluate(`AionNative.request({operation:'snapshot'})`)).tracks[0].name,boundary);
 }
 assert((await evaluate(`AionNative.request(${JSON.stringify({operation:'renameLoop',track:0,name:label})})`)).ok);
 for(let id=1;id<20;++id)assert((await evaluate(`AionNative.request({operation:'renameLoop',track:${id},name:'Loop label ${id}'})`)).ok);
 // Freeze dispatch only: DEL followed immediately by typing must keep the new name.
 const beforeNameClear=await evaluate(`AionNative.request({operation:'snapshot'})`);
 await evaluate(`window.namePump=AionCommandQueue.prototype.pump;AionCommandQueue.prototype.pump=function(){window.nameQueue=this;return window.namePump.call(this);};`);
 for(let n=0;n<40;++n){if(await evaluate(`!!window.nameQueue`))break;await delay(25);}
 await evaluate(`AionCommandQueue.prototype.pump=window.namePump;delete window.namePump;window.nameReady=window.nameQueue.ready;window.nameQueue.ready=()=>false;document.querySelector('#loop-19 .loop-track-delete').click();const field=document.querySelector('#loop-19 .track-name');field.focus();field.value='After DEL Á';field.dispatchEvent(new Event('input',{bubbles:true}));`);
 await delay(100);
 await evaluate(`window.nameQueue.ready=window.nameReady;delete window.nameReady;window.nameQueue.pump();`);
 for(let n=0;n<60;++n){named=await evaluate(`AionNative.request({operation:'snapshot'})`);if(named.lastRequestId>beforeNameClear.lastRequestId&&named.tracks[19].name==='After DEL Á')break;await delay(50);}
 assert(named.lastRequestId>beforeNameClear.lastRequestId,'Delayed DEL acknowledged by the audio owner');
 assert.equal(named.tracks[19].name,'After DEL Á','Typing after pending DEL survives its audio acknowledgement');
 assert((await evaluate(`AionNative.request({operation:'renameLoop',track:19,name:'Loop label 19'})`)).ok);
 // A controlled unrelated wait must not stall name editing or be discarded.
 const beforeNameWait=await evaluate(`AionNative.request({operation:'snapshot'})`);
 await evaluate(`(()=>{window.nameQueue.enqueue(()=>AionCommandQueue.wait,{track:18});const field=document.querySelector('#loop-17 .track-name');field.focus();field.value='Name while saving Á';field.dispatchEvent(new Event('input',{bubbles:true}));})()`);
 for(let n=0;n<24;++n){named=await evaluate(`AionNative.request({operation:'snapshot'})`);if(named.tracks[17].name==='Name while saving Á')break;await delay(50);}
 assert.equal(named.tracks[17].name,'Name while saving Á','Names bypass another track finalization wait');
 assert.equal(named.lastRequestId,beforeNameWait.lastRequestId,'Metadata never dispatches a transport action');
 assert(await evaluate(`window.nameQueue.items.some(item=>item.track===18)`),'Naming preserves another track deferred action');
 await evaluate(`window.nameQueue.reset();`);
 // Panic still goes first, but must not erase typed text waiting for dispatch.
 const beforeNamePanic=await evaluate(`AionNative.request({operation:'snapshot'})`);
 await evaluate(`(()=>{window.nameReady=window.nameQueue.ready;window.nameQueue.ready=()=>false;const field=document.querySelector('#loop-17 .track-name');field.value='Name survives STOP ALL Á';field.dispatchEvent(new Event('input',{bubbles:true}));document.getElementById('native-stop-all').click();window.nameQueue.ready=window.nameReady;delete window.nameReady;window.nameQueue.pump();})()`);
 for(let n=0;n<60;++n){named=await evaluate(`AionNative.request({operation:'snapshot'})`);if(named.lastRequestId>beforeNamePanic.lastRequestId&&named.tracks[17].name==='Name survives STOP ALL Á')break;await delay(50);}
 assert.equal(named.lastRequestId,beforeNamePanic.lastRequestId+1,'Only STOP ALL dispatches to the audio owner');
 assert.equal(named.tracks[17].name,'Name survives STOP ALL Á','STOP ALL preserves pending label edits');
 assert((await evaluate(`AionNative.request({operation:'renameLoop',track:17,name:'Loop label 17'})`)).ok);
 await call('Page.reload');
 for(let n=0;n<80;++n){if(await evaluate(`typeof window.AionNative?.request==='function'&&document.querySelectorAll('#loopContainer > div').length===20`))break;await delay(100);}
 await delay(200);
 assert.equal(await evaluate(`document.querySelector('#loop-0 .track-name').value`),label,'Reload retains names from native session and escapes markup');
 assert.equal(await evaluate(`document.querySelectorAll('#loopContainer tag').length`),0,'Name is text, never injected markup');
 named=await evaluate(`AionNative.request({operation:'snapshot'})`);
 for(let id=1;id<20;++id)assert.equal(named.tracks[id].name,'Loop label '+id,'Independent native labels');
 // Reload constructed a new scheduler; test the actual DEL ALL button as well.
 await evaluate(`window.namePump=AionCommandQueue.prototype.pump;AionCommandQueue.prototype.pump=function(){window.nameQueue=this;return window.namePump.call(this);};`);
 for(let n=0;n<40;++n){if(await evaluate(`!!window.nameQueue`))break;await delay(25);}
 const beforeGlobalNameClear=await evaluate(`AionNative.request({operation:'snapshot'})`);
 await evaluate(`(()=>{AionCommandQueue.prototype.pump=window.namePump;delete window.namePump;window.nameReady=window.nameQueue.ready;window.nameQueue.ready=()=>false;const confirm=window.confirm;window.confirm=()=>true;try{document.querySelector('#mod-menu [data-i18n="CLEAR_ALL"]').click();}finally{window.confirm=confirm;}const field=document.querySelector('#loop-0 .track-name');field.focus();field.value='Name after DEL ALL Á';field.dispatchEvent(new Event('input',{bubbles:true}));window.nameQueue.ready=window.nameReady;delete window.nameReady;window.nameQueue.pump();})()`);
 for(let n=0;n<60;++n){named=await evaluate(`AionNative.request({operation:'snapshot'})`);if(named.lastRequestId>beforeGlobalNameClear.lastRequestId&&named.tracks[0].name==='Name after DEL ALL Á')break;await delay(50);}
 assert.equal(named.lastRequestId,beforeGlobalNameClear.lastRequestId+1,'DEL ALL acknowledged once');
 assert.equal(named.tracks[0].name,'Name after DEL ALL Á','Name typed after DEL ALL waits for its acknowledgement');
 assert(named.tracks.slice(1).every(t=>t.name===''),'DEL ALL clears other names');
 for(let id=0;id<20;++id)assert((await evaluate(`AionNative.request({operation:'renameLoop',track:${id},name:''})`)).ok);
 await delay(200);
 const dom=await evaluate(`({handlers:[...document.querySelectorAll('#loopContainer *')].flatMap(e=>[...e.attributes].filter(a=>/^on/i.test(a.name))).length,unsupported:[...document.querySelectorAll('#loopContainer button')].filter(e=>e.textContent.trim()==='SAVE').every(e=>e.disabled),hasAudioContext:!!window.state?.audioContext,master:document.getElementById('native-master').value})`);
 assert.equal(dom.handlers,0);assert(dom.unsupported&&!dom.hasAudioContext);assert.equal(Number(dom.master),0);
 await evaluate(`document.documentElement.style.scrollBehavior='auto';window.scrollTo({top:0,behavior:'instant'})`);
 const screenshot=await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});fs.writeFileSync(path.join(output,'native-looper-top.png'),Buffer.from(screenshot.data,'base64'));
 await evaluate(`document.getElementById('loopContainer').scrollIntoView({behavior:'instant',block:'start'})`);await delay(150);
 const hit=await evaluate(`(()=>{const b=document.getElementById('loop-action-0'),r=b.getBoundingClientRect();return {visible:r.top>=0&&r.bottom<=innerHeight,hit:document.elementFromPoint(r.x+r.width/2,r.y+r.height/2)===b,disabled:b.disabled,rect:{x:r.x,y:r.y,w:r.width,h:r.height},under:document.elementFromPoint(r.x+r.width/2,r.y+r.height/2)?.outerHTML.slice(0,500)};})()`);
 assert(hit.visible&&hit.hit,JSON.stringify(hit));if(snapshot.audioReady)assert(!hit.disabled,'Record control is disabled');
 const loops=await call('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});fs.writeFileSync(path.join(output,'native-looper-cards.png'),Buffer.from(loops.data,'base64'));
 console.log('PASS: native WebView2, 20 original loop cards, IPC snapshots/parameter validation, expired-command rejection, '+(snapshot.audioReady?'audio-owner acknowledgements':'audio inactive')+', disabled unported controls, no legacy audio context');
 console.log(JSON.stringify({frame:snapshot.frame,sampleRate:snapshot.sampleRate,dom,info}));
 ws.close();
})().catch(e=>{console.error(e);debugSocket?.close();process.exitCode=1;});
