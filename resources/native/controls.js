// Native routing for the reference keyboard gestures. No browser audio code.
(() => {
    'use strict';
    // Message-thread only. Resolve gestures against the latest acknowledged state.
    window.AionCommandQueue = class {
        constructor({ready, run, notify, now=Date.now, limit=128}) {
            Object.assign(this,{ready,run,notify,now,limit});
            this.items=[]; this.active=false; this.closed=false;
        }
        enqueue(resolve, {key=null, stop=false}={}) {
            if(this.closed)return;
            if(stop)this.items=[];
            // Stop remains first even when controls arrive while audio is paused.
            if(key)this.items=this.items.filter(item=>item.key!==key);
            if(this.items.length>=this.limit){this.notify('Too many pending actions; wait for audio processing.');return;}
            const item={resolve,key,stop,deadline:this.now()+3000};
            if(stop)this.items.unshift(item);else this.items.push(item);
            this.pump();
        }
        async pump() {
            if(this.active || this.closed || !this.ready())return;
            this.active=true;
            try {
                while(!this.closed && this.ready() && this.items.length) {
                    const item=this.items.shift();
                    if(!item.stop && this.now()>item.deadline){this.notify('Pending action expired before audio resumed.');continue;}
                    try {const command=item.resolve();if(command)await this.run(command);}
                    catch(error){this.notify(error.message);}
                }
            } finally {this.active=false;}
        }
        reset() {this.items=[];}
        close() {this.closed=true;this.reset();}
    };
    window.AionPerformance = class {
        constructor(actions, timers = {set:(fn,delay)=>setTimeout(fn,delay), clear:id=>clearTimeout(id)}) {
            this.actions=actions;this.timers=timers;this.now=timers.now||(()=>globalThis.performance?.now?.()??Date.now());this.presses=new Map();
        }
        // A stop/finish/cancel gesture must not become a delete or a new stop
        // merely because its short action changed the track before the timer ran.
        static holdAction(initialState,currentState) {
            if(initialState==='stopped'&&currentState==='stopped')return 'clear';
            return ['stopped','playing','stopping'].includes(initialState)&&['playing','stopping'].includes(currentState)?'stopImmediate':null;
        }
        press(id,key) {
            if(this.presses.has(key))return;
            const track=this.actions.track(id);if(!track)return;
            const press={id,initialState:track.state,start:this.now(),fired:false,timer:null};this.presses.set(key,press);
            this.actions.action(id);
            if(!this.presses.has(key))return;
            press.timer=this.timers.set(()=>{if(this.presses.get(key)===press&&!press.fired){press.fired=true;this.actions.hold(id,press.initialState);}},600);
        }
        release(key,cancel=false) {
            const press=this.presses.get(key);if(!press)return;
            this.timers.clear(press.timer);this.presses.delete(key);
            // Background WebViews may defer a timer. A real >=600ms release
            // still performs one hold; cancellation/blur never invents one.
            if(!cancel&&!press.fired&&this.now()-press.start>=600)this.actions.hold(press.id,press.initialState);
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
        cancel() {for(const press of this.presses.values())this.timers.clear(press.timer);this.presses.clear();}
    };
})();
