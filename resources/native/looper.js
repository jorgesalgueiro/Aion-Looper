// Native controls for the preserved ASCII interface. Audio stays in C++.
(() => {
    'use strict';
    document.addEventListener('DOMContentLoaded', () => {
        const rpc = window.AionNative?.request;
        if (!rpc || !window.AionLoopMarkup) return;
        let alive = true, mode = 'normal', snapshot = null, pending = null, busy = false;
        let lastFrame = -1, advancedAt = 0, pollTimer, error = '';
        const rows = [], input = document.getElementById('inputTracksContainer');
        // The only new area is native input configuration, inside the original INPUT MIXER.
        input.innerHTML = `<div class="mixer-row input-track-row" style="display:block">
            <div class="mixer-label">NATIVE AUDIO INPUT</div>
            <label><input id="native-monitor" type="checkbox"> MONITOR</label>
            <p>Standalone: choose device/ASIO driver and buffer in Options, then Unmute Input. Plugins use the host's audio routing.</p>
            <p id="native-status" role="status">Waiting for audio...</p>
            <p>Session audio is temporary; save/export is not ported. Changing sample rate clears audio.</p></div>`;
        const output = document.createElement('div'); output.className='mixer-row';
        output.innerHTML=`<label for="native-master" class="mixer-label">VOLUME</label>
            <input id="native-master" class="mixer-slider" type="range" min="0" max="2" step="0.01" value="0" aria-label="Master output volume">
            <output id="native-master-value">MUTED</output>`;
        document.querySelector('#mod-master .module-content').append(output);
        // Capture original semantic selectors before stripping inline browser audio handlers.
        const stopAll=document.querySelector('#mod-menu [onclick="stopAll()"]'); stopAll.id='native-stop-all';
        const clearAll=document.querySelector('#mod-menu [onclick="clearAll()"]');
        // Remove legacy handlers before enabling any control; original source files remain intact.
        const stripHandlers = element => {
            for (const node of [element, ...element.querySelectorAll('*')]) {
                for (const attribute of [...node.attributes]) if (/^on/i.test(attribute.name)) node.removeAttribute(attribute.name);
            }
        };
        stripHandlers(document.documentElement);
        document.getElementById('startOverlay')?.remove();
        const fitLogo = () => {
            const pre = document.querySelector('#header pre');
            if (!pre) return;
            pre.style.zoom = '';
            const width = Math.min(window.innerWidth, document.documentElement.clientWidth) - 80;
            if (pre.scrollWidth > width) pre.style.zoom = width / pre.scrollWidth;
        };
        fitLogo(); window.addEventListener('resize', fitLogo);
        const sticky = document.querySelector('.sticky-top');
        const fitScrollTargets = () => {
            const margin = `${Math.ceil(sticky?.getBoundingClientRect().height || 0) + 12}px`;
            document.getElementById('loopContainer').style.scrollMarginTop = margin;
            document.getElementById('ws-loops').style.scrollMarginTop = margin;
        };
        const stickyObserver = new ResizeObserver(fitScrollTargets);
        if (sticky) stickyObserver.observe(sticky);
        fitScrollTargets();
        document.querySelectorAll('button,input,select,textarea').forEach(e => {e.disabled=true; e.title='Unavailable in this preview';});
        const status = input.querySelector('#native-status');
        const master = output.querySelector('#native-master'), monitor = input.querySelector('#native-monitor');
        const autoPlay = document.getElementById('autoPlayAfterRecord');
        const enable = element => {element.disabled=false;element.title=element.getAttribute('aria-label')||element.textContent.trim();};
        [master,monitor,autoPlay].forEach(enable);
        const bindTabs=(buttonClass,panelClass,ids)=>{
            const buttons=[...document.querySelectorAll(buttonClass)];
            buttons.forEach((button,i)=>{enable(button);button.addEventListener('click',()=>{
                document.querySelectorAll(panelClass).forEach(p=>p.classList.remove('active'));
                buttons.forEach(b=>b.classList.remove('active'));
                document.getElementById(ids[i]).classList.add('active');button.classList.add('active');
            });});
        };
        bindTabs('.main-tab-btn','.workspace-panel',['ws-loops','ws-drone','ws-tracker','ws-live']);
        bindTabs('.manual-tab-btn','.manual-content',['manual-en','manual-es','manual-pt']);

        document.querySelector('input[name=syncMode][value=off]').checked=true;
        const mapping=document.getElementById('keyMappingUI');
        mapping.innerHTML='<div class="mapping-tabs"><button class="tab-btn active">Keyboard</button></div><div class="mapping-scroll-container"><table class="mapping-table">'+Array.from({length:20},(_,i)=>`<tr><td>Loop ${i+1}</td><td>${i<10?'':'Shift+'}${(i+1)%10}</td></tr>`).join('')+'<tr><td>Global OD</td><td>+</td></tr><tr><td>Global SUB</td><td>-</td></tr><tr><td>STOP ALL</td><td>Space</td></tr><tr><td>Retrigger selected loop</td><td>/</td></tr></table></div>';
        document.getElementById('midiStatus').textContent='Native MIDI mapping is not ported yet.';
        let selected=0;
        let urgentStop=false;
        const controls = new Map();
        const queueControl=(action,track,value)=>{if(!enabled())return;controls.set(`${track}:${action}`,{action,track,value});drainControls();};
        const drainControls=()=>{if(busy)return;if(urgentStop){urgentStop=false;command('stopAll',0);return;}if(!controls.size)return;const [key,c]=controls.entries().next().value;controls.delete(key);command(c.action,c.track,c.value);};
        const enabled = () => snapshot?.audioReady && Date.now()-advancedAt < 1000;
        const actionFor = t => t.state === 'empty' ? 'record' : t.state === 'stopped' ? 'play'
            : t.state === 'playing' ? (mode === 'normal' ? 'stop' : mode) : 'finish';
        const notify = message => {error=String(message); status.textContent=error;};
        async function setParameter(id, value) {
            try {const reply=await rpc({operation:'setParameter',id,value});if(!reply.ok)throw Error(reply.error);}
            catch(e) {notify(e.message);}
        }
        async function command(action, track, value=0) {
            if (busy || !enabled()) return;
            if (['overdub','substitute'].includes(action) && snapshot.tracks[track].playbackRate!==1) {notify('Reset loop speed to 1.0x before overdub or substitute.');return;}
            busy=true; error=''; refresh();
            try {
                const generation=snapshot.generation;
                const reply=await rpc({operation:'loop',action,track,value,generation,expiresAt:Date.now()+3000});
                if(!reply.ok)throw Error(reply.error || 'Command was not queued');
                await new Promise((resolve,reject) => {pending={id:reply.requestId,generation,resolve,reject,deadline:Date.now()+4000};});
            } catch(e) {notify(e.message);}
            finally {busy=false;refresh();drainControls();}
        }
        master.addEventListener('input', () => {output.querySelector('#native-master-value').textContent=Number(master.value).toFixed(2);setParameter('masterGain',Number(master.value));});
        monitor.addEventListener('change', () => setParameter('inputMonitor',monitor.checked?1:0));
        autoPlay.addEventListener('change',()=>setParameter('autoPlayAfterRecord',autoPlay.checked?1:0));
        const stopSession=()=>{keyboard.cancel();controls.clear();urgentStop=true;drainControls();};
        stopAll.addEventListener('click', stopSession);
        clearAll.addEventListener('click', () => {if(window.confirm('Clear ALL loops and master recordings?')){keyboard.cancel();controls.clear();command('clearAll',0);}});
        const mainAction=id=>{
            selected=id;
            const owner=snapshot.tracks.findIndex(t=>['recording','overdubbing','substituting'].includes(t.state));
            if(snapshot.tracks[id].state==='empty'&&owner>=0&&owner!==id){command('finish',owner);return;}
            command(actionFor(snapshot.tracks[id]),id);
        };
        const keyboard = new window.AionPerformance({
            track:id=>snapshot?.tracks[id],
            action:id=>mainAction(id),
            hold:id=>{const state=snapshot?.tracks[id].state;if(state==='stopped')command('clear',id);else if(state==='playing')command('stop',id);},
            mode:next=>{mode=mode===next?'normal':next;refresh();},
            stop:stopSession,retrigger:()=>command('retrigger',selected),halfSpeed:()=>{if(snapshot?.tracks[selected].length)queueControl('speed',selected,snapshot.tracks[selected].playbackRate===1?0.5:1);}
        });
        document.addEventListener('keydown',e=>keyboard.down(e));
        document.addEventListener('keyup',e=>keyboard.up(e));
        window.addEventListener('blur',()=>keyboard.cancel());
        function createRows(s) {
            const markup=window.AionLoopMarkup(s,mode), container=document.getElementById('loopContainer');
            container.replaceChildren();
            for(let i=0;i<markup.length;++i) {
                const wrapper=document.createElement('div');wrapper.id=`loop-wrapper-${i}`;
                const row=document.createElement('div');row.id=`loop-${i}`;row.className='loop empty';row.innerHTML=markup[i];
                stripHandlers(row);
                row.querySelectorAll('button,input,select,textarea').forEach(e => {e.disabled=true;e.title='Unavailable in this preview';});
                const bind=(element,callback,event='click') => {element.dataset.nativeControl='true';element.title=element.getAttribute('aria-label')||element.textContent.trim();element.addEventListener(event,callback);return element;};
                const main=bind(row.querySelector(`#loop-action-${i}`),()=>mainAction(i));
                const volume=bind(row.querySelector(`#loop-vol-slider-${i}`),e=>queueControl('volume',i,Number(e.target.value)),'input');
                const feedback=bind(row.querySelector(`#loop-fbk-slider-${i}`),e=>queueControl('feedback',i,Number(e.target.value)),'input');
                const pan=bind(row.querySelector(`#loop-pan-slider-${i}`),e=>queueControl('pan',i,Number(e.target.value)),'input');
                const speed=bind(row.querySelector(`#loop-speed-slider-${i}`),e=>queueControl('speed',i,Number(e.target.value)),'input');
                const half=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='1/2'),()=>queueControl('speed',i,snapshot.tracks[i].playbackRate===1?0.5:1));
                const reset=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='1x'),()=>queueControl('speed',i,1));
                speed.addEventListener('dblclick',()=>queueControl('speed',i,1));
                const mute=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='MUTE'),()=>command('mute',i,snapshot.tracks[i].muted?0:1));
                bind(row.querySelector('.loop-track-delete'),()=>command('clear',i));
                const solo=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='SOLO'),()=>command('solo',i));
                const retrigger=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='RTRG'),()=>command('retrigger',i));
                feedback.addEventListener('dblclick',()=>queueControl('feedback',i,0.8));
                row.addEventListener('pointerdown',()=>{selected=i;});
                const canvas=row.querySelector('canvas');canvas.title='Playback position';
                rows.push({row,main,volume,feedback,pan,speed,half,reset,mute,solo,retrigger,canvas});wrapper.append(row);container.append(wrapper);
            }
        }
        function refresh() {
            if(!snapshot)return;
            const active=enabled();
            if(document.activeElement!==master)master.value=snapshot.masterGain;
            if(document.activeElement!==monitor)monitor.checked=snapshot.inputMonitor;
            output.querySelector('#native-master-value').textContent=snapshot.masterGain===0?'MUTED':snapshot.masterGain.toFixed(2);
            stopAll.disabled=!active;clearAll.disabled=!active||busy;
            if(document.activeElement!==autoPlay)autoPlay.checked=snapshot.autoPlayAfterRecord;
            document.getElementById('activeCount').textContent=`${snapshot.tracks.filter(t=>t.state!=='empty').length}/20`;
            document.getElementById('recStatus').textContent=snapshot.tracks.some(t=>['recording','overdubbing','substituting'].includes(t.state))?'RECORDING':'READY';
            const symbols={empty:'( )',recording:'(R)',playing:'[>]',stopped:'[S]',overdubbing:'(O)',substituting:'(-)'};
            rows.forEach((r,i)=>{
                const t=snapshot.tracks[i];r.row.className=`loop ${t.state}`;
                r.row.querySelectorAll('[data-native-control]').forEach(e=>e.disabled=!active||(busy&&!e.matches('input[type=range]')));
                r.main.disabled=!active||busy||(t.state==='empty'&&snapshot.freeFrames===0);
                r.main.textContent=actionFor(t)==='substitute'?'REPLACE':actionFor(t).toUpperCase();r.main.setAttribute('aria-label',`Loop ${i+1}: ${r.main.textContent}`);r.main.title=r.main.getAttribute('aria-label');
                r.row.querySelector(`#loop-state-symbol-${i}`).textContent=symbols[t.state];
                r.row.querySelector(`#loop-state-text-${i}`).textContent=t.state.toUpperCase();
                r.row.querySelector(`#loop-extra-info-${i}`).textContent=`${t.muted?'[MUTED] ':''}${t.length?`(${(t.length/snapshot.sampleRate/t.playbackRate).toFixed(1)}s)`:''}`;
                if(document.activeElement!==r.volume)r.volume.value=t.volume;
                if(document.activeElement!==r.feedback)r.feedback.value=t.feedback;
                r.row.querySelector(`#loop-vol-display-${i}`).textContent=t.volume>0?`${(20*Math.log10(t.volume)).toFixed(1)}dB`:'-inf';
                r.row.querySelector(`#loop-fbk-display-${i}`).textContent=`${Math.round(t.feedback*100)}%`;
                if(document.activeElement!==r.pan)r.pan.value=t.pan;
                if(document.activeElement!==r.speed)r.speed.value=t.playbackRate;
                r.row.querySelector(`#loop-pan-display-${i}`).textContent=t.pan;
                r.row.querySelector(`#loop-speed-display-${i}`).textContent=`${t.playbackRate.toFixed(2)}x`;
                const editing=['recording','overdubbing','substituting'].includes(t.state);
                r.speed.disabled=!active||editing;r.half.disabled=!active||busy||editing||!t.length;
                r.reset.disabled=!active||busy||editing||t.playbackRate===1;
                r.retrigger.disabled=!active||busy||!t.length||t.state==='recording';
                r.solo.setAttribute('aria-pressed',String(t.solo));r.solo.style.cssText=t.solo?'background:#ff0;color:#000;border-color:#ff0':'';
                r.mute.style.cssText=t.muted?'background:#f00;color:#000;border-color:#f00':'';
                r.mute.textContent=t.muted?'UNM':'MUTE';r.mute.setAttribute('aria-pressed',String(t.muted));
                const ctx=r.canvas.getContext('2d'),w=r.canvas.width,h=r.canvas.height;ctx.clearRect(0,0,w,h);ctx.fillStyle=t.state==='recording'?'#f00':'#0f0';
                if(t.length)ctx.fillRect(0,0,w*t.position/t.length,h);
                r.row.querySelector(`#loop-ascii-vu-${i}`).textContent='|'.repeat(Math.min(20,Math.round(t.peak*20)));
            });
            document.getElementById('odModeIndicator').style.display=mode==='overdub'?'':'none';
            document.getElementById('subModeIndicator').style.display=mode==='substitute'?'':'none';
            document.body.classList.toggle('mode-overdub',mode==='overdub');
            document.body.classList.toggle('mode-substitute',mode==='substitute');
            document.body.classList.toggle('is-recording',snapshot.tracks.some(t=>t.state==='recording'));
            const db=value=>value>0?`${(20*Math.log10(value)).toFixed(1)} dBFS`:'-inf dBFS';
            document.getElementById('master-ascii-vu').textContent=`MASTER | ${db(snapshot.outputPeak)}`;
            status.textContent=error || (!active?'Waiting for audio. Enable the audio device or host processing.':
                snapshot.poolFull?'Recording stopped: session audio memory is full. Clear a loop to free space.':
                snapshot.audioReset?'Session audio cleared after a sample-rate change.':
                `INPUT ${db(snapshot.inputPeak)} | OUTPUT ${db(snapshot.outputPeak)} | ${(snapshot.freeFrames/snapshot.sampleRate).toFixed(1)}s recording capacity remaining${busy?' | applying...':''}`);
        }
        async function poll() {
            try {
                const reply=await rpc({operation:'snapshot'});if(!reply.ok)throw Error(reply.error || 'State unavailable');
                if(!alive)return;
                if(snapshot && reply.generation!==snapshot.generation){controls.clear();urgentStop=false;}
                if(reply.frame!==lastFrame || reply.generation!==snapshot?.generation){lastFrame=reply.frame;advancedAt=Date.now();}
                snapshot=reply;
                if(!rows.length)createRows(reply);
                if(pending) {
                    if(reply.generation!==pending.generation){pending.reject(Error('Audio configuration changed'));pending=null;}
                    else if(reply.lastRequestId>=pending.id){reply.rejected?pending.reject(Error('Action is unavailable in the current loop state')):pending.resolve();pending=null;}
                    else if(Date.now()>pending.deadline){error='Audio is paused; the action remains queued until processing resumes.';}
                }
                refresh();
            } catch(e) {if(alive)notify(e.message);}
            finally {if(alive)pollTimer=setTimeout(poll,66);}
        }
        window.addEventListener('pagehide',()=>{alive=false;keyboard.cancel();stickyObserver.disconnect();clearTimeout(pollTimer);if(pending)pending.reject(Error('Editor closed'));pending=null;});
        document.title='Aion-Looper';
        poll();
    });
})();
