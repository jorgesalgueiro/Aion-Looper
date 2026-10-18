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
