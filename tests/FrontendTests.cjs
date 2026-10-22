const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const root=path.resolve(__dirname,'..');
{const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/midi.js'),'utf8'),native);const sync=new native.window.AionMidiMode();
 assert.equal(sync.observe({revision:0,modeRevision:0,mode:0}),'normal');
 const first=sync.begin(),second=sync.begin();sync.ack(first,1);assert.equal(sync.observe({revision:1,modeRevision:1,mode:1}),null,'Earlier mode reply cannot overwrite a newer keyboard gesture');
 sync.ack(second,2);assert.equal(sync.observe({revision:1,modeRevision:1,mode:1}),null,'Old audio snapshot stays behind newest acknowledgement fence');
 assert.equal(sync.observe({revision:2,modeRevision:2,mode:2}),'substitute');assert.equal(sync.observe({revision:2,modeRevision:3,mode:1}),'overdub','Subsequent hardware mode reaches UI');
 const failed=sync.begin();sync.fail(failed);assert.equal(sync.observe({revision:2,modeRevision:3,mode:1}),'overdub','Failed edit restores authoritative mode');
 console.log('PASS: MIDI/UI mode acknowledgement fence, stale reply/snapshot protection, hardware updates and failure recovery');}
// Learned sliders update while focused; only an active drag/key owns their value.
{const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/midi.js'),'utf8'),native);
 const target=()=>({listeners:{},addEventListener(type,fn){(this.listeners[type]??=[]).push(fn);},send(type,event={}){for(const fn of this.listeners[type]||[])fn(event);}});
 const element=target(),events=target();element.value=1;let focus=element;
 const edit=new native.window.AionMidiSliderEdit(element,{events,active:()=>focus});
 edit.update(2,false);assert.equal(element.value,1);edit.update(2,true);assert.equal(element.value,2,'Focused bound slider follows hardware');
 element.send('pointerdown',{button:2,pointerId:2});edit.update(0,true);assert.equal(element.value,0,'Right-click does not hold a drag');
 element.send('pointerdown',{button:0,pointerId:3});edit.update(2,true);assert.equal(element.value,0,'Drag protected');events.send('pointerup',{pointerId:4});edit.update(2,true);assert.equal(element.value,0);events.send('pointerup',{pointerId:3});edit.update(2,true);assert.equal(element.value,2);
 element.send('keydown',{code:'ArrowLeft'});element.send('keydown',{code:'ArrowLeft'});edit.update(0,true);assert.equal(element.value,2,'Key repeat protected');events.send('keyup',{code:'ArrowLeft'});edit.update(0,true);assert.equal(element.value,0);
 for(const end of ['pointercancel','lostpointercapture','blur']){element.send('pointerdown',{button:0,pointerId:5});(end==='lostpointercapture'?element:events).send(end,{pointerId:5});edit.update(1,true);assert.equal(element.value,1);}
 focus=null;edit.update(2,false);assert.equal(element.value,2);
 console.log('PASS: focused learned sliders follow MIDI; drag/key/repeat/cancel/blur and unassigned focus behavior retained');}
// Merge confirmations follow successful native commits, never stale editor
// snapshots or a source/destination replacement while the prompt is pending.
{const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),native);
 let snapshot={generation:1,tracks:[{mergeSerial:0,clearEpoch:0},{waveformId:8,clearEpoch:2,state:'playing'}]},timers=[],clears=[],prompts=[];
 const notice=new native.window.AionMerge({current:()=>snapshot,schedule:fn=>timers.push(fn),confirm:m=>(prompts.push(m),true),clear:(...v)=>clears.push(v)});
 notice.observe(snapshot);
 const committed=serial=>({...snapshot,generation:1,tracks:[{mergeSerial:serial,clearEpoch:0,mergeSource:1,mergeSourceWaveform:8,mergeSourceClearEpoch:2},snapshot.tracks[1]]});
 snapshot=committed(1);notice.observe(snapshot);notice.observe(snapshot);assert.equal(timers.length,1);timers.shift()();assert.equal(prompts[0],'Merge successful. Clear Source Loop 2?');assert.deepEqual(clears,[[0,1,1]]);
 snapshot=committed(2);notice.observe(snapshot);snapshot={...snapshot,tracks:[snapshot.tracks[0],{waveformId:9,clearEpoch:3,state:'playing'}]};timers.shift()();assert.equal(clears.length,1,'Replacement source is never cleared');
 snapshot={...snapshot,tracks:[snapshot.tracks[0],{waveformId:8,clearEpoch:2,state:'playing'}]};snapshot=committed(3);notice.observe(snapshot);snapshot={...snapshot,generation:2};timers.shift()();assert.equal(clears.length,1,'Generation cancels old prompt');notice.observe(snapshot);assert.equal(timers.length,0,'Reload/bootstrap never replays a prior completion');
 snapshot={...snapshot,generation:1};notice.observe(snapshot);snapshot=committed(4);notice.observe(snapshot);snapshot={...snapshot,tracks:[{mergeSerial:0,clearEpoch:1},snapshot.tracks[1]]};timers.shift()();assert.equal(clears.length,1,'Destination DEL cancels confirmation');
 snapshot=committed(5);notice.observe(snapshot);notice.confirm=()=>false;timers.shift()();assert.equal(clears.length,1,'Declining keeps source');
 snapshot=committed(6);notice.observe(snapshot);notice.cancel();timers.shift()();assert.equal(clears.length,1,'STOP ALL cancels scheduled confirmation');
 snapshot=committed(7);notice.observe(snapshot);snapshot={...snapshot,tracks:[{...snapshot.tracks[0],waveformId:999},snapshot.tracks[1]]};timers.shift()();assert.equal(clears.length,1,'Undo/new destination PCM cancels earlier confirmation');
 snapshot=committed(8);notice.observe(snapshot);notice.confirm=()=>{snapshot={...snapshot,generation:99};return true;};timers.shift()();assert.equal(clears.length,1,'Recheck after blocking confirmation');
 console.log('PASS: merge success-only confirmation, exact text, repeat/reload/generation/DEL/source replacement and post-dialog guards');}
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
const keyboard=new keyboardContext.window.AionPerformance({track:()=>({}),action:i=>events.push(['action',i]),hold:i=>events.push(['hold',i]),mode:m=>events.push(['mode',m]),stop:()=>events.push(['stop']),retrigger:()=>events.push(['retrigger']),halfSpeed:()=>events.push(['half']),history:redo=>events.push(['history',redo]),multiply:()=>events.push(['multiply']),metronome:()=>events.push(['metronome'])},{set:fn=>{callbacks.set(++timerId,fn);return timerId;},clear:id=>callbacks.delete(id)});
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
keyboard.down(key('KeyN','n'));assert.deepEqual(events.pop(),['metronome']);
for(const extra of [{repeat:true},{ctrlKey:true},{target:{tagName:'INPUT'}}])keyboard.down(key('KeyN','n',extra));assert.equal(events.length,0,'N repeat/modifier/editing guards');
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

// Execute the original name handler and Loop.clear, stubbing only browser IO.
const nameOracle={window:{},document:{createElement:()=>({}),head:{appendChild(){}},getElementById:()=>null},effects:{},FACTORY_EFFECTS:{},console,setTimeout,clearTimeout};
vm.createContext(nameOracle);vm.runInContext(original,nameOracle);
const nameHandler=/class="track-name"[\s\S]*?oninput="([^"]+)"/.exec(original)?.[1];assert(nameHandler);
vm.runInContext(`const state={loops:[],soloState:{active:false},playingSources:{},undoStack:[],redoStack:[],isRecording:false};state.loops=[new Loop(0)];`,nameOracle);
// Use a function body so the unchanged handler receives the original input receiver.
nameOracle.rename=vm.runInContext('(function(value){const event={stopPropagation(){}};return (function(){'+nameHandler.replaceAll('${index}','0')+';return state.loops[0].name;}).call({value});})',nameOracle);
for(const value of ['', 'Guitarra Á 1 + 😀', '<tag> " &', 'a'.repeat(4096)])assert.equal(nameOracle.rename(value),value,'Original input name semantics');
assert.equal(vm.runInContext("state.loops[0].clear();state.loops[0].name",nameOracle),'','Original DEL clears the name');
nameOracle.rename('Nome antes de STOP ALL Á');
assert.equal(vm.runInContext("globalThis.AudioEngine={currentTime:0};state.samplers=[];UIManager.updateLoop=()=>{};LoopManager.stopAll();state.loops[0].name",nameOracle),'Nome antes de STOP ALL Á','Unchanged original STOP ALL preserves names');


// The unchanged name input is text, including when markup is initially rebuilt.
for(const name of ['', 'Guitarra Á 1 + 😀', '<tag> " &', 'a'.repeat(4096)]) {
 snapshot.tracks[0].name=name;
 const named=context.window.AionLoopMarkup(snapshot,'normal')[0];
 const escape=name.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');
 assert(named.includes('class="track-name" value="'+escape+'"'),'Native name must use the original escaped input');
}
snapshot.tracks[0].name='';

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

 // Exercise the production erase binding: a held STOP is a real track clear.
 const looperSource=fs.readFileSync(path.join(root,'resources/native/looper.js'),'utf8');
 const eraseBinding=looperSource.slice(looperSource.indexOf('            erase:'),looperSource.indexOf('            mode:next'));
 let eraseReady=false,eraseTime=0;const eraseSent=[];
 const eraseQueue=new Queue({ready:()=>eraseReady,notify(){},run:async c=>eraseSent.push(c)});
 const eraseContext={window:keyboardContext.window,scheduler:eraseQueue,snapshot:{tracks:[{state:'playing'}]}};
 const productionErase=vm.runInNewContext('({'+eraseBinding+'}).erase',eraseContext);
 const eraseKeys=new Performance({track:()=>({state:'playing'}),action(){},hold(){},erase:productionErase},{set:()=>1,clear(){},now:()=>eraseTime});
 eraseContext.keyboard=eraseKeys;
 eraseKeys.press(0,'Digit1');const oldHold=eraseKeys.presses.get('Digit1').valid;
 eraseKeys.press(0,'Numpad1');const overlappingHold=eraseKeys.presses.get('Numpad1').valid;
 eraseQueue.enqueue(()=>Queue.wait,{track:1});eraseQueue.enqueue(()=>({action:'record',track:0}),{track:0});
 eraseTime=4000;eraseKeys.release('Digit1');eraseReady=true;await eraseQueue.pump();await settle();
 assert.equal(eraseSent.length,1,'Four-second STOP bypasses finalization and supersedes old track actions');
 assert.equal(eraseSent[0].action,'clear');assert.equal(oldHold(),false,'Committed hold deletion invalidates old track holds');
 assert.equal(overlappingHold(),false,'Deletion also invalidates a second physical hold on the same track');
 assert.equal(eraseKeys.presses.size,0,'Deleting a track clears all its active timers');
 assert.equal(eraseQueue.items.length,1,'Another track wait is retained');
 eraseReady=false;eraseKeys.press(0,'Digit1');eraseTime+=4000;eraseKeys.release('Digit1');eraseKeys.cancel();
 eraseReady=true;await eraseQueue.pump();await settle();assert.equal(eraseSent.length,1,'Blur still cancels a promoted hold deletion');
 // An ineligible or already-cancelled gesture must not discard valid queued work.
 eraseQueue.reset();eraseReady=false;eraseQueue.enqueue(()=>'keep',{track:0});
 eraseContext.snapshot.tracks[0].state='recording';productionErase(0,'playing',()=>true);
 eraseContext.snapshot.tracks[0].state='playing';productionErase(0,'playing',()=>false);
 productionErase(0,'recording',()=>true);
 assert.equal(eraseQueue.items.length,1,'Invalid hold deletion leaves accepted actions intact');
 eraseReady=true;await eraseQueue.pump();await settle();assert.equal(eraseSent.at(-1),'keep');

 // Session metadata uses the same order as audio, but can run with no device.
 let metadataAudio=false,clearAck;const metadataSent=[];
 const metadataQueue=new Queue({ready:()=>metadataAudio||metadataQueue.items[0]?.metadata===true,notify(){},run:async c=>{metadataSent.push(c);if(c==='clear')await new Promise(r=>clearAck=r);}});
 metadataQueue.enqueue(()=>'offline-name',{track:0,key:'name:0',metadata:true});await settle();
 assert.deepEqual(metadataSent,['offline-name'],'Labels need no audio owner');
 metadataAudio=true;metadataQueue.enqueue(()=>'clear',{track:0,clearTrack:true});
 metadataQueue.enqueue(()=>'old-name',{track:0,key:'name:0',metadata:true});metadataQueue.enqueue(()=>'new-name',{track:0,key:'name:0',metadata:true});
 assert.deepEqual(metadataSent,['offline-name','clear'],'Typing after DEL waits for its acknowledgement');
 clearAck();await settle();assert.deepEqual(metadataSent,['offline-name','clear','new-name'],'Only the newest pending text follows deletion');

 // Naming must stay usable while an unrelated take is being finalized.
 const liveNames=[],liveNameErrors=[];let nameRelease;
 const liveNameQueue=new Queue({ready:()=>true,notify:e=>liveNameErrors.push(e),run:async c=>{liveNames.push(c);if(c==='active')await new Promise(r=>nameRelease=r);}});
 liveNameQueue.enqueue(()=>Queue.wait,{track:1});liveNameQueue.enqueue(()=>'keep-audio',{track:2});
 liveNameQueue.enqueue(()=>'rename-0',{track:0,key:'name:0',metadata:true});await settle();
 assert.deepEqual(liveNames,['rename-0'],'Name edits bypass unrelated finalization waits');
 assert.equal(liveNameQueue.items.length,2,'Naming retains the deferred audio actions');
 liveNameQueue.items[0].resolve=()=>null;await liveNameQueue.pump();await settle();
 assert.deepEqual(liveNames,['rename-0','keep-audio'],'Audio work retains its original relative order');
 liveNameQueue.reset();liveNameQueue.enqueue(()=>'active',{track:1});
 liveNameQueue.enqueue(()=>'rename-0-after',{track:0,key:'name:0',metadata:true});
 liveNameQueue.enqueue(()=>'stale-audio',{track:1});liveNameQueue.enqueue(()=>'panic',{stop:true});
 nameRelease();await settle();assert.deepEqual(liveNames.slice(-3),['active','panic','rename-0-after'],'STOP ALL cancels audio work but preserves pending text');
 // Even a queue filled entirely with metadata must not reject panic or exceed its bound.
 let fullReady=false;const fullNames=[],fullNameErrors=[];
 const fullNameQueue=new Queue({limit:2,ready:()=>fullReady,notify:e=>fullNameErrors.push(e),run:async c=>fullNames.push(c)});
 fullNameQueue.enqueue(()=>'name-0',{track:0,metadata:true});fullNameQueue.enqueue(()=>'name-1',{track:1,metadata:true});
 fullNameQueue.enqueue(()=>'panic',{stop:true});assert.equal(fullNameQueue.items.length,2);
 fullReady=true;await fullNameQueue.pump();await settle();assert.equal(fullNames[0],'panic','Panic always has capacity');
 assert(fullNameErrors.some(e=>e.includes('cancelled')),'Capacity cancellation is explicit');

 // DEL ALL is also an acknowledgement barrier for names typed afterwards.
 let globalNameReady=false;const globalNameSent=[];
 const globalNameQueue=new Queue({ready:()=>globalNameReady,notify(){},run:async c=>globalNameSent.push(c)});
 globalNameQueue.enqueue(()=>'clear-all',{barrier:true});globalNameQueue.enqueue(()=>'name-after-clear-all',{track:0,metadata:true});
 globalNameReady=true;await globalNameQueue.pump();await settle();
 assert.deepEqual(globalNameSent,['clear-all','name-after-clear-all'],'Name priority must not jump a global destructive action');

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

{
 const reference=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');
 const a=reference.indexOf('                // Draw Waveform',reference.indexOf('static updateLivePerformanceState'));
 const b=reference.indexOf(' else if ((loop.state',a);assert(a>=0&&b>a);
 const original=reference.slice(a,b);
 const renderer=fs.readFileSync(path.join(root,'resources/native/loop-view.js'),'utf8').split('window.AionDrawWaveform=')[1];assert(renderer);
 for(const dpr of [1,1.25,2,4])for(const w of [1,180,720,1200])for(const state of ['empty','playing','stopped','queued','stopping','overdubbing','multiplying'])for(const progress of [0,0.333,1]){
  const run=native=>{const ops=[],ctx={};for(const key of ['resetTransform','scale','clearRect','fillRect'])ctx[key]=(...v)=>ops.push([key,...v,ctx.fillStyle,ctx.globalAlpha]);
   const cvs={clientWidth:w,clientHeight:30,width:300,height:40,getContext:()=>ctx},loop={state,wavePeaks:Array.from({length:720},(_,i)=>Math.abs(Math.sin(i*0.19))*0.8)};
   const context={window:{devicePixelRatio:dpr},cvs,loop,progress};vm.createContext(context);
   if(native)vm.runInContext('const draw='+renderer+';draw(cvs,loop,progress);',context);
   else vm.runInContext(`const dpr=window.devicePixelRatio,w=cvs.clientWidth,h=cvs.clientHeight,playheadX=Math.floor(progress*w),ctx=cvs.getContext('2d');cvs.width=Math.round(w*dpr);cvs.height=Math.round(h*dpr);ctx.resetTransform();ctx.scale(dpr,dpr);ctx.clearRect(0,0,w,h);${original};if(loop.state!=='empty'){ctx.fillStyle='#fff';ctx.fillRect(playheadX,0,1,h);}`,context);
   return {ops,width:cvs.width,height:cvs.height};};
  assert.deepEqual(run(true),run(false),'Original waveform canvas parity');
 }
 console.log('PASS: original waveform colors, dim/bright bars, white playhead and HiDPI canvas operations');
}

{
 const reference=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8'),a=reference.indexOf('                // Draw Waveform',reference.indexOf('static updateLivePerformanceState')),b=reference.indexOf('            // Draw Playhead',a);
 const original=reference.slice(a,b),renderer=fs.readFileSync(path.join(root,'resources/native/loop-view.js'),'utf8').split('window.AionDrawWaveform=')[1];
 for(const dpr of [1,1.25,2,4])for(const width of [1,180,720,1200])for(const mode of ['off','dynamic'])for(const captureState of ['recording','overdubbing','substituting','multiplying'])for(const now of [0.25,2.5,50]){
  const run=native=>{const ops=[],ctx={};for(const name of ['resetTransform','scale','clearRect','fillRect','beginPath','moveTo','lineTo','stroke','fillText'])ctx[name]=(...args)=>ops.push([name,...args,ctx.fillStyle,ctx.globalAlpha,ctx.strokeStyle,ctx.lineWidth,ctx.font]);
   const cvs={clientWidth:width,clientHeight:30,width:300,height:40,getContext:()=>ctx},loop={state:captureState,wavePeaks:null,graph:{startTime:0.25}},progress=0.333,live={samples:Array.from(Float32Array.from({length:512},(_,i)=>0.5*Math.sin(i*0.31))),syncMode:mode,now,barDuration:60/137.3*7};
   const context={window:{devicePixelRatio:dpr},cvs,loop,progress,live};vm.createContext(context);
   if(native)vm.runInContext('const draw='+renderer+';draw(cvs,loop,progress,live);',context);
   else vm.runInContext(`const dpr=window.devicePixelRatio,w=cvs.clientWidth,h=cvs.clientHeight,playheadX=Math.floor(progress*w),ctx=cvs.getContext('2d'),state={syncMode:live.syncMode,liveRecordAnalyser:{getFloatTimeDomainData:out=>out.set(live.samples)},liveRecordAnalyserData:new Float32Array(512)},now=live.now,SyncManager={getBarDuration:()=>live.barDuration};cvs.width=Math.round(w*dpr);cvs.height=Math.round(h*dpr);ctx.resetTransform();ctx.scale(dpr,dpr);ctx.clearRect(0,0,w,h);${original};if(loop.state!=='empty'){ctx.fillStyle='#fff';ctx.fillRect(playheadX,0,1,h);}`,context);
   return {ops,width:cvs.width,height:cvs.height};};
  assert.deepEqual(run(true),run(false),'Original live trace and dynamic bar markers');
 }
 console.log('PASS: original live trace fills/line/colors, rolling four-bar markers and HiDPI operations');
}

{
 const reference=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8'),a=reference.indexOf('    getProgress(now) {',reference.indexOf('class Loop {')),method=reference.slice(a,reference.indexOf('\n    }',a)+6);assert(a>=0);
 const product=fs.readFileSync(path.join(root,'resources/native/looper.js'),'utf8').split('\n').find(line=>line.includes('const progress=recording?'));assert(product);
 for(const rate of [8000,48000,96000])for(const mode of [0,1,2])for(const elapsed of [1/rate,0.3,2.75,12]){
  const snapshot={frame:Math.round((0.137+elapsed)*rate),sampleRate:rate,syncMode:mode,gridLength:60/137.3*7},t={state:'recording',captureStartFrame:Math.round(0.137*rate)};t.graph={startTime:t.captureStartFrame/rate};
  const native={snapshot,t,recording:true};vm.createContext(native);vm.runInContext(product+';globalThis.result=progress;',native);
  const original={snapshot,t,state:{syncEnabled:mode!==0},SyncManager:{getLoopLength:()=>snapshot.gridLength}};vm.createContext(original);vm.runInContext(`class Original{${method}}globalThis.result=Original.prototype.getProgress.call(t,snapshot.frame/snapshot.sampleRate);`,original);
  assert(Math.abs(native.result-original.result)<=1e-12,'Recording playhead follows unchanged getProgress after mode changes');
 }
 console.log('PASS: original DYNAMIC recording playhead, including sync-mode changes during capture');
}

// A queued STUT start must be invalid after release/cancel, even without audio ack.
{const sandbox={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),sandbox);
 let start=[],stop=[];const hold=new sandbox.window.AionStutter({start:(id,valid)=>start.push({id,valid}),stop:id=>stop.push(id)});
 hold.press(0,'p1');hold.press(0,'p1');hold.press(0,'p2');assert.equal(start.length,1);assert(start[0].valid());
 hold.release('p1');assert(!start[0].valid());assert.deepEqual(stop,[0]);hold.release('p1');assert.equal(stop.length,1);
 hold.press(0,'p1');hold.press(1,'p2');hold.cancelTrack(0);assert(!start[1].valid());assert(start[2].valid());hold.cancel();assert(!start[2].valid());assert.equal(hold.presses.size,0);
 console.log('PASS: STUT momentary ownership, repeated/multi-pointer guards, queued cancellation, per-track/global cleanup');}

// STUT audio cursor may repeat; the original waveform playhead follows graph/master time.
{const code=fs.readFileSync(path.join(root,'resources/native/looper.js'),'utf8').split('\n').find(line=>line.includes('const progress=recording?'));assert(code);
 for(const progress of [0,0.123,0.5,0.999])for(const stuttering of [false,true]){const native={snapshot:{},t:{length:4096,position:17,stuttering,stutterClock:true,clockProgress:progress},recording:false};vm.createContext(native);vm.runInContext(code+';globalThis.result=progress;',native);assert.equal(native.result,progress,'Held/released STUT routes original clock to unchanged canvas');}
 console.log('PASS: original STUT waveform clock remains independent of fragment cursor after release');}

// Export must follow an acknowledged edit/finalization, retain its click-time
// name and remain owned by its track's pending-clear barrier.
(async()=>{
 const native={window:{},snapshot:{generation:1,tracks:[{state:'playing',length:512,waveformId:1}]},rows:[{name:{value:'Before'}}],enabled:()=>true,Date:{now:()=>123}};vm.createContext(native);
 vm.runInContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),native);
 let ready=true,finish;const calls=[];
 native.scheduler=new native.window.AionCommandQueue({ready:()=>ready,notify:message=>assert.fail(message),run:async c=>{calls.push(c);if(c.action==='normalize'){native.snapshot.tracks[0].state='finalizing';await new Promise(r=>finish=r);}if(c.action==='clear')Object.assign(native.snapshot.tracks[0],{state:'empty',length:0,waveformId:0});}});
 const controller=fs.readFileSync(path.join(root,'resources/native/looper.js'),'utf8'),method=/const exportLoop=track=>\{[\s\S]*?\n        \};/.exec(controller);assert(method);vm.runInContext(method[0]+'globalThis.exportLoop=exportLoop;',native);
 native.scheduler.enqueue(()=>({action:'normalize',track:0}),{track:0});native.exportLoop(0);native.rows[0].name.value='Later';
 assert.equal(calls.length,1);finish();await new Promise(setImmediate);assert.equal(calls.length,1,'Export waits for worker finalization after the edit acknowledgement');
 Object.assign(native.snapshot.tracks[0],{state:'playing',waveformId:2});await native.scheduler.pump();assert.equal(calls.length,2);assert.equal(calls[1].action,'exportLoop');assert.equal(calls[1].waveformId,2,'Export latest committed edited clip');assert.equal(calls[1].filename,'Before_123.wav','Filename belongs to the click, not later typing');
 ready=false;native.exportLoop(0);assert.equal(native.scheduler.items[0].track,0);native.scheduler.enqueue(()=>({action:'clear',track:0}),{track:0,clearTrack:true});ready=true;await native.scheduler.pump();assert.equal(calls.filter(c=>c.action==='exportLoop').length,1,'DEL supersedes an unsent export');
 native.exportLoop(0);assert.equal(native.scheduler.items.length,0,'Empty waveform is a no-op');
 console.log('PASS: export gesture FIFO, finalization wait, latest clip identity, click-time filename and DEL/empty cancellation');
})().catch(e=>{console.error(e);process.exitCode=1;});

{
 const sandbox={window:{}};vm.createContext(sandbox);vm.runInContext(fs.readFileSync(path.join(root,'resources/native/midi.js'),'utf8'),sandbox);
 const target=sandbox.window.AionMidiSliderTarget;
 for(let i=0;i<20;++i){assert.equal(target(`loop-vol-slider-${i}`),33+i);assert.equal(target(`loop-pan-slider-${i}`),53+i);}
 for(const id of ['loop-vol-slider-20','loop-vol-slider--1','loop-fbk-slider-0','loop-speed-slider-0','master','map-midi-0','loop-pan-slider-0-extra'])assert.equal(target(id),-1,'Unported/non-slider targets never start continuous learn');
 console.log('PASS: original volume/pan slider MIDI target routing, all 20 loops and unsupported controls');
}

{
 const source=fs.readFileSync(path.join(root,'resources/native/midi.js'),'utf8'),listener=source.split(/\r?\n/).find(l=>l.includes("button.disabled=false;button.addEventListener('click'"));assert(listener);
 for(const initial of [false,true]){
  const native={enabled:initial};let press;
  const context={current:{enabled:initial},button:{addEventListener:(event,callback)=>{assert.equal(event,'click');press=callback;}},send:async request=>{if(request.action==='enable')native.enabled=request.enabled;else if(request.action==='toggle')native.enabled=!native.enabled;else assert.fail('Unexpected MIDI enable action');return{ok:true};},listDevices:()=>{},notify:message=>assert.fail(message)};
  vm.runInNewContext(listener,context);press();press();assert.equal(native.enabled,initial,'Two rapid clicks before a new snapshot must preserve MIDI enable state');press();assert.equal(native.enabled,!initial,'Third rapid click must toggle once');
 }
 console.log('PASS: rapid MIDI enable/disable clicks before snapshot updates preserve every gesture');
}

{
 const source=fs.readFileSync(path.join(root,'resources/native/midi.js'),'utf8'),listener=source.split(/\r?\n/).find(l=>l.includes("document.addEventListener('keydown'"));assert(listener);
 for(const current of [undefined,{learnTarget:-1},{learnTarget:33}]){
  let nativeLearn=33,keyDown;
  const context={current,document:{addEventListener:(event,callback)=>{assert.equal(event,'keydown');keyDown=callback;}},send:async request=>{assert.equal(request.action,'cancelLearn');nativeLearn=-1;return{ok:true};},notify:message=>assert.fail(message)};
  vm.runInNewContext(listener,context);keyDown({code:'Escape',preventDefault(){}});assert.equal(nativeLearn,-1,'Escape must cancel accepted learning even before its snapshot arrives');
  nativeLearn=33;keyDown({code:'KeyA',preventDefault(){assert.fail('Ordinary keys must be unaffected');}});assert.equal(nativeLearn,33);
 }
 console.log('PASS: Escape cancellation before first/updated MIDI snapshot, with unrelated keys preserved');
}

{
 const midiSource=fs.readFileSync(path.join(root,'resources/native/midi.js'),'utf8'),looper=fs.readFileSync(path.join(root,'resources/native/looper.js'),'utf8');
 const docLine=midiSource.split(/\r?\n/).find(l=>l.includes("document.addEventListener('keydown'")),nameLine=looper.split(/\r?\n/).find(l=>l.includes("name.addEventListener('keydown'"));assert(docLine&&nameLine);
 let docHandler,nameHandler,capture=false,nativeLearn=33,stopped=false;
 const context={current:{learnTarget:33},document:{addEventListener:(type,handler,options)=>{docHandler=handler;capture=options===true||options?.capture===true;}},name:{addEventListener:(type,handler)=>{nameHandler=handler;},blur(){}},send:async()=>{nativeLearn=-1;return{ok:true};},notify:message=>assert.fail(message)};
 vm.runInNewContext(docLine+'\n'+nameLine,context);
 const event={code:'Escape',key:'Escape',preventDefault(){},stopPropagation(){stopped=true;}};
 if(capture)docHandler(event);nameHandler(event);if(!stopped)docHandler(event);
 assert.equal(nativeLearn,-1,'Escape must cancel MIDI learn even when the original native name field stops key bubbling');
 console.log('PASS: MIDI Escape survives focused loop-name keyboard isolation');
}

(async()=>{
 const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/midi.js'),'utf8'),native);
 const requests=[],waiting=[],errors=[];let focused=null,helper;
 const element=id=>({blurs:0,blur(){++this.blurs;focused=null;helper.blur(id,this);}}),a=element(0),b=element(1);
 helper=new native.window.AionMidiFieldLearn({active:()=>focused,notify:message=>errors.push(message),send:request=>{requests.push(request);return request.action==='learn'?new Promise((resolve,reject)=>waiting.push({resolve,reject})):Promise.resolve({ok:true});}});
 focused=a;helper.focus(0,a);helper.render({revision:0,learnTarget:-1});assert.equal(a.blurs,0,'Pre-learn snapshot cannot blur field');helper.blur(0,a);focused=b;helper.focus(1,b);
 waiting[0].resolve({revision:4});await Promise.resolve();helper.render({revision:99,learnTarget:-1});assert.equal(b.blurs,0,'Old focus reply cannot complete newer field');
 waiting[1].resolve({revision:5});await Promise.resolve();helper.render({revision:4,learnTarget:-1});assert.equal(b.blurs,0,'Old snapshot cannot complete accepted field');helper.render({revision:5,learnTarget:1});assert.equal(b.blurs,0);helper.render({revision:6,learnTarget:-1});assert.equal(b.blurs,1,'Accepted completion releases focus');assert.equal(requests.filter(r=>r.action==='cancelLearn').length,1,'Completion blur cannot issue another cancellation');assert.equal(requests[1].target,0,'Explicit blur cancellation is target scoped');
 focused=a;helper.focus(0,a);waiting[2].reject(Error('queue full'));await Promise.resolve();await Promise.resolve();assert.equal(a.blurs,1,'Failed learn releases focus for retry');assert.deepEqual(errors,['queue full']);
 focused=a;helper.focus(0,a);waiting[3].resolve({revision:8});await Promise.resolve();helper.render({revision:9,learnTarget:33});assert.equal(a.blurs,2,'New slider learn releases old field without cancelling slider');assert.equal(requests.filter(r=>r.action==='cancelLearn').length,1);
 console.log('PASS: MIDI mapping focus/blur ownership, delayed replies/snapshots, completion blur, failed retry and slider handoff');
})().catch(error=>{console.error(error);process.exitCode=1;});

// Compare default X routing with the unchanged original keyboard dispatcher.
{
 const original=fs.readFileSync(path.join(root,'resources/ui/main.js'),'utf8');
 const start=original.indexOf('static handleKeyboardPress('),end=original.indexOf('\n    }',start)+6;assert(start>=0);
 let expected=0,actual=0;
 const oracle={document:{getElementById:()=>null},SAMPLER_HOTKEYS:[],state:{listeningForInput:{active:false},keyMapping:{kbd:new Array(33).fill(null)},midiLearn:{active:false}},LoopManager:{clearAll(){++expected;}}};
 vm.createContext(oracle);vm.runInContext(`class App{${original.slice(start,end)}};globalThis.dispatch=e=>App.handleKeyboardPress(e);`,oracle);
 const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),native);
 const keyboard=new native.window.AionPerformance({track:()=>null,clearAll(){++actual;}});
 const targets=[{tagName:'BODY'},{tagName:'INPUT',type:'text'},{tagName:'TEXTAREA'},{tagName:'SELECT'},...['range','checkbox','radio','button','file','submit','reset','number'].map(type=>({tagName:'INPUT',type}))];
 for(const target of targets)for(const flags of [{},{shiftKey:true},{repeat:true},{ctrlKey:true},{metaKey:true},{altKey:true}]){
  let originalPrevented=0,nativePrevented=0;expected=actual=0;
  const event={code:'KeyX',key:flags.shiftKey?'X':'x',target,...flags};
  oracle.dispatch({...event,preventDefault(){++originalPrevented;}});
  keyboard.down({...event,preventDefault(){++nativePrevented;}});
  assert.equal(actual,expected,'Original X dispatch '+JSON.stringify(event));assert.equal(nativePrevented,originalPrevented,'Original X preventDefault');
 }
 for(const event of [{code:'KeyC',key:'x'},{code:'KeyX',key:'x',target:{tagName:'DIV',isContentEditable:true}}]){
  actual=0;keyboard.down({target:{tagName:'BODY'},preventDefault(){},...event});assert.equal(actual,0,'Physical-key routing and native editable-content guard');
 }
 console.log('PASS: original X/Shift-X/repeat shortcut, physical key, modifiers and text/range/control guards');
}

// A fresh keyboard press recovers the original lost-release entry only after >10s.
{
 const source=fs.readFileSync(path.join(root,'resources/ui/main.js'),'utf8');
 const method=name=>{const start=source.indexOf('static '+name+'('),end=source.indexOf('\n    }',start)+6;assert(start>=0);return source.slice(start,end);};
 // Globals are unmapped only to isolate every original Shift+Digit loop path; SUS/keymap migration is separate.
 for(const shift of [false,true])for(const digit of '1234567890'){
  let clock=1000,expected=0,actual=0,timer=0;const originalTimers=new Map();
  const oracle={window:{},SAMPLER_HOTKEYS:[],Date:{now:()=>clock},setTimeout:fn=>(originalTimers.set(++timer,fn),timer),clearTimeout:id=>originalTimers.delete(id),document:{getElementById:()=>null},LoopManager:{handleAction(){++expected;}},state:{listeningForInput:{active:false},midiLearn:{active:false},keyMapping:{kbd:Array.from('1234567890').concat(new Array(23).fill(null))},inputPressTimers:{},globalSusMode:false,loops:Array.from({length:20},()=>({state:'recording'}))}};
  vm.createContext(oracle);vm.runInContext(`class App{${method('handleKeyboardPress')}${method('handleInputPress')}${method('keyboardPressId')}};globalThis.dispatch=e=>App.handleKeyboardPress(e);`,oracle);
  const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),native);
  const keyboard=new native.window.AionPerformance({track:()=>({state:'recording'}),action(){++actual;},hold(){}},{now:()=>clock,set:()=>++timer,clear(){}});
  const event={code:'Digit'+digit,key:shift?')!@#$%^&*('[Number(digit)]:digit,shiftKey:shift,target:{tagName:'BODY'},preventDefault(){}};
  for(const elapsed of [0,9999,10000,10001,10002,20001,20002]){clock=1000+elapsed;oracle.dispatch(event);keyboard.down(event);assert.equal(actual,expected,'Original keyboard lost-release boundary '+JSON.stringify({shift,digit,elapsed}));}
  const before=actual;clock+=10001;keyboard.down({...event,repeat:true});keyboard.down({...event,ctrlKey:true});keyboard.down({...event,target:{tagName:'INPUT',type:'text'}});assert.equal(actual,before,'Repeat/modifier/text events cannot recover or refresh a held key');
  oracle.dispatch(event);keyboard.down(event);assert.equal(actual,expected,'Ignored packets cannot postpone fresh-press recovery');keyboard.cancel();
 }
 {
  let clock=0,timer=0,actions=0,erased=0;const timers=new Map(),holds=[];const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),native);
  const keyboard=new native.window.AionPerformance({track:()=>({state:'playing'}),action(){++actions;},hold:(id,state,valid)=>holds.push(valid),erase(){++erased;}},{now:()=>clock,set:fn=>(timers.set(++timer,fn),timer),clear:id=>timers.delete(id)});
  keyboard.press(0,'Digit1');const old=[...timers.values()];old[0]();keyboard.press(1,'Digit2');
  clock=10001;assert.equal(actions,2,'Age alone never expires a held press');keyboard.press(0,'Digit1');assert.equal(actions,3,'Fresh press recovers even with monotonic origin zero');assert(!holds[0](),'Queued old cut is invalid after recovery');
  for(const callback of old)callback();assert.equal(erased,0,'Old deferred hold/delete callbacks cannot act on replacement press');assert(keyboard.presses.get('Digit2').valid(),'Recovery is scoped to its key');assert.equal(timers.size,4,'Only recovered key timers are replaced');keyboard.cancel();assert.equal(timers.size,0);
 }
 console.log('PASS: unchanged original fresh-key >10s recovery for all 20 loops, exact boundary/no age refresh, ignored repeats/focus, scoped timer/queued-action cancellation');
}

// The original maps keypad characters; NumLock navigation is not a loop press.
{
 const source=fs.readFileSync(path.join(root,'resources/ui/main.js'),'utf8');
 const method=name=>{const start=source.indexOf('static '+name+'('),end=source.indexOf('\n    }',start)+6;assert(start>=0);return source.slice(start,end);};
 let expected=[],actual=[],timer=0;const timers=new Map();
 const oracle={window:{},SAMPLER_HOTKEYS:[],document:{getElementById:()=>null},state:{listeningForInput:{active:false},midiLearn:{active:false},keyMapping:{kbd:Array.from('1234567890').concat(new Array(20).fill(null),['+','-','*'])},loops:Array.from({length:20},()=>({}))}};
 vm.createContext(oracle);oracle.record=id=>expected.push(id);
 vm.runInContext(`class App{${method('handleKeyboardPress')}${method('keyboardPressId')}static handleInputPress(id){record(id);}};globalThis.dispatch=e=>App.handleKeyboardPress(e);`,oracle);
 const native={window:{}};vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),native);
 const keyboard=new native.window.AionPerformance({track:()=>({state:'recording'}),action:id=>actual.push(id),hold(){}},{now:()=>1000,set:fn=>(timers.set(++timer,fn),timer),clear:id=>timers.delete(id)});
 const navigation=['Insert','End','ArrowDown','PageDown','ArrowLeft','Clear','ArrowRight','Home','ArrowUp','PageUp'];
 for(const digit of '0123456789')for(const shiftKey of [false,true])for(const key of [digit,navigation[Number(digit)],'Unidentified','']){
  const event={code:'Numpad'+digit,key,shiftKey,target:{tagName:'BODY'},preventDefault(){}};expected=[];actual=[];
  oracle.dispatch(event);keyboard.down(event);assert.deepEqual(actual,expected,'Original keypad digit/navigation routing '+JSON.stringify({digit,key,shiftKey}));
  keyboard.up({...event,key:navigation[Number(digit)],shiftKey:!shiftKey});assert.equal(keyboard.presses.size,0,'Release follows physical keypad key even if NumLock/Shift changed');assert.equal(timers.size,0);
 }
 // Dispatch uses the reported digit, not the number in its physical code.
 for(const digit of '0123456789')for(const key of '0123456789'){
  const event={code:'Numpad'+digit,key,shiftKey:true,target:{tagName:'BODY'},preventDefault(){}};expected=[];actual=[];oracle.dispatch(event);keyboard.down(event);assert.deepEqual(actual,expected,'Keypad character routing');keyboard.up(event);
 }
 for(const flags of [{repeat:true},{ctrlKey:true},{metaKey:true},{altKey:true},{target:{tagName:'INPUT',type:'text'}},{target:{tagName:'TEXTAREA'}},{target:{tagName:'SELECT'}}]){
  const event={code:'Numpad1',key:'1',shiftKey:true,target:{tagName:'BODY'},preventDefault(){},...flags};expected=[];actual=[];oracle.dispatch(event);keyboard.down(event);assert.deepEqual(actual,expected,'Original keypad repeat/modifier/text guard');assert.equal(timers.size,0);
 }
 keyboard.down({code:'Numpad1',key:'End',target:{tagName:'BODY'},preventDefault(){assert.fail('Navigation must not be swallowed');}});
 keyboard.down({code:'Numpad1',key:'1',target:{tagName:'DIV',isContentEditable:true},preventDefault(){assert.fail('Editable content is protected');}});
 assert.equal(timers.size,0);assert.equal(keyboard.presses.size,0);
 console.log('PASS: unchanged original keypad characters, Shift stays on loops 1-10, NumLock navigation ignored, physical release and repeat/modifier/text guards');
}


// Original input-wheel hardening must survive disabling legacy main.js.
{
 const source=fs.readFileSync(path.join(root,'.reference/Ascii-Looper/main.js'),'utf8');
 const begin=source.indexOf("window.addEventListener('wheel', (e) => {");
 assert(begin>=0);const end=source.indexOf('// Force uppercase',begin);
 let originalHandler,originalOptions,nativeHandler,nativeOptions;
 vm.runInNewContext(source.slice(begin,end),{window:{addEventListener(type,fn,options){assert.equal(type,'wheel');originalHandler=fn;originalOptions=options;}}});
 const native={window:{addEventListener(type,fn,options){assert.equal(type,'wheel');nativeHandler=fn;nativeOptions=options;}}};
 vm.runInNewContext(fs.readFileSync(path.join(root,'resources/native/controls.js'),'utf8'),native);
 assert.equal(typeof nativeHandler,'function','Native adapter must install original wheel protection');
 assert.equal(nativeOptions.capture,originalOptions.capture);assert.equal(nativeOptions.passive,originalOptions.passive);
 let cases=0;
 for(const tagName of ['INPUT','TEXTAREA','SELECT','BUTTON','DIV'])for(const type of ['range','number','text','checkbox','radio','button','file'])for(const shiftKey of [false,true])for(const deltaMode of [0,1,2])for(const deltaY of [-120,0,120]){
  const invoke=handler=>{const events=[];const target={tagName,type,disabled:false,blur(){assert.equal(this,target,'blur receiver');events.push('blur');}};handler({target,shiftKey,deltaY,deltaMode,preventDefault(){events.push('preventDefault');},stopPropagation(){events.push('stopPropagation');}});return events;};
  assert.deepEqual(invoke(nativeHandler),invoke(originalHandler),'Original wheel semantics '+JSON.stringify({tagName,type,shiftKey,deltaMode,deltaY}));++cases;
 }
 console.log('PASS: original wheel protection, capture/passive options, focus/propagation and Shift bypass: '+cases+' cases');
}
