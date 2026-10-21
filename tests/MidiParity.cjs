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

// Compare continuous routing and original setters, including parseInt pan steps.
const loops=fs.readFileSync(path.join(root,'resources/ui/Modules/loopTracks.js'),'utf8');
const method=name=>{const a=loops.indexOf('    static '+name+'('),b=loops.indexOf('\n    }',a)+6;assert(a>=0);return loops.slice(a,b);};
vm.runInContext(`state.loops=[{volume:1,pan:5}];state.midiCCMap={11:{type:'loop_vol',id:0,min:0,max:2},12:{type:'loop_pan',id:0,min:0,max:10}};class OriginalUI{${method('setLoopVolume')}${method('setLoopPan')}};UIManager.setLoopVolume=OriginalUI.setLoopVolume;UIManager.setLoopPan=OriginalUI.setLoopPan;globalThis.ccOracle=data=>{App.handleMIDIMessage({data});return [state.loops[0].volume,state.loops[0].pan];};`,sandbox);
const continuous=[],values=[];
for(let channel=0;channel<16;++channel)for(const cc of [11,12])for(const value of [...Array(128).keys(),64,63,0,127,0]){const data=[176+channel,cc,value];continuous.push([...data,3]);values.push(sandbox.ccOracle(data));}
const ccRun=spawnSync(process.argv[2],['continuous'],{input:continuous.map(c=>c.join(' ')).join('\n')+'\n',encoding:'utf8',maxBuffer:4*1024*1024});assert.equal(ccRun.status,0,ccRun.stderr);
const actual=ccRun.stdout.trim().split('\n').map(l=>l.trim().split(/\s+/).map(Number));assert.equal(actual.length,values.length);
let maximum=0;actual.forEach((row,i)=>row.forEach((v,j)=>{const error=Math.abs(v-values[i][j]);maximum=Math.max(maximum,error);assert(error<=1e-7,`CC ${continuous[i]} parameter ${j}: ${v} != ${values[i][j]}`);}));
console.log(JSON.stringify({result:'PASS',continuousMessages:continuous.length,maximum,oracle:'Unchanged App.handleMIDIMessage + UIManager.setLoopVolume/setLoopPan, including zero, repeated/nonmonotonic expression, all channels and original pan quantization'}));

// The unchanged JS consumes the learn packet, then applies the very next CC.
sandbox.KeyMapManager={setStatus(){}};sandbox.setTimeout=()=>0;
vm.runInContext(`state.loops[0]={volume:1,pan:5};state.midiCCMap={};state.midiLearn={active:true,type:'loop_vol',id:0,min:0,max:2};`,sandbox);
const learnBurst=[[176,13,0,3],[176,13,127,3],[191,13,64,3],[176,13,0,3],[176,13,127,3]],learnExpected=learnBurst.map(c=>sandbox.ccOracle(c.slice(0,3)));
const learnedRun=spawnSync(process.argv[2],['learn-volume'],{input:learnBurst.map(c=>c.join(' ')).join('\n')+'\n',encoding:'utf8'});assert.equal(learnedRun.status,0,learnedRun.stderr);
const learnedActual=learnedRun.stdout.trim().split('\n').map(l=>l.trim().split(/\s+/).map(Number));assert.equal(learnedActual.length,learnExpected.length);learnedActual.forEach((row,i)=>row.forEach((v,j)=>assert(Math.abs(v-learnExpected[i][j])<=1e-7,`immediate learned CC ${i}: ${v} != ${learnExpected[i][j]}`)));
console.log('PASS: unchanged JS learn-event consumption and immediate following expression, without message-thread acknowledgement');

const escapeStart=source.indexOf('static handleKeyboardPress('),escapeEnd=source.indexOf('\n    }',escapeStart)+6;assert(escapeStart>=0&&escapeEnd>escapeStart);
vm.runInContext(`class OriginalKeyboard{${source.slice(escapeStart,escapeEnd)}};state.midiLearn={active:true};OriginalKeyboard.handleKeyboardPress({code:'Escape',key:'Escape',target:{tagName:'INPUT',type:'range'}});globalThis.originalEscapeActive=state.midiLearn.active;state.midiLearn={active:true};OriginalKeyboard.handleKeyboardPress({code:'Escape',key:'Escape',target:{tagName:'INPUT',type:'text'}});globalThis.originalTextEscapeActive=state.midiLearn.active;`,sandbox);assert.equal(sandbox.originalEscapeActive,false);assert.equal(sandbox.originalTextEscapeActive,false);
console.log('PASS: unchanged original keyboard Escape cancels active MIDI learn');

// Execute the original context-menu and Escape paths between a NoteOn/NoteOff.
// The spy observes performance ownership, not simulated audio/DSP behavior.
const touchStart=source.indexOf('    static setupTouchSafety('),touchEnd=source.indexOf('\n    }',touchStart)+6;assert(touchStart>=0);
sandbox.document.addEventListener=(type,handler)=>{if(type==='contextmenu')sandbox.originalContextMenu=handler;};sandbox.clearTimeout=()=>{};
vm.runInContext(`class OriginalTouch{${source.slice(touchStart,touchEnd)}};OriginalTouch.setupTouchSafety();globalThis.heldEdges=[];App.handleInputPress=id=>heldEdges.push(['down',id]);App.handleInputRelease=id=>heldEdges.push(['up',id]);state.midiLearn={active:false};App.handleMIDIMessage({data:[144,36,100]});`,sandbox);
const slider={tagName:'INPUT',type:'range',min:'0',max:'2',style:{},matches:()=>false,getAttribute:()=>"UIManager.setLoopVolume(1, this.value)"};
sandbox.originalContextMenu({target:slider,preventDefault(){}});assert.equal(sandbox.heldEdges.length,1,'Original learn does not release held note');
sandbox.ccOracle([176,13,0]);assert.equal(sandbox.heldEdges.length,1,'Original binding acknowledgement does not release held note');
sandbox.originalContextMenu({target:slider,preventDefault(){}});
vm.runInContext(`OriginalKeyboard.handleKeyboardPress({code:'Escape',key:'Escape',target:{tagName:'INPUT',type:'range'}});`,sandbox);assert.equal(sandbox.heldEdges.length,1,'Original learn cancellation does not release held note');
vm.runInContext(`App.handleMIDIMessage({data:[128,36,0]});`,sandbox);assert.deepEqual(JSON.parse(JSON.stringify(sandbox.heldEdges)),[['down',0],['up',0]]);
console.log('PASS: unchanged original slider learn, acknowledgement and Escape preserve a held note until its NoteOff');

const fieldMethod=name=>{const a=source.indexOf('    static '+name+'('),b=source.indexOf('\n    }',a)+6;assert(a>=0);return source.slice(a,b);};
vm.runInContext(`class OriginalMapping{${fieldMethod('handleInputFocus')}${fieldMethod('handleInputBlur')}static setStatus(){}};globalThis.mappingElement={value:'',classList:{add(){},remove(){}},id:'map-midi-0'};OriginalMapping.handleInputFocus('midi',0,mappingElement);globalThis.originalFieldActive=state.listeningForInput.active;OriginalMapping.handleInputBlur(mappingElement);globalThis.originalFieldCancelled=!state.listeningForInput.active&&state.listeningForInput.element===null;`,sandbox);assert.equal(sandbox.originalFieldActive,true);assert.equal(sandbox.originalFieldCancelled,true);
console.log('PASS: unchanged original mapping-field focus arms learning and blur cancels it');
