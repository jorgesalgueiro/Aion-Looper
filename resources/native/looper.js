// Native controls for the preserved ASCII interface. Audio stays in C++.
(() => {
    'use strict';
    document.addEventListener('DOMContentLoaded', () => {
        const rpc = window.AionNative?.request;
        if (!rpc || !window.AionLoopMarkup) return;
        let alive = true, mode = 'normal', snapshot = null, pending = null, busy = false;
        let lastFrame = -1, advancedAt = 0, pollTimer, error = '';
        const rows = [], panel = document.createElement('section');
        panel.className = 'retro-module'; panel.style.margin = '8px 0';
        panel.innerHTML = `<div class="module-header">AION-LOOPER - NATIVE LOOP PREVIEW</div>
            <div class="module-content"><p><a href="#ws-loops">[LOOPS]</a> - Standalone: enable input with the window's Unmute Input control and choose the device in Options.</p><p>Session audio is temporary. Project save/export, undo, synths and effects are not available yet. Changing sample rate clears session audio.</p>
            <div style="display:flex; flex-wrap:wrap; gap:12px; align-items:center">
                <label>OUTPUT <input id="native-master" type="range" min="0" max="2" step="0.01" value="0" aria-label="Master volume"></label>
                <output id="native-master-value">MUTED</output>
                <label><input id="native-monitor" type="checkbox"> INPUT MONITOR</label>
                <label>MODE <select id="native-mode"><option value="normal">NORMAL</option><option value="overdub">OVERDUB</option><option value="substitute">SUBSTITUTE</option></select></label>
                <button id="native-stop-all" class="std-btn btn-red">STOP ALL</button>
            </div><p id="native-status" role="status">Waiting for audio...</p></div>`;
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
        document.getElementById('part0').after(panel);
        const status = panel.querySelector('#native-status');
        const master = panel.querySelector('#native-master'), monitor = panel.querySelector('#native-monitor');
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
            busy=true; error=''; refresh();
            try {
                const generation=snapshot.generation;
                const reply=await rpc({operation:'loop',action,track,value,generation,expiresAt:Date.now()+3000});
                if(!reply.ok)throw Error(reply.error || 'Command was not queued');
                await new Promise((resolve,reject) => {pending={id:reply.requestId,generation,resolve,reject,deadline:Date.now()+4000};});
            } catch(e) {notify(e.message);}
            finally {busy=false;refresh();}
        }
        master.addEventListener('input', () => {panel.querySelector('#native-master-value').textContent=Number(master.value).toFixed(2);setParameter('masterGain',Number(master.value));});
        monitor.addEventListener('change', () => setParameter('inputMonitor',monitor.checked?1:0));
        panel.querySelector('#native-mode').addEventListener('change', e => {mode=e.target.value;refresh();});
        panel.querySelector('#native-stop-all').addEventListener('click', () => command('stopAll',0));
        function createRows(s) {
            const markup=window.AionLoopMarkup(s,mode), container=document.getElementById('loopContainer');
            container.replaceChildren();
            for(let i=0;i<markup.length;++i) {
                const wrapper=document.createElement('div');wrapper.id=`loop-wrapper-${i}`;
                const row=document.createElement('div');row.id=`loop-${i}`;row.className='loop empty';row.innerHTML=markup[i];
                stripHandlers(row);
                row.querySelectorAll('button,input,select,textarea').forEach(e => {e.disabled=true;e.title='Unavailable in this preview';});
                const bind=(element,callback,event='click') => {element.dataset.nativeControl='true';element.title=element.getAttribute('aria-label')||element.textContent.trim();element.addEventListener(event,callback);return element;};
                const main=bind(row.querySelector(`#loop-action-${i}`),()=>command(actionFor(snapshot.tracks[i]),i));
                const volume=bind(row.querySelector(`#loop-vol-slider-${i}`),e=>command('volume',i,Number(e.target.value)),'change');
                const feedback=bind(row.querySelector(`#loop-fbk-slider-${i}`),e=>command('feedback',i,Number(e.target.value)),'change');
                const mute=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='MUTE'),()=>command('mute',i,snapshot.tracks[i].muted?0:1));
                bind(row.querySelector('.loop-track-delete'),()=>command('clear',i));
                const canvas=row.querySelector('canvas');canvas.title='Playback position';
                rows.push({row,main,volume,feedback,mute,canvas});wrapper.append(row);container.append(wrapper);
            }
        }
        function refresh() {
            if(!snapshot)return;
            const active=enabled();
            if(document.activeElement!==master)master.value=snapshot.masterGain;
            if(document.activeElement!==monitor)monitor.checked=snapshot.inputMonitor;
            panel.querySelector('#native-master-value').textContent=snapshot.masterGain===0?'MUTED':snapshot.masterGain.toFixed(2);
            panel.querySelector('#native-stop-all').disabled=!active||busy;
            const symbols={empty:'( )',recording:'(R)',playing:'[>]',stopped:'[S]',overdubbing:'(O)',substituting:'(-)'};
            rows.forEach((r,i)=>{
                const t=snapshot.tracks[i];r.row.className=`loop ${t.state}`;
                r.row.querySelectorAll('[data-native-control]').forEach(e=>e.disabled=!active||busy);
                r.main.disabled=!active||busy||(t.state==='empty'&&snapshot.freeFrames===0);
                r.main.textContent=actionFor(t)==='substitute'?'REPLACE':actionFor(t).toUpperCase();r.main.setAttribute('aria-label',`Loop ${i+1}: ${r.main.textContent}`);r.main.title=r.main.getAttribute('aria-label');
                r.row.querySelector(`#loop-state-symbol-${i}`).textContent=symbols[t.state];
                r.row.querySelector(`#loop-state-text-${i}`).textContent=t.state.toUpperCase();
                r.row.querySelector(`#loop-extra-info-${i}`).textContent=`${t.muted?'[MUTED] ':''}${(t.length/snapshot.sampleRate).toFixed(2)}s`;
                if(document.activeElement!==r.volume)r.volume.value=t.volume;
                if(document.activeElement!==r.feedback)r.feedback.value=t.feedback;
                r.row.querySelector(`#loop-vol-display-${i}`).textContent=t.volume>0?`${(20*Math.log10(t.volume)).toFixed(1)}dB`:'-inf';
                r.row.querySelector(`#loop-fbk-display-${i}`).textContent=`${Math.round(t.feedback*100)}%`;
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
        window.addEventListener('pagehide',()=>{alive=false;stickyObserver.disconnect();clearTimeout(pollTimer);if(pending)pending.reject(Error('Editor closed'));pending=null;});
        document.title='Aion-Looper - Native Loop Preview';
        poll();
    });
})();
