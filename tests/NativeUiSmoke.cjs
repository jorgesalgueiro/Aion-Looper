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
 let info;
 for(let n=0;n<60;++n){info=await evaluate(`({cards:document.querySelectorAll('#loopContainer > div').length,status:document.getElementById('native-status')?.textContent,native:typeof window.AionNative?.request})`);if(info.cards===20)break;await delay(150);}
 assert.equal(info.cards,20,JSON.stringify(info));assert.equal(info.native,'function');
 const visible=await evaluate(`({overlay:!!document.getElementById('startOverlay'),text:document.getElementById('native-status').textContent,mainVisible:document.getElementById('native-master').getBoundingClientRect().height>0})`);
 assert(!visible.overlay&&visible.mainVisible,JSON.stringify(visible));
 assert(await evaluate(`!document.getElementById('loopContainer').textContent.includes('undefined')`),'Missing renderer field');assert(!visible.text.includes('\u00c3'),'Garbled native status text');
 const snapshot=await evaluate(`AionNative.request({operation:'snapshot'})`);assert(snapshot.ok&&snapshot.tracks.length===20);
 assert(snapshot.tracks.every(t=>t.state==='empty'),'Smoke test must not capture audio');
 const layout=await evaluate(`({extraPanel:!!document.getElementById('native-mode'),masterInOriginal:!!document.querySelector('#mod-master #native-master'),inputInOriginal:!!document.querySelector('#mod-input #native-monitor'),stopInOriginal:!!document.querySelector('#mod-menu #native-stop-all'),syncOff:document.querySelector('input[name=syncMode][value=off]').checked})`);
 assert(!layout.extraPanel&&layout.masterInOriginal&&layout.inputInOriginal&&layout.stopInOriginal&&layout.syncOff,JSON.stringify(layout));
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
   await evaluate(`__JUCE__.backend.emitByBackend=window.savedBackendEmit;delete window.savedBackendEmit`);
 }
 const invalid=await evaluate(`AionNative.request({operation:'setParameter',id:'masterGain',value:3})`);assert(!invalid.ok);
 const muted=await evaluate(`AionNative.request({operation:'setParameter',id:'masterGain',value:0})`);assert(muted.ok);
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
 const dom=await evaluate(`({handlers:[...document.querySelectorAll('#loopContainer *')].flatMap(e=>[...e.attributes].filter(a=>/^on/i.test(a.name))).length,unsupported:[...document.querySelectorAll('#loopContainer button')].filter(e=>e.textContent.trim()==='UNDO').every(e=>e.disabled),hasAudioContext:!!window.state?.audioContext,master:document.getElementById('native-master').value})`);
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
