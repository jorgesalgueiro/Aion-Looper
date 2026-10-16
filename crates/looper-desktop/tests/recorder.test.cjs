const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const vm=require('node:vm');
function recorder(){
    let source=fs.readFileSync(path.join(__dirname,'../../../ui/Modules/loopTracks.js'),'utf8');
    for(const [from,to] of JSON.parse(fs.readFileSync(path.join(__dirname,'../assets/recording-edits.json'),'utf8'))){
        assert.ok(source.includes(from),'generated replacement exists');source=source.replace(from,to);
    }
    const worklet=source.slice(source.indexOf('class RecorderProcessor'),source.indexOf("registerProcessor('recorder-processor'"));
    const messages=[];
    const context={currentFrame:0,sampleRate:48000,Float32Array,
        AudioWorkletProcessor:class{constructor(){this.port={postMessage:(message,transfer=[])=>{
            const bytes=transfer.reduce((sum,buffer)=>sum+buffer.byteLength,0);
            messages.push({message:structuredClone(message,{transfer}),count:transfer.length,bytes});
            assert.ok(transfer.every(buffer=>buffer.byteLength===0),'ownership transferred');
        }};}}};
    vm.createContext(context);vm.runInContext(worklet+';this.recorder=new RecorderProcessor();',context);
    const node=context.recorder;
    return {node,messages,command:(command,args={})=>node.handleMessage({data:{command,...args}}),
        feed(channels){node.process([channels],[[]]);context.currentFrame+=channels[0].length;}};
}
test('recording stop transfers bounded chunks with exact stereo PCM and partial tail',()=>{
    const h=recorder();h.command('start');
    const left=[],right=[];
    for(let block=0;block<173;block++){
        const a=Float32Array.from({length:128},(_,i)=>(block*128+i)/32768);
        const b=Float32Array.from(a,v=>-v);left.push(...a);right.push(...b);h.feed([a,b]);
    }
    h.command('stop');const result=h.messages[0];
    assert.equal(result.message.startFrame,0);
    assert.equal(result.message.chunks.length,Math.ceil(left.length/4096));
    assert.equal(result.count,result.message.chunks.length*2);
    assert.equal(result.bytes,left.length*8);
    assert.deepEqual(result.message.chunks.flatMap(chunk=>Array.from(chunk[0])),left);
    assert.deepEqual(result.message.chunks.flatMap(chunk=>Array.from(chunk[1])),right);
    h.command('start');h.feed([Float32Array.of(-0,.5)]);h.command('stop');
    assert.deepEqual(Array.from(h.messages[1].message.chunks[0][0]),[-0,.5]);
});
test('prebuffer promotion preserves ordered tail and fresh partial capture',()=>{
    const h=recorder();h.command('startPreBuffer',{seconds:1,channels:1});
    h.feed([Float32Array.of(1,2,3,4)]);
    h.command('promoteToRecording',{snapSamples:3});h.feed([Float32Array.of(5,6)]);h.command('stop');
    assert.deepEqual(h.messages[0].message.chunks.flatMap(chunk=>Array.from(chunk[0])),[2,3,4,5,6]);
    h.command('promoteToRecording',{snapSamples:0});h.feed([Float32Array.of(7)]);h.command('stop');
    assert.deepEqual(h.messages[1].message.chunks.flatMap(chunk=>Array.from(chunk[0])),[7]);
});
