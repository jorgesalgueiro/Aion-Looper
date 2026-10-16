const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const tick = () => new Promise(resolve => setImmediate(resolve));
function transport(streaming = false) {
    const calls = [], pending = [], nodes = [], timers = [], events = {};
    const context = {state:'running',sampleRate:48000,destination:{},
        audioWorklet:{addModule:async()=>{}},
        createGain() { const node = {channelCount:2,gain:{value:1},connections:[],connect(to){this.connections.push(to);},disconnect(){this.connections=[];}};nodes.push(node);return node; },
        createMediaStreamSource:()=> 'browser-source'
    };
    const state = {audioContext:context,inputs:[]};
    const InputManager = {renderUI(){this.renders=(this.renders||0)+1;}};
    class WorkletNode {
        constructor(context,name,options) { this.context=context;this.options=options;this.messages=[];this.connections=[];this.port={postMessage:data=>this.messages.push(data)};nodes.push(this); }
        connect(to){this.connections.push(to);}
        disconnect(){this.connections=[];}
    }
    const window = {addEventListener:(event,handler)=>{events[event]=handler;},RustLooper:{request:async(operation,args)=>{
        calls.push({operation,...args});
        if (operation==='input_open') return {token:7,streaming,info:{host:'WASAPI',device:'stable-id',label:'Mic',channels:2,sample_rate:48000}};
        if (operation==='input_read') return new Promise(resolve=>pending.push(resolve));
        if (operation==='input_devices') return {devices:[{host:'WASAPI',device:'stable-id',label:'Mic'}]};
        return {};
    }}};
    vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../assets/native-input.js'),'utf8'),{window,state,InputManager,AudioWorkletNode:WorkletNode,atob,console:{error(){}},setTimeout:handler=>{timers.push(handler);return timers.length;},clearTimeout(){}});
    return {window,input:window.RustLooper.input,state,context,nodes,calls,pending,timers,events,InputManager};
}
function packet(position,frames) {
    const bytes = Buffer.alloc(frames.length*8);
    frames.forEach((frame,index)=>{bytes.writeFloatLE(frame[0],index*8);bytes.writeFloatLE(frame[1],index*8+4);});
    return {position,length:frames.length,pcm:bytes.toString('base64'),frames:position+frames.length,max_callback_frames:128};
}
test('native input preserves device identity and sends finite PCM to the existing graph',async()=>{
    const h=transport(), stream=await h.input.getStream({audio:{deviceId:{exact:'native:{"host":"WASAPI","device":"stable-id"}'}}});
    assert.equal(h.calls[0].host,'WASAPI'); assert.equal(h.calls[0].device,'stable-id');
    assert.equal(stream.getAudioTracks()[0].getSettings().deviceId,'native:{"host":"WASAPI","device":"stable-id"}');
    const source=h.input.source(h.context,stream), processor=h.nodes[0];
    assert.equal(source.channelCount,2); assert.equal(h.nodes[2].gain.value,0);
    assert.equal(h.input.source(h.context,{}),'browser-source');
    h.pending.shift()(packet(0,[[1.5,-.25],[-0,.5]])); await tick();
    const pcm=processor.messages.find(message=>message.type==='pcm').samples;
    assert.deepEqual(Array.from(pcm),[1.5,-.25,-0,.5]);
    stream.getTracks()[0].stop();
    assert.equal(stream.getTracks()[0].readyState,'ended'); assert.equal(h.input.status().length,0);
    assert.equal(h.calls.at(-1).operation,'input_close');
});
test('bad packet continuity closes native capture and reports through the original input error banner',async()=>{
    const h=transport(), stream=await h.input.getStream({audio:{}});
    h.state.inputs.push({id:0,stream});
    h.pending.shift()(packet(10,[[.1,.2]])); await tick();
    assert.equal(stream.getTracks()[0].readyState,'ended');
    assert.match(h.InputManager.lastInputError,/discontinuity/);
    assert.equal(h.calls.at(-1).operation,'input_close');
});
test('stopping during an outstanding read discards its reply and schedules no new read',async()=>{
    const h=transport(), stream=await h.input.getStream({audio:{}}), processor=h.nodes[0];
    stream.getTracks()[0].stop();
    h.pending.shift()(packet(0,[[.1,.2]])); await tick();
    assert.equal(processor.messages.filter(message=>message.type==='pcm').length,0);
    assert.equal(h.timers.length,0);
});
test('native input closes after a context change and page exit',async()=>{
    const h=transport(), stream=await h.input.getStream({audio:{}});
    assert.throws(()=>h.input.source({},stream),/another audio context/);
    h.pending.shift()(packet(0,[[.1,.2]])); await tick();
    h.state.audioContext={state:'running'};
    await h.timers.shift()();
    assert.equal(stream.getTracks()[0].readyState,'ended');
    const second=transport(), other=await second.input.getStream({audio:{}});
    second.events.pagehide();
    assert.equal(other.getTracks()[0].readyState,'ended');
});
test('device enumeration keeps host and stable IDs in the existing selector shape',async()=>{
    const h=transport(), devices=await h.input.devices();
    assert.equal(devices[0].kind,'audioinput'); assert.equal(devices[0].label,'WASAPI / Mic');
    assert.equal(devices[0].deviceId,'native:{"host":"WASAPI","device":"stable-id"}');
});
test('suspended contexts discard captured packets and resume with fresh audio',async()=>{
    const h=transport(), stream=await h.input.getStream({audio:{}}), processor=h.nodes[0];
    h.context.state='suspended';
    h.pending.shift()(packet(0,[[.1,.2]])); await tick();
    assert.equal(processor.messages.filter(message=>message.type==='pcm').length,0);
    assert.equal(processor.messages.filter(message=>message.type==='flush').length,1);
    h.context.state='running';
    const pulling=h.timers.shift()();
    h.pending.shift()(packet(1,[[.5,.25]])); await pulling;
    const samples=processor.messages.find(message=>message.type==='pcm').samples;
    assert.deepEqual(Array.from(samples),[.5,.25]);
    stream.getTracks()[0].stop();
});
function worklet(targetFrames=16,channels=2) {
    let Processor;
    class Base { constructor(){this.port={postMessage:message=>{this.status=message;}};} }
    vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../assets/native-input-worklet.js'),'utf8'),{AudioWorkletProcessor:Base,registerProcessor:(_name,value)=>{Processor=value;}});
    return new Processor({processorOptions:{targetFrames,channels}});
}
test('input worklet preserves float headroom and stereo PCM at aligned clocks',()=>{
    const p=worklet(), samples=Float32Array.from({length:32},(_,i)=>i%2 ? -.25 : 1.5);
    p.port.onmessage({data:{type:'pcm',samples}});
    const output=[new Float32Array(4),new Float32Array(4)];p.process([],[output]);
    assert.deepEqual(Array.from(output[0]),[1.5,1.5,1.5,1.5]);
    assert.deepEqual(Array.from(output[1]),[-.25,-.25,-.25,-.25]);
    assert.equal(p.step,1);
});
test('mono native input keeps one output channel and ignores the internal duplicate',()=>{
    const p=worklet(16,1), samples=Float32Array.from({length:32},(_,i)=>i%2 ? 99 : .125);
    p.port.onmessage({data:{type:'pcm',samples}});
    const output=[new Float32Array(4)];p.process([],[output]);
    assert.deepEqual(Array.from(output[0]),[.125,.125,.125,.125]);
});
test('input worklet prefills, reports underruns and flushes suspended audio',()=>{
    const p=worklet(), output=[new Float32Array(32),new Float32Array(32)];
    p.process([],[output]); assert.equal(p.underruns,0);
    p.port.onmessage({data:{type:'pcm',samples:new Float32Array(32).fill(.1)}});
    p.process([],[output]); assert.equal(p.underruns,1);
    assert.equal(output[0][31],0);
    p.port.onmessage({data:{type:'flush'}});assert.equal(p.write,0);assert.equal(p.read,0);
    p.port.onmessage({data:{type:'stop'}});assert.equal(p.process([],[output]),false);
});
test('input worklet bounds queued storage and tracks clock correction without unbounded speed changes',()=>{
    const p=worklet();
    p.port.onmessage({data:{type:'pcm',samples:new Float32Array(32768)}});
    assert.equal(p.overruns,1);assert.equal(p.write,0);
    p.port.onmessage({data:{type:'pcm',samples:new Float32Array(128).fill(.2)}});
    p.process([],[[new Float32Array(4),new Float32Array(4)]]);
    assert.ok(p.step>1 && p.step<=1.001);
    p.port.onmessage({data:{type:'status'}});assert.equal(p.status.overruns,1);
});



test('failed worklet loading permits retry without opening a native device early',async()=>{
    const h=transport();
    let attempts=0;
    h.context.audioWorklet.addModule=async()=>{
        if (++attempts===1) throw new Error('Transient worklet load failure');
    };
    await assert.rejects(h.input.getStream({audio:{}}),/Transient worklet/);
    assert.equal(h.calls.length,0);
    const stream=await h.input.getStream({audio:{}});
    assert.equal(attempts,2);
    assert.equal(stream.getTracks()[0].readyState,'live');
    stream.getTracks()[0].stop();
});

test('concurrent inputs share one worklet load and can retry its failure',async()=>{
    const h=transport();
    let attempts=0,rejectLoad;
    h.context.audioWorklet.addModule=()=>{
        attempts++;
        return new Promise((resolve,reject)=>{rejectLoad=reject;});
    };
    const first=h.input.getStream({audio:{}}),second=h.input.getStream({audio:{}});
    const results=Promise.allSettled([first,second]);
    assert.equal(attempts,1);
    rejectLoad(new Error('Load failed'));
    assert.deepEqual((await results).map(result=>result.status),['rejected','rejected']);
    assert.equal(h.calls.length,0);
    h.context.audioWorklet.addModule=async()=>{attempts++;};
    const stream=await h.input.getStream({audio:{}});
    assert.equal(attempts,2);
    stream.getTracks()[0].stop();
});

for (const change of ['replace','close']) {
    test(`pending input reply after context ${change} closes capture without forwarding PCM`,async()=>{
        const h=transport(),stream=await h.input.getStream({audio:{}}),processor=h.nodes[0];
        if(change==='replace') h.state.audioContext={state:'running'};
        else h.context.state='closed';
        h.pending.shift()(packet(0,[[.4,.2]]));
        await tick();
        assert.equal(stream.getTracks()[0].readyState,'ended');
        assert.equal(processor.messages.filter(message=>message.type==='pcm').length,0);
        assert.equal(h.timers.length,0);
        assert.equal(h.calls.at(-1).operation,'input_close');
        assert.equal(h.input.status().length,0);
    });
}

test('bounded prefill absorbs batched IPC delivery without losing a continuous input clock',()=>{
    function run(target) {
        const p=worklet(target);
        p.port.onmessage({data:{type:'pcm',samples:new Float32Array(target*2).fill(.125)}});
        for(let block=0;block<240;block++) {
            // A continuous native stream is delivered in 32 ms IPC batches.
            if(block%12===11) p.port.onmessage({data:{type:'pcm',samples:new Float32Array(1536*2).fill(.125)}});
            p.process([],[[new Float32Array(128),new Float32Array(128)]]);
        }
        return p;
    }
    assert.ok(run(768).underruns>0);
    const buffered=run(3072);
    assert.equal(buffered.underruns,0);
    assert.equal(buffered.overruns,0);
    assert.ok(buffered.write-buffered.read<=buffered.capacity);
});

test('acknowledged push forwards each packet once without input polling timers',async()=>{
    const h=transport(true),stream=await h.input.getStream({audio:{}}),processor=h.nodes[0];
    assert.equal(h.calls[0].streaming,true);
    assert.equal(h.calls[1].operation,'input_ack');assert.equal(h.calls[1].sequence,0);
    assert.equal(h.pending.length,0);assert.equal(h.timers.length,0);
    h.window.RustLooper.onInputEvent({token:99,sequence:1,packet:packet(0,[[9,9]])});
    h.window.RustLooper.onInputEvent({token:7,sequence:1,packet:packet(0,[[1.5,-0],[.25,-.5]])});
    await tick();
    const pcm=processor.messages.find(message=>message.type==='pcm').samples;
    assert.deepEqual(Array.from(pcm),[1.5,-0,.25,-.5]);
    assert.equal(h.calls.at(-1).sequence,1);
    assert.equal(stream.nativeStatus.delivery,'acknowledged-push');
    assert.equal(h.calls.some(call=>call.operation==='input_read'),false);
    assert.equal(h.timers.length,0);
    stream.getTracks()[0].stop();
    const count=h.calls.length;
    h.window.RustLooper.onInputEvent({token:7,sequence:2,packet:packet(2,[[9,9]])});
    assert.equal(h.calls.length,count);
});

for(const failure of ['sequence','position','fault','context']) {
    test(`invalid or obsolete pushed ${failure} closes input without acknowledging PCM`,async()=>{
        const h=transport(true),stream=await h.input.getStream({audio:{}});
        h.state.inputs.push({id:0,stream});
        if(failure==='context')h.state.audioContext={state:'running'};
        h.window.RustLooper.onInputEvent({token:7,sequence:failure==='sequence'?2:1,
            ...(failure==='fault'?{error:'Device disconnected'}:{packet:packet(failure==='position'?10:0,[[.25,-.5]])})});
        assert.equal(stream.getTracks()[0].readyState,'ended');
        assert.equal(h.calls.filter(call=>call.operation==='input_ack').length,1);
        assert.equal(h.calls.at(-1).operation,'input_close');
        assert.equal(h.nodes[0].messages.filter(message=>message.type==='pcm').length,0);
    });
}

test('failed push handshake rejects opening and releases its native stream',async()=>{
    const h=transport(true),request=h.window.RustLooper.request;
    h.window.RustLooper.request=async(operation,args)=>{
        if(operation==='input_ack')throw new Error('Capture owner gone');
        return request(operation,args);
    };
    await assert.rejects(h.input.getStream({audio:{}}),/Capture owner gone/);
    assert.equal(h.input.status().length,0);
    assert.equal(h.calls.at(-1).operation,'input_close');
});
