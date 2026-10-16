// Native input adapter for the exact MIDIAccess subset used by the workstation.
// Notes/CC/transport routing and learn remain the original App implementation.
(() => {
    const rpc=(...args)=>window.RustLooper.request(...args);
    let current=null,pending=null,exiting=false;
    function emit(target,type,fields) {
        const event=new Event(type);Object.assign(event,fields);
        target.dispatchEvent(event);
        try {target['on'+type]?.call(target,event);} catch(error){console.error('MIDI handler:',error);}
    }
    function valid(data) {
        if(!Array.isArray(data)||data.length<1||data.length>3||data.some(n=>!Number.isInteger(n)||n<0||n>255))return false;
        const status=data[0];
        const length=(status>=0x80&&status<=0xBF||status>=0xE0&&status<=0xEF||status===0xF2)?3
            :(status>=0xC0&&status<=0xDF||status===0xF1||status===0xF3)?2
            :[0xF6,0xF8,0xFA,0xFB,0xFC,0xFE,0xFF].includes(status)?1:0;
        return data.length===length&&data.slice(1).every(n=>n<128);
    }
    class Input extends EventTarget {
        constructor(info){super();Object.assign(this,{id:info.id,name:info.name,generation:info.generation,type:'input',manufacturer:'',version:'',state:'connected',connection:info.connection,onmidimessage:null,onstatechange:null});this.held=new Set();}
        message(data,receivedTime=performance.now()) {
            const type=data[0]&0xF0,key=((data[0]&15)<<8)|data[1];
            if(type===0x90&&data[2]>0)this.held.add(key);
            if(type===0x80||type===0x90&&data[2]===0)this.held.delete(key);
            emit(this,'midimessage',{data:Uint8Array.from(data),receivedTime});
        }
        release(){for(const key of [...this.held])this.message([0x80|(key>>8),key&127,0]);this.held.clear();}
    }
    class Access extends EventTarget {
        constructor(token){super();this.token=token;this.inputs=new Map();this.outputs=new Map();this.sysexEnabled=false;this.onstatechange=null;this.alive=true;this.nativeStatus={dropped:0,error:null};}
        change(port){emit(port,'statechange',{port});emit(this,'statechange',{port});}
        snapshot(snapshot){
            if(!this.alive)return;
            if(!snapshot||!Array.isArray(snapshot.ports)||snapshot.ports.length>16||!Array.isArray(snapshot.events)||snapshot.events.length>256
                ||!Array.isArray(snapshot.reset)||snapshot.reset.length>16||!Number.isSafeInteger(snapshot.dropped)||snapshot.dropped<0)throw new Error('Invalid native MIDI snapshot.');
            const ids=new Set();
            for(const info of snapshot.ports){
                if(typeof info.id!=='string'||!info.id||info.id.length>4096||ids.has(info.id)||typeof info.name!=='string'||info.name.length>4096
                    ||!['open','closed'].includes(info.connection)||!Number.isSafeInteger(info.generation)||info.generation<0)throw new Error('Invalid native MIDI port.');
                ids.add(info.id);
            }
            // Validate the whole event batch before it can trigger any control.
            for(const event of snapshot.events)if(!ids.has(event.port)||!valid(event.data)||!Number.isSafeInteger(event.age_us)||event.age_us<0)throw new Error('Invalid native MIDI message.');
            for(const id of snapshot.reset)if(!ids.has(id))throw new Error('Invalid native MIDI reset.');
            for(const [id,input] of this.inputs)if(!ids.has(id)){
                input.release();input.state='disconnected';input.connection='closed';this.inputs.delete(id);this.change(input);
            }
            for(const info of snapshot.ports){
                if(!this.alive)return;
                let input=this.inputs.get(info.id);
                if(input&&(input.name!==info.name||input.generation!==info.generation)){input.release();input.state='disconnected';input.connection='closed';this.inputs.delete(info.id);this.change(input);input=null;}
                if(!input){input=new Input(info);this.inputs.set(info.id,input);this.change(input);}
                else if(input.connection!==info.connection){if(info.connection==='closed')input.release();input.connection=info.connection;this.change(input);}
            }
            for(const id of snapshot.reset)this.inputs.get(id).release();
            this.nativeStatus.dropped=snapshot.dropped;
            for(const event of snapshot.events){const input=this.inputs.get(event.port);if(input?.connection==='open')input.message(event.data,performance.now()-event.age_us/1000);}
        }
        async poll(){
            if(!this.alive)return;
            try {
                const snapshot=await rpc('midi_poll',{token:this.token});
                if(!this.alive)return;
                this.snapshot(snapshot);
                if(this.alive)this.timer=setTimeout(()=>this.poll(),8);
            } catch(error){if(this.alive){this.nativeStatus.error=String(error);this.close();console.error('Native MIDI:',error);}}
        }
        close(){
            if(!this.alive)return;
            this.alive=false;clearTimeout(this.timer);
            for(const input of [...this.inputs.values()]){input.release();input.state='disconnected';input.connection='closed';this.inputs.delete(input.id);this.change(input);}
            if(current===this)current=null;
            rpc('midi_close',{token:this.token}).catch(error=>console.error('Native MIDI close:',error));
        }
    }
    Object.defineProperty(navigator,'requestMIDIAccess',{configurable:true,value:function(options={}){
        if(options.sysex)return Promise.reject(new Error('SysEx is not enabled in Rust-Looper.'));
        if(exiting)return Promise.reject(new Error('MIDI page has closed.'));
        if(current?.alive)return Promise.resolve(current);
        if(pending)return pending;
        pending=(async()=>{
            const opened=await rpc('midi_open',{synthetic:window.RustLooper.nativeMidiTestMode===true});
            if(!Number.isSafeInteger(opened.token)||opened.token<=0)throw new Error('Invalid native MIDI token.');
            const access=new Access(opened.token);
            if(exiting){access.close();throw new Error('MIDI page has closed.');}
            try{access.snapshot(opened.snapshot);}catch(error){access.close();throw error;}
            current=access;window.RustLooper.midiAccess=access;
            access.nativeStatus.transport=opened.streaming?'native-push':'poll';
            if(!opened.streaming)access.timer=setTimeout(()=>access.poll(),0);
            return access;
        })().finally(()=>{pending=null;});
        return pending;
    }});
    window.RustLooper.onMidiEvent=async packet=>{
        const access=current;
        if(!access?.alive||packet.token!==access.token)return;
        try {
            if(packet.error)throw new Error(packet.error);
            if(!Number.isSafeInteger(packet.sequence)||packet.sequence<=0||packet.sequence<=(access.sequence||0))throw new Error('Invalid native MIDI sequence.');
            access.snapshot(packet.snapshot);access.sequence=packet.sequence;
            if(access.alive)await rpc('midi_ack',{token:access.token,sequence:packet.sequence});
        }catch(error){if(access.alive){access.nativeStatus.error=String(error);access.close();console.error('Native MIDI:',error);}}
    };
    window.addEventListener('pagehide',()=>{exiting=true;current?.close();});
})();
