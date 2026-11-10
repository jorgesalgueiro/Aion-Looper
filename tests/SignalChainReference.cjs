// SPDX-License-Identifier: AGPL-3.0-only
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict'),{spawnSync}=require('node:child_process');
const source=fs.readFileSync(path.join(__dirname,'../resources/ui/Modules/loopTracks.js'),'utf8').replace(/\r\n/g,'\n');
function method(a,b){const from=source.indexOf(a),to=source.indexOf(b,from);assert(from>=0&&to>from);return source.slice(from,to).trim();}
const setter=method('    setSignalChain(chain) {','    /**'),builder=method('    buildEffectsChain(inputNode,','    _createGriz(inputNode) {'),mapping=method('    _getEffectByChar(char) {','    /**');
const Original=vm.runInNewContext(`(class {${setter}\n${builder}\n${mapping}})`,{state:{customEffects:{}},UIManager:{updateLoop(){}},EffectManager:{applyMixInFade(){throw Error('unexpected fade');}}});
const loop=new Original();loop.id=0;loop.state='playing';let rebuilds=0;loop.graph={rebuild(){++rebuilds;}};
const inputs=['','cq','cQcq','AQ','A','?!',' b v d t f o c k q a z g h ','QCATFODBVKZG','hH','00q11c22a'];
let seed=42;for(let n=0;n<300;++n){let s='';for(let i=0;i<n%80;++i){seed=(Math.imul(seed,1664525)+1013904223)>>>0;s+=String.fromCharCode(32+seed%95);}inputs.push(s);}
const expected=inputs.map(s=>{loop.setSignalChain(s);return loop.signalChain;});assert.equal(rebuilds,inputs.length);
const probe=spawnSync(process.argv[2],['normalize'],{input:inputs.join('\n')+'\n',encoding:'utf8'});assert.equal(probe.status,0,probe.stderr);assert.deepEqual(probe.stdout.trim().split(/\r?\n/),expected);
for(const order of ['QC','CQ','ACQ','Q','A','QCATFODBVKZG'])for(const enabled of [[],['eq'],['compressor'],['eq','compressor','arpDelay']]){
 const graph=new Original();graph.loop={signalChain:order,effects:Object.fromEntries(enabled.map(x=>[x,true]))};graph.nodes={};const visited=[];
 graph._createEffectNode=(name,input)=>{visited.push(name);return {output:{name,input},nodes:{owner:name}};};
 const input={name:'input'},output=graph.buildEffectsChain(input);const want=[...order].map(c=>graph._getEffectByChar(c)).filter(n=>enabled.includes(n));assert.deepEqual(visited,want);
 let current=output;for(const name of [...want].reverse()){assert.equal(current.name,name);current=current.input;}assert.equal(current,input);
}
console.log(`PASS unchanged original chain setter (${inputs.length} cases), enabled-order/omission graph routing (24 cases)`);
