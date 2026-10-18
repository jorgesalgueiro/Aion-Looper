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
const keyboard=new keyboardContext.window.AionPerformance({track:()=>({}),action:i=>events.push(['action',i]),hold:i=>events.push(['hold',i]),mode:m=>events.push(['mode',m]),stop:()=>events.push(['stop']),retrigger:()=>events.push(['retrigger']),halfSpeed:()=>events.push(['half']),history:redo=>events.push(['history',redo])},{set:fn=>{callbacks.set(++timerId,fn);return timerId;},clear:id=>callbacks.delete(id)});
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
console.log('PASS: reference keyboard loop keys, shifted bank, modes, stop, retrigger, long-press, repeat/focus/blur guards');

snapshot.tracks[0]={...snapshot.tracks[0],pan:10,playbackRate:0.5};
cards=context.window.AionLoopMarkup(snapshot,'normal');
assert(cards[0].includes('0.50x')&&cards[0].includes('value="10"'),'Original pan and speed values must use native state');

// Exercise the same scheduler shipped in the native editor, with delayed audio acknowledgements.
(async()=>{
 const Queue=keyboardContext.window.AionCommandQueue;
 let ready=true,clock=0,release,state='empty';const sent=[],errors=[];
 const queue=new Queue({ready:()=>ready,now:()=>clock,notify:e=>errors.push(e),run:async c=>{
   sent.push(c);await new Promise(r=>release=r);state=c;
 }});
 const settle=async()=>{for(let i=0;i<8;++i)await Promise.resolve();};
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
