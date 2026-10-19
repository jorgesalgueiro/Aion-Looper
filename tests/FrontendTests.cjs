const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const root=path.resolve(__dirname,'..');
const original=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
const adapter=fs.readFileSync(path.join(root,'resources/native/loop-view.js'),'utf8');
for(const name of ['createEffectToggleHTML','generateEffectsControlsHTML','getLoopAction','generateLoopHTML']) {
    const start=original.indexOf('    static '+name+'('),end=original.indexOf('\n    }',start)+6;
    assert(start>=0 && adapter.includes(original.slice(start,end)), 'original markup method changed: '+name);
}
const scales=fs.readFileSync(path.join(root,'resources/ui/Modules/scales.js'),'utf8').replace(/\r\n/g,'\n');
const markerStart=scales.indexOf('    static renderTimelineMarkers() {'),markerEnd=scales.indexOf('\n    }',markerStart)+6;
assert(adapter.includes(scales.slice(markerStart,markerEnd).replace(/[ \t]+$/gm,'')),'Original sync timeline marker method changed');
const context={window:{}};vm.runInNewContext(adapter,context);
const snapshot={sampleRate:48000,tracks:Array.from({length:20},()=>({state:'empty',length:0,volume:1,feedback:0.8,muted:false}))};
let cards=context.window.AionLoopMarkup(snapshot,'normal');
assert.equal(cards.length,20);
assert(!cards[0].includes('undefined'),'Native view model must fill original renderer fields');
assert(cards[0].includes('Loop 1: RECORD'));
snapshot.tracks[0]={...snapshot.tracks[0],state:'playing',length:48000};
cards=context.window.AionLoopMarkup(snapshot,'overdub');assert(cards[0].includes('Loop 1: OVERDUB'));
cards=context.window.AionLoopMarkup(snapshot,'substitute');assert(cards[0].includes('Loop 1: REPLACE'));
snapshot.tracks[0].state='recording';cards=context.window.AionLoopMarkup(snapshot,'normal');assert(cards[0].includes('Loop 1: FINISH'));
for(const file of ['bridge.js','looper.js'])new vm.Script(fs.readFileSync(path.join(root,'resources/native',file),'utf8'),{filename:file});
console.log('PASS: original loop markup retained, 20 cards, record/finish/overdub/substitute presentation, native script syntax');

const callbacks=new Map(),events=[];
const keyboardContext={window:{},setTimeout,clearTimeout};
vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),keyboardContext);
let timerId=0;
const keyboard=new keyboardContext.window.AionPerformance({track:()=>({}),action:i=>events.push(['action',i]),hold:i=>events.push(['hold',i]),mode:m=>events.push(['mode',m]),stop:()=>events.push(['stop']),retrigger:()=>events.push(['retrigger']),halfSpeed:()=>events.push(['half']),history:redo=>events.push(['history',redo]),multiply:()=>events.push(['multiply'])},{set:fn=>{callbacks.set(++timerId,fn);return timerId;},clear:id=>callbacks.delete(id)});
const key=(code,key,extra={})=>({code,key,target:{tagName:'BODY'},preventDefault(){},...extra});
keyboard.down(key('Digit1','1'));assert.deepEqual(events.pop(),['action',0]);
keyboard.down(key('Digit1','1',{repeat:true}));assert.equal(events.length,0);
keyboard.up(key('Digit1','!'));assert.equal(callbacks.size,0);
keyboard.down(key('Digit0',')',{shiftKey:true}));assert.deepEqual(events.pop(),['action',19]);
[...callbacks.values()][0]();assert.deepEqual(events.pop(),['hold',19]);
keyboard.up(key('Digit0','0'));assert.equal(callbacks.size,0);
for(const e of [key('Equal','+'),key('Minus','-'),key('Space',' '),key('Slash','/'),key('KeyH','h')])keyboard.down(e);
assert.deepEqual(events.splice(0),[['mode','overdub'],['mode','substitute'],['stop'],['retrigger'],['half']]);
keyboard.down(key('Digit2','2',{target:{tagName:'INPUT'}}));keyboard.down(key('Digit2','2',{ctrlKey:true}));assert.equal(events.length,0);
keyboard.down(key('Digit3','3'));keyboard.cancel();assert.equal(callbacks.size,0);
for(const extra of [{ctrlKey:true},{metaKey:true,shiftKey:true},{ctrlKey:true,repeat:true}])keyboard.down(key('KeyZ','z',extra));
assert.deepEqual(events.splice(0),[['action',2],['history',false],['history',true],['history',false]]);
keyboard.down(key('KeyZ','z',{ctrlKey:true,target:{tagName:'INPUT'}}));assert.equal(events.length,0);
keyboard.down(key('Backslash','\\'));assert.deepEqual(events.pop(),['multiply']);
keyboard.down(key('Backslash','\\',{target:{tagName:'INPUT'}}));assert.equal(events.length,0);
keyboard.press(4,'pointer:8');assert.deepEqual(events.pop(),['action',4]);
keyboard.press(4,'pointer:8');assert.equal(events.length,0,'Pointer duplicate is ignored');
keyboard.release('pointer:8');assert.equal(callbacks.size,0,'Pointer release cancels hold');
keyboard.press(4,'pointer:9');assert.deepEqual(events.pop(),['action',4]);[...callbacks.values()][0]();assert.deepEqual(events.pop(),['hold',4]);keyboard.cancel();assert.equal(callbacks.size,0);
let heldClock=0;const delayed=[];const delayedKeyboard=new keyboardContext.window.AionPerformance({track:()=>({}),action:i=>delayed.push(['action',i]),hold:i=>delayed.push(['hold',i])},{set:()=>1,clear(){},now:()=>heldClock});
delayedKeyboard.press(0,'pointer:1');heldClock=650;delayedKeyboard.release('pointer:1');delayedKeyboard.release('pointer:1');assert.deepEqual(delayed.splice(0),[['action',0],['hold',0]],'Delayed timer fallback performs exactly one real hold');
delayedKeyboard.press(1,'pointer:2');heldClock+=1000;delayedKeyboard.release('pointer:2',true);assert.deepEqual(delayed.splice(0),[['action',1]],'Pointer cancellation never invents a delayed hold');
delayedKeyboard.press(2,'Digit3');heldClock+=1000;delayedKeyboard.cancel();assert.deepEqual(delayed.splice(0),[['action',2]],'Blur/panic cancel never fires delayed holds');
const strictBrowserTimers={window:{},setTimeout:function(){'use strict';if(this?.set)throw new TypeError('Illegal invocation');return 1;},clearTimeout:function(){'use strict';if(this?.clear)throw new TypeError('Illegal invocation');}};
vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),strictBrowserTimers);
const browserKeys=new strictBrowserTimers.window.AionPerformance({track:()=>({}),action(){},hold(){}});browserKeys.press(0,'mouse');browserKeys.release('mouse');browserKeys.press(0,'mouse');browserKeys.cancel();assert.equal(browserKeys.presses.size,0,'Browser timer receiver and release cleanup');
// Cross-product guards against timer callbacks observing unrelated later states.
const phases=['empty','armed','recording','playing','stopped','overdubbing','substituting','finalizing','multiplying','queued','stopping'];
const holdPairs=new Map([
 ['stopped:playing','stopImmediate'],['stopped:stopping','stopImmediate'],
 ['playing:playing','stopImmediate'],['playing:stopping','stopImmediate'],
 ['stopping:playing','stopImmediate'],['stopping:stopping','stopImmediate']
]);
for(const start of phases)for(const end of phases)assert.equal(keyboardContext.window.AionPerformance.holdAction(start,end),holdPairs.get(`${start}:${end}`)||null,`Hold ${start} -> ${end}`);
let origin='recording',originNow=0;const heldOrigins=[],originTimers=new Map();let originTimer=0;
const originKeys=new keyboardContext.window.AionPerformance({track:()=>({state:origin}),action(){origin='playing';},hold:(id,start)=>heldOrigins.push(start)},{set:fn=>{originTimers.set(++originTimer,fn);return originTimer;},clear:id=>originTimers.delete(id),now:()=>originNow});
originKeys.press(0,'pointer:origin');[...originTimers.values()][0]();originKeys.release('pointer:origin');
origin='queued';originKeys.press(0,'Digit1');origin='stopped';originNow=650;originKeys.release('Digit1');
assert.deepEqual(heldOrigins,['recording','queued'],'Timer and delayed-release paths retain the state before the short action');
// Boundary tests use elapsed monotonic time, including deferred/early timers.
const Performance=keyboardContext.window.AionPerformance;
for(const initial of phases)for(const current of phases)assert.equal(Performance.deleteAction(initial,current),['playing','stopping'].includes(initial)&&['playing','stopping','stopped'].includes(current)?'clear':null);
for(const duration of [599,600,850,3999,4000,4001]) {
 let clock=0;const calls=[];
 const gesture=new Performance({track:()=>({state:'playing'}),action(){},hold:()=>calls.push('cut'),erase:()=>calls.push('delete')},{set:()=>1,clear(){},now:()=>clock});
 gesture.press(0,'mouse');clock=duration;gesture.release('mouse');gesture.release('mouse');
 assert.deepEqual(calls,duration<600?[]:duration<4000?['cut']:['cut','delete'],'Release boundary '+duration);
}
let modeClock=0,modeDelete=0,deleteAllowed=false;
const modePress=new Performance({track:()=>({state:'playing'}),canDelete:()=>deleteAllowed,action(){deleteAllowed=true;},hold(){},erase(){modeDelete++;}},{set:()=>1,clear(){},now:()=>modeClock});
modePress.press(0,'mouse');modeClock=5000;modePress.release('mouse');assert.equal(modeDelete,0,'OVERDUB/REPLACE intent cannot become delete if mode changes during a hold');
let deleteClock=0;const deleteTimers=new Map(),deleteCalls=[];let deleteId=0;
const deleteKeys=new Performance({track:()=>({state:'playing'}),action(){},hold(){},erase:()=>deleteCalls.push('delete')},{set:(fn,delay)=>{const id=++deleteId;deleteTimers.set(id,{fn:()=>{deleteTimers.delete(id);fn();},delay});return id;},clear:id=>deleteTimers.delete(id),now:()=>deleteClock});
deleteKeys.press(0,'mouse');const earlyDelete=[...deleteTimers.values()].find(t=>t.delay===4000);deleteClock=3999;earlyDelete.fn();assert.deepEqual(deleteCalls,[],'No deletion at 3999ms even if timer fires early');
deleteClock=4000;[...deleteTimers.values()].find(t=>t.delay===1).fn();deleteKeys.release('mouse');assert.deepEqual(deleteCalls,['delete'],'4s timer/release deletes once');assert.equal(deleteTimers.size,0);
for(const cancel of [false,true]){deleteClock=0;deleteKeys.press(0,'mouse');deleteClock=5000;if(cancel)deleteKeys.cancel();else deleteKeys.release('mouse',true);assert.equal(deleteCalls.length,1,'Blur/cancel never recovers deletion');assert.equal(deleteTimers.size,0);}
console.log('PASS: reference keyboard loop keys, shifted bank, modes, stop, retrigger, long-press, repeat/focus/blur guards');

snapshot.tracks[0]={...snapshot.tracks[0],pan:10,playbackRate:0.5};
cards=context.window.AionLoopMarkup(snapshot,'normal');
assert(cards[0].includes('0.50x')&&cards[0].includes('value="10"'),'Original pan and speed values must use native state');

// Exercise the same scheduler shipped in the native editor, with delayed audio acknowledgements.
(async()=>{
 // Execute the original long-action handler, retaining its safe clear/cut semantics.
 // Playback holds retain the user-confirmed quick-delete workflow; recording and
 // cancellation holds remain scoped to their original gesture.
 const actionStart=original.indexOf('    static async handleAction('),actionEnd=original.indexOf('\n    }',actionStart)+6;
 assert(actionStart>=0&&actionEnd>actionStart,'Original main action method found');
 for(const phase of ['stopped','playing','stopping']) {
   const referenceCalls=[],loop={state:phase,playRequest:0,clear:()=>referenceCalls.push('clear'),stop:()=>referenceCalls.push('stopImmediate')};
   const handler=vm.runInNewContext('('+original.slice(actionStart,actionEnd).replace('static async handleAction','async function')+')',{
     state:{loops:[loop]},AudioEngine:{resume:async()=>true},EffectManager:{setActiveTab(){}},window:{}
   });
   await handler.call({transportGeneration:0},0,'long');
   assert.equal(referenceCalls[0],phase==='stopped'?keyboardContext.window.AionPerformance.deleteAction('playing','stopped'):keyboardContext.window.AionPerformance.holdAction(phase,phase),'Unchanged-JS clear/cut intent with user-requested 4s delete: '+phase);
 }
 console.log('PASS: unchanged-JS long-action clear/cut intent, 121 native hold-state pairs, latched timer/release origins');
 const Queue=keyboardContext.window.AionCommandQueue;
 let ready=true,clock=0,release,state='empty';const sent=[],errors=[];
 const queue=new Queue({ready:()=>ready,now:()=>clock,notify:e=>errors.push(e),run:async c=>{
   sent.push(c);await new Promise(r=>release=r);state=c;
 }});
 const settle=async()=>{for(let i=0;i<8;++i)await Promise.resolve();};
 // A hold that has entered the message queue must still be cancellable.
 let holdReady=false,holdTime=0;const heldSent=[];
 const heldQueue=new Queue({ready:()=>holdReady,notify(){},run:async c=>heldSent.push(c)});
 const heldPerformance=new Performance({track:()=>({state:'playing'}),action(){},hold(){},erase:(id,start,valid)=>heldQueue.enqueue(()=>!valid||valid()?'clear':null)},{set:()=>1,clear(){},now:()=>holdTime});
 heldPerformance.press(0,'mouse');holdTime=4000;heldPerformance.release('mouse');heldPerformance.cancel();
 holdReady=true;await heldQueue.pump();await settle();
 assert.deepEqual(heldSent,[],'Blur after a 4s release must invalidate a deletion still waiting in the queue');
 for(const cancellation of ['none','blur','pointer','track','otherTrack']) {
   let runnable=false,time=0;const dispatched=[];
   const pendingQueue=new Queue({ready:()=>runnable,notify(){},run:async c=>dispatched.push(c)});
   const pendingKeys=new Performance({track:()=>({state:'playing'}),action(){},hold:(id,start,valid)=>pendingQueue.enqueue(()=>valid()?'cut':null),erase:(id,start,valid)=>pendingQueue.enqueue(()=>valid()?'clear':null)},{set:()=>1,clear(){},now:()=>time});
   pendingKeys.press(0,'mouse');time=4000;pendingKeys.release('mouse',cancellation==='pointer');
   if(cancellation==='blur')pendingKeys.cancel();
   if(cancellation==='track')pendingKeys.cancelTrack(0);
   if(cancellation==='otherTrack')pendingKeys.cancelTrack(1);
   runnable=true;await pendingQueue.pump();await settle();
   assert.deepEqual(dispatched,['none','otherTrack'].includes(cancellation)?['cut','clear']:[],'Queued hold cancellation: '+cancellation);
 }
 // A track clear invalidates old holds without cancelling another track's hold.
 const activeTimers=new Map();let activeId=0;
 const activeKeys=new Performance({track:()=>({state:'playing'}),action(){},hold(){},erase(){}},{set:fn=>{activeTimers.set(++activeId,fn);return activeId;},clear:id=>activeTimers.delete(id)});
 activeKeys.press(0,'Digit1');activeKeys.press(1,'Digit2');activeKeys.cancelTrack(0);
 assert.equal(activeTimers.size,2);assert.equal(activeKeys.presses.size,1);assert(activeKeys.presses.has('Digit2'));activeKeys.cancel();assert.equal(activeTimers.size,0);

 // Keep an already-accepted rapid gesture while the worker finalizes PCM.
 let finalizing=true;const deferredSent=[],deferredErrors=[];let deferredTime=0;
 const deferred=new Queue({ready:()=>true,now:()=>deferredTime,notify:e=>deferredErrors.push(e),run:async c=>deferredSent.push(c)});
 deferred.enqueue(()=>finalizing?Queue.wait:'play');deferred.enqueue(()=>'after-play');
 await deferred.pump();assert.deepEqual(deferredSent,[]);assert.equal(deferred.items.length,2,'Finalizing retains original order without spinning');
 finalizing=false;await deferred.pump();await settle();assert.deepEqual(deferredSent,['play','after-play'],'Third tap dispatches after finalization');
 finalizing=true;deferred.enqueue(()=>Queue.wait);deferredTime=4000;await deferred.pump();assert.equal(deferred.items.length,0);assert(deferredErrors.some(e=>e.includes('expired')),'Deferred gestures retain original expiry');
 deferred.enqueue(()=>Queue.wait);deferred.enqueue(()=>'panic',{stop:true});await settle();assert.equal(deferredSent.at(-1),'panic');assert.equal(deferred.items.length,0,'Panic removes deferred gestures');
 // DEL must supersede unsent gestures, including a finalization wait.
 const clearSent=[];const clearQueue=new Queue({ready:()=>true,notify(){},run:async c=>clearSent.push(c)});
 clearQueue.enqueue(()=>Queue.wait,{track:0});clearQueue.enqueue(()=>'stale-record',{track:0});
 clearQueue.enqueue(()=>'clear-0',{track:0,clearTrack:true});await clearQueue.pump();await settle();
 assert.deepEqual(clearSent,['clear-0'],'DEL must cancel its deferred gesture and dispatch immediately');
 let scopedReady=false;const scopedSent=[],scopedErrors=[];
 const scoped=new Queue({ready:()=>scopedReady,notify:e=>scopedErrors.push(e),run:async c=>scopedSent.push(c)});
 scoped.enqueue(()=>Queue.wait,{track:1});scoped.enqueue(()=>'keep-1',{track:1});
 scoped.enqueue(()=>'discard-0',{track:0});scoped.enqueue(()=>'global');
 scoped.enqueue(()=>'clear-0',{track:0,clearTrack:true});scopedReady=true;await scoped.pump();await settle();
 assert.deepEqual(scopedSent,['clear-0'],'Clear bypasses a different track wait');assert.equal(scoped.items.length,3,'Other track/global actions retained');
 scoped.items[0].resolve=()=>null;await scoped.pump();await settle();assert.deepEqual(scopedSent,['clear-0','keep-1','global'],'Unaffected actions retain order');
 scopedReady=false;scoped.enqueue(()=>'panic',{stop:true});scoped.enqueue(()=>'clear-2',{track:2,clearTrack:true});scopedReady=true;await scoped.pump();await settle();
 assert.deepEqual(scopedSent.slice(-2),['panic','clear-2'],'Global panic precedes a track clear');
 scoped.enqueue(()=>'invalid-clear',{clearTrack:true});assert.equal(scopedSent.at(-1),'clear-2');assert(scopedErrors.some(e=>e.includes('valid track')));
 // Already-dispatched work completes; clear then removes later work on that track.
 let finishActive;const activeSent=[];const inFlight=new Queue({ready:()=>true,notify(){},run:async c=>{activeSent.push(c);if(c==='active')await new Promise(r=>finishActive=r);}});
 inFlight.enqueue(()=>'active',{track:0});inFlight.enqueue(()=>'stale',{track:0});inFlight.enqueue(()=>'other',{track:1});inFlight.enqueue(()=>'clear',{track:0,clearTrack:true});
 assert.deepEqual(activeSent,['active']);finishActive();await settle();assert.deepEqual(activeSent,['active','clear','other'],'Clear follows in-flight work without replaying stale actions');

 const gesture=()=>queue.enqueue(()=>state==='empty'?'record':state==='record'?'finish':'play');
 gesture();gesture();gesture();
 assert.deepEqual(sent,['record'],'Only one command can await acknowledgement');
 release();await settle();assert.deepEqual(sent,['record','finish'],'Second press resolves after the acknowledged record');
 release();await settle();assert.deepEqual(sent,['record','finish','play']);
 release();await settle();
 queue.enqueue(()=>'volume-start');
 queue.enqueue(()=>'volume-old',{key:'volume'});queue.enqueue(()=>'volume-new',{key:'volume'});
 release();await settle();assert.equal(sent.at(-1),'volume-new','Sliders retain their last pending value');
 release();await settle();
 queue.enqueue(()=>'in-flight');queue.enqueue(()=>'discard-on-stop');
 queue.enqueue(()=>'stopAll',{stop:true});queue.enqueue(()=>'after-stop');
 release();await settle();assert.equal(sent.at(-1),'stopAll','Stop precedes and removes unsent gestures');
 release();await settle();assert.equal(sent.at(-1),'after-stop');release();await settle();
 ready=false;queue.enqueue(()=>'stale');clock=4000;queue.pump();assert(!sent.includes('stale'));
 queue.enqueue(()=>'stopAll',{stop:true});clock=10000;ready=true;queue.pump();
 assert.equal(sent.at(-1),'stopAll','Urgent stop survives a pause beyond normal command expiry');release();await settle();
 ready=false;queue.enqueue(()=>'expired');clock+=4000;ready=true;queue.pump();await settle();
 assert(!sent.includes('expired'));assert(errors.some(e=>e.includes('expired')));
 ready=false;queue.enqueue(()=>'wrong-generation');queue.reset();ready=true;queue.pump();await settle();
 assert(!sent.includes('wrong-generation'));
 ready=false;for(let i=0;i<129;++i)queue.enqueue(()=>i);
 assert.equal(queue.items.length,128);assert(errors.some(e=>e.includes('Too many')));
 queue.close();ready=true;queue.pump();await settle();assert.equal(queue.items.length,0);
 // Rejected commands must not deadlock subsequent input.
 const recovered=[];const retry=new Queue({ready:()=>true,notify:e=>errors.push(e),run:async c=>{if(c==='fail')throw Error('rejected');recovered.push(c);}});
 retry.enqueue(()=>'fail');retry.enqueue(()=>'recovered');await settle();assert.deepEqual(recovered,['recovered']);
 console.log('PASS: rapid gestures ordered by acknowledged state, slider coalescing, priority Stop, pause/resume, expiry, generation reset, bounded queue, close and rejection recovery');
})().catch(error=>{console.error(error);process.exitCode=1;});

// Reuse the exact original reverse checkbox markup with native requested state.
snapshot.tracks[0].reverse=true;cards=context.window.AionLoopMarkup(snapshot,'normal');
assert(/type="checkbox" checked id="loop-0-reverse"/.test(cards[0]));
snapshot.tracks[0].reverse=false;cards=context.window.AionLoopMarkup(snapshot,'normal');
assert(/type="checkbox"  id="loop-0-reverse"/.test(cards[0]));

keyboard.down(key('KeyZ','z',{ctrlKey:true,target:{tagName:'BUTTON'}}));
keyboard.down(key('KeyZ','z',{metaKey:true,shiftKey:true,target:{tagName:'INPUT',type:'range'}}));
assert.deepEqual(events.splice(0),[['history',false],['history',true]]);
snapshot.tracks[0].undoCount=2;snapshot.tracks[0].redoCount=1;cards=context.window.AionLoopMarkup(snapshot,'normal');
assert(/title="Undo Overdub" >UNDO/.test(cards[0])&&/title="Redo Overdub" >REDO/.test(cards[0]),'Original history buttons reflect native counts');
