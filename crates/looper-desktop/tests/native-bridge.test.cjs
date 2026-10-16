const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

function buffer(channels = 2, length = 2, sampleRate = 48000) {
    const data = Array.from({ length: channels }, () => Float32Array.from({length}, (_, i) => i % 2 ? -.5 : .25));
    return { numberOfChannels: channels, length, sampleRate, duration: length / sampleRate, getChannelData: i => data[i], copyToChannel:(source,c)=>data[c].set(source) };
}
function harness(referenceEngine = false, referenceLab = false) {
    const messages = [], updates = [];
    class Loop {
        constructor() { this.id = 0; this.state = 'stopped'; this.audioBuffer = buffer(); this.fallbacks = 0; }
        normalize() { this.fallbacks++; }
        multiplyInstant() { this.fallbacks++; }
        saveStateForUndo() { this.saved = this.audioBuffer; }
    }
    class AudioEngine {
        static async mixBuffersCircular(destination) { destination.fallbacks = (destination.fallbacks || 0) + 1; }
        static bufferToWAV(source) {
            return wav(Array.from({length:source.length},(_, i) => Array.from({length:source.numberOfChannels},(_, c) => source.getChannelData(c)[i])),source.sampleRate,source.numberOfChannels);
        }
    }
    class SamplerManager {
        static _stopSampler(id) { state.samplers[id].state = 'stopped'; }
        static drawWaveform(id) { updates.push(`sampler-${id}`); }
        static reverse(id) { state.samplers[id].fallbacks++; }
        static normalize(id) { state.samplers[id].fallbacks++; }
    }
    class Context {
        constructor() { this.state = 'running'; this.sampleRate = 48000; this.decodeFallbacks = 0; }
        createBuffer(channels, length, sampleRate) { return buffer(channels,length,sampleRate); }
        decodeAudioData(data, success) {
            const view = Object.prototype.toString.call(data) === '[object ArrayBuffer]' && data.byteLength >= 44 ? new DataView(data) : null;
            const ext = view && view.getUint16(20,true)===0xfffe;
            const bits = view && (ext ? view.getUint16(38,true) : view.getUint16(34,true));
            const width = view && view.getUint16(34,true)/8, offset=ext?68:44;
            const sentinel = bits===16 ? 8784 : 1048577, maximum=2**(bits-1)-1;
            const raw=index=>Array.from({length:width},(_,byte)=>view.getUint8(offset+index*width+byte)*2**(byte*8)).reduce((a,b)=>a+b,0)/2**(width*8-bits);
            if (view && view.getUint16(22,true)===1 && data.byteLength===offset+width*2
                && view.getUint32(offset-4,true)===width*2 && [16,24].includes(bits)
                && raw(0)===maximum && raw(1)===sentinel) {
                this.probes = (this.probes || 0) + 1;
                (this.probeBytes ||= []).push(data.slice(0));
                const decoded = buffer(1,2),peak=this.probePeak??1;
                decoded.getChannelData(0)[0]=peak;
                decoded.getChannelData(0)[1]=peak===Math.fround(maximum/(maximum+1)) ? Math.fround(sentinel/(maximum+1))
                    : this.probeReciprocal ? Math.fround(sentinel*Math.fround(1/maximum)) : Math.fround(sentinel/maximum);
                return Promise.resolve(decoded);
            }
            this.decodeFallbacks++; this.lastDecodeData = data; const decoded = buffer(); if (success) success(decoded); return Promise.resolve(decoded);
        }
    }
    const downloads = [];
    class ProjectManager {
        static exportLoop() { this.exportFallbacks = (this.exportFallbacks || 0) + 1; }
        static downloadWAV(data, name) { downloads.push({data,name}); }
    }
    const loop = new Loop();
    const state = { loops:[loop], samplers: [{buffer:buffer(), state:'playing', fallbacks:0}], audioContext: new Context() };
    const context = vm.createContext({ Loop, AudioEngine, SamplerManager, ProjectManager, state, structuredClone, document: {getElementById:() => null}, setTimeout, clearTimeout, console: { error() {} }, btoa, atob,
        UIManager: { generateWaveformPeaks: () => [], updateLoop: id => updates.push(id) },
        window: { AudioContext:Context, ipc: { postMessage: message => messages.push(JSON.parse(message)) } } });
    if (referenceEngine) {
        Object.assign(context.document,{createElement:()=>({}),head:{appendChild(){}}});
        vm.runInContext(fs.readFileSync(path.join(__dirname,'../../../ui/Modules/audioEngine.js'),'utf8'),context);
    }
    const elements=new Map();
    if(referenceLab) {
        for(const [id,value] of [['labStart','0'],['labEnd','1'],['labStatus',''],['labUndoBtn',''],['labZC',''],['labStretch','1.5'],['labPitch','0']]) elements.set(id,{value,textContent:'',disabled:true,checked:false});
        context.document.getElementById=id=>elements.get(id)||null;
        context.document.querySelector=()=>null;
        context.alert=message=>updates.push(message);context.I18n={t:key=>key};
        let labSource=fs.readFileSync(path.join(__dirname,'../../../ui/Modules/tracker.js'),'utf8');
        for(const [from,to] of JSON.parse(fs.readFileSync(path.join(__dirname,'../assets/stretch-edits.json'),'utf8'))) labSource=labSource.replace(from,to);
        vm.runInContext(labSource+'\nglobalThis.actualLab=SampleLab;',context);
        context.actualLab.updateDisplays=()=>{context.actualLab.processedBuffer=null;};
        context.actualLab.renderWaveform=()=>updates.push('waveform');
        context.actualLab.rawBuffer=buffer(2,8,44100);
        context.actualLab.playBuf=buf=>{context.actualLab.played=buf;};
        context.actualLab.stretchFallbacks=[];
        context.actualLab.soundTouchTimeStretch=(source,ratio,pitch)=>{
            context.actualLab.stretchFallbacks.push({source,ratio,pitch});
            const next=buffer(source.numberOfChannels,Math.max(1,Math.round(source.length*ratio)),source.sampleRate);
            for(let c=0;c<next.numberOfChannels;c++) next.getChannelData(c).fill(source.getChannelData(c)[0]);
            return next;
        };
    }
    const originalMix=vm.runInContext('AudioEngine.mixBuffersCircular',context);
    vm.runInContext(fs.readFileSync(path.join(__dirname, '../assets/native-bridge.js'), 'utf8'), context);
    context.window.UIManager = context.UIManager;
    return { context, loop, lab:context.actualLab,elements,projects:ProjectManager, downloads, samplers:SamplerManager, engine: vm.runInContext('AudioEngine',context), originalMix, state, bridge: context.window.RustLooper, messages, updates };
}
const tick = () => new Promise(resolve => setImmediate(resolve));

test('Sample Lab process dispatches frozen ratio/pitch to R3 and commits exact native metadata',async()=>{
    const h=harness(true,true),source=h.lab.rawBuffer;
    h.elements.get('labPitch').value='7';h.lab.process();
    assert.equal(h.lab.isProcessing,true);await new Promise(resolve=>setTimeout(resolve,20));
    const message=h.messages[0];assert.equal(message.operation,'stretch');assert.equal(message.stretch_ratio,1.5);
    assert.equal(message.pitch_semitones,7);assert.equal(message.sample_rate,44100);assert.equal(message.channels,2);
    h.elements.get('labStretch').value='2';h.elements.get('labPitch').value='-12';
    h.bridge.receive({id:message.id,ok:true,value:{engine:'rubberband-r3',length:12,sample_rate:44100,channels:2,pcm:pcm(Array.from({length:12},()=>[.4,-.2]))}});await tick();
    assert.equal(h.lab.processedBuffer.length,12);assert.equal(h.lab.played,h.lab.processedBuffer);
    assert.equal(h.lab.rawBuffer,source);assert.equal(h.lab.isProcessing,false);
    assert.equal(h.lab.stretchFallbacks.length,0);assert.equal(h.bridge.lastStretchEngine,'Rubber Band 4.0.0 R3');
});

test('obsolete R3 source/context replies do not play or restore a stale Lab result',async()=>{
    for(const change of ['source','context','closed','normalize']) {
        const h=harness(true,true);h.lab.process();await new Promise(resolve=>setTimeout(resolve,20));
        if(change==='source') h.lab.rawBuffer=buffer(1);
        if(change==='context') h.state.audioContext=new h.context.window.AudioContext();
        if(change==='closed') h.state.audioContext.state='closed';
        if(change==='normalize') h.lab.normalize();
        h.bridge.receive({id:h.messages[0].id,ok:true,value:{engine:'rubberband-r3',length:12,sample_rate:44100,channels:2,pcm:pcm(Array.from({length:12},()=>[.4,-.2]))}});await tick();
        assert.equal(h.lab.processedBuffer,null);assert.equal(h.lab.played,undefined);assert.equal(h.lab.isProcessing,false);
        assert.equal(h.lab.stretchFallbacks.length,0);
    }
});

test('failed or malformed native stretching retains submitted PCM and parameters for the compatibility fallback',async()=>{
    for(const malformed of [false,true]) {
        const h=harness(true,true),source=h.lab.rawBuffer;
        const pending=h.bridge.stretchSample(source,1.5,3);source.getChannelData(0).fill(99);
        h.bridge.receive({id:h.messages[0].id,ok:malformed,value:{engine:'wrong',length:12},error:'native unavailable'});
        const output=await pending;
        assert.equal(output.length,12);assert.equal(output.getChannelData(0)[0],.25);
        assert.equal(h.lab.stretchFallbacks[0].ratio,1.5);assert.equal(h.lab.stretchFallbacks[0].pitch,3);
        assert.equal(h.bridge.lastStretchEngine,'SoundTouch JS fallback');
    }
});

test('wide/oversized stretch jobs keep original processing and identity settings avoid a native request',async()=>{
    const h=harness(true,true),wide=buffer(4,8,44100);
    const output=await h.bridge.stretchSample(wide,1.5,0);
    assert.equal(output.numberOfChannels,4);assert.equal(h.messages.length,0);
    await h.bridge.stretchSample(h.lab.rawBuffer,200000,0);
    assert.equal(h.messages.length,0);assert.equal(h.lab.stretchFallbacks.length,2);
    h.elements.get('labStretch').value='1';h.lab.process();
    assert.equal(h.lab.processedBuffer.length,8);assert.equal(h.messages.length,0);
});

test('Sample Lab normalization preserves source identity, deferred undo and no-op history',async()=>{
    const h=harness(true,true),source=h.lab.rawBuffer;
    const pending=h.lab.normalize();await tick();
    assert.equal(h.lab._undoBuffer,undefined);assert.equal(source.getChannelData(0)[0],.25);
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm(Array.from({length:8},(_,i)=>i%2?[-1,-1]:[.5,.5])),changed:true}});
    await pending;
    assert.equal(h.lab.rawBuffer,source);assert.equal(h.lab._undoBuffer.getChannelData(0)[0],.25);
    const previous=h.lab._undoBuffer;
    const again=h.lab.normalize();await tick();
    h.bridge.receive({id:h.messages[1].id,ok:true,value:{pcm:pcm(Array.from({length:8},(_,i)=>i%2?[-1,-1]:[.5,.5])),changed:false}});await again;
    assert.equal(h.lab._undoBuffer,previous);assert.equal(h.elements.get('labStatus').textContent,'ALREADY MAXED.');
    h.lab.undo();assert.equal(h.lab.rawBuffer.getChannelData(0)[0],.25);assert.equal(h.lab._undoBuffer,null);
});

test('rapid Lab reverse edits serialize and retain the immediately preceding take for undo',async()=>{
    const h=harness(true,true),original=h.lab.rawBuffer.getChannelData(0).slice();
    const first=h.lab.reverse(),second=h.lab.reverse();await tick();
    assert.equal(h.messages.length,1);
    const reversed=Array.from(original).reverse().map(x=>[x,x]);
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm(reversed)}});await first;await tick();
    assert.equal(h.messages.length,2);assert.equal(Buffer.from(h.messages[1].pcm,'base64').readFloatLE(0),-.5);
    h.bridge.receive({id:h.messages[1].id,ok:true,value:{pcm:pcm(Array.from(original,x=>[x,x]))}});await second;
    assert.deepEqual(h.lab.rawBuffer.getChannelData(0),original);
    assert.deepEqual(Array.from(h.lab._undoBuffer.getChannelData(0)),Array.from(original).reverse());
    h.lab.undo();assert.deepEqual(Array.from(h.lab.rawBuffer.getChannelData(0)),Array.from(original).reverse());
});

test('Lab crop keeps source validation, zero crossing snap and sample-exact undo',async()=>{
    const h=harness(true,true);h.elements.get('labStart').value='.2';h.elements.get('labEnd').value='.8';
    h.elements.get('labZC').checked=true;
    let snaps=0;h.lab.snapToZeroCross=(_buffer,ratio)=>{snaps++;return ratio<.5?.25:.75;};
    const source=h.lab.rawBuffer,pending=h.lab.crop();await tick();
    const message=h.messages[0];assert.equal(message.operation,'slice');assert.equal(message.start_sample,2);assert.equal(message.target_frames,4);assert.equal(snaps,2);
    assert.equal(h.lab.rawBuffer,source);assert.equal(h.lab._undoBuffer,undefined);
    h.bridge.receive({id:message.id,ok:true,value:{pcm:pcm([[.25,.25],[-.5,-.5],[.25,.25],[-.5,-.5]])}});await pending;
    assert.equal(h.lab.rawBuffer.length,4);assert.equal(h.lab.rawBuffer.sampleRate,44100);
    assert.equal(h.elements.get('labStart').value,0);assert.equal(h.elements.get('labEnd').value,1);
    h.lab.undo();assert.equal(h.lab.rawBuffer.length,8);assert.deepEqual(h.lab.rawBuffer.getChannelData(0),source.getChannelData(0));
    h.elements.get('labStart').value='1';h.elements.get('labEnd').value='1';await h.lab.crop();
    assert.ok(h.updates.includes('Selection too short.'));assert.equal(h.messages.length,1);
});

test('Lab source/session/selection changes cancel pending edits without undo or fallback',async()=>{
    for(const change of ['source','context','closed','recording','processing','selection','snap','preview']) {
        const h=harness(true,true);h.elements.get('labStart').value='.25';h.elements.get('labEnd').value='.75';
        const source=h.lab.rawBuffer,pending=h.lab.crop();await tick();
        if(change==='source') h.lab.rawBuffer=buffer();
        if(change==='context') h.state.audioContext=new h.context.window.AudioContext();
        if(change==='closed') h.state.audioContext.state='closed';
        if(change==='recording') h.lab.isRecording=true;
        if(change==='processing') h.lab.isProcessing=true;
        if(change==='selection') h.elements.get('labEnd').value='.5';
        if(change==='snap') h.elements.get('labZC').checked=true;
        if(change==='preview') h.lab.previewRequest++;
        const selected=h.lab.rawBuffer;
        h.bridge.receive({id:h.messages[0].id,ok:false,error:'worker unavailable'});await pending;
        assert.equal(h.lab.rawBuffer,selected);assert.equal(h.lab._undoBuffer,undefined);
        assert.equal(source.getChannelData(0)[0],.25);
    }
});

test('failed Lab edits process submitted PCM snapshots and wider sources retain original behavior',async()=>{
    for(const operation of ['reverse','normalize','crop']) {
        const h=harness(true,true);h.elements.get('labStart').value='.25';h.elements.get('labEnd').value='.75';
        const pending=h.lab[operation]();await tick();h.lab.rawBuffer.getChannelData(0).fill(99);
        h.bridge.receive({id:h.messages[0].id,ok:false,error:'worker unavailable'});await pending;
        const expected=operation==='normalize'?[.5,-1,.5,-1,.5,-1,.5,-1]:operation==='reverse'?[-.5,.25,-.5,.25,-.5,.25,-.5,.25]:[.25,-.5,.25,-.5];
        assert.deepEqual(Array.from(h.lab.rawBuffer.getChannelData(0)),expected);
        assert.equal(h.lab._undoBuffer.getChannelData(0)[0],.25);
    }
    const h=harness(true,true);h.lab.rawBuffer=buffer(4,8,44100);await h.lab.reverse();
    assert.equal(h.messages.length,0);assert.equal(h.lab.rawBuffer.numberOfChannels,4);assert.equal(h.lab.rawBuffer.getChannelData(3)[0],-.5);
});

test('Lab SAVE captures processed audio, label and sample rate before awaiting native encoding',async()=>{
    const h=harness(true,true);h.lab.processedBuffer=buffer(1,8,44100);
    const pending=h.lab.save();h.lab.processedBuffer.getChannelData(0).fill(99);h.lab.processedBuffer=null;await tick();
    replyWav(h,h.messages[0]);await pending;
    assert.match(h.downloads[0].name,/^sampler_processed_\d+\.wav$/);
    const bytes=Buffer.from(h.downloads[0].data);assert.equal(bytes.readUInt32LE(24),44100);assert.equal(bytes.readInt16LE(44),Math.trunc(.25*32767));
});
function actualMultiply(h,patchPunch = false) {
    Object.assign(h.context.document,{createElement:()=>({}),head:{appendChild(){}}});
    Object.assign(h.context,{InputManager:{getRecordingNode:()=>null},Log:{error(){},warn(){}}});
    let source=fs.readFileSync(path.join(__dirname,'../../../ui/Modules/loopTracks.js'),'utf8').replace(...JSON.parse(fs.readFileSync(path.join(__dirname,'../assets/multiply-edits.json'),'utf8'))[0]);
    if (patchPunch) for (const edit of JSON.parse(fs.readFileSync(path.join(__dirname,'../assets/punch-edits.json'),'utf8'))) source=source.replace(...edit);
    vm.runInContext(source
        +'\nUIManager.generateWaveformPeaks=()=>[]; UIManager.updateLoop=()=>{}; UIManager.updateStatus=()=>{}; globalThis.actualLoops=LoopManager;',h.context);
    h.context.actualLoops.createBufferFromChunks = () => buffer(2,160);
    Object.assign(h.state,{isRecording:true,isFinishingRecording:true,recordingLoopId:0,loopRecorder:{disconnect(){}}});
    h.loop.state='multiplying'; h.loop.restart=()=>{h.loop.restarted=true;};
}
function actualPunch(h,patchPunch=true) {
    actualMultiply(h,patchPunch);
    h.loop.state='overdubbing';h.loop.feedback=.8;
    h.state.loopRecorder.port={postMessage(){},onmessage:null};
    Object.assign(h.state.audioContext,{baseLatency:.003,outputLatency:.004});
    Object.assign(h.state,{inputLatencyMs:2,recordingStartOffset:-.003});
    h.context.actualLoops.pushUndoState=()=>{h.loop.undoSnapshot=Array.from({length:h.loop.audioBuffer.numberOfChannels},(_,c)=>h.loop.audioBuffer.getChannelData(c).slice());};
    vm.runInContext('UIManager.updateLoopDisplays=()=>{};',h.context);
}

function actualRecording(h, patched=true, config={}) {
    Object.assign(h.context.document,{createElement:()=>({}),head:{appendChild(){}}});
    Object.assign(h.context,{InputManager:{getRecordingNode:()=>null},Log:{error(){},warn(){}},
        SyncManager:{getBarDuration:()=>config.bar??.06,getLoopLength:()=>config.length??.09}});
    let source=fs.readFileSync(path.join(__dirname,'../../../ui/Modules/loopTracks.js'),'utf8');
    if (patched) for (const edit of JSON.parse(fs.readFileSync(path.join(__dirname,'../assets/recording-edits.json'),'utf8'))) source=source.replace(...edit);
    vm.runInContext(source+'\nUIManager.generateWaveformPeaks=()=>[]; UIManager.updateLoop=()=>{}; UIManager.updateExportButtons=()=>{}; UIManager.updateStatus=()=>{}; globalThis.actualLoops=LoopManager;',h.context);
    Object.assign(h.state.audioContext,{sampleRate:1000,baseLatency:config.latency??.009,outputLatency:0});
    Object.assign(h.state,{syncMode:config.mode??'none',syncEnabled:config.sync??false,loopLength:config.length??.09,
        recordingActualStartTime:.001,inputLatencyMs:0,bpm:123,isRecording:true,isFinishingRecording:true,recordingLoopId:0,
        loopRecorder:{port:{postMessage(){}},disconnect(){this.disconnected=true;}},autoPlayAfterRecord:false,autoRecordNext:false});
    h.loop.state='recording';h.loop.audioBuffer=null;h.loop.graph=config.graph ? {startTime:config.graph}:null;
    h.loop.clear=()=>{throw new Error('Unexpected recording failure');};
    const raw=buffer(config.channels??2,137,1000);
    for (let c=0;c<raw.numberOfChannels;c++) for (let i=0;i<raw.length;i++) raw.getChannelData(c)[i]=c%2 ? -.31+Math.cos(i*.23)*.73 : .23+Math.sin(i*.43)*.81;
    h.context.actualLoops.createBufferFromChunks=()=>raw;
    return raw;
}

test('first recording sends frozen frame metadata and commits native PCM before original cleanup',async()=>{
    for (const [config,mode,length,start,offset] of [
        [{},'free',137,0,0],
        [{mode:'dynamic'},'dynamic',120,0,0],
        [{mode:'dynamic',bar:.2},'dynamic',137,0,0],
        [{mode:'dynamic',bar:0},'free',137,0,0],
        [{sync:true,graph:.013},'precision',90,22,0],
        [{sync:true,graph:.013,start:40},'precision',90,0,18],
        [{sync:true},'sync_fallback',90,0,0]
    ]) {
        const h=harness(true),raw=actualRecording(h,true,config),left=raw.getChannelData(0).slice();
        const pending=h.context.actualLoops.processRecordedData([{}],0,config.start??0);
        assert.equal(h.loop.audioBuffer,null);assert.equal(h.state.isRecording,true);
        const sent=h.messages[0];assert.equal(sent.operation,'finalize_recording');assert.equal(sent.record_mode,mode);
        assert.equal(sent.start_sample,start);assert.equal(sent.destination_offset,offset);
        h.state.bpm=200;h.state.syncMode='changed';
        h.bridge.receive({id:sent.id,ok:true,value:{pcm:pcm(Array.from({length},()=>[.5,-.25]))}});await pending;
        assert.equal(h.loop.audioBuffer.length,length);assert.equal(h.loop.originalBpm,123);
        assert.equal(h.loop.dynamicRecorded,config.mode==='dynamic');assert.equal(h.loop.playbackRate,1);
        assert.equal(h.loop.state,'stopped');assert.equal(h.state.loopRecorder,null);assert.equal(h.state.isRecording,false);
        assert.deepEqual(raw.getChannelData(0),left);assert.equal(h.bridge.lastNativeOperation,'finalize_recording');
    }
});

test('stale first-recording replies cannot restore a replaced take or stop another capture',async()=>{
    for (const transition of ['take','loop','graph','generation','closed','context','recorder']) {
        const h=harness(true);actualRecording(h);const recorder=h.state.loopRecorder;
        const pending=h.context.actualLoops.processRecordedData([{}],0,0);
        const replacement=buffer();
        if (transition==='take') h.loop.audioBuffer=replacement;
        if (transition==='loop') h.state.loops[0]={audioBuffer:replacement};
        if (transition==='graph') h.loop.graph={startTime:4};
        if (transition==='generation') h.context.actualLoops.transportGeneration++;
        if (transition==='closed') h.state.audioContext.state='closed';
        if (transition==='context') h.state.audioContext=new h.context.window.AudioContext();
        if (transition==='recorder') h.state.loopRecorder={port:{},disconnect(){throw new Error('New recorder touched');}};
        const newRecorder=h.state.loopRecorder;
        h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm(Array.from({length:137},()=>[.5,-.25]))}});await pending;
        assert.equal(h.loop.audioBuffer,transition==='take'?replacement:null);
        if (transition==='context'||transition==='recorder') { assert.equal(h.state.loopRecorder,newRecorder);assert.equal(recorder.disconnected,undefined); }
        else { assert.equal(h.state.loopRecorder,null);assert.equal(recorder.disconnected,true); }
        assert.deepEqual(Array.from(replacement.getChannelData(0)),[.25,-.5]);
    }
});

test('recording fallback matches the complete original pipeline despite pending PCM and settings changes',async()=>{
    for (const config of [{latency:0},{},{latency:1},{mode:'dynamic'},{mode:'dynamic',bar:.2},{mode:'dynamic',bar:0},
        {sync:true,graph:.013},{sync:true,graph:.013,start:40},{sync:true,graph:.013,length:.2},
        {sync:true,graph:.013,length:.008},{sync:true},{sync:true,length:.2}]) {
        const reference=harness(true);actualRecording(reference,false,config);
        await reference.context.actualLoops.processRecordedData([{}],0,config.start??0);
        const h=harness(true),raw=actualRecording(h,true,config);
        const pending=h.context.actualLoops.processRecordedData([{}],0,config.start??0);
        raw.getChannelData(0).fill(9);h.state.inputLatencyMs=900;h.state.syncMode='dynamic';h.state.loopLength=1;
        h.state.audioContext.baseLatency=1;h.context.SyncManager.getBarDuration=()=>1;h.context.SyncManager.getLoopLength=()=>1;
        h.bridge.receive({id:h.messages[0].id,ok:false,error:'worker unavailable'});await pending;
        assert.equal(h.loop.audioBuffer.length,reference.loop.audioBuffer.length);
        for (let c=0;c<2;c++) assert.deepEqual(h.loop.audioBuffer.getChannelData(c),reference.loop.audioBuffer.getChannelData(c));
    }
});

test('wide first recordings retain every channel through original processing',async()=>{
    const config={channels:4,mode:'dynamic'};
    const reference=harness(true);actualRecording(reference,false,config);
    await reference.context.actualLoops.processRecordedData([{}],0,0);
    const h=harness(true);actualRecording(h,true,config);
    await h.context.actualLoops.processRecordedData([{}],0,0);
    assert.equal(h.messages.length,0);assert.equal(h.loop.audioBuffer.numberOfChannels,4);
    for (let c=0;c<4;c++) assert.deepEqual(h.loop.audioBuffer.getChannelData(c),reference.loop.audioBuffer.getChannelData(c));
});
test('complete punch commits once after Rust finishes, preserving history and frozen latency metadata',async()=>{
    const h=harness(true);actualPunch(h);const destination=h.loop.audioBuffer;
    const work=h.context.actualLoops.processOverdubData([],0,false);
    assert.equal(h.messages[0].operation,'finalize_punch');assert.equal(h.messages[0].offset,-144);
    assert.ok(Math.abs(h.messages[0].latency_seconds-.009)<1e-12);assert.equal(h.messages[0].feedback,.8);
    assert.equal(h.loop.undoSnapshot,undefined);assert.equal(destination.getChannelData(0)[0],.25);
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm([[.5,-.25],[.75,-.125]])}});await work;
    assert.equal(h.loop.audioBuffer,destination);assert.deepEqual(Array.from(h.loop.undoSnapshot[0]),[.25,-.5]);
    assert.deepEqual(Array.from(destination.getChannelData(0)),[.5,.75]);
    assert.equal(h.state.isRecording,false);assert.equal(h.state.loopRecorder,null);
});
test('replaced punch takes reject delayed PCM and finish only their own capture without saving history',async()=>{
    const h=harness(true);actualPunch(h);
    const work=h.context.actualLoops.processOverdubData([],0,true), replacement=buffer();
    h.loop.audioBuffer=replacement;
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm([[1,1],[1,1]])}});await work;
    assert.equal(h.messages[0].feedback,0);assert.equal(h.loop.audioBuffer,replacement);
    assert.equal(replacement.getChannelData(0)[0],.25);assert.equal(h.loop.undoSnapshot,undefined);
    assert.equal(h.state.isRecording,false);assert.equal(h.state.loopRecorder,null);
});
test('obsolete punch results leave a replacement context and recorder running',async()=>{
    const h=harness(true);actualPunch(h);const destination=h.loop.audioBuffer;
    const work=h.context.actualLoops.processOverdubData([],0,false),replacement={disconnect(){throw new Error('wrong recorder');}};
    h.state.loopRecorder=replacement;h.state.recordingLoopId=1;h.state.audioContext={state:'running'};
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm([[1,1],[1,1]])}});await work;
    assert.equal(h.loop.audioBuffer,destination);assert.equal(destination.getChannelData(0)[0],.25);
    assert.equal(h.loop.undoSnapshot,undefined);assert.equal(h.state.loopRecorder,replacement);assert.equal(h.state.isRecording,true);
});
test('punch fallback matches the original full pipeline using submitted PCM and latency settings',async()=>{
    const expected=harness(true);actualPunch(expected,false);expected.engine.mixBuffersCircular=expected.originalMix;
    const destination=buffer(2,8,1000),raw=buffer(2,137,1000);
    for (let c=0;c<2;c++) for (let i=0;i<raw.length;i++) raw.getChannelData(c)[i]=.2*(c? -1:1)+Math.sin(i*.43+c)*.6;
    expected.loop.audioBuffer=destination;
    expected.context.actualLoops.createBufferFromChunks=()=>raw;
    await expected.context.actualLoops.processOverdubData([],0,false);
    const h=harness(true);actualPunch(h);const submitted=buffer(2,8,1000),take=buffer(2,137,1000);
    for (let c=0;c<2;c++) for (let i=0;i<take.length;i++) take.getChannelData(c)[i]=.2*(c? -1:1)+Math.sin(i*.43+c)*.6;
    const work=h.bridge.finalizePunch(h.state.audioContext,submitted,take,-.003,.8);
    for (let c=0;c<2;c++) {submitted.getChannelData(c).fill(99);take.getChannelData(c).fill(99);}
    h.state.inputLatencyMs=999;h.state.audioContext.baseLatency=999;
    h.bridge.receive({id:h.messages[0].id,ok:false,error:'worker unavailable'});
    const result=await work;
    for (let c=0;c<2;c++) assert.deepEqual(Array.from(result.getChannelData(c)),Array.from(destination.getChannelData(c)));
});
test('interactive MULT commits Rust audio once, saves original history and completes recorder cleanup',async()=>{
    const h=harness();actualMultiply(h);
    const before=h.loop.audioBuffer, output=[[.25,.25],[-.5,-.5],...Array.from({length:160},()=>[.125,-.25])];
    const work=h.context.actualLoops.processMultiplyData([],0);
    assert.equal(h.messages[0].operation,'append_capture');assert.equal(h.loop.saved,undefined);
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm(output)}});await work;
    assert.equal(h.loop.saved,before);assert.equal(h.loop.audioBuffer.length,162);
    assert.equal(h.loop.audioBuffer.getChannelData(1)[161],-.25);
    assert.equal(h.loop.state,'playing');assert.equal(h.loop.restarted,true);
    assert.equal(h.state.isRecording,false);assert.equal(h.state.loopRecorder,null);
});
test('clearing a multiplying loop while Rust works discards the reply and completes the original capture',async()=>{
    const h=harness();actualMultiply(h);
    const work=h.context.actualLoops.processMultiplyData([],0);
    h.loop.audioBuffer=null;h.loop.state='empty';
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm(Array.from({length:162},()=>[0,0]))}});await work;
    assert.equal(h.loop.audioBuffer,null);assert.equal(h.loop.saved,undefined);assert.equal(h.loop.restarted,undefined);
    assert.equal(h.state.isRecording,false);assert.equal(h.state.loopRecorder,null);
});
test('an obsolete MULT reply cannot finish a replacement recorder or context',async()=>{
    const h=harness();actualMultiply(h);const before=h.loop.audioBuffer;
    const work=h.context.actualLoops.processMultiplyData([],0), replacement={disconnect(){throw new Error('new recorder disconnected');}};
    h.state.loopRecorder=replacement;h.state.audioContext={state:'running'};h.state.recordingLoopId=1;
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm(Array.from({length:162},()=>[0,0]))}});await work;
    assert.equal(h.loop.audioBuffer,before);assert.equal(h.loop.saved,undefined);
    assert.equal(h.state.loopRecorder,replacement);assert.equal(h.state.isRecording,true);assert.equal(h.state.recordingLoopId,1);
});
test('failed native append retains its submitted prefix and captured snapshot for original fallback',async()=>{
    const h=harness(), original=buffer(1), take=buffer(1,160);
    take.getChannelData(0).fill(.5);
    h.engine.seamlessLoopCrossfade=()=>{};
    const work=h.bridge.appendCapture(h.state.audioContext,original,take);
    original.getChannelData(0).fill(99);take.getChannelData(0).fill(99);
    h.bridge.receive({id:h.messages[0].id,ok:false,error:'worker unavailable'});
    const result=await work;
    assert.equal(result.numberOfChannels,1);assert.equal(result.sampleRate,48000);
    assert.deepEqual(Array.from(result.getChannelData(0)),[.25,-.5,...new Array(160).fill(0)]);
});
function pcm(frames) {
    const bytes = Buffer.alloc(frames.length * 8);
    frames.forEach((frame, i) => { bytes.writeFloatLE(frame[0], i * 8); bytes.writeFloatLE(frame[1], i * 8 + 4); });
    return bytes.toString('base64');
}
function wav(frames, rate = 48000, channels = 2) {
    const bytes = Buffer.alloc(44 + frames.length * channels * 2);
    bytes.write('RIFF',0); bytes.writeUInt32LE(bytes.length - 8,4); bytes.write('WAVEfmt ',8);
    bytes.writeUInt32LE(16,16); bytes.writeUInt16LE(1,20); bytes.writeUInt16LE(channels,22);
    bytes.writeUInt32LE(rate,24); bytes.writeUInt32LE(rate * channels * 2,28);
    bytes.writeUInt16LE(channels * 2,32); bytes.writeUInt16LE(16,34); bytes.write('data',36);
    bytes.writeUInt32LE(bytes.length - 44,40);
    frames.forEach((frame, i) => frame.slice(0,channels).forEach((raw,c) => {
        const sample = Number.isFinite(raw) ? Math.max(-1,Math.min(1,raw)) : 0;
        bytes.writeInt16LE(Math.trunc(sample * (sample < 0 ? 32768 : 32767)),44 + (i * channels + c) * 2);
    }));
    return bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset + bytes.byteLength);
}
function replyWav(h, message) {
    const data = Buffer.from(message.pcm,'base64');
    let frames = Array.from({length:data.length / 8},(_, i) => [data.readFloatLE(i*8),data.readFloatLE(i*8+4)]);
    if (message.fade_seconds) {
        const source=buffer(message.channels,frames.length,message.sample_rate);
        for (let c=0;c<source.numberOfChannels;c++) source.getChannelData(c).set(frames.map(f=>f[c]));
        h.engine.applyFades(source,message.fade_seconds);
        frames=frames.map((_,i)=>Array.from({length:source.numberOfChannels},(_,c)=>source.getChannelData(c)[i]));
    }
    h.bridge.receive({id:message.id,ok:true,value:{wav:Buffer.from(wav(frames,message.sample_rate,message.channels)).toString('base64')}});
}

function referenceExport(h,source,seconds=0) {
    const copy=buffer(source.numberOfChannels,source.length,source.sampleRate);
    for (let c=0;c<copy.numberOfChannels;c++) copy.getChannelData(c).set(source.getChannelData(c));
    h.engine.applyFades(copy,seconds);
    return Buffer.from(h.engine.bufferToWAV(copy));
}
function actualExports() {
    const h=harness(true),button={disabled:false,textContent:'[E] EXP ALL'},alerts=[],errors=[],songs=[],downloads=[],decoded=[];
    Object.assign(h.state,{masterChunks:[],inputChunks:[]});
    Object.assign(h.context,{Blob,Date:{now:()=>1234},DroneSynth:{instances:[]},TrackerManager:{saveSong:name=>songs.push(name)},
        alert:message=>alerts.push(message),Log:{error:(...args)=>errors.push(args)},
        setTimeout:(fn,delay)=>delay===300 ? (fn(),0) : setTimeout(fn,delay)});
    h.context.window.DroneSynth=h.context.DroneSynth;h.context.window.TrackerManager=h.context.TrackerManager;
    h.context.document.getElementById=id=>id==='projectName'?{value:'Live Set'}:id==='exportAllBtn'?button:null;
    h.context.UIManager.updateExportButtons=()=>h.updates.push('exports');
    h.state.audioContext.decodeAudioData=async()=>{assert.ok(decoded.length,'Unexpected decode');return decoded.shift();};
    let source=fs.readFileSync(path.join(__dirname,'../../../ui/Modules/projectManager.js'),'utf8');
    for (const [from,to] of JSON.parse(fs.readFileSync(path.join(__dirname,'../assets/export-edits.json'),'utf8'))) source=source.replaceAll(from,to);
    vm.runInContext(source+'\nglobalThis.ExportProjectManager=ProjectManager;',h.context);
    const manager=h.context.ExportProjectManager;
    manager.downloadWAV=(data,name)=>downloads.push({data:Buffer.from(data),name});
    return Object.assign(h,{manager,button,alerts,errors,songs,downloads,decoded});
}
async function completeWavExports(h,work,respond=replyWav) {
    let done=false,error,next=h.completedExportMessages??0;
    work.then(()=>{done=true;},e=>{done=true;error=e;});
    for (let i=0;i<200&&!done;i++) {
        await tick();
        if (next<h.messages.length) respond(h,h.messages[next++]);
    }
    h.completedExportMessages=next;
    assert.equal(done,true,'Export did not finish');if(error)throw error;
    await work;
}

test('export all snapshots dry takes, preserves names/order and sends master/input/stem fades through Rust',async()=>{
    const h=actualExports(),first=buffer(2,137,1000),second=buffer(1,18,1000);
    h.state.loops=[{id:0,name:'First / take',audioBuffer:first,stemChunks:[new Blob(['wet'])]},
        {id:1,name:'Second take',audioBuffer:second}];
    h.state.masterChunks=[new Blob(['master'])];h.state.inputChunks=[new Blob(['input'])];
    h.context.DroneSynth.instances=[{id:0,name:'Drone One',stemChunks:[new Blob(['drone'])]}];
    const wet=Array.from({length:4},(_,i)=>buffer(i%2+1,i===3?2:137,1000));
    h.decoded.push(...wet);
    const expected=[referenceExport(h,first),referenceExport(h,second),...wet.map(b=>referenceExport(h,b,.01))];
    const pending=h.manager.exportAllTracks();
    assert.equal(h.button.disabled,true);
    second.getChannelData(0).fill(1);h.state.loops[1].audioBuffer=null;h.state.loops[1].name='Replaced';
    await completeWavExports(h,pending);
    assert.equal(h.bridge.nativeWavEncodes,6);assert.equal(h.messages.length,6);
    assert.deepEqual(h.messages.map(m=>m.fade_seconds),[0,0,.01,.01,.01,.01]);
    assert.deepEqual(h.downloads.map(d=>d.name),['Live_Set_First___take_1234.wav','Live_Set_Second_take_1234.wav',
        'Live_Set_Master_Mix_1234.wav','Live_Set_Input_Rec_1234.wav','Live_Set_Wet_First___take_1234.wav','Live_Set_Wet_Drone_One_1234.wav']);
    h.downloads.forEach((d,i)=>assert.deepEqual(d.data,expected[i]));
    assert.deepEqual(h.songs,['Live_Set_Performance_1234.trk']);assert.deepEqual(h.alerts,['6 track(s) exported!']);
    assert.equal(h.button.disabled,false);assert.equal(h.button.textContent,'[E] EXP ALL');assert.deepEqual(h.errors,[]);
    assert.deepEqual(wet.map(b=>Array.from(b.getChannelData(0))),Array.from({length:4},(_,i)=>Array.from(buffer(i%2+1,i===3?2:137,1000).getChannelData(0))));
});

test('dedicated master and input exports retain original bytes and tracker sidecar, including native failure fallback',async()=>{
    const h=actualExports();h.state.masterChunks=[new Blob(['master'])];h.state.inputChunks=[new Blob(['input'])];
    for (const [method,label,channels] of [['exportMasterOnly','Master_Mix',2],['exportInputOnly','Input_Only',1]]) {
        const source=buffer(channels,137,1000),expected=referenceExport(h,source,.01);h.decoded.push(source);
        const pending=h.manager[method]();let replied=false;
        await completeWavExports(h,pending,(target,message)=>{
            if(replied)return;replied=true;
            target.bridge.receive({id:message.id,ok:false,error:'native unavailable'});
        });
        assert.deepEqual(h.downloads.at(-1),{data:expected,name:`Live_Set_${label}_1234.wav`});
    }
    assert.deepEqual(h.songs,['Live_Set_Performance_1234.trk']);assert.deepEqual(h.errors,[]);
    h.state.masterChunks=[];await h.manager.exportMasterOnly();
    h.state.audioContext.state='closed';await h.manager.exportInputOnly();
    assert.deepEqual(h.alerts,['No master recording found.','Audio context unavailable.']);
});

test('faded WAV fallback validates output, uses submitted PCM and does not leak queue capacity',async()=>{
    const h=harness(true);
    for (const channels of [1,2]) {
        const source=buffer(channels,137,1000),expected=referenceExport(h,source,.01);
        const pending=h.bridge.encodeWAVBuffer(source,.01);source.getChannelData(0).fill(99);await tick();
        const message=h.messages.at(-1);assert.equal(message.fade_seconds,.01);
        h.bridge.receive({id:message.id,ok:true,value:{wav:'invalid'}});
        assert.deepEqual(Buffer.from(await pending),expected);
    }
    assert.equal(h.bridge.nativeWavEncodes,0);
    const next=h.bridge.encodeWAVBuffer(buffer(),0);await tick();replyWav(h,h.messages.at(-1));await next;
    assert.equal(h.bridge.nativeWavEncodes,1);
    for (const invalid of [-1,NaN,Infinity]) await assert.rejects(h.bridge.encodeWAVBuffer(buffer(),invalid),/Invalid export fade/);
});

test('wide and excess queued faded WAV jobs keep the source encoder without changing original PCM',async()=>{
    const h=harness(true),wide=buffer(4,18,1000),expected=referenceExport(h,wide,.01),before=wide.getChannelData(0).slice();
    assert.deepEqual(Buffer.from(await h.bridge.encodeWAVBuffer(wide,.01)),expected);
    assert.deepEqual(wide.getChannelData(0),before);assert.equal(h.messages.length,0);
    const jobs=Array.from({length:32},()=>h.bridge.encodeWAVBuffer(buffer()));
    const overflow=buffer(1,137,1000),overflowBefore=overflow.getChannelData(0).slice();
    assert.deepEqual(Buffer.from(await h.bridge.encodeWAVBuffer(overflow,.01)),referenceExport(h,overflow,.01));
    assert.deepEqual(overflow.getChannelData(0),overflowBefore);
    for (let i=0;i<32;i++) {await tick();replyWav(h,h.messages[i]);await jobs[i];}
    assert.equal(h.bridge.nativeWavEncodes,32);
});

test('one failed bulk export does not discard later files or leave the export control busy',async()=>{
    const h=actualExports();
    h.state.loops=[{id:0,name:'Broken',audioBuffer:buffer()},{id:1,name:'Good',audioBuffer:buffer()}];
    const encode=h.bridge.encodeWAVBuffer;let first=true;
    h.bridge.encodeWAVBuffer=(...args)=>{if(first){first=false;return Promise.reject(new Error('encode failed'));}return encode(...args);};
    await completeWavExports(h,h.manager.exportAllTracks());
    assert.equal(h.downloads.length,1);assert.equal(h.downloads[0].name,'Live_Set_Good_1234.wav');
    assert.deepEqual(h.alerts,['1 track(s) exported!\n\nErrors:\nDry loop 1: encode failed']);
    assert.equal(h.button.disabled,false);assert.equal(h.button.textContent,'[E] EXP ALL');
});

test('twenty-loop project save preserves original schema and PCM16 assets through native encoding', async () => {
    const h = harness(), errors = [], nameInput = {value:'Before'};
    let savedBlob, savedName;
    h.state.loops = Array.from({length:20},(_, id) => ({id,name:`Loop ${id}`,audioBuffer:buffer(),feedback:.6,originalBpm:103,dynamicRecorded:true,params:{test:id},effects:{reverse:true}}));
    h.state.samplers = [{id:0,name:'One shot',buffer:buffer(1),speed:1.25,volume:.6,pan:3,muted:true,isLooping:false},{id:1,buffer:null}];
    Object.assign(h.state,{inputs:[],tracker:{patterns:[{rows:[]}],playlist:[0],mode:'song'},metronome:{volume:.3},keyMapping:{kbd:['a']},countIn:{audio:true},bpm:113});
    Object.assign(h.context,{
        VERSION:'unchanged-format',MAX_LOOPS:20,effects:{},prompt:()=>'Interop Set',
        InputManager:{},DroneSynth:{instances:[],PRESETS:{}},EffectManager:{},
        Log:{error:(...args)=>errors.push(args)},Blob,
        FileReader:class {
            readAsDataURL(blob) { blob.arrayBuffer().then(data => {this.result=`data:audio/wav;base64,${Buffer.from(data).toString('base64')}`;this.onload();}).catch(error=>{this.error=error;this.onerror();}); }
        },
        URL:{createObjectURL:blob=>{savedBlob=blob;return'blob:project';},revokeObjectURL(){}},
        document:{getElementById:()=>nameInput,querySelectorAll:()=>[],createElement:()=>({click(){savedName=this.download;}})}
    });
    let source = fs.readFileSync(path.join(__dirname,'../../../ui/Modules/projectManager.js'),'utf8');
    const edits = JSON.parse(fs.readFileSync(path.join(__dirname,'../assets/project-edits.json'),'utf8'));
    for (const [from,to] of edits) source = source.replace(from,to);
    vm.runInContext(source+'\nglobalThis.NativeProjectManager = ProjectManager;',h.context);
    const pending = h.context.NativeProjectManager.save();
    for (let i = 0; i < 21; i++) {
        await tick();
        assert.equal(h.messages.length,i+1);
        assert.equal(h.messages[i].operation,'encode_wav');
        replyWav(h,h.messages[i]);
    }
    await pending;
    assert.deepEqual(errors,[]);
    const saved = JSON.parse(await savedBlob.text());
    assert.equal(saved.version,'unchanged-format');
    assert.equal(saved.projectName,'Interop Set');
    assert.equal(savedName,'Interop_Set.alp');
    assert.equal(saved.bpm,113);
    assert.equal(saved.loops.length,20);
    assert.equal(saved.loops[19].params.test,19);
    assert.equal(saved.loops[0].feedback,.6);
    assert.equal(saved.loops[0].originalBpm,103);
    assert.equal(saved.loops[0].dynamicRecorded,true);
    assert.deepEqual(saved.tracker.patterns,[{rows:[]}]);
    assert.equal(saved.loops[0].audioWavBase64,Buffer.from(wav([[.25,.25],[-.5,-.5]])).toString('base64'));
    assert.equal(saved.samplers[0].audioWavBase64,Buffer.from(wav([[.25],[-.5]],48000,1)).toString('base64'));
    assert.equal(saved.samplers[0].speed,1.25);
    assert.equal(saved.samplers[0].muted,true);
    assert.equal(saved.samplers[1].audioWavBase64,null);
    assert.equal(h.bridge.nativeWavEncodes,21);
});

test('WAV encoding queue retains each submitted snapshot and recovers after a worker failure', async () => {
    const h = harness(), first = buffer(), second = buffer(1);
    const a = h.bridge.encodeWAVBuffer(first), b = h.bridge.encodeWAVBuffer(second);
    first.getChannelData(0).fill(1);
    second.getChannelData(0).fill(-1);
    await tick();
    assert.equal(h.messages.length,1);
    h.bridge.receive({id:h.messages[0].id,ok:false,error:'worker busy'});
    const fallback = Buffer.from(await a);
    assert.equal(fallback.readInt16LE(44),Math.trunc(.25*32767));
    await tick();
    assert.equal(h.messages.length,2);
    assert.equal(Buffer.from(h.messages[1].pcm,'base64').readFloatLE(0),.25);
    replyWav(h,h.messages[1]);
    assert.equal(Buffer.from(await b).readUInt16LE(22),1);
    assert.equal(h.bridge.nativeWavEncodes,1);
});

test('excess WAV encode jobs retain original encoding without filling the worker queue', async () => {
    const h = harness(), jobs = Array.from({length:33},()=>h.bridge.encodeWAVBuffer(buffer()));
    const fallback = Buffer.from(await jobs[32]);
    assert.equal(fallback.readInt16LE(44),Math.trunc(.25*32767));
    for (let i = 0; i < 32; i++) {
        await tick();
        assert.equal(h.messages.length,i+1);
        replyWav(h,h.messages[i]);
        await jobs[i];
    }
    assert.equal(h.bridge.nativeWavEncodes,32);
});

test('matching-rate WAV import uses Rust and transfers ownership of the input bytes', async () => {
    const h = harness(), data = wav([[.25],[-.5]],48000,1);
    const pending = h.state.audioContext.decodeAudioData(data);
    assert.equal(data.byteLength,0);
    await tick();
    assert.equal(h.messages[0].operation,'decode_wav');
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm([[.25,.25],[-.5,-.5]]),sample_rate:48000,channels:1,length:2}});
    const decoded = await pending;
    assert.equal(decoded.numberOfChannels,1);
    assert.equal(decoded.sampleRate,48000);
    assert.deepEqual(Array.from(decoded.getChannelData(0)),[.25,-.5]);
    assert.equal(h.state.audioContext.decodeFallbacks,0);
});

test('browser integer scaling is calibrated once per context and format', async () => {
    for (const [peak,reciprocal] of [[1,false],[1,true],[32767/32768,false]]) {
        const h = harness(); h.state.audioContext.probePeak = peak;h.state.audioContext.probeReciprocal=reciprocal;
        for (let i = 0; i < 2; i++) {
            const pending = h.state.audioContext.decodeAudioData(wav([[.25,-.5]]));
            await tick();
            assert.equal(h.messages[i].positive_full_scale,peak === 1);
            assert.equal(h.messages[i].positive_reciprocal,reciprocal);
            h.bridge.receive({id:h.messages[i].id,ok:true,value:{pcm:pcm([[.25,-.5]]),sample_rate:48000,channels:2,length:1}});
            await pending;
        }
        assert.equal(h.state.audioContext.probes,1);
    }
});

function extensibleWav(bits=16,valid=16,tag=1,channels=2) {
    const bytes=Buffer.alloc(68+2*channels*bits/8);
    bytes.write('RIFF'); bytes.writeUInt32LE(bytes.length-8,4); bytes.write('WAVEfmt ',8);
    bytes.writeUInt32LE(40,16); bytes.writeUInt16LE(0xfffe,20); bytes.writeUInt16LE(channels,22);
    bytes.writeUInt32LE(48000,24); bytes.writeUInt32LE(48000*channels*bits/8,28);
    bytes.writeUInt16LE(channels*bits/8,32); bytes.writeUInt16LE(bits,34);
    bytes.writeUInt16LE(22,36); bytes.writeUInt16LE(valid,38); bytes.writeUInt32LE(channels===1?4:3,40);
    bytes.set([tag,0,0,0,0,0,0x10,0,0x80,0,0,0xaa,0,0x38,0x9b,0x71],44);
    bytes.write('data',60); bytes.writeUInt32LE(bytes.length-68,64);
    return bytes.buffer.slice(bytes.byteOffset,bytes.byteOffset+bytes.byteLength);
}

test('extensible WAV imports calibrate each container and precision independently',async()=>{
    const h=harness();
    for(const [bits,valid,tag] of [[16,16,1],[24,24,1],[32,24,1],[32,16,1],[32,32,3]]) {
        for(let iteration=0;iteration<2;iteration++) {
            const data=extensibleWav(bits,valid,tag),expected=Buffer.from(data).toString('base64');
            const pending=h.state.audioContext.decodeAudioData(data); await tick();
            assert.equal(data.byteLength,0);
            const message=h.messages.at(-1);
            assert.equal(message.operation,'decode_wav');assert.equal(message.wav,expected);
            assert.equal(message.positive_full_scale,tag===1);
            h.bridge.receive({id:message.id,ok:true,value:{pcm:pcm([[.25,-.5],[1.5,-0]]),sample_rate:48000,channels:2,length:2}});
            assert.equal((await pending).getChannelData(0)[1],1.5);
        }
    }
    assert.equal(h.state.audioContext.probes,4);
    for(const [index,[bits,valid]] of [[16,16],[24,24],[32,24],[32,16]].entries()) {
        const view=new DataView(h.state.audioContext.probeBytes[index]);
        assert.equal(view.getUint16(20,true),0xfffe);
        assert.equal(view.getUint16(34,true),bits);assert.equal(view.getUint16(38,true),valid);
        assert.equal(view.getUint32(40,true),4);assert.equal(view.getUint16(44,true),1);
    }
    assert.equal(h.state.audioContext.decodeFallbacks,0);
});

test('unsupported extensible metadata preserves original browser decoding',async()=>{
    for(const [offset,value] of [[36,21],[36,23],[38,0],[38,33],[40,4],[46,1],[59,0]]) {
        const h=harness(),data=extensibleWav(32,24),view=new DataView(data);
        view.setUint8(offset,value);
        const expected=Buffer.from(data).toString('base64');
        await h.state.audioContext.decodeAudioData(data);
        assert.equal(h.messages.length,0,`offset ${offset}`);
        assert.equal(h.state.audioContext.decodeFallbacks,1);
        assert.equal(Buffer.from(h.state.audioContext.lastDecodeData).toString('base64'),expected);
    }
});

test('unknown browser scaling retains original decoding without a native request', async () => {
    const h = harness(); h.state.audioContext.probePeak = .5;
    await h.state.audioContext.decodeAudioData(wav([[.25,-.5]]));
    assert.equal(h.messages.length,0);
    assert.equal(h.state.audioContext.decodeFallbacks,1);
});

test('resampling, compressed audio and callback overloads retain browser decoding', async () => {
    const h = harness();
    await h.state.audioContext.decodeAudioData(wav([[.1,.2]],44100));
    await h.state.audioContext.decodeAudioData(new ArrayBuffer(4));
    let called = false;
    await h.state.audioContext.decodeAudioData(wav([[.1,.2]]),() => {called = true;});
    await h.state.audioContext.decodeAudioData(new Uint8Array(wav([[.1,.2]])));
    assert.equal(called,true);
    assert.equal(h.state.audioContext.decodeFallbacks,4);
    assert.equal(h.messages.length,0);
});

test('failed native import retains independent original WAV bytes for browser fallback', async () => {
    const h = harness(), data = wav([[.25,-.5]]), expected = Buffer.from(data).toString('base64');
    const pending = h.state.audioContext.decodeAudioData(data);
    await tick();
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{pcm:pcm([[NaN,0]]),sample_rate:48000,channels:2,length:1}});
    await pending;
    assert.equal(h.state.audioContext.decodeFallbacks,1);
    assert.equal(Buffer.from(h.state.audioContext.lastDecodeData).toString('base64'),expected);
});

test('loop export uses native WAV bytes and preserves filename sanitization', async () => {
    const h = harness();
    h.loop.name = 'Take / One';
    const pending = h.projects.exportLoop(0);
    await tick();
    assert.equal(h.messages[0].operation,'encode_wav');
    assert.equal(h.messages[0].channels,2);
    const expected = wav([[.25,.25],[-.5,-.5]]);
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{wav:Buffer.from(expected).toString('base64')}});
    await pending;
    assert.deepEqual(Buffer.from(h.downloads[0].data),Buffer.from(expected));
    assert.match(h.downloads[0].name,/^Take___One_\d+\.wav$/);
});

test('failed export uses its PCM snapshot even if the take changes while waiting', async () => {
    const h = harness();
    const pending = h.projects.exportLoop(0);
    await tick();
    h.loop.audioBuffer.getChannelData(0).fill(1);
    h.loop.audioBuffer = buffer(1);
    h.bridge.receive({id:h.messages[0].id,ok:true,value:{wav:'AAAA'}});
    await pending;
    const exported = Buffer.from(h.downloads[0].data);
    assert.equal(exported.readInt16LE(44),Math.trunc(.25 * 32767));
    assert.equal(exported.readUInt16LE(22),2);
});

test('native normalization copies results into the original stereo take and refreshes UI', async () => {
    const h = harness();
    const pending = h.loop.normalize();
    assert.equal(h.messages[0].operation, 'normalize');
    h.bridge.receive({ id: h.messages[0].id, ok: true, value: { frames: [[.5, .25], [-1, -.5]] } });
    await pending;
    assert.deepEqual(Array.from(h.loop.audioBuffer.getChannelData(0)), [.5, -1]);
    assert.deepEqual(h.updates, [0]);
});

test('take, transport, and context changes reject stale normalization and fallback', async () => {
    const transitions = [
        h => { h.loop.audioBuffer = buffer(); },
        h => { h.loop.state = 'playing'; },
        h => { h.loop.state = 'overdubbing'; },
        h => { h.state.audioContext = new h.context.window.AudioContext(); },
        h => { h.state.audioContext.state = 'closed'; }
    ];
    for (const transition of transitions) for (const ok of [true, false]) {
        const h = harness(), original = h.loop.audioBuffer;
        const pending = h.loop.normalize();
        transition(h);
        h.bridge.receive({ id: h.messages[0].id, ok, value: { frames: [[1, 1], [1, 1]] }, error: 'worker failed' });
        await pending;
        assert.deepEqual(Array.from(original.getChannelData(0)), [.25, -.5]);
        assert.deepEqual(Array.from(h.loop.audioBuffer.getChannelData(0)), [.25, -.5]);
        assert.equal(h.loop.fallbacks, 0);
        assert.deepEqual(h.updates, []);
    }
});

test('active playback and wider layouts preserve original synchronous behavior', async () => {
    const h = harness();
    h.loop.state = 'playing';
    await h.loop.normalize();
    h.loop.state = 'stopped';
    h.loop.audioBuffer = buffer(4);
    await h.loop.normalize();
    assert.equal(h.loop.fallbacks, 2);
    assert.equal(h.messages.length, 0);
});

test('binary normalization carries exact float samples including signed zero', async () => {
    const h = harness();
    const pending = h.loop.normalize();
    const sent = Buffer.from(h.messages[0].pcm, 'base64');
    assert.equal(sent.readFloatLE(0), .25);
    assert.equal(sent.readFloatLE(8), -.5);
    h.bridge.receive({ id:h.messages[0].id, ok:true, value:{pcm:pcm([[.5, -0], [-1, .75]])} });
    await pending;
    assert.ok(Object.is(h.loop.audioBuffer.getChannelData(1)[0], -0));
    assert.equal(h.loop.audioBuffer.getChannelData(0)[1], -1);
});

test('native mixes for one destination run in order and read the preceding committed result', async () => {
    const h = harness(), destination = buffer(), take = buffer();
    const first = h.engine.mixBuffersCircular(destination, take, -.001, .8);
    const second = h.engine.mixBuffersCircular(destination, take, 0, 0);
    await tick();
    assert.equal(h.messages.length, 1);
    assert.equal(h.messages[0].offset, -48);
    h.bridge.receive({ id:h.messages[0].id, ok:true, value:{pcm:pcm([[.4, .2], [.8, .6]])} });
    await first;
    await tick();
    assert.equal(h.messages.length, 2);
    const input = Buffer.from(h.messages[1].pcm, 'base64');
    assert.ok(Math.abs(input.readFloatLE(0) - .4) < .000001);
    assert.equal(h.messages[1].feedback, 0);
    h.bridge.receive({ id:h.messages[1].id, ok:true, value:{pcm:pcm([[.1, .2], [.3, .4]])} });
    await second;
    assert.ok(Math.abs(destination.getChannelData(0)[1] - .3) < .000001);
});

test('context replacement cancels a pending mix without changing its PCM', async () => {
    const h = harness(), destination = buffer();
    const pending = h.engine.mixBuffersCircular(destination, buffer(), 0, .8);
    await tick();
    h.state.audioContext = { state: 'running' };
    h.bridge.receive({ id:h.messages[0].id, ok:true, value:{pcm:pcm([[1, 1], [1, 1]])} });
    await pending;
    assert.equal(destination.getChannelData(0)[0], .25);
});

test('malformed native PCM falls back before any destination samples are changed', async () => {
    const h = harness(), destination = buffer();
    const pending = h.engine.mixBuffersCircular(destination, buffer(), 0, .8);
    await tick();
    h.bridge.receive({ id:h.messages[0].id, ok:true, value:{pcm:pcm([[1, 1], [NaN, 1]])} });
    await pending;
    assert.equal(destination.getChannelData(0)[0], .25);
    assert.equal(destination.fallbacks, 1);
});

test('a response from an older document cannot complete a current edit', async () => {
    const h = harness();
    const pending = h.loop.normalize();
    h.bridge.receive({id:h.messages[0].id, session:'older-document', ok:true, value:{pcm:pcm([[1, 1], [1, 1]])}});
    assert.equal(h.loop.audioBuffer.getChannelData(0)[0], .25);
    h.bridge.receive({id:h.messages[0].id, session:h.messages[0].session, ok:true, value:{pcm:pcm([[.5, 1], [-1, .5]])}});
    await pending;
    assert.equal(h.loop.audioBuffer.getChannelData(0)[0], .5);
});

test('stopped multiply uses Rust repeat, preserves mono/rate and saves the previous take', async () => {
    const h = harness();
    h.loop.audioBuffer = buffer(1, 2, 44100);
    const before = h.loop.audioBuffer;
    const pending = h.loop.multiplyInstant();
    await tick();
    assert.equal(h.messages[0].operation, 'repeat');
    assert.equal(h.messages[0].times, 2);
    h.bridge.receive({id:h.messages[0].id, ok:true, value:{pcm:pcm([[.25,.25],[-.5,-.5],[.25,.25],[-.5,-.5]])}});
    await pending;
    assert.equal(h.loop.audioBuffer.numberOfChannels, 1);
    assert.equal(h.loop.audioBuffer.sampleRate, 44100);
    assert.equal(h.loop.duration, 4 / 44100);
    assert.deepEqual(Array.from(h.loop.audioBuffer.getChannelData(0)), [.25,-.5,.25,-.5]);
    assert.equal(h.loop.saved, before);
});

test('a replaced take cancels both in-flight and queued multiply without saving history', async () => {
    const h = harness();
    const first = h.loop.multiplyInstant(), second = h.loop.multiplyInstant();
    await tick();
    const replacement = buffer();
    h.loop.audioBuffer = replacement;
    h.bridge.receive({id:h.messages[0].id, ok:true, value:{pcm:pcm([[1,1],[1,1],[1,1],[1,1]])}});
    await Promise.all([first, second]);
    assert.equal(h.loop.audioBuffer, replacement);
    assert.equal(h.loop.saved, undefined);
    assert.equal(h.messages.length, 1);
});

test('sampler reverse and normalize serialize and leave the original source buffer intact', async () => {
    const h = harness(), before = h.state.samplers[0].buffer;
    const first = h.samplers.reverse(0), second = h.samplers.normalize(0);
    await tick();
    assert.equal(h.state.samplers[0].state, 'stopped');
    assert.equal(h.messages[0].operation, 'reverse');
    h.bridge.receive({id:h.messages[0].id, ok:true, value:{pcm:pcm([[-.5,-.5],[.25,.25]])}});
    await first;
    await tick();
    assert.equal(h.messages[1].operation, 'normalize');
    assert.equal(Buffer.from(h.messages[1].pcm, 'base64').readFloatLE(0), -.5);
    h.bridge.receive({id:h.messages[1].id, ok:true, value:{pcm:pcm([[-1,-1],[.5,.5]])}});
    await second;
    assert.equal(before.getChannelData(0)[0], .25);
    assert.deepEqual(Array.from(h.state.samplers[0].buffer.getChannelData(0)), [-1,.5]);
    assert.deepEqual(h.updates, ['sampler-0','sampler-0']);
});

test('sampler replacement or resumed playback discards a delayed transform', async () => {
    for (const change of ['replace','play']) {
        const h = harness();
        const pending = h.samplers.reverse(0);
        await tick();
        if (change === 'replace') h.state.samplers[0].buffer = buffer();
        else h.state.samplers[0].state = 'playing';
        const expected = h.state.samplers[0].buffer;
        h.bridge.receive({id:h.messages[0].id, ok:true, value:{pcm:pcm([[-.5,-.5],[.25,.25]])}});
        await pending;
        assert.equal(h.state.samplers[0].buffer, expected);
        assert.deepEqual(h.updates, []);
    }
});

test('failed multiply leaves history untouched and uses the original fallback', async () => {
    const h = harness();
    const pending = h.loop.multiplyInstant();
    await tick();
    h.bridge.receive({id:h.messages[0].id, ok:false, error:'worker busy'});
    await pending;
    assert.equal(h.loop.fallbacks, 1);
    assert.equal(h.loop.saved, undefined);
    h.loop.state = 'playing';
    await h.loop.multiplyInstant();
    assert.equal(h.loop.fallbacks, 2);
    assert.equal(h.messages.length, 1);
});

test('two rapid MULT presses repeat the committed result and keep consecutive undo states', async () => {
    const h = harness();
    const first = h.loop.multiplyInstant(), second = h.loop.multiplyInstant();
    await tick();
    h.bridge.receive({id:h.messages[0].id, ok:true, value:{pcm:pcm([[.25,.25],[-.5,-.5],[.25,.25],[-.5,-.5]])}});
    await first;
    const intermediate = h.loop.audioBuffer;
    await tick();
    assert.equal(Buffer.from(h.messages[1].pcm, 'base64').length, 4 * 8);
    h.bridge.receive({id:h.messages[1].id, ok:true, value:{pcm:pcm(Array.from({length:8}, (_, i) => i % 2 ? [-.5,-.5] : [.25,.25]))}});
    await second;
    assert.equal(h.loop.audioBuffer.length, 8);
    assert.equal(h.loop.saved, intermediate);
});

test('sampler errors and unsupported layouts retain original transform behavior', async () => {
    const h = harness();
    const before = h.state.samplers[0].buffer;
    const pending = h.samplers.reverse(0);
    await tick();
    h.bridge.receive({id:h.messages[0].id, ok:true, value:{pcm:pcm([[NaN,1],[1,1]])}});
    await pending;
    assert.equal(h.state.samplers[0].buffer, before);
    assert.equal(h.state.samplers[0].fallbacks, 1);
    h.state.samplers[0].buffer = buffer(4);
    await h.samplers.normalize(0);
    assert.equal(h.state.samplers[0].fallbacks, 2);
    assert.equal(h.messages.length, 1);
});
