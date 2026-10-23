// Native routing for the reference keyboard gestures. No browser audio code.
(() => {
    'use strict';
    // Original main.js input hardening; legacy JS is not executed natively.
    // Capture must be non-passive so focused controls cannot change on scroll.
    if(typeof window.addEventListener==='function')window.addEventListener('wheel',e=>{
        if(!e.shiftKey&&e.target.tagName==='INPUT'&&(e.target.type==='range'||e.target.type==='number')){
            e.preventDefault();e.stopPropagation();e.target.blur();
        }
    },{passive:false,capture:true});
    // Original reset methods explicitly move the range even while it has focus.
    // Keep the existing target listener responsible for the native audio command.
    if(typeof window.addEventListener==='function')window.addEventListener('dblclick',event=>{
        const target=event.target;
        if(!target||target.tagName!=='INPUT'||target.type!=='range'||target.disabled)return;
        const match=/^loop-(speed|fbk)-slider-(0|[1-9]\d?)$/.exec(target.id||'');
        if(match&&Number(match[2])<20)target.value=match[1]==='speed'?'1':'0.8';
    },{capture:true});
    // Original UIManager.setLoopSpeed musical ratios; native DSP still owns playback.
    window.AionLoopSpeed=(value,sync)=>{
        const speed=parseFloat(value);
        if(Number.isNaN(speed))return null;
        return sync?(speed<0.38?0.25:speed<0.75?0.5:speed<1.5?1:2):speed;
    };
    // Original updateLoopDisplays status text, including its spacing and pending STOP.
    window.AionLoopStatus=(track,sampleRate)=>({
        symbol:({empty:'( )',armed:'(A)',recording:'(R)',playing:'[>]',stopped:'[S]',stopping:'[.]',overdubbing:'(O)',substituting:'(-)',multiplying:'(M)',queued:'[Q]',finalizing:'[..]'})[track.state]||'( )',
        text:track.state==='finalizing'?'SAVING':track.state.toUpperCase(),
        extra:`${track.muted?' [MUTED]':''}${track.length?` (${(track.length/sampleRate/track.playbackRate).toFixed(1)}s)`:''}`
    });
    // Original updateStatus precedence; native capture ownership supplies isRecording.
    window.AionGlobalStatus=(snapshot,mode,{body,header,recording,label})=>{
        const owner=snapshot.tracks.findIndex(t=>['armed','recording','overdubbing','substituting','multiplying'].includes(t.state));
        const state=owner<0?null:snapshot.tracks[owner].state;
        const substitute=mode==='substitute',sustain=!!snapshot.midi?.sustain;
        const flags={
            'mode-overdub':mode==='overdub'&&!sustain,'mode-rec-master':false,
            'mode-substitute':substitute,'mode-sus':sustain&&!substitute,
            'is-recording':state==='recording'||state==='armed',
            'is-overdubbing':state==='overdubbing','is-substituting':state==='substituting',
            'is-multiplying':state==='multiplying','is-queued':snapshot.tracks.some(t=>t.state==='queued')
        };
        for(const [name,on] of Object.entries(flags))body.classList.toggle(name,on);
        if(header)header.textContent=substitute?'MASTER ● SUBSTITUTE MODE':sustain?'MASTER ● SUSTAIN MODE':mode==='overdub'?'MASTER ● OVERDUB MODE':label;
        recording.textContent=owner>=0?`YES (Loop ${owner+1})`:'NO';
        recording.style.color=owner>=0?'#f00':'#464';
    };
    // Original updateLiveLoop display values; apply names through textContent.
    window.AionLiveLoopStatus=(track,id)=>({
        className:`loop ${track.state}`,
        text:track.state==='finalizing'?'SAVING':track.state.toUpperCase(),
        color:({recording:'#f00',armed:'#ff0',playing:'#0f0',stopped:'#0ff',overdubbing:'#f0f',substituting:'#00cccc',multiplying:'#ffaa00',queued:'#0088aa'})[track.state]||'#555',
        name:(track.name||`Loop ${id+1}`).replace(/"/g,'&quot;')
    });
    // Message-thread only. Resolve gestures against the latest acknowledged state.
    window.AionCommandQueue = class {
        static wait = Symbol('waitForState');
        constructor({ready, run, notify, now=Date.now, limit=128}) {
            Object.assign(this,{ready,run,notify,now,limit});
            this.items=[]; this.active=false; this.closed=false;
        }
        enqueue(resolve, {key=null, stop=false, track=null, clearTrack=false, metadata=false, barrier=false}={}) {
            if(this.closed)return;
            if(clearTrack&&(!Number.isInteger(track)||track<0)){this.notify('Track clear requires a valid track.');return;}
            if(stop){
                // Panic cancels performance work; committed/user-entered labels are metadata.
                this.items=this.items.filter(item=>item.metadata);
                if(this.items.length>=this.limit){
                    this.items=this.items.slice(0,Math.max(0,this.limit-1));
                    this.notify('Pending name edit cancelled to reserve room for STOP ALL.');
                }
            }
            else if(clearTrack)this.items=this.items.filter(item=>item.track!==track);
            if(key)this.items=this.items.filter(item=>item.key!==key);
            if(this.items.length>=this.limit){this.notify('Too many pending actions; wait for audio processing.');return;}
            const item={resolve,key,stop,track,clearTrack,metadata,barrier,deadline:this.now()+3000};
            if(stop)this.items.unshift(item);
            else if(clearTrack||barrier||metadata){
                // Labels bypass audio waits, but stay after panic/clears and prior labels.
                // Clear still supersedes its track's labels before inserting here.
                const firstOrdinary=this.items.findIndex(queued=>!queued.stop&&!queued.clearTrack&&!queued.barrier&&(clearTrack||barrier||!queued.metadata));
                this.items.splice(firstOrdinary<0?this.items.length:firstOrdinary,0,item);
            }else this.items.push(item);
            this.pump();
        }
        async pump() {
            if(this.active || this.closed || !this.ready())return;
            this.active=true;
            try {
                while(!this.closed && this.ready() && this.items.length) {
                    const item=this.items.shift();
                    if(!item.stop && this.now()>item.deadline){this.notify('Pending action expired before audio resumed.');continue;}
                    try {
                        const command=item.resolve();
                        if(command===window.AionCommandQueue.wait){this.items.unshift(item);break;}
                        if(command)await this.run(command);
                    }
                    catch(error){this.notify(error.message);}
                }
            } finally {this.active=false;}
        }
        reset() {this.items=[];}
        close() {this.closed=true;this.reset();}
    };
    // A momentary gesture remains cancellable after its start resolver is queued.
    window.AionStutter = class {
        constructor(actions){this.actions=actions;this.presses=new Map();}
        press(id,key){
            if(this.presses.has(key)||[...this.presses.values()].some(p=>p.id===id))return;
            const press={id};this.presses.set(key,press);
            this.actions.start(id,()=>this.presses.get(key)===press);
        }
        release(key){const press=this.presses.get(key);if(!press)return;
            this.presses.delete(key);this.actions.stop(press.id);
        }
        cancelTrack(id){for(const [key,p] of this.presses)if(p.id===id)this.release(key);}
        cancel(){for(const key of [...this.presses.keys()])this.release(key);}
    };
    window.AionPerformance = class {
        constructor(actions, timers = {set:(fn,delay)=>setTimeout(fn,delay), clear:id=>clearTimeout(id)}) {
            this.actions=actions;this.timers=timers;this.now=timers.now||(()=>globalThis.performance?.now?.()??Date.now());this.presses=new Map();this.epoch=0;this.trackEpoch=new Map();
        }
        // Cut at 600ms; only an uninterrupted STOP held for 4s may delete.
        static holdAction(initialState,currentState) {
            return ['stopped','playing','stopping'].includes(initialState)&&['playing','stopping'].includes(currentState)?'stopImmediate':null;
        }
        static deleteAction(initialState,currentState) {
            return ['playing','stopping'].includes(initialState)&&['playing','stopping','stopped'].includes(currentState)?'clear':null;
        }
        press(id,key) {
            const now=this.now(),existing=this.presses.get(key);
            if(existing){
                // Original recovery is fresh-press driven, strictly after 10s.
                // Cancel stale queued/timer work without inventing a release action.
                if(now-existing.start<=10000)return;
                this.release(key,true);
            }
            const track=this.actions.track(id);if(!track)return;
            const press={id,initialState:track.state,canDelete:this.actions.canDelete?.(id)??['playing','stopping'].includes(track.state),start:now,fired:false,erased:false,timer:null,eraseTimer:null};this.presses.set(key,press);
            const epoch=this.epoch,revision=this.trackEpoch.get(id)||0;
            press.valid=()=>!press.cancelled&&epoch===this.epoch&&revision===(this.trackEpoch.get(id)||0);
            this.actions.action(id);
            if(!this.presses.has(key))return;
            press.timer=this.timers.set(()=>{if(this.presses.get(key)===press&&!press.fired){press.fired=true;this.actions.hold(id,press.initialState,press.valid);}},600);
            const erase=()=>{
                if(this.presses.get(key)!==press||press.erased)return;
                const remaining=4000-(this.now()-press.start);
                if(remaining>0){press.eraseTimer=this.timers.set(erase,remaining);return;}
                press.erased=true;if(press.canDelete)this.actions.erase?.(id,press.initialState,press.valid);
            };
            press.eraseTimer=this.timers.set(erase,4000);
        }
        release(key,cancel=false) {
            const press=this.presses.get(key);if(!press)return;
            this.timers.clear(press.timer);this.timers.clear(press.eraseTimer);this.presses.delete(key);
            // Delayed browser timers recover once on release, never on cancellation.
            if(cancel)press.cancelled=true;
            const elapsed=this.now()-press.start;
            if(!cancel&&!press.fired&&elapsed>=600)this.actions.hold(press.id,press.initialState,press.valid);
            if(!cancel&&!press.erased&&press.canDelete&&elapsed>=4000)this.actions.erase?.(press.id,press.initialState,press.valid);
        }
        down(event) {
            const target=event.target;
            const textInput=target?.tagName==='INPUT'&&!['CHECKBOX','RADIO','RANGE','BUTTON','FILE','SUBMIT','RESET'].includes((target.type||'text').toUpperCase());
            if (event.altKey || target?.isContentEditable || textInput
                || ['TEXTAREA','SELECT'].includes(target?.tagName)) return;
            if(event.code==='KeyZ' && (event.ctrlKey||event.metaKey)){event.preventDefault();this.actions.history?.(!!event.shiftKey);return;}
            // Original DEL ALL fallback runs before the generic repeat guard.
            if(event.code==='KeyX'&&!event.ctrlKey&&!event.metaKey){event.preventDefault();this.actions.clearAll?.();return;}
            // Original STOP ALL also runs on repeat, before loop-key debounce.
            if(event.code==='Space'&&!event.ctrlKey&&!event.metaKey){event.preventDefault();this.actions.stop();return;}
            if(event.repeat || event.ctrlKey || event.metaKey)return;
            const key=event.code || event.key;
            if(event.key==='+' || event.code==='NumpadAdd') {event.preventDefault();this.actions.mode('overdub');return;}
            if(event.key==='-' || event.code==='NumpadSubtract') {event.preventDefault();this.actions.mode('substitute');return;}
            if(event.code==='KeyN'){event.preventDefault();this.actions.metronome?.();return;}
            if(event.key==='/' || event.code==='NumpadDivide') {event.preventDefault();this.actions.retrigger();return;}
            if(event.key==='\\') {event.preventDefault();this.actions.multiply?.();return;}
            if(event.code==='KeyH') {event.preventDefault();this.actions.halfSpeed();return;}
            const match=/^(Digit|Numpad)([0-9])$/.exec(event.code);
            if(!match)return;
            // The original maps keypad characters. NumLock navigation is not a
            // digit, and only the main Digit row has the Shift bank fallback.
            const numpad=match[1]==='Numpad';
            if(numpad&&!/^[0-9]$/.test(event.key))return;
            const digit=Number(numpad?event.key:match[2]),id=(digit===0?9:digit-1)+(!numpad&&event.shiftKey?10:0);
            if(!this.actions.track(id))return;
            event.preventDefault();this.press(id,key);
        }
        up(event) {
            const key=event.code || event.key;
            this.release(key);
        }
        cancelTrack(id) {
            this.trackEpoch.set(id,(this.trackEpoch.get(id)||0)+1);
            for(const [key,press] of this.presses)if(press.id===id)this.release(key,true);
        }
        cancel() {++this.epoch;for(const press of this.presses.values()){this.timers.clear(press.timer);this.timers.clear(press.eraseTimer);}this.presses.clear();}
    };
    window.AionExportName=(name,id,now)=>`${(name||`Loop_${id+1}`).replace(/[^a-z0-9]/gi,'_')}_${now}.wav`;
    window.AionMerge = class {
        constructor({current,confirm,clear,schedule=fn=>window.setTimeout(fn,50)}) {
            Object.assign(this,{current,confirm,clear,schedule});this.seen=[];this.generation=null;this.epoch=0;
        }
        cancel(){++this.epoch;}
        observe(snapshot) {
            if(this.generation!==snapshot.generation){this.generation=snapshot.generation;this.seen=snapshot.tracks.map(t=>t.mergeSerial??0);return;}
            snapshot.tracks.forEach((t,id)=>{
                const serial=t.mergeSerial??0;if(this.seen[id]===serial)return;this.seen[id]=serial;
                if(!serial)return;
                const generation=snapshot.generation,epoch=t.clearEpoch,source=t.mergeSource,cancelEpoch=this.epoch;
                const valid=()=>{const s=this.current(),dest=s?.tracks[id],src=s?.tracks[source];return cancelEpoch===this.epoch&&s?.generation===generation&&dest?.clearEpoch===epoch&&dest.mergeSerial===serial&&dest.waveformId===t.waveformId&&src?.waveformId===t.mergeSourceWaveform&&src?.clearEpoch===t.mergeSourceClearEpoch&&['playing','stopped'].includes(src.state);};
                this.schedule(()=>{if(valid()&&this.confirm(`Merge successful. Clear Source Loop ${source+1}?`)&&valid())this.clear(id,source,serial);});
            });
        }
    };
})();
