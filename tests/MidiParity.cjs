const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const root=path.resolve(__dirname,'..'),source=fs.readFileSync(path.join(root,'resources/ui/main.js'),'utf8');
const start=source.indexOf('    static handleMIDIMessage('),end=source.indexOf('\n    }',start)+6;assert(start>=0);
const mapping=/midi: (new Array\(33\)[^\n]+),/.exec(source);assert(mapping);
const sandbox={window:{},document:{getElementById:()=>null},DroneSynth:{handleMidi(){},handlePitchBend(){},handleModWheel(){}},LoopManager:{stopAll(){}},EffectManager:{},UIManager:{},InputManager:{}};vm.createContext(sandbox);
vm.runInContext(`const state={listeningForInput:{active:false},keyMapping:{midi:${mapping[1]}},midiLearn:{active:false},midiCCMap:{}};class App{${source.slice(start,end)}};globalThis.oracle=data=>{globalThis.result=-1;App.handleInputPress=id=>globalThis.result=id;App.handleInputRelease=id=>globalThis.result=id;App.toggleGlobalOverdub=()=>globalThis.result=30;App.toggleGlobalSubstitute=()=>globalThis.result=31;App.toggleGlobalSus=()=>globalThis.result=32;App.handleMIDIMessage({data});return result;};`,sandbox);
const cases=[],expected=[];
for(let channel=0;channel<16;++channel)for(let note=0;note<128;++note)for(const velocity of [0,1,127])for(const type of [128,144]){cases.push([type+channel,note,velocity,3]);expected.push(sandbox.oracle([type+channel,note,velocity]));}
for(let channel=0;channel<16;++channel)for(let n=0;n<128;++n){cases.push([192+channel,n,0,2],[176+channel,n,127,3]);expected.push(-1,-1);}
const run=spawnSync(process.argv[2],[],{input:cases.map(c=>c.join(' ')).join('\n')+'\n',encoding:'utf8',maxBuffer:4*1024*1024});assert.equal(run.status,0,run.stderr);assert.deepEqual(run.stdout.trim().split(/\s+/).map(Number),expected);
// The native adapter reuses the preserved layout exactly, not a redesigned panel.
const adapter=fs.readFileSync(path.join(root,'resources/native/midi-view.js'),'utf8').replace(/\r\n/g,'\n'),original=source.replace(/\r\n/g,'\n');
for(const name of ['renderLayout','generateTableHTML']){const a=original.indexOf('    static '+name+'('),b=original.indexOf('\n    }',a)+6;assert(adapter.includes(original.slice(a,b)),name);}
console.log(JSON.stringify({result:'PASS',messages:cases.length,oracle:'Unchanged App.handleMIDIMessage + original default mappings across all channels/note velocities; original mapping layout byte-preserved. Learned CC/PC loop bindings are a native extension.'}));
