// Native controls for the preserved ASCII interface. Audio stays in C++.
(() => {
    'use strict';
    document.addEventListener('DOMContentLoaded', () => {
        const rpc = window.AionNative?.request;
        if (!rpc || !window.AionLoopMarkup || !document.getElementById('inputTracksContainer')) return;
        let alive = true, mode = 'normal', snapshot = null, pending = null, busy = false;
        let lastFrame = -1, advancedAt = 0, pollTimer, error = '';
        let markerKey='';window.addEventListener('resize',()=>markerKey='');
        const rows = [], liveRows = [], liveMixerRows = [], input = document.getElementById('inputTracksContainer');
        // The only new area is native input configuration, inside the original INPUT MIXER.
        input.innerHTML = `<div class="mixer-row input-track-row" style="display:block">
            <div class="mixer-label">NATIVE AUDIO INPUT</div>
            <label><input id="native-monitor" type="checkbox"> MONITOR</label>
            <p>Standalone: choose device/ASIO driver and buffer in Options, then Unmute Input. Plugins use the host's audio routing.</p>
            <p id="native-status" role="status">Waiting for audio...</p>
            <p>Click a recorded loop waveform to export WAV. Session audio and names are temporary; project saving is not ported. Changing sample rate clears session audio and names.</p></div>`;
        const output = document.createElement('div'); output.className='mixer-row';
        output.innerHTML=`<label for="native-master" class="mixer-label">VOLUME</label>
            <input id="native-master" class="mixer-slider" type="range" min="0" max="2" step="0.01" value="0" aria-label="Master output volume">
            <output id="native-master-value">MUTED</output>`;
        document.querySelector('#mod-master .module-content').append(output);
        const masterHeader=document.querySelector('#mod-master .module-header span:first-child');
        const masterLabel=masterHeader?.textContent??'MASTER OUTPUT';
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
        // User-supplied marquee; keep the original reference assets intact.
        const logo = document.querySelector('#header pre .rainbow-text');
        if (logo) logo.textContent = `\n               ███                                                               ████
              ████                      ██      ███                 █           ██████                ███████                ████████                   ████           ██████████████         █████████████████         █████████
             ██████            █████████      ██████            █████          ██                   ██████████            ██████████████        ███       ████          █████████████████  ███████████████████        ████████████████
           ██  ████           ████████      ███ █████       █████████         ██                   ████   ███           ██████████████████    ██        ███████       ██████          ████        ███                 █████       ███
          ██   █████             ████      ███   ████     ███   ██████       ██                    ███                ████████     ████████ ███             ████   ████ ████           ███       ████               ██████      ███
         ██      ███   ██████   ████       ██     ██████ ███    ███████     ██                     ██                 ██████          ████████               ████  █    ████         ███         ███████████           ███ █████
        █        ███████████    ███       ███     ██████        ██ █████   ███                     ██                █████             ██████                 ████      ███     █████           ███████████            ██████
      ██   ██████████            ██       ██      █████        ██   █████  ███                    ███                █████              ██████                ████       ███████                 ███                   ███████
   ████████      ████           ███      ██      ██████        ███   ████████                     ███                █████               █████                ████      ██                      ███                   ██  ████
 ██████          ████           ███      ██       ███          ██      ██████                    ███                 █████               ██████              ████       ██                      ██                    ██  ██████
   ██            ████           ███      ██      ██           ███       █████                    ███                 █████               ███████            █████       ██                      ██           ██      ██     █████
████             █████          ██████    █    ███            ██         █████              ██   ██           █████   █████            ███████████        ██████       ██                      ███         █████     ██         ██████
███               ████      ████████      ██████            ████          ████            ████████████████ ██████      ███████       █████   ██████████████████        ██                ████████████ ██████████     ██           █████
██                ██       ████                             ███           ███              ███████████████████          █████████████████      ██████████████         ███                ███████████████████        ███
                                                                                                                           ████████████            ██████            ██                      ███                   ███
                                                                                                                                                                 █████                      ███                  ████
                                                                                                                                                                 ███                                             ███\n`;
        // Align glyph columns, not each ragged row independently.
        if (logo) logo.style.textAlign = 'left';
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
        const metroButton=document.getElementById('metronomeBtn'),metroVolume=document.getElementById('metronomeVol'),metroLabel=metroButton.textContent.trim();
        const globalUndo=document.getElementById('btnGlobalUndo'),globalRedo=document.getElementById('btnGlobalRedo');
        const enable = element => {element.disabled=false;element.title=element.getAttribute('aria-label')||element.textContent.trim();};
        [master,monitor,autoPlay,metroButton,metroVolume].forEach(enable);
        const toggleMetronome=async()=>{try{const reply=await rpc({operation:'toggleMetronome'});if(!reply.ok)throw Error(reply.error||'Metronome unavailable');}catch(e){notify(e.message);}};
        metroButton.addEventListener('click',toggleMetronome);
        metroVolume.addEventListener('input',()=>{document.getElementById('metroVolDisplay').textContent=Number(metroVolume.value).toFixed(2);setParameter('metronomeVolume',Number(metroVolume.value));});
        const bindTabs=(buttonClass,panelClass,ids)=>{
            const buttons=[...document.querySelectorAll(buttonClass)];
            buttons.forEach((button,i)=>{enable(button);button.addEventListener('click',()=>{
                document.querySelectorAll(panelClass).forEach(p=>p.classList.remove('active'));
                buttons.forEach(b=>b.classList.remove('active'));
                document.getElementById(ids[i]).classList.add('active');button.classList.add('active');
                if(ids[i]==='ws-live'&&snapshot){createLiveRows(snapshot);refresh();}
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
        holdManual.en.push('Metronome: use Metro[N]ome or N. Its VOLUME is independent of MASTER and its sound is excluded from internal loop recording. Static recording: SOUND plays a 100 ms triangle cue on each beat of the final bar, with a higher final cue. Cues bypass MASTER and stay out of internal loop recordings; CANCEL, DEL and STOP ALL silence pending cues. VISUAL and SOUND independently request the same lead-in. OFF and DYNAMIC start immediately.');
        holdManual.es.push('Metrónomo: usa Metro[N]ome o N. Su VOLUME es independiente de MASTER y su sonido no se incluye en la grabación interna de loops. En grabación STATIC, SOUND reproduce tonos de 100 ms en el último compás, con un tono final más agudo. No pasan por MASTER ni entran en los loops. CANCEL, DEL y STOP ALL cancelan los tonos pendientes. VISUAL y SOUND son independientes; OFF y DYNAMIC empiezan de inmediato.');
        holdManual.pt.push('Metrónomo: usa Metro[N]ome ou N. O seu VOLUME é independente de MASTER e o som não entra na gravação interna dos loops. Na gravação STATIC, SOUND toca sinais de 100 ms no último compasso, com o último sinal mais agudo. Não passam por MASTER nem entram nos loops. CANCEL, DEL e STOP ALL cancelam os sinais pendentes. VISUAL e SOUND são independentes; OFF e DYNAMIC começam de imediato.');
        holdManual.en.push('Start: drag the original slider to choose a playback phase; release applies it after 50 ms while playing. Dragging alone keeps the current source running. The recorded audio is preserved. STOP ALL keeps the chosen phase and cancels pending restarts; DEL resets it. Q snaps Start to the nearest sixteenth-note division of the recorded duration using the current BPM; during playback it applies after 50 ms. Clips shorter than half a sixteenth stay unchanged. The < and > buttons nudge Start by 1/64 note at the current BPM and apply after 50 ms while playing. Tiny clips wrap across multiple cycles safely; stopped loops stay stopped.');
        holdManual.es.push('Start: arrastra el deslizador para elegir la fase; al soltar se aplica tras 50 ms durante la reproducción. Mientras arrastras continúa la fuente actual. El audio grabado se conserva. STOP ALL conserva la fase y cancela los reinicios pendientes; DEL la restablece. Q ajusta Start a la división de semicorchea más cercana según la duración grabada y el BPM actual; durante la reproducción se aplica tras 50 ms. Los clips menores que media semicorchea no cambian. Los botones < y > ajustan Start en 1/64 de nota al BPM actual y se aplican tras 50 ms durante la reproducción. Los clips muy cortos se envuelven entre varios ciclos; los loops detenidos no empiezan.');
        holdManual.pt.push('Start: arrasta o controlo para escolher a fase; ao soltar aplica-se após 50 ms durante a reprodução. Enquanto arrastas, a fonte atual continua. O áudio gravado é preservado. STOP ALL mantém a fase e cancela os reinícios pendentes; DEL repõe a fase. Q ajusta Start à divisão de semicolcheia mais próxima usando a duração gravada e o BPM atual; durante a reprodução aplica-se após 50 ms. Clips menores que meia semicolcheia não mudam. Os botões < e > ajustam Start em 1/64 de nota ao BPM atual e aplicam-se após 50 ms durante a reprodução. Clips muito curtos dão a volta a vários ciclos em segurança; loops parados continuam parados.');
        holdManual.en.push('WAV: click a recorded loop waveform and choose the filename and folder in the native save dialog. Exports use the stored take at its capture sample rate in stereo PCM16. Volume, pan, reverse, speed and STUT control playback; exporting reads the stored samples. Cancel keeps the loop. Closing or reloading the editor cancels an unchosen destination; a selected background write completes.');
        holdManual.es.push('WAV: haz clic en la forma de onda de un loop grabado y elige el nombre y la carpeta en el diálogo nativo. Se exporta la toma almacenada a su frecuencia de captura en PCM16 estéreo. Volumen, panorama, reverse, velocidad y STUT controlan la reproducción; la exportación lee las muestras almacenadas. Cancel conserva el loop. Cerrar o recargar el editor cancela un destino pendiente; una escritura ya elegida termina en segundo plano.');
        holdManual.pt.push('WAV: clica na forma de onda de um loop gravado e escolhe o nome e a pasta no diálogo nativo. A exportação usa a tomada guardada à frequência de captura em PCM16 estéreo. Volume, panorama, reverse, velocidade e STUT controlam a reprodução; a exportação lê as amostras guardadas. Cancel mantém o loop. Fechar ou recarregar o editor cancela um destino por escolher; uma escrita já escolhida termina em segundo plano.');
        for(const [language,copy] of Object.entries(holdManual)){
            const note=document.createElement('div');note.id=`native-hold-manual-${language}`;
            for(const [index,text] of copy.entries()){const line=document.createElement(index?'p':'h4');line.textContent=text;note.append(line);}
            const panel=document.getElementById(`manual-${language}`);panel.insertBefore(note,panel.querySelector('h4'));
        }


        const syncModes=[...document.querySelectorAll('input[name=syncMode]')];
        const timingControls={syncBpm:document.getElementById('bpmInput'),syncBars:document.getElementById('numBars'),syncNum:document.getElementById('timeSigNum'),syncDen:document.getElementById('timeSigDen'),countInVisual:document.getElementById('countInVisual'),countInAudio:document.getElementById('countInAudio')};
        syncModes.forEach((radio,index)=>{enable(radio);radio.addEventListener('change',()=>{if(radio.checked)setParameter('syncMode',index);});});
        for(const [id,element] of Object.entries(timingControls)){enable(element);element.addEventListener('change',()=>setParameter(id,element.type==='checkbox'?(element.checked?1:0):Number(element.value)));}
        for(const [id,min,max] of [['syncBpm',10,999],['syncBars',1,64],['syncNum',1,32],['syncDen',1,32]]){timingControls[id].min=min;timingControls[id].max=max;}

        const mapping=document.getElementById('keyMappingUI');
        mapping.innerHTML='<div class="mapping-tabs"><button class="tab-btn active">Keyboard</button></div><div class="mapping-scroll-container"><table class="mapping-table">'+Array.from({length:20},(_,i)=>`<tr><td>Loop ${i+1}</td><td>${i<10?'':'Shift+'}${(i+1)%10}</td></tr>`).join('')+'<tr><td>Global OD</td><td>+</td></tr><tr><td>Global SUB</td><td>-</td></tr><tr><td>STOP ALL</td><td>Space</td></tr><tr><td>Metronome</td><td>N</td></tr><tr><td>Retrigger selected loop</td><td>/</td></tr><tr><td>Multiply selected loop</td><td>&#92;</td></tr></table></div>';
        const midi=window.AionMidiSetup({rpc,notify:message=>notify(message),setMode:next=>{mode=next;}});
        let selected=0;
        const enabled = () => snapshot?.audioReady && Date.now()-advancedAt < 1000;
        const scheduler = new window.AionCommandQueue({ready:()=>enabled()||scheduler.items[0]?.metadata===true,
            run:async c=>{
                if(c.action==='exportLoop'){
                    const reply=await rpc({operation:'exportLoop',track:c.track,generation:c.generation,waveformId:c.waveformId,filename:c.filename});
                    if(!reply.ok&&!reply.exportBusy)throw Error(reply.error||'WAV export unavailable');
                    if(reply.ok)error='';return;
                }
                if(c.action!=='renameLoop')return sendCommand(c.action,c.track,c.value);
                const reply=await rpc({operation:'renameLoop',track:c.track,name:c.name});
                if(!reply.ok)throw Error(reply.error||'Name was not changed');
            },notify:message=>notify(message)});
        const command=(action,track,value=0)=>scheduler.enqueue(()=>({action,track,value}),{track:action==='clearAll'?null:track,clearTrack:action==='clear',barrier:action==='clearAll'});
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
        const merge=(id,source)=>{
            if(source==='')return;
            const src=Number(source);if(!Number.isInteger(src)||src<0||src>=20||src===id)return;
            scheduler.enqueue(()=>{
                if(snapshot.tracks.some(t=>t.state==='finalizing'))return window.AionCommandQueue.wait;
                return snapshot.tracks[src].length?{action:'merge',track:id,value:src}:null;
            },{track:id});
        };
        const mergeNotice=new window.AionMerge({current:()=>snapshot,confirm:message=>window.confirm(message),clear:(dest,source,serial)=>{
            stutter.cancelTrack(source);keyboard.cancelTrack(source);
            scheduler.enqueue(()=>({action:'clearMergedSource',track:dest,value:serial}),{track:source});
        }});
        const speedCommand=(track,value)=>{
            const sync=snapshot.syncMode!==0,rate=window.AionLoopSpeed(value,sync);
            if(rate===null)return null;
            const slider=document.getElementById(`loop-speed-slider-${track}`);
            if(slider&&(sync||document.activeElement!==slider))slider.value=rate;
            return {action:'speed',track,value:rate};
        };
        const halfSpeed=id=>scheduler.enqueue(()=>snapshot.tracks[id].length?speedCommand(id,snapshot.tracks[id].playbackRate===1?0.5:1):null,{track:id});
        const queueControl=(action,track,value)=>{if(enabled())scheduler.enqueue(()=>action==='speed'?speedCommand(track,value):({action,track,value}),{key:`${track}:${action}`,track});};
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
        const releaseStutter=track=>{if(snapshot)rpc({operation:'releaseStutter',track,generation:snapshot.generation}).catch(e=>notify(e.message));};
        const exportLoop=track=>{
            const t=snapshot?.tracks[track];if(!enabled()||!t?.length||!t.waveformId)return;
            const filename=window.AionExportName(rows[track]?.name.value??t.name,track,Date.now());
            scheduler.enqueue(()=>{
                const current=snapshot.tracks[track];
                if(current.state==='finalizing')return window.AionCommandQueue.wait;
                return current.length&&current.waveformId?{action:'exportLoop',track,generation:snapshot.generation,waveformId:current.waveformId,filename}:null;
            },{track});
        };
        const stutter=new window.AionStutter({
            start:(track,valid)=>{const generation=snapshot?.generation,clearEpoch=snapshot?.tracks[track]?.clearEpoch;
                scheduler.enqueue(()=>valid()&&snapshot.generation===generation&&snapshot.tracks[track].clearEpoch===clearEpoch&&snapshot.tracks[track].state==='playing'?{action:'stutterStart',track,value:0}:null,{track});},
            stop:releaseStutter
        });
        window.addEventListener('pointerup',e=>stutter.release(`stutter:${e.pointerId}`));
        window.addEventListener('pointercancel',e=>stutter.release(`stutter:${e.pointerId}`));
        window.addEventListener('blur',()=>{stutter.cancel();releaseStutter(20);});
        const stopSession=()=>{mergeNotice.cancel();stutter.cancel();keyboard.cancel();scheduler.enqueue(()=>({action:'stopAll',track:0,value:0}),{stop:true});};
        stopAll.addEventListener('click', stopSession);
        globalUndo.title='Undo Last Action';globalRedo.title='Redo Last Action';
        globalUndo.addEventListener('click',()=>restoreHistory(false));
        globalRedo.addEventListener('click',()=>restoreHistory(true));
        const clearSession=()=>{if(enabled()&&window.confirm('Clear ALL loops and master recordings?')){stutter.cancel();keyboard.cancel();scheduler.reset();command('clearAll',0);}};
        clearAll.addEventListener('click',clearSession);
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
            mode:next=>{mode=mode===next?'normal':next;midi.mode(['normal','overdub','substitute'].indexOf(mode));refresh();},
            history:redo=>restoreHistory(redo),
            multiply:()=>multiply(selected),
            metronome:toggleMetronome,
            clearAll:clearSession,
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
                const name=bind(row.querySelector('.track-name'),e=>{
                    e.stopPropagation();const value=e.target.value;
                    // Keep typing after DEL ordered after the audio owner's clear ack.
                    scheduler.enqueue(()=>({action:'renameLoop',track:i,name:value}),{track:i,key:`name:${i}`,metadata:true});
                },'input');
                name.addEventListener('keydown',e=>{if(e.key==='Enter')name.blur();e.stopPropagation();});
                name.addEventListener('click',e=>e.stopPropagation());
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
                const volumeMidi=new window.AionMidiSliderEdit(volume),panMidi=new window.AionMidiSliderEdit(pan);
                const start=bind(row.querySelector(`#loop-start-slider-${i}`),e=>queueControl('startDelay',i,Number(e.target.value)),'input');
                start.addEventListener('change',e=>{e.stopPropagation();command('applyStart',i);});
                const quantize=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='Q'),()=>command('quantizeStart',i));
                quantize.title='Quantize Start to 16th Note';
                for(const direction of [-1,1]){
                    const nudge=bind(row.querySelector(`[aria-label="Nudge loop start ${direction<0?'left':'right'}"]`),()=>command('nudgeStart',i,direction));
                    nudge.title=`Nudge ${direction<0?'Left':'Right'} (1/64)`;
                }
                const speed=bind(row.querySelector(`#loop-speed-slider-${i}`),e=>queueControl('speed',i,Number(e.target.value)),'input');
                const half=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='1/2'),()=>halfSpeed(i));
                const reset=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='1x'),()=>queueControl('speed',i,1));
                speed.addEventListener('dblclick',()=>queueControl('speed',i,1));
                const mute=bind([...row.querySelectorAll('button')].find(e=>['MUTE','UNM'].includes(e.textContent.trim())),()=>scheduler.enqueue(()=>({action:'mute',track:i,value:snapshot.tracks[i].muted?0:1}),{track:i}));
                bind(row.querySelector('.loop-track-delete'),()=>{stutter.cancelTrack(i);keyboard.cancelTrack(i);command('clear',i);});
                const solo=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='SOLO'),()=>command('solo',i));
                const retrigger=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='RTRG'),()=>command('retrigger',i));
                const stut=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='STUT'),e=>{e.preventDefault();e.stopPropagation();});
                stut.title='Stutter: hold during playback, release to resume';
                stut.addEventListener('pointerdown',e=>{if(e.button!==0||e.isPrimary===false)return;e.preventDefault();e.stopPropagation();selected=i;stutter.press(i,`stutter:${e.pointerId}`);});
                stut.addEventListener('lostpointercapture',e=>stutter.release(`stutter:${e.pointerId}`));
                const undo=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='UNDO'),()=>restoreHistory(false,i));
                const redo=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='REDO'),()=>restoreHistory(true,i));
                const reverse=bind(row.querySelector(`#loop-${i}-reverse`),()=>scheduler.enqueue(()=>({action:'reverse',track:i,value:snapshot.tracks[i].reverse?0:1}),{track:i}),'change');
                const mult=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='MULT'),()=>multiply(i));
                mult.title='Multiply [\\] — press to capture, press again to extend. No input = double.';
                const normalize=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='NORM'),()=>command('normalize',i));
                const mergeSource=bind(row.querySelector(`#merge-src-${i}`),()=>{},'change');
                const mergeGo=bind([...row.querySelectorAll('button')].find(e=>e.textContent.trim()==='GO'),()=>merge(i,mergeSource.value));
                mergeGo.title='Merge selected source into this loop';
                feedback.addEventListener('dblclick',()=>queueControl('feedback',i,0.8));
                row.addEventListener('pointerdown',()=>{selected=i;});
                const canvas=row.querySelector('canvas');canvas.title='Click to Export WAV';
                canvas.addEventListener('click',e=>{e.stopPropagation();exportLoop(i);});
                rows.push({row,main,name,volume,feedback,pan,volumeMidi,panMidi,start,quantize,speed,half,reset,mute,solo,retrigger,stut,normalize,mergeSource,mergeGo,mult,reverse,undo,redo,canvas});wrapper.append(row);container.append(wrapper);
            }
        }
        function createLiveMixer(s) {
            const container=document.getElementById('live-mastermix-container');
            // The original loop strips only. Master/input/drone/sampler routing
            // and RMS/held-peak metering have independent migration gates.
            if(liveMixerRows.length)return;
            container.innerHTML=window.AionLiveMixerMarkup(s);stripHandlers(container);
            container.querySelector('.live-mixer-console').firstElementChild.remove();
            for(let i=0;i<s.tracks.length;++i){
                const volume=document.getElementById(`live_mm_slider_l_${i}`);
                const strip=volume.closest('.live-mixer-fader').parentElement;
                const bind=(element,type,handler)=>{element.addEventListener(type,handler);return element;};
                bind(volume,'input',e=>queueControl('volume',i,Number(e.target.value)));
                const edit=new window.AionMidiSliderEdit(volume);
                const mute=bind(document.getElementById(`live_mm_mute_l_${i}`),'click',()=>scheduler.enqueue(()=>({action:'mute',track:i,value:snapshot.tracks[i].muted?0:1}),{track:i}));
                const solo=bind(document.getElementById(`live_mm_solo_l_${i}`),'click',()=>command('solo',i));
                // Retain the original strip wheel step, after the global input
                // wheel guard has had its opportunity to block non-Shift input.
                bind(strip,'wheel',e=>{e.preventDefault();if(!enabled())return;volume.value=Math.max(0,Math.min(2,Number(volume.value)+(e.deltaY<0?0.05:-0.05)));queueControl('volume',i,Number(volume.value));});
                strip.addEventListener('pointerdown',()=>{selected=i;});
                const meter=document.getElementById(`live_mm_vu_l_${i}`);
                meter.setAttribute('aria-disabled','true');meter.title='RMS / held-peak meter is not available in this preview';
                liveMixerRows.push({strip,volume,edit,mute,solo,label:strip.querySelector('label'),value:document.getElementById(`live_mm_l_v_${i}`)});
            }
        }
        function createLiveRows(s) {
            createLiveMixer(s);
            const container=document.getElementById('live-loops-container');liveRows.length=0;
            // Blank-name original markup avoids interpreting session names as HTML;
            // the original updater's text value is applied before the next paint.
            container.innerHTML=window.AionLiveLoopMarkup(s).join('');stripHandlers(container);
            for(let i=0;i<s.tracks.length;++i){
                const row=document.getElementById(`live-loop-${i}`),header=row.querySelector('.loop-header');
                row.querySelectorAll('input').forEach(e=>{e.disabled=true;e.title='Unavailable in this preview';});
                header.addEventListener('click',()=>{if(enabled())mainAction(i);});
                row.addEventListener('pointerdown',()=>{selected=i;});
                const reverse=row.querySelector(`#live-loop-${i}-reverse`);
                reverse.addEventListener('change',()=>scheduler.enqueue(()=>({action:'reverse',track:i,value:snapshot.tracks[i].reverse?0:1}),{track:i}));
                liveRows.push({row,header,reverse,text:row.querySelector('.live-state-text'),number:row.querySelector('strong'),name:row.querySelector('.loop-header span:not(.live-state-text)')});
            }
        }
        function refresh() {
            if(!snapshot)return;
            const active=enabled();
            if(document.activeElement!==master)master.value=snapshot.masterGain;
            if(document.activeElement!==monitor)monitor.checked=snapshot.inputMonitor;
            output.querySelector('#native-master-value').textContent=snapshot.masterGain===0?'MUTED':snapshot.masterGain.toFixed(2);
            const metroOn=!!snapshot.metronomeEnabled,metroFlash=metroOn&&!!snapshot.metronomeFlash;
            metroButton.textContent=metroOn?`${metroLabel} [ON]`:metroLabel;metroButton.setAttribute('aria-pressed',String(metroOn));
            metroButton.style.color=metroOn?(metroFlash?'#000':'#0f0'):'inherit';metroButton.style.borderColor=metroOn?'#0f0':'currentColor';metroButton.style.backgroundColor=metroFlash?'#0f0':'transparent';
            if(document.activeElement!==metroVolume)metroVolume.value=snapshot.metronomeVolume??0.5;
            document.getElementById('metroVolDisplay').textContent=Number(snapshot.metronomeVolume??0.5).toFixed(2);
            stopAll.disabled=!active;clearAll.disabled=!active||busy;
            const historyAvailable=id=>Number.isInteger(id)&&id>=0&&['playing','stopped'].includes(snapshot.tracks[id]?.state);
            globalUndo.disabled=!active||!historyAvailable(snapshot.nextUndo);
            globalRedo.disabled=!active||!historyAvailable(snapshot.nextRedo);
            globalUndo.textContent=snapshot.nextUndo>=0?`UNDO L${snapshot.nextUndo+1}`:'UNDO';
            globalRedo.textContent=snapshot.nextRedo>=0?`REDO L${snapshot.nextRedo+1}`:'REDO';
            if(document.activeElement!==autoPlay)autoPlay.checked=snapshot.autoPlayAfterRecord;
            document.getElementById('activeCount').textContent=`${snapshot.tracks.filter(t=>t.state!=='empty').length}/20`;
            window.AionGlobalStatus(snapshot,mode,{body:document.body,header:masterHeader,recording:document.getElementById('recStatus'),label:masterLabel});
            rows.forEach((r,i)=>{
                const t=snapshot.tracks[i];
                const sources=snapshot.tracks.map((s,id)=>id!==i&&s.length?id:-1).filter(id=>id>=0);
                const mergeKey=sources.join(',');
                if(r.mergeKey!==mergeKey){const selected=r.mergeSource.value;r.mergeSource.replaceChildren(new Option('-',''),...sources.map(id=>new Option(String(id+1),String(id))));r.mergeSource.value=sources.includes(Number(selected))&&selected!==''?selected:'';r.mergeKey=mergeKey;}
                if(!t.waveformId){r.waveformId=0;r.wavePeaks=null;}
                else if(t.wavePeaks){r.waveformId=t.waveformId;r.wavePeaks=t.wavePeaks;}
                if(document.activeElement!==r.name)r.name.value=t.name??'';
                r.row.className=`loop ${t.state}`;
                r.row.querySelectorAll('[data-native-control]').forEach(e=>e.disabled=!active);r.name.disabled=false;
                r.main.disabled=!active||t.state==='finalizing'||(t.state==='empty'&&snapshot.freeFrames===0);
                r.main.textContent=t.state==='finalizing'?'SAVING':['armed','queued'].includes(t.state)?'CANCEL':t.state==='stopping'?'STOP NOW':actionFor(t)==='substitute'?'REPLACE':actionFor(t).toUpperCase();r.main.setAttribute('aria-label',`Loop ${i+1}: ${r.main.textContent}`);r.main.title=r.main.getAttribute('aria-label');
                const status=window.AionLoopStatus(t,snapshot.sampleRate);
                r.row.querySelector(`#loop-state-symbol-${i}`).textContent=status.symbol;
                r.row.querySelector(`#loop-state-text-${i}`).textContent=status.text;
                r.row.querySelector(`#loop-extra-info-${i}`).textContent=status.extra;
                r.volumeMidi.update(t.volume,!!snapshot.midi?.bindings[33+i]);
                if(document.activeElement!==r.feedback)r.feedback.value=t.feedback;
                r.row.querySelector(`#loop-vol-display-${i}`).textContent=`${t.volume<=0?'-inf':(20*Math.log10(t.volume)).toFixed(1)}dB`;
                r.row.querySelector(`#loop-fbk-display-${i}`).textContent=`${Math.round(t.feedback*100)}%`;
                r.panMidi.update(t.pan,!!snapshot.midi?.bindings[53+i]);
                if(document.activeElement!==r.speed)r.speed.value=t.playbackRate;
                if(document.activeElement!==r.start)r.start.value=t.startDelay??0;
                r.row.querySelector(`#loop-delay-display-${i}`).textContent=`${Math.floor((t.startDelay??0)*(t.length/snapshot.sampleRate*1000))}ms`;
                r.row.querySelector(`#loop-pan-display-${i}`).textContent=t.pan;
                r.row.querySelector(`#loop-speed-display-${i}`).textContent=`${t.playbackRate.toFixed(2)}x`;
                const editing=['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(t.state);
                r.speed.disabled=!active||editing;r.half.disabled=!active||editing||!t.length;
                r.reset.disabled=!active||editing||t.playbackRate===1;
                r.retrigger.disabled=!active||!t.length||editing||['queued','stopping'].includes(t.state);
                r.stut.disabled=!active||t.state!=='playing';r.stut.classList.toggle('btn-active',!!t.stuttering);
                r.undo.disabled=!active||!t.undoCount||!['playing','stopped'].includes(t.state);
                r.redo.disabled=!active||!t.redoCount||!['playing','stopped'].includes(t.state);
                r.reverse.checked=!!t.reverse;
                r.mult.disabled=!active||!t.length||!['playing','stopped','multiplying'].includes(t.state)||snapshot.tracks.some((other,id)=>id!==i&&['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(other.state));
                r.normalize.disabled=!active||!t.length||!['playing','stopped'].includes(t.state)||snapshot.tracks.some(t=>['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(t.state));
                const mergeBusy=snapshot.tracks.some(t=>['armed','recording','overdubbing','substituting','multiplying','finalizing'].includes(t.state));
                r.mergeSource.disabled=!active||mergeBusy;
                r.mergeGo.disabled=!active||mergeBusy||!['empty','playing','stopped'].includes(t.state)||!sources.includes(Number(r.mergeSource.value))||r.mergeSource.value==='';
                r.solo.setAttribute('aria-pressed',String(t.solo));r.solo.style.cssText=t.solo?'background:#ff0;color:#000;border-color:#ff0':'';
                r.mute.style.background=t.muted?'#f00':'';
                r.mute.style.color=t.muted?'#000':'';
                r.mute.style.borderColor=t.muted?'#f00':'';
                r.mute.textContent=t.muted?'UNM':'MUTE';r.mute.setAttribute('aria-pressed',String(t.muted));
                const input=snapshot.liveInput,live=input?.track===i&&input.startFrame===t.captureStartFrame?{...input,syncMode:['off','static','dynamic'][snapshot.syncMode],now:snapshot.frame/snapshot.sampleRate,barDuration:snapshot.barLength}:null;
                const recording=t.state==='recording'&&t.dynamicRecorded;
                const progress=recording?Math.min(1,Math.max(0,(snapshot.frame-t.captureStartFrame)/snapshot.sampleRate/(snapshot.syncMode===0?10:snapshot.gridLength))):(t.stutterClock?t.clockProgress:(t.length?t.position/t.length:0));
                window.AionDrawWaveform(r.canvas,{state:t.state,graph:recording?{startTime:t.captureStartFrame/snapshot.sampleRate}:null,wavePeaks:r.waveformId===t.waveformId?r.wavePeaks:null},progress,live);
                r.row.querySelector(`#loop-ascii-vu-${i}`).textContent='|'.repeat(Math.min(20,Math.round(t.peak*20)));
            });
            liveRows.forEach((r,i)=>{
                const t=snapshot.tracks[i],view=window.AionLiveLoopStatus(t,i);
                r.row.className=view.className;r.text.textContent=view.text;
                r.text.style.color=r.number.style.color=r.name.style.color=view.color;
                r.name.textContent=view.name;r.reverse.checked=!!t.reverse;r.reverse.disabled=!active;
            });
            liveMixerRows.forEach((r,i)=>{
                const t=snapshot.tracks[i],color=t.length?'#0f0':'#444';
                r.edit.update(t.volume,!!snapshot.midi?.bindings[33+i]);
                r.value.textContent=t.volume.toFixed(2);r.label.style.color=color;r.strip.style.borderTopColor=color;
                r.volume.disabled=r.mute.disabled=r.solo.disabled=!active;
                r.mute.style.background=t.muted?'#f00':'#222';r.mute.style.color=t.muted?'#000':'#fff';r.mute.setAttribute('aria-pressed',String(t.muted));
                r.solo.style.background=t.solo?'#ff0':'#222';r.solo.style.color=t.solo?'#000':'#fff';r.solo.setAttribute('aria-pressed',String(t.solo));
            });
            window.AionLivePanelState(snapshot);
            document.getElementById('odModeIndicator').style.display=mode==='overdub'?'':'none';
            document.getElementById('subModeIndicator').style.display=mode==='substitute'?'':'none';
            syncModes.forEach((radio,index)=>radio.checked=snapshot.syncMode===index);
            for(const [id,element] of Object.entries(timingControls))if(document.activeElement!==element){if(element.type==='checkbox')element.checked=!!snapshot[id];else element.value=snapshot[id];}
            document.getElementById('dynamicOptionsPanel').style.display=snapshot.syncMode===2?'':'none';
            const nextMarkers=`${snapshot.syncMode}:${snapshot.syncBars}:${snapshot.syncNum}`;
            if(markerKey!==nextMarkers){window.AionSyncMarkup(snapshot);markerKey=nextMarkers;}
            document.getElementById('loopLength').textContent=`${snapshot.gridLength.toFixed(2)}s`;
            document.getElementById('playhead').style.left=snapshot.syncMode===0?'0%':`${snapshot.gridPosition*100}%`;
            const armed=snapshot.tracks.find(t=>t.state==='armed');
            const pulse=active&&snapshot.countInFlash;
            const masterModule=document.getElementById('mod-master');
            masterModule.style.borderColor=pulse?(pulse===2?'#f00':'#ff0'):'';
            masterModule.style.boxShadow=pulse?`0 0 10px ${pulse===2?'#f00':'#ff0'}`:'';
            const db=value=>value>0?`${(20*Math.log10(value)).toFixed(1)} dBFS`:'-inf dBFS';
            document.getElementById('master-ascii-vu').textContent=`MASTER | ${db(snapshot.masterPeak??snapshot.outputPeak)}`;
            status.textContent=error || (!active?'Waiting for audio. Enable the audio device or host processing.':
                snapshot.tracks.some(t=>t.stopPending)?'Recording: finish scheduled at the next bar. Press FINISH again to stop now.':
                armed?`Armed: recording starts in ${Math.max(0,(armed.scheduledFrame-snapshot.syncFrame)/snapshot.sampleRate).toFixed(2)}s. Press CANCEL to disarm.`:
                snapshot.tracks.some(t=>t.finalizationFailed)?'Take finalization failed; existing audio was kept.':
                snapshot.poolFull?'Recording stopped: session audio memory is full. Clear a loop to free space.':
                snapshot.audioReset?'Session audio cleared after a sample-rate change.':
                `INPUT ${db(snapshot.inputPeak)} | OUTPUT ${db(snapshot.outputPeak)} | ${(snapshot.freeFrames/snapshot.sampleRate).toFixed(1)}s recording capacity remaining${busy?' | applying...':''}`);
            if(snapshot.exportStatus)status.textContent+=` | ${snapshot.exportStatus}`;
        }
        async function poll() {
            try {
                const reply=await rpc({operation:'snapshot',knownGeneration:snapshot?.generation??0,knownWaveforms:rows.map(r=>r.waveformId??0)});if(!reply.ok)throw Error(reply.error || 'State unavailable');
                if(!alive)return;
                if(snapshot && reply.generation!==snapshot.generation){scheduler.reset();stutter.cancel();keyboard.cancel();rows.forEach(r=>{r.waveformId=0;r.wavePeaks=null;});}
                if(reply.frame!==lastFrame || reply.generation!==snapshot?.generation){lastFrame=reply.frame;advancedAt=Date.now();}
                snapshot=reply;
                midi.render(reply.midi);
                mergeNotice.observe(reply);
                // Native navigation owns export cancellation; late frontend cleanup could cancel a new picker.
                if(!rows.length){releaseStutter(20);createRows(reply);createLiveRows(reply);}
                if(pending) {
                    if(reply.generation!==pending.generation){pending.reject(Error('Audio configuration changed'));pending=null;}
                    else if(reply.lastRequestId>=pending.id){reply.rejected?pending.reject(Error('Action is unavailable in the current loop state')):pending.resolve();pending=null;}
                    else if(Date.now()>pending.deadline){error='Audio is paused; the action remains queued until processing resumes.';}
                }
                refresh();scheduler.pump();
            } catch(e) {if(alive)notify(e.message);}
            finally {if(alive)pollTimer=setTimeout(poll,snapshot?.tracks.some(t=>t.state==='recording'&&t.dynamicRecorded)?16:66);}
        }
        window.addEventListener('pagehide',()=>{stutter.cancel();releaseStutter(20);alive=false;scheduler.close();keyboard.cancel();stickyObserver.disconnect();clearTimeout(pollTimer);if(pending)pending.reject(Error('Editor closed'));pending=null;});
        document.title='Aion-Looper';
        poll();
    });
})();
