(() => {
    'use strict';
    const session=crypto.randomUUID?crypto.randomUUID():`${Date.now()}-${Math.random()}`,pending=new Map();let sequence=0;
    function request(operation,args={}) {
        const id=++sequence;
        return new Promise((resolve,reject)=>{
            const timer=setTimeout(()=>{pending.delete(id);reject(new Error('Native audio request timed out.'));},15000);
            pending.set(id,{resolve,reject,timer});
            try{window.ipc.postMessage(JSON.stringify({...args,id,session,operation}));}catch(reason){clearTimeout(timer);pending.delete(id);reject(reason);}
        });
    }
    window.RustLooper={receive(reply){if(reply.session!==session)return;const job=pending.get(reply.id);if(!job)return;pending.delete(reply.id);clearTimeout(job.timer);if(reply.ok)job.resolve(reply.value);else job.reject(new Error(reply.error));}};
    const get=id=>document.getElementById(id==='start'?'btn-start':id==='rate'?'sampleRateSelect':id);
    const notice='NATIVE AUDIO | Takes cannot be saved yet; closing audio or the window discards them. Grey controls await migration. Each loop holds 30 seconds.';
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
    let token=0,info=null,monitor=false,polling=false,pollEnabled=true,generation=0,lastTracks=[],opening=false;
    const busy=new Set(),trackControls=[];
    const error=reason=>{const text=reason?.message||String(reason);get('error').textContent=text;get('startupError').textContent=text;};
    function enable(el,title){el.disabled=false;el.title=title;return el;}
    const input=get('inputTracksContainer');
    input.innerHTML='<div class="native-controls"><button id="close" class="std-btn btn-red" disabled>CLOSE &amp; CLEAR</button><button id="monitor" class="std-btn btn-green" aria-pressed="false" disabled>MON OFF</button><label>INPUT <input id="input-gain" type="range" min="0" max="2" step="0.01" value="1"></label><meter id="input-meter" min="0" max="1" aria-label="Native input peak"></meter></div><p id="error" role="alert"></p><div id="status" role="status">Start audio to connect your interface.</div>';
    const master=document.querySelector('#mod-master .module-content');
    master.insertAdjacentHTML('beforeend','<div class="native-controls"><label>MASTER <input id="master" type="range" min="0" max="1" step="0.01" value="0.25"></label><meter id="output-meter" min="0" max="1" aria-label="Native output peak"></meter></div><p class="native-notice"></p>');
    master.querySelector('.native-notice').textContent=notice;
    get('driverSelection').insertAdjacentHTML('afterbegin','<div class="native-controls"><label>ASIO INTERFACE <select id="device" aria-label="ASIO interface"></select></label><button id="refresh" class="std-btn">REFRESH</button></div>');
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
        const controls={action,mute,clear,gain:gain||card.querySelector(`[id="loop-vol-slider-${i}"]`),feedback:feedback||card.querySelector(`[id="loop-fbk-slider-${i}"]`)};
        trackControls.push(controls);
        controls.action.onclick=()=>perform(i);
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
        for(const id of ['device','rate','refresh','start']){get(id).disabled=running||opening;get(id).title=running?'Close audio before changing the interface':'Native ASIO audio';}
        for(const id of ['close','monitor','input-gain','master']){get(id).disabled=!running;get(id).title='Native audio control';}
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
            model.loops.forEach((loop,i)=>{loop.volume=1;loop.feedback=.8;trackControls[i].gain.value='1';trackControls[i].feedback.value='.8';get(`loop-vol-display-${i}`).textContent='0.0dB';get(`loop-fbk-display-${i}`).textContent='80%';});
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
            const t=tracks[i]||{state:'empty',length:0,position:0,muted:false},card=get(`loop-${i}`),c=trackControls[i],available=Boolean(token&&i<tracks.length&&!busy.has(i));
            Object.assign(model.loops[i],{state:t.state,muted:t.muted,duration:t.length/(info?.sample_rate||48000)});
            card.className=`loop ${t.state}`;c.action.textContent=renderer.getLoopAction(model.loops[i]);c.action.setAttribute('aria-label',`Loop ${i+1}: ${c.action.textContent}`);
            get(`loop-state-symbol-${i}`).textContent=symbols[t.state]||'( )';get(`loop-state-text-${i}`).textContent=t.state.toUpperCase();get(`loop-extra-info-${i}`).textContent=`${t.muted?'[MUTED] ':''}${t.length?`${model.loops[i].duration.toFixed(1)}s`:''}`;
            for(const el of [c.action,c.gain,c.feedback]){el.disabled=!available;el.title=el===c.action?`Loop ${i+1}: ${c.action.textContent}`:'Native audio control';}
            c.mute.disabled=!available||!t.length;c.clear.disabled=!available||t.state==='empty';c.clear.title='Discard this native take';c.mute.title='Native loop mute';c.mute.textContent=t.muted?'UNM':'MUTE';c.mute.setAttribute('aria-pressed',String(t.muted));c.mute.style.cssText=t.muted?'background:#f00;color:#000;border-color:#f00':'';
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
    window.AionNative={open,close,command,status,devices,request,setPolling(enabled){pollEnabled=enabled;}};
    controls(false);draw([]);devices().catch(error);window.dispatchEvent(new Event('aion-native-ready'));
})();
