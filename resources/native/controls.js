// Native routing for the reference keyboard gestures. No browser audio code.
(() => {
    'use strict';
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
            if(this.presses.has(key))return;
            const track=this.actions.track(id);if(!track)return;
            const press={id,initialState:track.state,canDelete:this.actions.canDelete?.(id)??['playing','stopping'].includes(track.state),start:this.now(),fired:false,erased:false,timer:null,eraseTimer:null};this.presses.set(key,press);
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
            if(event.repeat || event.ctrlKey || event.metaKey)return;
            const key=event.code || event.key;
            if(this.presses.has(key))return;
            if(event.key==='+' || event.code==='NumpadAdd') {event.preventDefault();this.actions.mode('overdub');return;}
            if(event.key==='-' || event.code==='NumpadSubtract') {event.preventDefault();this.actions.mode('substitute');return;}
            if(event.code==='Space') {event.preventDefault();this.actions.stop();return;}
            if(event.key==='/' || event.code==='NumpadDivide') {event.preventDefault();this.actions.retrigger();return;}
            if(event.key==='\\') {event.preventDefault();this.actions.multiply?.();return;}
            if(event.code==='KeyH') {event.preventDefault();this.actions.halfSpeed();return;}
            const match=/^(?:Digit|Numpad)([0-9])$/.exec(event.code);
            if(!match)return;
            const digit=Number(match[1]),id=(digit===0?9:digit-1)+(event.shiftKey?10:0);
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
})();
