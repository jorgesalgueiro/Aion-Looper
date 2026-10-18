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
const keyboard=new keyboardContext.window.AionPerformance({track:()=>({}),action:i=>events.push(['action',i]),hold:i=>events.push(['hold',i]),mode:m=>events.push(['mode',m]),stop:()=>events.push(['stop']),retrigger:()=>events.push(['retrigger']),halfSpeed:()=>events.push(['half'])},{set:fn=>{callbacks.set(++timerId,fn);return timerId;},clear:id=>callbacks.delete(id)});
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
console.log('PASS: reference keyboard loop keys, shifted bank, modes, stop, retrigger, long-press, repeat/focus/blur guards');

snapshot.tracks[0]={...snapshot.tracks[0],pan:10,playbackRate:0.5};
cards=context.window.AionLoopMarkup(snapshot,'normal');
assert(cards[0].includes('0.50x')&&cards[0].includes('value="10"'),'Original pan and speed values must use native state');
