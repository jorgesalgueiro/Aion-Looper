// Native controls for the preserved ASCII interface. Audio stays in C++.
(() => {
    'use strict';
    document.addEventListener('DOMContentLoaded', () => {
        const rpc = window.AionNative?.request;
        if (!rpc || !window.AionLoopMarkup) return;
        let alive = true, mode = 'normal', snapshot = null, pending = null, busy = false;
        let lastFrame = -1, advancedAt = 0, pollTimer, error = '';
        let markerKey='';window.addEventListener('resize',()=>markerKey='');
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
        const globalUndo=document.getElementById('btnGlobalUndo'),globalRedo=document.getElementById('btnGlobalRedo');
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
        // Native manual addendum; preserved reference assets remain byte-identical.
        const holdManual={
            en:['Delete a bad loop: hold STOP for 4 seconds','Press and continuously hold the main STOP button or assigned number key for 4 s to delete the loop and its Undo/Redo history. Releasing before 4 s keeps the audio. The four seconds start on the initial press, not when playback stops.','A quick tap stops playback or schedules its stop when sync is enabled. Holding for 600 ms cuts pending playback immediately, without deleting audio. Keep holding until 4 s to delete, in any sync mode. DEL cancels the track’s unsent actions and clears it, including while saving.','Only a press that starts on STOP or STOP NOW can hold-to-delete. Holding PLAY, FINISH, CANCEL or RECORD never deletes the resulting loop. Header/state-text clicks are short actions only. Releasing before 4 s, cancelling pointer capture, losing focus or STOP ALL cancels a pending deletion, even while it is queued. Clearing a track or changing audio configuration also cancels its old holds.'],
            es:['Borrar un loop: mantén STOP durante 4 segundos','Mantén pulsado continuamente el botón principal STOP o la tecla asignada durante 4 s para borrar el loop y su historial Undo/Redo. Soltar antes de 4 s conserva el audio. El tiempo empieza al pulsar, no al detenerse la reproducción.','Un toque breve detiene o programa la parada con sync. Mantener durante 600 ms corta inmediatamente la reproducción pendiente sin borrar audio. Sigue pulsando hasta 4 s para borrar, en cualquier modo sync. DEL cancela las acciones pendientes de esa pista y la borra, incluso mientras guarda.','Solo una pulsación iniciada en STOP o STOP NOW permite borrar al mantener. Mantener PLAY, FINISH, CANCEL o RECORD no borra el loop. La cabecera y el estado solo ejecutan la acción breve. Soltar antes de 4 s, cancelar la captura del puntero, perder el foco o STOP ALL cancela el borrado pendiente, incluso si espera en la cola. Borrar una pista o cambiar la configuración de audio también cancela sus pulsaciones antiguas.'],
            pt:['Apagar um loop: manter STOP durante 4 segundos','Mantém continuamente o botão principal STOP ou a tecla atribuída durante 4 s para apagar o loop e o histórico Undo/Redo. Soltar antes de 4 s mantém o áudio. O tempo conta desde que primes, não desde que a reprodução para.','Um toque breve para a reprodução ou agenda a paragem com sync. Manter durante 600 ms corta imediatamente a reprodução pendente sem apagar áudio. Continua a premir até 4 s para apagar, em qualquer modo sync. DEL cancela as ações pendentes dessa pista e apaga-a, mesmo durante a gravação dos dados.','Só uma pressão iniciada em STOP ou STOP NOW permite apagar ao manter. Manter PLAY, FINISH, CANCEL ou RECORD não apaga o loop. O cabeçalho e o estado só executam a ação breve. Soltar antes de 4 s, cancelar a captura do ponteiro, perder o foco ou STOP ALL cancela a eliminação pendente, mesmo em espera na fila. Apagar uma pista ou mudar a configuração de áudio também cancela as pressões antigas.']
        };
        for(const [language,copy] of Object.entries(holdManual)){
            const note=document.createElement('div');note.id=`native-hold-manual-${language}`;
            for(const [index,text] of copy.entries()){const line=document.createElement(index?'p':'h4');line.textContent=text;note.append(line);}
            const panel=document.getElementById(`manual-${language}`);panel.insertBefore(note,panel.querySelector('h4'));
        }


        const syncModes=[...document.querySelectorAll('input[name=syncMode]')];
        const timingControls={syncBpm:document.getElementById('bpmInput'),syncBars:document.getElementById('numBars'),syncNum:document.getElementById('timeSigNum'),syncDen:document.getElementById('timeSigDen'),countInVisual:document.getElementById('countInVisual')};
        syncModes.forEach((radio,index)=>{enable(radio);radio.addEventListener('change',()=>{if(radio.checked)setParameter('syncMode',index);});});
        for(const [id,element] of Object.entries(timingControls)){enable(element);element.addEventListener('change',()=>setParameter(id,element.type==='checkbox'?(element.checked?1:0):Number(element.value)));}
        for(const [id,min,max] of [['syncBpm',10,999],['syncBars',1,64],['syncNum',1,32],['syncDen',1,32]]){timingControls[id].min=min;timingControls[id].max=max;}

        const mapping=document.getElementById('keyMappingUI');
        mapping.innerHTML='<div class="mapping-tabs"><button class="tab-btn active">Keyboard</button></div><div class="mapping-scroll-container"><table class="mapping-table">'+Array.from({length:20},(_,i)=>`<tr><td>Loop ${i+1}</td><td>${i<10?'':'Shift+'}${(i+1)%10}</td></tr>`).join('')+'<tr><td>Global OD</td><td>+</td></tr><tr><td>Global SUB</td><td>-</td></tr><tr><td>STOP ALL</td><td>Space</td></tr><tr><td>Retrigger selected loop</td><td>/</td></tr><tr><td>Multiply selected loop</td><td>&#92;</td></tr></table></div>';
        document.getElementById('midiStatus').textContent='Native MIDI mapping is not ported yet.';
        let selected=0;
        const enabled = () => snapshot?.audioReady && Date.now()-advancedAt < 1000;
        const scheduler = new window.AionCommandQueue({ready:enabled,
            run:c=>sendCommand(c.action,c.track,c.value),notify:message=>notify(message)});
        const command=(action,track,value=0)=>scheduler.enqueue(()=>({action,track,value}),{track:action==='clearAll'?null:track,clearTrack:action==='clear'});
        const restoreHistory=(redo,id=null)=>scheduler.enqueue(()=>{
            const target=id??snapshot[redo?'nextRedo':'nextUndo'],t=snapshot.tracks[target];
            // Original empty history is a no-op. Resolve at dispatch so a rapid
            // repeated gesture uses the last acknowledged depth, not stale UI.
            if(!t?.[redo?'redoCount':'undoCount']||!['playing','stopped'].includes(t.state))return null;
            return {action:redo?(id===null?'redoGlobal':'redo'):(id===null?'undoGlobal':'undo'),track:id??0,value:0};
        },{track:id});
        const multiply=id=>scheduler.enqueue(()=>{
            const t=snapshot.tracks[id];
            return t.length&&['playing','stopped','multiplying'].includes(t.state)?{action:'multiply',track:id,value:0}:null;
        },{track:id});
        const halfSpeed=id=>scheduler.enqueue(()=>snapshot.tracks[id].length?{action:'speed',track:id,value:snapshot.tracks[id].playbackRate===1?0.5:1}:null,{track:id});
        const queueControl=(action,track,value)=>{if(enabled())scheduler.enqueue(()=>({action,track,value}),{key:`${track}:${action}`,track});};
        const actionFor = t => t.state === 'empty' ? 'record' : t.state === 'stopped' ? 'play'
            : ['queued','stopping'].includes(t.state)?'stop'
            : t.state === 'playing' ? (mode === 'normal' ? 'stop' : mode) : 'finish';
        const notify = message => {error=String(message); status.textContent=error;};
        async function setParameter(id, value) {
            try {const reply=await rpc({operation:'setParameter',id,value});if(!reply.ok)throw Error(reply.error);}
            catch(e) {notify(e.message);}
        }
        async function sendCommand(action, track, value=0) {
            if (!enabled()) return;
            if (['overdub','substitute'].includes(action) && snapshot.tracks[track].playbackRate!==1) {notify('Reset loop speed to 1.0x before overdub or substitute.');return;}
            if(action==='record'&&snapshot.syncMode===1&&Math.round(snapshot.gridLength*snapshot.sampleRate)>snapshot.freeFrames){notify('Static loop length exceeds available recording capacity. Reduce bars or clear a loop.');return;}
            busy=true; error=''; refresh();
            try {
                const generation=snapshot.generation;
                const reply=await rpc({operation:'loop',action,track,value,generation,expiresAt:Date.now()+3000});
                if(!reply.ok)throw Error(reply.error || 'Command was not queued');
                await new Promise((resolve,reject) => {pending={id:reply.requestId,generation,resolve,reject,deadline:Date.now()+4000};});
            } catch(e) {notify(e.message);}
            finally {busy=false;refresh();}
        }
        master.addEventListener('input', () => {output.querySelector('#native-master-value').textContent=Number(master.value).toFixed(2);setParameter('masterGain',Number(master.value));});
        monitor.addEventListener('change', () => setParameter('inputMonitor',monitor.checked?1:0));
        autoPlay.addEventListener('change',()=>setParameter('autoPlayAfterRecord',autoPlay.checked?1:0));
        const stopSession=()=>{keyboard.cancel();scheduler.enqueue(()=>({action:'stopAll',track:0,value:0}),{stop:true});};
        stopAll.addEventListener('click', stopSession);
        globalUndo.title='Undo Last Action';globalRedo.title='Redo Last Action';
        globalUndo.addEventListener('click',()=>restoreHistory(false));
        globalRedo.addEventListener('click',()=>restoreHistory(true));
        clearAll.addEventListener('click', () => {if(window.confirm('Clear ALL loops and master recordings?')){keyboard.cancel();scheduler.reset();command('clearAll',0);}});
        const mainAction=id=>{
            selected=id;
            const requestedMode=mode;
            scheduler.enqueue(()=>{
                const t=snapshot.tracks[id];
                if(t.state==='finalizing')return window.AionCommandQueue.wait;
                const owner=snapshot.tracks.findIndex(t=>['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(t.state));
                if(t.state==='empty'&&owner>=0&&owner!==id) {
                    if(snapshot.tracks[owner].state==='finalizing')return null;
                    return {action:'finish',track:owner,value:0};
                }
                return {action:t.state==='playing'?(requestedMode==='normal'?'stop':requestedMode):actionFor(t),track:id,value:0};
            },{track:id});
        };
        const keyboard = new window.AionPerformance({
            track:id=>snapshot?.tracks[id],
            canDelete:id=>snapshot?.tracks[id]?.state==='stopping'||(snapshot?.tracks[id]?.state==='playing'&&mode==='normal'),
            action:id=>mainAction(id),
            hold:(id,initialState,valid)=>scheduler.enqueue(()=>{if(!valid())return null;const action=window.AionPerformance.holdAction(initialState,snapshot.tracks[id].state);return action?{action,track:id,value:0}:null;},{track:id}),
            erase:(id,initialState,valid)=>{
                if(!valid()||!window.AionPerformance.deleteAction(initialState,snapshot.tracks[id].state))return;
                scheduler.enqueue(()=>{
                    if(!valid())return null;
                    const action=window.AionPerformance.deleteAction(initialState,snapshot.tracks[id].state);
                    if(!action)return null;
                    // Once deletion commits, no older hold may touch a replacement take.
                    keyboard.cancelTrack(id);
                    return {action,track:id,value:0};
                },{track:id,clearTrack:true});
            },
            mode:next=>{mode=mode===next?'normal':next;refresh();},
            history:redo=>restoreHistory(redo),
            multiply:()=>multiply(selected),
            stop:stopSession,retrigger:()=>command('retrigger',selected),halfSpeed:()=>halfSpeed(selected)
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
                // Original header background/state text is also a short-action target.
                // Nested controls and the waveform keep their own actions; main clicks
                // are handled below, so bubbling must never trigger a second command.
                row.querySelector('.loop-header').addEventListener('click',e=>{
                    if(enabled()&&!e.target.closest('button,input,select,label,textarea,a,canvas'))mainAction(i);
                });
                const main=bind(row.querySelector(`#loop-action-${i}`),e=>{if(e.detail===0)mainAction(i);});
                main.addEventListener('pointerdown',e=>{
                    if(e.button!==0||e.isPrimary===false)return;
                    e.preventDefault();selected=i;main.setPointerCapture(e.pointerId);keyboard.press(i,`pointer:${e.pointerId}`);
                });
                main.addEventListener('pointerup',e=>keyboard.release(`pointer:${e.pointerId}`));
                for(const event of ['pointercancel','lostpointercapture'])main.addEventListener(event,e=>keyboard.release(`pointer:${e.pointerId}`,true));
                const volume=bind(row.querySelector(`#loop-vol-slider-${i}`),e=>queueControl('volume',i,Number(e.target.value)),'input');
                const feedback=bind(row.querySelector(`#loop-fbk-slider-${i}`),e=>queueControl('feedback',i,Number(e.target.value)),'input');
                const pan=bind(row.querySelector(`#loop-pan-slider-${i}`),e=>queueControl('pan',i,Number(e.target.value)),'input');
                const speed=bind(row.querySelector(`#loop-speed-slider-${i}`),e=>queueControl('speed',i,Number(e.target.value)),'input');
                const half=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='1/2'),()=>halfSpeed(i));
                const reset=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='1x'),()=>queueControl('speed',i,1));
                speed.addEventListener('dblclick',()=>queueControl('speed',i,1));
                const mute=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='MUTE'),()=>scheduler.enqueue(()=>({action:'mute',track:i,value:snapshot.tracks[i].muted?0:1}),{track:i}));
                bind(row.querySelector('.loop-track-delete'),()=>{keyboard.cancelTrack(i);command('clear',i);});
                const solo=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='SOLO'),()=>command('solo',i));
                const retrigger=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='RTRG'),()=>command('retrigger',i));
                const undo=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='UNDO'),()=>restoreHistory(false,i));
                const redo=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='REDO'),()=>restoreHistory(true,i));
                const reverse=bind(row.querySelector(`#loop-${i}-reverse`),()=>scheduler.enqueue(()=>({action:'reverse',track:i,value:snapshot.tracks[i].reverse?0:1}),{track:i}),'change');
                const mult=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='MULT'),()=>multiply(i));
                mult.title='Multiply [\\] — press to capture, press again to extend. No input = double.';
                const normalize=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='NORM'),()=>command('normalize',i));
                feedback.addEventListener('dblclick',()=>queueControl('feedback',i,0.8));
                row.addEventListener('pointerdown',()=>{selected=i;});
                const canvas=row.querySelector('canvas');canvas.title='Playback position';
                rows.push({row,main,volume,feedback,pan,speed,half,reset,mute,solo,retrigger,normalize,mult,reverse,undo,redo,canvas});wrapper.append(row);container.append(wrapper);
            }
        }
        function refresh() {
            if(!snapshot)return;
            const active=enabled();
            if(document.activeElement!==master)master.value=snapshot.masterGain;
            if(document.activeElement!==monitor)monitor.checked=snapshot.inputMonitor;
            output.querySelector('#native-master-value').textContent=snapshot.masterGain===0?'MUTED':snapshot.masterGain.toFixed(2);
            stopAll.disabled=!active;clearAll.disabled=!active||busy;
            const historyAvailable=id=>Number.isInteger(id)&&id>=0&&['playing','stopped'].includes(snapshot.tracks[id]?.state);
            globalUndo.disabled=!active||!historyAvailable(snapshot.nextUndo);
            globalRedo.disabled=!active||!historyAvailable(snapshot.nextRedo);
            globalUndo.textContent=snapshot.nextUndo>=0?`UNDO L${snapshot.nextUndo+1}`:'UNDO';
            globalRedo.textContent=snapshot.nextRedo>=0?`REDO L${snapshot.nextRedo+1}`:'REDO';
            if(document.activeElement!==autoPlay)autoPlay.checked=snapshot.autoPlayAfterRecord;
            document.getElementById('activeCount').textContent=`${snapshot.tracks.filter(t=>t.state!=='empty').length}/20`;
            document.getElementById('recStatus').textContent=snapshot.tracks.some(t=>['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(t.state))?'RECORDING':'READY';
            const symbols={empty:'( )',recording:'(R)',playing:'[>]',stopped:'[S]',overdubbing:'(O)',substituting:'(-)',multiplying:'(M)',finalizing:'[..]',armed:'(A)',queued:'[Q]',stopping:'[S]'};
            rows.forEach((r,i)=>{
                const t=snapshot.tracks[i];r.row.className=`loop ${t.state}`;
                r.row.querySelectorAll('[data-native-control]').forEach(e=>e.disabled=!active);
                r.main.disabled=!active||t.state==='finalizing'||(t.state==='empty'&&snapshot.freeFrames===0);
                r.main.textContent=t.state==='finalizing'?'SAVING':['armed','queued'].includes(t.state)?'CANCEL':t.state==='stopping'?'STOP NOW':actionFor(t)==='substitute'?'REPLACE':actionFor(t).toUpperCase();r.main.setAttribute('aria-label',`Loop ${i+1}: ${r.main.textContent}`);r.main.title=r.main.getAttribute('aria-label');
                r.row.querySelector(`#loop-state-symbol-${i}`).textContent=symbols[t.state];
                r.row.querySelector(`#loop-state-text-${i}`).textContent=t.state==='finalizing'?'SAVING':t.state.toUpperCase();
                r.row.querySelector(`#loop-extra-info-${i}`).textContent=`${t.muted?'[MUTED] ':''}${t.length?`(${(t.length/snapshot.sampleRate/t.playbackRate).toFixed(1)}s)`:''}`;
                if(document.activeElement!==r.volume)r.volume.value=t.volume;
                if(document.activeElement!==r.feedback)r.feedback.value=t.feedback;
                r.row.querySelector(`#loop-vol-display-${i}`).textContent=t.volume>0?`${(20*Math.log10(t.volume)).toFixed(1)}dB`:'-inf';
                r.row.querySelector(`#loop-fbk-display-${i}`).textContent=`${Math.round(t.feedback*100)}%`;
                if(document.activeElement!==r.pan)r.pan.value=t.pan;
                if(document.activeElement!==r.speed)r.speed.value=t.playbackRate;
                r.row.querySelector(`#loop-pan-display-${i}`).textContent=t.pan;
                r.row.querySelector(`#loop-speed-display-${i}`).textContent=`${t.playbackRate.toFixed(2)}x`;
                const editing=['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(t.state);
                r.speed.disabled=!active||editing;r.half.disabled=!active||editing||!t.length;
                r.reset.disabled=!active||editing||t.playbackRate===1;
                r.retrigger.disabled=!active||!t.length||editing||['queued','stopping'].includes(t.state);
                r.undo.disabled=!active||!t.undoCount||!['playing','stopped'].includes(t.state);
                r.redo.disabled=!active||!t.redoCount||!['playing','stopped'].includes(t.state);
                r.reverse.checked=!!t.reverse;
                r.mult.disabled=!active||!t.length||!['playing','stopped','multiplying'].includes(t.state)||snapshot.tracks.some((other,id)=>id!==i&&['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(other.state));
                r.normalize.disabled=!active||!t.length||!['playing','stopped'].includes(t.state)||snapshot.tracks.some(t=>['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(t.state));
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
            document.body.classList.toggle('is-multiplying',snapshot.tracks.some(t=>t.state==='multiplying'));
            document.body.classList.toggle('is-recording',snapshot.tracks.some(t=>['recording','armed'].includes(t.state)));
            document.body.classList.toggle('is-queued',snapshot.tracks.some(t=>t.state==='queued'));
            syncModes.forEach((radio,index)=>radio.checked=snapshot.syncMode===index);
            for(const [id,element] of Object.entries(timingControls))if(document.activeElement!==element){if(element.type==='checkbox')element.checked=!!snapshot[id];else element.value=snapshot[id];}
            document.getElementById('dynamicOptionsPanel').style.display=snapshot.syncMode===2?'':'none';
            const nextMarkers=`${snapshot.syncMode}:${snapshot.syncBars}:${snapshot.syncNum}`;
            if(markerKey!==nextMarkers){window.AionSyncMarkup(snapshot);markerKey=nextMarkers;}
            document.getElementById('loopLength').textContent=`${snapshot.gridLength.toFixed(2)}s`;
            document.getElementById('playhead').style.left=snapshot.syncMode===0?'0%':`${snapshot.gridPosition*100}%`;
            const armed=snapshot.tracks.find(t=>t.state==='armed');
            const beats=armed?(armed.scheduledFrame-snapshot.syncFrame)/snapshot.sampleRate/(60/snapshot.syncBpm):0;
            const pulse=active&&snapshot.countInVisual&&beats>0&&beats<=snapshot.syncNum&&Math.ceil(beats)-beats<0.15/(60/snapshot.syncBpm);
            const masterModule=document.getElementById('mod-master');
            masterModule.style.borderColor=pulse?(beats<=1?'#f00':'#ff0'):'';
            masterModule.style.boxShadow=pulse?`0 0 10px ${beats<=1?'#f00':'#ff0'}`:'';
            const db=value=>value>0?`${(20*Math.log10(value)).toFixed(1)} dBFS`:'-inf dBFS';
            document.getElementById('master-ascii-vu').textContent=`MASTER | ${db(snapshot.outputPeak)}`;
            status.textContent=error || (!active?'Waiting for audio. Enable the audio device or host processing.':
                snapshot.tracks.some(t=>t.stopPending)?'Recording: finish scheduled at the next bar. Press FINISH again to stop now.':
                armed?`Armed: recording starts in ${Math.max(0,(armed.scheduledFrame-snapshot.syncFrame)/snapshot.sampleRate).toFixed(2)}s. Press CANCEL to disarm.`:
                snapshot.tracks.some(t=>t.finalizationFailed)?'Take finalization failed; existing audio was kept.':
                snapshot.poolFull?'Recording stopped: session audio memory is full. Clear a loop to free space.':
                snapshot.audioReset?'Session audio cleared after a sample-rate change.':
                `INPUT ${db(snapshot.inputPeak)} | OUTPUT ${db(snapshot.outputPeak)} | ${(snapshot.freeFrames/snapshot.sampleRate).toFixed(1)}s recording capacity remaining${busy?' | applying...':''}`);
        }
        async function poll() {
            try {
                const reply=await rpc({operation:'snapshot'});if(!reply.ok)throw Error(reply.error || 'State unavailable');
                if(!alive)return;
                if(snapshot && reply.generation!==snapshot.generation){scheduler.reset();keyboard.cancel();}
                if(reply.frame!==lastFrame || reply.generation!==snapshot?.generation){lastFrame=reply.frame;advancedAt=Date.now();}
                snapshot=reply;
                if(!rows.length)createRows(reply);
                if(pending) {
                    if(reply.generation!==pending.generation){pending.reject(Error('Audio configuration changed'));pending=null;}
                    else if(reply.lastRequestId>=pending.id){reply.rejected?pending.reject(Error('Action is unavailable in the current loop state')):pending.resolve();pending=null;}
                    else if(Date.now()>pending.deadline){error='Audio is paused; the action remains queued until processing resumes.';}
                }
                refresh();scheduler.pump();
            } catch(e) {if(alive)notify(e.message);}
            finally {if(alive)pollTimer=setTimeout(poll,66);}
        }
        window.addEventListener('pagehide',()=>{alive=false;scheduler.close();keyboard.cancel();stickyObserver.disconnect();clearTimeout(pollTimer);if(pending)pending.reject(Error('Editor closed'));pending=null;});
        document.title='Aion-Looper';
        poll();
    });
})();
