// Native routing for the reference keyboard gestures. No browser audio code.
(() => {
    'use strict';
    window.AionPerformance = class {
        constructor(actions, timers = {set:setTimeout, clear:clearTimeout}) {
            this.actions=actions; this.timers=timers; this.presses=new Map();
        }
        down(event) {
            const target=event.target;
            if (event.repeat || event.ctrlKey || event.metaKey || event.altKey || target?.isContentEditable
                || ['INPUT','TEXTAREA','SELECT','BUTTON'].includes(target?.tagName)) return;
            const key=event.code || event.key;
            if(this.presses.has(key))return;
            if(event.key==='+' || event.code==='NumpadAdd') {event.preventDefault();this.actions.mode('overdub');return;}
            if(event.key==='-' || event.code==='NumpadSubtract') {event.preventDefault();this.actions.mode('substitute');return;}
            if(event.code==='Space') {event.preventDefault();this.actions.stop();return;}
            if(event.key==='/' || event.code==='NumpadDivide') {event.preventDefault();this.actions.retrigger();return;}
            const match=/^(?:Digit|Numpad)([0-9])$/.exec(event.code);
            if(!match)return;
            const digit=Number(match[1]),id=(digit===0?9:digit-1)+(event.shiftKey?10:0);
            if(!this.actions.track(id))return;
            event.preventDefault();this.actions.action(id);
            this.presses.set(key,this.timers.set(()=>{this.actions.hold(id);},600));
        }
        up(event) {
            const key=event.code || event.key;
            if(this.presses.has(key)){this.timers.clear(this.presses.get(key));this.presses.delete(key);}
        }
        cancel() {for(const timer of this.presses.values())this.timers.clear(timer);this.presses.clear();}
    };
})();
