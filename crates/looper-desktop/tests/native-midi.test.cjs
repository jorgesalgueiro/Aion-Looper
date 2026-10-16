const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),path=require('node:path'),vm=require('node:vm');
const snapshot=(ports=[{id:'port',name:'Keyboard',connection:'open',generation:1}],events=[],reset=[],dropped=0)=>({ports,events,reset,dropped});
const message=(data,port='port')=>({port,data,age_us:1000,timestamp_us:17});
function harness(deferredOpen=false,streaming=false){
    const calls=[],pending=[],openings=[],timers=new Map(),handlers={},navigator={};let serial=0,token=0;
    const window={addEventListener:(type,handler)=>{handlers[type]=handler;},RustLooper:{request:(operation,args)=>{
        calls.push({operation,...args});
        if(operation==='midi_open') {
            const value={token:++token,snapshot:snapshot(),streaming};
            return deferredOpen?new Promise(resolve=>openings.push(()=>resolve(value))):Promise.resolve(value);
        }
        if(operation==='midi_poll')return new Promise((resolve,reject)=>pending.push({resolve,reject}));
        return Promise.resolve({});
    }}};
    vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../assets/native-midi.js'),'utf8'),{window,navigator,Event,EventTarget,Uint8Array,performance:{now:()=>500},console:{error(){}},
        setTimeout:(fn,delay)=>{timers.set(++serial,{fn,delay});return serial;},clearTimeout:id=>timers.delete(id)});
    function poll(){const [id,{fn}]=timers.entries().next().value;timers.delete(id);return fn();}
    return {window,navigator,calls,pending,openings,timers,handlers,poll};
}
test('native MIDI access is shared and delivers exact short messages through properties and event listeners',async()=>{
    const h=harness(),a=h.navigator.requestMIDIAccess(),b=h.navigator.requestMIDIAccess();assert.equal(a,b);
    const access=await a;assert.equal(access,await h.navigator.requestMIDIAccess());assert.equal(h.calls.length,1);
    assert.equal(access.sysexEnabled,false);assert.equal(access.outputs.size,0);
    const input=access.inputs.get('port'),received=[],listened=[];
    input.onmidimessage=e=>received.push({data:Array.from(e.data),time:e.receivedTime,target:e.target});
    input.addEventListener('midimessage',e=>listened.push(Array.from(e.data)));
    const work=h.poll(),data=[[0x90,60,100],[0x90,60,0],[0xB0,1,127],[0xE0,0,64],[0xC0,2],[0xF8],[0xFA],[0xFB],[0xFC]];
    h.pending.shift().resolve(snapshot(undefined,data.map(d=>message(d))));await work;
    assert.deepEqual(received.map(e=>e.data),data);assert.deepEqual(listened,data);assert.ok(received.every(e=>e.time===499&&e.target===input));
    assert.equal([...h.timers.values()][0].delay,8);access.close();assert.equal(h.calls.at(-1).operation,'midi_close');
});

test('native push dispatches without a JavaScript polling timer and acknowledges each batch',async()=>{
    const h=harness(false,true),access=await h.navigator.requestMIDIAccess(),received=[];
    access.inputs.get('port').onmidimessage=e=>received.push(Array.from(e.data));
    assert.equal(h.timers.size,0);assert.equal(access.nativeStatus.transport,'native-push');
    await h.window.RustLooper.onMidiEvent({token:999,sequence:1,snapshot:snapshot(undefined,[message([0xFA])])});
    assert.equal(received.length,0);
    await h.window.RustLooper.onMidiEvent({token:access.token,sequence:1,snapshot:snapshot(undefined,[message([0x90,60,100])])});
    assert.deepEqual(received,[[0x90,60,100]]);assert.equal(h.calls.at(-1).operation,'midi_ack');assert.equal(h.calls.at(-1).sequence,1);
    await h.window.RustLooper.onMidiEvent({token:access.token,sequence:2,snapshot:snapshot()});
    assert.equal(h.calls.at(-1).sequence,2);assert.equal(h.timers.size,0);access.close();
});

test('replayed native push batches close the access and release held notes',async()=>{
    const h=harness(false,true),access=await h.navigator.requestMIDIAccess(),received=[];
    access.inputs.get('port').onmidimessage=e=>received.push(Array.from(e.data));
    const packet={token:access.token,sequence:1,snapshot:snapshot(undefined,[message([0x90,60,100])])};
    await h.window.RustLooper.onMidiEvent(packet);await h.window.RustLooper.onMidiEvent(packet);
    assert.equal(access.alive,false);assert.deepEqual(received,[[0x90,60,100],[0x80,60,0]]);assert.match(access.nativeStatus.error,/sequence/);
});
test('hotplug releases held notes, replaces disconnected input objects and rebinds without duplicates',async()=>{
    const h=harness(),access=await h.navigator.requestMIDIAccess(),received=[];let changes=0;
    const bind=()=>{for(const input of access.inputs.values())input.onmidimessage=e=>received.push(Array.from(e.data));};
    access.onstatechange=()=>{changes++;bind();};bind();const old=access.inputs.get('port');
    let work=h.poll();h.pending.shift().resolve(snapshot(undefined,[message([0x92,60,100])]));await work;
    work=h.poll();h.pending.shift().resolve(snapshot([]));await work;
    assert.equal(old.state,'disconnected');assert.equal(access.inputs.size,0);assert.deepEqual(received,[[0x92,60,100],[0x82,60,0]]);
    work=h.poll();h.pending.shift().resolve(snapshot(undefined,[message([0x90,61,99])]));await work;
    assert.notEqual(access.inputs.get('port'),old);assert.equal(changes,2);assert.deepEqual(received.at(-1),[0x90,61,99]);
    const replaced=access.inputs.get('port');work=h.poll();
    h.pending.shift().resolve(snapshot([{id:'port',name:'Keyboard',connection:'open',generation:2}],[message([0x90,62,99])]));await work;
    assert.equal(replaced.state,'disconnected');assert.notEqual(access.inputs.get('port'),replaced);
    assert.deepEqual(received.slice(-2),[[0x80,61,0],[0x90,62,99]]);
    access.close();
});
test('MIDI overflow resets held notes while keeping the port available for fresh messages',async()=>{
    const h=harness(),access=await h.navigator.requestMIDIAccess(),input=access.inputs.get('port'),received=[];input.onmidimessage=e=>received.push(Array.from(e.data));
    let work=h.poll();h.pending.shift().resolve(snapshot(undefined,[message([0x90,60,100]),message([0x91,62,100])]));await work;
    work=h.poll();h.pending.shift().resolve(snapshot(undefined,[],['port'],12));await work;
    assert.deepEqual(received.slice(2),[[0x80,60,0],[0x81,62,0]]);assert.equal(access.alive,true);assert.equal(access.nativeStatus.dropped,12);
    work=h.poll();h.pending.shift().resolve(snapshot(undefined,[message([0x90,64,100])]));await work;assert.deepEqual(received.at(-1),[0x90,64,100]);access.close();
});
test('malformed MIDI batches cannot trigger a partial control and close the failed access',async()=>{
    for(const invalid of [{...message([0x90,60,128])},message([0xF0,1,0xF7]),message([0x90,60,100],'other'),{...message([0xFA]),age_us:-1}]){
        const h=harness(),access=await h.navigator.requestMIDIAccess(),received=[];access.inputs.get('port').onmidimessage=e=>received.push(Array.from(e.data));
        const work=h.poll();h.pending.shift().resolve(snapshot(undefined,[message([0x90,61,100]),invalid]));await work;
        assert.deepEqual(received,[]);assert.equal(access.alive,false);assert.equal(access.inputs.size,0);assert.equal(h.timers.size,0);
        assert.match(access.nativeStatus.error,/Invalid native MIDI/);
    }
});
test('page exit closes pending reads and opening requests without accepting late MIDI',async()=>{
    const h=harness(),access=await h.navigator.requestMIDIAccess(),received=[];access.inputs.get('port').onmidimessage=e=>received.push(Array.from(e.data));
    const work=h.poll();h.handlers.pagehide();h.pending.shift().resolve(snapshot(undefined,[message([0x90,60,100])]));await work;
    assert.deepEqual(received,[]);assert.equal(h.timers.size,0);assert.equal(h.calls.filter(c=>c.operation==='midi_close').length,1);
    await assert.rejects(h.navigator.requestMIDIAccess(),/closed/);
    const opening=harness(true),request=opening.navigator.requestMIDIAccess();opening.handlers.pagehide();opening.openings.shift()();
    await assert.rejects(request,/closed/);assert.equal(opening.calls.at(-1).operation,'midi_close');
});
test('MIDI errors release notes and allow a new access while SysEx is rejected before opening',async()=>{
    const h=harness();await assert.rejects(h.navigator.requestMIDIAccess({sysex:true}),/SysEx/);assert.equal(h.calls.length,0);
    const access=await h.navigator.requestMIDIAccess(),received=[];access.inputs.get('port').onmidimessage=e=>received.push(Array.from(e.data));
    let work=h.poll();h.pending.shift().resolve(snapshot(undefined,[message([0x90,60,100])]));await work;
    work=h.poll();h.pending.shift().reject(new Error('connection lost'));await work;
    assert.deepEqual(received,[[0x90,60,100],[0x80,60,0]]);assert.equal(access.alive,false);
    const replacement=await h.navigator.requestMIDIAccess();assert.notEqual(replacement,access);assert.notEqual(replacement.token,access.token);replacement.close();
});
test('closing access inside a MIDI callback stops later messages from the same batch',async()=>{
    const h=harness(),access=await h.navigator.requestMIDIAccess();let starts=0;
    access.inputs.get('port').onmidimessage=e=>{if(e.data[0]===0x90){starts++;access.close();}};
    const work=h.poll();h.pending.shift().resolve(snapshot(undefined,[message([0x90,60,100]),message([0x90,61,100])]));await work;
    assert.equal(starts,1);assert.equal(access.alive,false);assert.equal(h.timers.size,0);
});
