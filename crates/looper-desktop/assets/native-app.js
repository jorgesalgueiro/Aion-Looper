(() => {
    'use strict';
    const session=crypto.randomUUID?crypto.randomUUID():`${Date.now()}-${Math.random()}`,pending=new Map();let sequence=0;
    function request(operation,args={}) {
        const id=++sequence;
        return new Promise((resolve,reject)=>{
            const timer=setTimeout(()=>{pending.delete(id);reject(new Error('Native audio request timed out.'));},operation.startsWith('engine_project_')?120000:15000);
            pending.set(id,{resolve,reject,timer});
            try{window.ipc.postMessage(JSON.stringify({...args,id,session,operation}));}catch(reason){clearTimeout(timer);pending.delete(id);reject(reason);}
        });
    }
    window.RustLooper={receive(reply){if(reply.session!==session)return;const job=pending.get(reply.id);if(!job)return;pending.delete(reply.id);clearTimeout(job.timer);if(reply.ok)job.resolve(reply.value);else job.reject(new Error(reply.error));}};
    const get=id=>document.getElementById(id==='start'?'btn-start':id==='rate'?'sampleRateSelect':id);
    const notice='NATIVE AUDIO | SAVE PROJ keeps your takes in AionProjects. Save before closing; unsaved changes are discarded. Grey controls await migration. Each loop holds 30 seconds.';
    function stripAndDisable(root){
        for(const el of [root,...root.querySelectorAll('*')]){
            for(const attr of [...el.attributes])if(attr.name.startsWith('on'))el.removeAttribute(attr.name);
            if(el.matches('button,input,select,textarea')){el.disabled=true;el.title='Not yet available in native audio mode';}
        }
    }
    stripAndDisable(document.documentElement);
    // Keep the original reference DOM and loop markup, with a separate native model.
    const model={loops:[],soloState:{active:false},fxPresets:{},globalPresets:{},customEffects:{}};
    for(let i=0;i<10;i++)model.loops.push({id:i,state:'empty',name:'',volume:1,pan:5,startDelay:0,duration:0,playbackRate:1,feedback:.8,muted:false,audioBuffer:null,effects:{},signalChain:'QCATFODBVKZG',undoStack:[],redoStack:[]});
    const escapeHTML=value=>String(value).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
    const renderer=window.AionOriginalRenderer(model,{t:key=>key},{escapeHTML},{});
    let token=0,info=null,monitor=false,polling=false,pollEnabled=true,generation=0,lastTracks=[],opening=false,fileBusy=false;
    const busy=new Set(),trackControls=[];
    document.querySelector('.project-file-section').insertAdjacentHTML('beforeend','<label>Native projects <select id="native-projects" aria-label="Saved native projects"></select></label><button id="projects-refresh" class="std-btn">REFRESH PROJECTS</button><p id="project-status" role="status"></p>');
    document.querySelector('.project-format-badge').textContent='.AION';
    document.querySelector('.project-file-section').insertAdjacentHTML('beforeend','<label>WAV path <input id="import-path" type="text" placeholder="Drop a WAV onto a stopped loop, or paste its full path" aria-label="Native WAV path"></label><div class="native-controls"><label>Loop <select id="import-track" aria-label="WAV target loop"></select></label><button id="import-wave" class="std-btn btn-orange" disabled>IMPORT WAV</button></div>');
    for(let i=0;i<10;i++)get('import-track').add(new Option(String(i+1),String(i)));

    enableProjectFieldsLater();
    function enableProjectFieldsLater(){get('projectName').disabled=false;get('projectName').title='Native project name';get('projectName').maxLength=80;}

    const error=reason=>{const text=reason?.message||String(reason);get('error').textContent=text;get('startupError').textContent=text;};
    function enable(el,title){el.disabled=false;el.title=title;return el;}
    const input=get('inputTracksContainer');
    input.innerHTML='<div class="native-controls"><button id="close" class="std-btn btn-red" disabled>CLOSE &amp; CLEAR</button><button id="monitor" class="std-btn btn-green" aria-pressed="false" disabled>MON OFF</button><label>INPUT <input id="input-gain" type="range" min="0" max="2" step="0.01" value="1"></label><meter id="input-meter" min="0" max="1" aria-label="Native input peak"></meter></div><p id="error" role="alert"></p><div id="status" role="status">Start audio to connect your interface.</div>';
    const master=document.querySelector('#mod-master .module-content');
    master.insertAdjacentHTML('beforeend','<div class="native-controls"><label>MASTER <input id="master" type="range" min="0" max="1" step="0.01" value="0.25"></label><meter id="output-meter" min="0" max="1" aria-label="Native output peak"></meter></div><p class="native-notice"></p>');
    master.querySelector('.native-notice').textContent=notice;
    get('driverSelection').insertAdjacentHTML('afterbegin','<div class="native-controls"><label>ASIO INTERFACE <select id="device" aria-label="ASIO interface"></select></label><button id="refresh" class="std-btn">REFRESH</button></div>');
    get('driverSelection').insertAdjacentHTML('beforeend','<div class="native-controls"><label>Saved project <select id="startup-projects" aria-label="Native project to reopen"></select></label><button id="project-load" class="std-btn">LOAD PROJECT</button></div><p id="startup-project-status" role="status"></p>');

    get('rate').innerHTML='<option value="48000">48000 Hz</option><option value="44100">44100 Hz</option>';
    get('start').textContent='[ START AION LOOPER ]';get('driverInfo').textContent=notice+' Rust owns ASIO input and output. Monitoring starts off.';
    get('overlayMobile').hidden=true;get('overlayMobile').style.display='none';
    get('latencySelect').closest('label').title='Buffer size is configured by the ASIO driver';
    get('ws-loops').querySelector('.section-title').insertAdjacentHTML('beforeend',' <label style="float:right;font-size:11px">ACTION <select id="action-mode"><option value="normal">PLAY / STOP</option><option value="overdub">OVERDUB</option><option value="substitute">SUBSTITUTE</option></select></label>');
    enable(get('action-mode'),'Action when pressing a playing loop');
    get('action-mode').onchange=()=>{model.globalOverdubMode=get('action-mode').value==='overdub';model.globalSubstituteMode=get('action-mode').value==='substitute';draw(lastTracks);};
    for(let i=0;i<10;i++){
        const wrapper=document.createElement('div');wrapper.id=`loop-wrapper-${i}`;
        const card=document.createElement('div');card.id=`loop-${i}`;card.className='loop empty';card.innerHTML=renderer.generateLoopHTML(model.loops[i],i);stripAndDisable(card);
        card.insertAdjacentHTML('beforeend',`<div class="native-position" role="progressbar" aria-label="Loop ${i+1} playback position" aria-valuemin="0" aria-valuemax="100" aria-valuenow="0"><div></div></div>`);
        const buttons=[...card.querySelectorAll('button')],mute=buttons.find(b=>b.textContent==='MUTE'),clear=card.querySelector('.loop-track-delete');
        const gain=get(`loop-vol-slider-${i}`),feedback=get(`loop-fbk-slider-${i}`),action=get(`loop-action-${i}`)||card.querySelector('.loop-track-action');
        // IDs are resolved within the detached card until it is attached.
        const controls={action,mute,clear,exportWav:buttons.find(b=>b.textContent==='SAVE'),gain:gain||card.querySelector(`[id="loop-vol-slider-${i}"]`),feedback:feedback||card.querySelector(`[id="loop-fbk-slider-${i}"]`)};
        trackControls.push(controls);
        controls.action.onclick=()=>perform(i);
        controls.exportWav.onclick=()=>exportWave(i).catch(error);
        mute.onclick=()=>runTrack(i,()=>command('mute',i,Number(!lastTracks[i]?.muted)));
        clear.onclick=()=>runTrack(i,()=>command('clear',i));
        controls.gain.oninput=()=>{const value=Number(controls.gain.value);model.loops[i].volume=value;get(`loop-vol-display-${i}`).textContent=value>0?`${(20*Math.log10(value)).toFixed(1)}dB`:'-inf dB';};
        controls.gain.onchange=()=>command('gain',i,Number(controls.gain.value)).catch(error);
        controls.feedback.oninput=()=>{model.loops[i].feedback=Number(controls.feedback.value);get(`loop-fbk-display-${i}`).textContent=`${Math.round(Number(controls.feedback.value)*100)}%`;};
        controls.feedback.onchange=()=>command('feedback',i,Number(controls.feedback.value)).catch(error);
        const name=card.querySelector('.track-name');enable(name,'Loop name (this session only)');name.oninput=()=>{model.loops[i].name=name.value;};
        const canvas=card.querySelector('canvas');canvas.title='Waveform display and export await migration';
        card.querySelector('.ascii-vu-meter').textContent='Level display pending';
        wrapper.appendChild(card);get('loopContainer').appendChild(wrapper);
    }
    function controls(running){
        get('import-wave').disabled=!running||fileBusy;get('import-wave').title='Import WAV into an empty/stopped native loop';
        get('btnSaveProj').disabled=!running||fileBusy;get('btnSaveProj').title='Save native takes and settings (.aion)';get('btnLoadProj').disabled=running||fileBusy;get('btnLoadProj').title='Load the selected native project after closing audio';get('project-load').disabled=running||fileBusy;
        for(const id of ['device','rate','refresh','start']){get(id).disabled=running||opening;get(id).title=running?'Close audio before changing the interface':'Native ASIO audio';}
        for(const id of ['close','monitor','input-gain','master']){get(id).disabled=!running;get(id).title='Native audio control';}
    }

    async function projects(){
        const result=await request('engine_projects');
        for(const id of ['native-projects','startup-projects']){const select=get(id),selected=select.value;select.replaceChildren();select.add(new Option('Choose a saved .aion project',''));for(const name of result.projects)select.add(new Option(name,name));if(result.projects.includes(selected))select.value=selected;}
        get('project-status').textContent=`Projects: ${result.directory}`;return result;
    }

    async function importWave(track,path){
        if(!token)throw new Error('Start native audio before importing.');if(fileBusy)throw new Error('A project operation is already running.');
        if(!lastTracks[track]||!['empty','stopped'].includes(lastTracks[track].state))throw new Error('Stop the target loop before importing. Other loops can keep playing.');
        fileBusy=true;get('import-wave').disabled=true;draw(lastTracks);
        try{const result=await request('engine_project_import',{token,track,path});const c=trackControls[track];model.loops[track].name='';model.loops[track].volume=1;model.loops[track].feedback=.8;get(`loop-${track}`).querySelector('.track-name').value='';c.gain.value='1';c.feedback.value='.8';get(`loop-vol-display-${track}`).textContent='0.0dB';get(`loop-fbk-display-${track}`).textContent='80%';get('project-status').textContent=`Imported WAV into loop ${track+1}: ${(result.frames/result.sample_rate).toFixed(2)}s. Save the project to keep it.`;get('error').textContent='';await status();return result;}
        finally{fileBusy=false;get('import-wave').disabled=!token;draw(lastTracks);}
    }
    async function dropFile(paths,x,y){
        if(paths.length!==1)throw new Error('Drop one WAV file onto one stopped loop.');
        const card=document.elementFromPoint(x,y)?.closest('.loop');if(!card)throw new Error('Drop the WAV onto an empty or stopped loop track.');
        const track=Number(card.id.slice(5));get('import-path').value=paths[0];get('import-track').value=String(track);return importWave(track,paths[0]);
    }
    async function exportWave(track){
        if(fileBusy)throw new Error('A project operation is already running.');fileBusy=true;draw(lastTracks);
        try{const result=await request('engine_project_export',{token,track});get('project-status').textContent=`Exported ${result.file} in ${result.directory}`;return result;}
        finally{fileBusy=false;draw(lastTracks);}
    }
    async function saveProject(){
        if(!token)throw new Error('Start audio before saving.');if(fileBusy)throw new Error('A project operation is already running.');
        fileBusy=true;get('btnSaveProj').disabled=true;draw(lastTracks);
        try{const result=await request('engine_project_save',{token,project:get('projectName').value.trim()||'Project',names:model.loops.slice(0,lastTracks.length).map(loop=>loop.name)});await projects();get('native-projects').value=result.project;get('startup-projects').value=result.project;get('project-status').textContent=`Saved ${result.project} in ${result.directory}`;return result;}
        finally{fileBusy=false;get('btnSaveProj').disabled=!token;draw(lastTracks);}
    }
    async function loadProject(name){
        if(token||opening)throw new Error('Save your current project and CLOSE & CLEAR audio before loading another project.');if(fileBusy)throw new Error('A project operation is already running.');
        fileBusy=true;get('project-load').disabled=true;
        try{const result=await request('engine_project_load',{project:name});if(![...get('rate').options].some(o=>o.value===String(result.sample_rate)))get('rate').add(new Option(`${result.sample_rate} Hz`,String(result.sample_rate)));get('rate').value=String(result.sample_rate);get('master').value=String(result.master);get('projectName').value=name.replace(/-\d+\.aion$/,'');get('startup-project-status').textContent=`Loaded ${name}. Press START to open your interface. Takes reopen stopped; monitoring is off.`;return result;}
        finally{fileBusy=false;get('project-load').disabled=false;}
    }
    async function devices(){
        const result=await request('engine_devices');get('device').replaceChildren();
        for(const device of result.devices)get('device').add(new Option(device.label,JSON.stringify(device)));
        const selection=window.AionNativeInputSelection;
        if(selection?.device)for(const option of get('device').options){const d=JSON.parse(option.value);if(d.device===selection.device||d.device===`asio:${selection.device}`)get('device').value=option.value;}
        if(!result.devices.length)throw new Error('No ASIO drivers are available in this build. Use the ASIO-enabled Windows package.');
    }
    async function open(options={}){
        if(token||opening)throw new Error('Close audio before opening another interface.');
        const selection=options.synthetic?{}:JSON.parse(get('device').value||'null');
        if(!selection&&!options.synthetic)throw new Error('Choose an ASIO interface.');
        opening=true;controls(false);
        try{
            await request('authorize_audio');
            const result=await request('engine_open',{...selection,sample_rate:Number(get('rate').value),tracks:10,seconds:30,gain:Number(get('master').value),...options});
            token=result.token;info=result.info;monitor=false;generation++;get('error').textContent='';get('startupError').textContent='';get('input-gain').value='1';
            get('master').max=String(Math.max(1,result.master));get('input-gain').max=String(Math.max(2,result.input_gain));get('master').value=String(result.master);get('input-gain').value=String(result.input_gain);
            model.loops.forEach((loop,i)=>{const settings=result.settings[i]||{gain:1,feedback:.8};loop.volume=settings.gain;loop.feedback=settings.feedback;loop.name=result.names[i]||'';trackControls[i].gain.max=String(Math.max(2,loop.volume));trackControls[i].gain.value=String(loop.volume);trackControls[i].feedback.value=String(loop.feedback);get(`loop-vol-display-${i}`).textContent=loop.volume>0?`${(20*Math.log10(loop.volume)).toFixed(1)}dB`:'-inf dB';get(`loop-fbk-display-${i}`).textContent=`${Math.round(loop.feedback*100)}%`;get(`loop-${i}`).querySelector('.track-name').value=loop.name;});
            get('monitor').textContent='MON OFF';get('monitor').setAttribute('aria-pressed','false');get('startOverlay').style.display='none';
            return result;
        }finally{opening=false;controls(Boolean(token));}
    }
    function command(command,track=0,value=0){if(!token)return Promise.reject(new Error('Start native audio first.'));return request('engine_control',{token,command,track,value});}
    async function close(){const old=token;token=0;generation++;try{if(old)await request('engine_close',{token:old});}finally{monitor=false;controls(false);get('monitor').textContent='MON OFF';get('monitor').setAttribute('aria-pressed','false');get('input-meter').value=0;get('output-meter').value=0;get('status').textContent='Audio closed. Unsaved native loops have been released.';draw([]);get('startOverlay').style.display='flex';}}
    function draw(tracks){
        lastTracks=tracks;
        const symbols={empty:'( )',recording:'(R)',playing:'[>]',stopped:'[S]',overdubbing:'(O)',substituting:'(-)'};
        for(let i=0;i<10;i++){
            const t=tracks[i]||{state:'empty',length:0,position:0,muted:false},card=get(`loop-${i}`),c=trackControls[i],available=Boolean(token&&i<tracks.length&&!busy.has(i)&&!fileBusy);
            Object.assign(model.loops[i],{state:t.state,muted:t.muted,duration:t.length/(info?.sample_rate||48000)});
            card.className=`loop ${t.state}`;c.action.textContent=renderer.getLoopAction(model.loops[i]);c.action.setAttribute('aria-label',`Loop ${i+1}: ${c.action.textContent}`);
            get(`loop-state-symbol-${i}`).textContent=symbols[t.state]||'( )';get(`loop-state-text-${i}`).textContent=t.state.toUpperCase();get(`loop-extra-info-${i}`).textContent=`${t.muted?'[MUTED] ':''}${t.length?`${model.loops[i].duration.toFixed(1)}s`:''}`;
            for(const el of [c.action,c.gain,c.feedback]){el.disabled=!available;el.title=el===c.action?`Loop ${i+1}: ${c.action.textContent}`:'Native audio control';}
            c.exportWav.disabled=!available||!t.length;c.exportWav.title='Export dry stereo float32 WAV to AionProjects';c.mute.disabled=!available||!t.length;c.clear.disabled=!available||t.state==='empty';c.clear.title='Discard this native take';c.mute.title='Native loop mute';c.mute.textContent=t.muted?'UNM':'MUTE';c.mute.setAttribute('aria-pressed',String(t.muted));c.mute.style.cssText=t.muted?'background:#f00;color:#000;border-color:#f00':'';
            const position=t.length?100*t.position/t.length:0,progress=card.querySelector('.native-position');progress.setAttribute('aria-valuenow',position.toFixed(1));progress.firstElementChild.style.width=`${position}%`;
        }
        for(const state of ['recording','overdubbing','substituting'])document.body.classList.toggle(`is-${state}`,tracks.some(t=>t.state===state));
    }
    async function runTrack(i,job){if(!token||busy.has(i))return;busy.add(i);draw(lastTracks);try{await job();await status();}catch(reason){error(reason);}finally{busy.delete(i);draw(lastTracks);}}
    function perform(i){return runTrack(i,async()=>{
        const state=lastTracks[i]?.state;
        if(state==='empty')await command('record',i);
        else if(state==='stopped')await command('play',i);
        else if(['recording','overdubbing','substituting'].includes(state)){await command('finish',i);await command('play',i);}
        else if(state==='playing')await command(get('action-mode').value==='normal'?'stop':get('action-mode').value,i);
    });}
    async function status(){if(!token)return null;const current=token,epoch=generation;const value=await request('engine_status',{token:current});if(current!==token||epoch!==generation)return null;
        draw(value.tracks);get('input-meter').value=value.input_peak;get('output-meter').value=value.output_peak;
        get('timeDisplay').textContent=`${(value.frames/value.info.sample_rate).toFixed(1)}s`;
        get('master-ascii-vu').textContent=`MASTER ${value.output_peak>0?(20*Math.log10(value.output_peak)).toFixed(1):'-inf'} dBFS`;
        get('status').textContent=`${value.info.host} / ${value.info.device} | ${value.info.sample_rate} Hz | ${value.callback_frames} frames/callback\nInput underrun frames ${value.underruns} | dropped ${value.capture.dropped_frames} | stream errors ${value.stream_errors+value.capture.errors} | command errors ${value.command_errors}`;
        if(value.underruns||value.capture.dropped_frames||value.stream_errors||value.capture.errors||value.discontinuities)error('Native audio reported a gap or device error. Close audio and check the interface settings.');
        return value;
    }
    get('import-wave').onclick=()=>importWave(Number(get('import-track').value),get('import-path').value.trim()).catch(error);
    get('btnSaveProj').onclick=()=>saveProject().catch(error);get('btnLoadProj').onclick=()=>loadProject(get('native-projects').value).catch(error);get('project-load').onclick=()=>loadProject(get('startup-projects').value).catch(error);get('projects-refresh').onclick=()=>projects().catch(error);
    get('start').onclick=()=>open().then(status).catch(error);get('close').onclick=()=>close().catch(error);get('refresh').onclick=()=>devices().catch(error);
    get('monitor').onclick=()=>{const next=!monitor;command('monitor',0,Number(next)).then(()=>{monitor=next;get('monitor').textContent=next?'MON ON':'MON OFF';get('monitor').setAttribute('aria-pressed',String(next));}).catch(error);};
    get('master').onchange=()=>{if(token)command('master',0,Number(get('master').value)).catch(error);};get('input-gain').onchange=()=>{if(token)command('input_gain',0,Number(get('input-gain').value)).catch(error);};
    for(const button of document.querySelectorAll('.project-session-actions button')){
        const clear=button.textContent==='DEL ALL';if(!clear&&button.textContent!=='STOP ALL')continue;
        enable(button,clear?'Discard all native takes':'Stop all native loops');button.onclick=async()=>{for(let i=0;i<lastTracks.length;i++){const state=lastTracks[i].state;if(state==='empty'||(!clear&&state==='stopped'))continue;await runTrack(i,()=>command(clear?'clear':['recording','overdubbing','substituting'].includes(state)?'finish':'stop',i));}};
    }
    document.addEventListener('keydown',event=>{if(event.repeat||event.ctrlKey||event.metaKey||event.altKey||event.shiftKey||event.target.closest('input,select,textarea,[contenteditable]')||!token||get('startOverlay').style.display!=='none')return;if(/^[0-9]$/.test(event.key)){event.preventDefault();perform(event.key==='0'?9:Number(event.key)-1);}});
    function fitLogos(){for(const pre of document.querySelectorAll('#header pre,#startOverlay pre')){pre.style.fontSize='10px';if(pre.scrollWidth>pre.clientWidth)pre.style.fontSize=`${Math.max(2,10*pre.clientWidth/pre.scrollWidth)}px`;}}
    window.addEventListener('resize',fitLogos);fitLogos();
    setInterval(async()=>{if(!pollEnabled||polling||!token)return;polling=true;try{await status();}catch(reason){error(reason);}finally{polling=false;}},100);
    window.addEventListener('pagehide',()=>{if(token)request('engine_close',{token}).catch(()=>{});});
    window.AionNative={open,close,command,status,devices,request,projects,saveProject,loadProject,exportWave,importWave,dropFile,onDrop(paths,x,y){dropFile(paths,x,y).catch(error);},setPolling(enabled){pollEnabled=enabled;}};
    controls(false);draw([]);projects().catch(error);devices().catch(error);window.dispatchEvent(new Event('aion-native-ready'));
})();
