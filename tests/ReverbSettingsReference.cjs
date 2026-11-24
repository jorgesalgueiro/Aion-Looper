// SPDX-License-Identifier: AGPL-3.0-only
// Original settings/room/preset dispatch only; native IR sound is a new model.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),read=p=>fs.readFileSync(p,'utf8').replace(/\r\n/g,'\n');
const source=read(path.join(root,'resources/ui/Modules/effects.js'));
const reference=read(path.join(root,'.reference/Ascii-Looper/Modules/effects.js'));
const method=(s,name)=>{const a=s.indexOf('    static '+name+'('),b=s.indexOf('\n    }',a)+6;assert(a>=0&&b>a,name);return s.slice(a,b);};
const extract=s=>({defaults:s.match(/    reverb: (\{[\s\S]*?\n    \}),/)[1],presets:s.match(/const REVERB_PRESETS = (\{[\s\S]*?\n\});/)[1],markup:s.slice(s.indexOf("    'B': { key: 'reverb'"),s.indexOf("    'D': { key: 'delay'")),methods:['update','updateReverbRoom','applyGenericPreset'].map(n=>method(s,n)).join('\n')});
const original=extract(source);assert.deepEqual(original,extract(reference),'unchanged defaults, controls, presets and actual handlers match independent reference');
const probe=spawnSync(process.argv[2],['catalog'],{encoding:'utf8'});assert.equal(probe.status,0,probe.stderr);const native=JSON.parse(probe.stdout);
const defaults=vm.runInNewContext('('+original.defaults+')'),presets=vm.runInNewContext('('+original.presets+')'),markup=vm.runInNewContext('({'+original.markup+'}).B');
assert.deepEqual(native.controls.map(c=>c.name),['volume','mix','duration','decay','damp','predelay']);
for(const c of native.controls){const widget=markup.controls.find(w=>w.p===c.name);assert(widget);assert.equal(c.initial,defaults[c.name]);for(const [n,w] of [['min','min'],['max','max'],['step','step']])assert.equal(c[n],widget[w]);}
assert.deepEqual(native.presets.map(p=>p.name),Object.keys(presets));
const context={document:{getElementById:()=>null,activeElement:null},AudioEngine:{currentTime:0}};
vm.runInNewContext(`const effects={reverb:${original.defaults}},presets=${original.presets};const state={audioContext:{sampleRate:48000},loops:[{state:'stopped',graph:null,params:{},activePresets:{}}]};let regenerations=0;
class EffectManager {${original.methods}
static activeTab=0;static REVERB_PRESETS=presets;static regenerateReverb(){++regenerations;}static renderEffectsPanel(){}static updateControlUI(){}
}
globalThis.seed=values=>{state.loops[0].params.reverb={...effects.reverb,...values};state.loops[0].activePresets={reverb:'old'};regenerations=0;};
globalThis.room=name=>{EffectManager.update('reverb','room',name);return {...state.loops[0].params.reverb};};
globalThis.preset=name=>{EffectManager.applyGenericPreset('reverb',name);return {params:{...state.loops[0].params.reverb},active:state.loops[0].activePresets.reverb,regenerations};};
globalThis.manual=(p,v)=>{EffectManager.update('reverb',p,v);return {params:{...state.loops[0].params.reverb},active:state.loops[0].activePresets.reverb,regenerations};};`,context);
let cases=0;
for(const initial of [{},{duration:4.9,decay:4.9,mix:.8,volume:2.1,damp:1000,predelay:.2},{duration:.2,decay:.1,mix:0,volume:.1,damp:500,predelay:0}]){
 for(const room of native.rooms){context.seed(initial);const actual=context.room(room.name);assert.equal(actual.duration,room.duration);assert.equal(actual.decay,room.decay);for(const n of ['volume','mix','damp','predelay'])assert.equal(actual[n],initial[n]??defaults[n]);++cases;}
 for(const p of native.presets){context.seed(initial);const actual=context.preset(p.name);assert.equal(actual.params.room,p.room);assert.deepEqual(native.controls.map(c=>actual.params[c.name]),p.values);assert.equal(actual.active,p.name);assert.equal(actual.regenerations,1,'room-first/overrides batch regenerates once');++cases;}
}
for(const c of native.controls)for(const v of [c.min,c.max,c.initial]){context.seed({});const actual=context.manual(c.name,v);assert.equal(actual.params[c.name],v);assert.equal(actual.active,'');assert.equal(actual.regenerations,['duration','decay'].includes(c.name)?1:0);++cases;}
console.log('PASS unchanged original B controls, 8 room mappings, 17 room-first presets, manual Custom and IR-regeneration routing: '+cases+' cases; no audio-model parity claim');
