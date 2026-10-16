(() => {
    const session=typeof crypto==='object'&&crypto.randomUUID?crypto.randomUUID():`${Date.now()}-${Math.random()}`,pending=new Map();let sequence=0;
    function request(operation,args={}) {
        const id=++sequence;
        return new Promise((resolve,reject)=>{
            const timer=setTimeout(()=>{pending.delete(id);reject(new Error('Native audio request timed out.'));},15000);
            pending.set(id,{resolve,reject,timer});
            try{window.ipc.postMessage(JSON.stringify({...args,id,session,operation}));}catch(error){clearTimeout(timer);pending.delete(id);reject(error);}
        });
    }
    window.RustLooper={receive(reply){if(reply.session!==session)return;const job=pending.get(reply.id);if(!job)return;pending.delete(reply.id);clearTimeout(job.timer);if(reply.ok)job.resolve(reply.value);else job.reject(new Error(reply.error));}};
    let token=0,info=null,monitor=false,polling=false,pollEnabled=true,generation=0;
    const get=id=>document.getElementById(id),error=reason=>{get('error').textContent=reason?.message||String(reason);};
    function controls(running){for(const id of ['device','rate','refresh','start'])get(id).disabled=running;for(const id of ['close','monitor'])get(id).disabled=!running;}
    async function devices(){
        const result=await request('engine_devices');get('device').replaceChildren();
        for(const device of result.devices)get('device').add(new Option(device.label,JSON.stringify(device)));
        const selection=window.AionNativeInputSelection;
        if(selection?.device)for(const option of get('device').options){const d=JSON.parse(option.value);if(d.device===selection.device||d.device===`asio:${selection.device}`)get('device').value=option.value;}
        if(!result.devices.length)throw new Error('No ASIO drivers are available in this build. Use the ASIO-enabled Windows package.');
    }
    async function open(options={}){
        if(token)throw new Error('Close audio before opening another interface.');
        const selection=options.synthetic?{}:JSON.parse(get('device').value||'null');
        if(!selection&&!options.synthetic)throw new Error('Choose an ASIO interface.');
        await request('authorize_audio');
        const result=await request('engine_open',{...selection,sample_rate:Number(get('rate').value),tracks:10,seconds:30,gain:Number(get('master').value),...options});
        token=result.token;info=result.info;monitor=false;generation++;get('error').textContent='';controls(true);get('input-gain').value='1';for(const slider of document.querySelectorAll('[data-control]'))slider.value=slider.dataset.control==='gain'?'1':'0.8';
        get('monitor').textContent='MON OFF';get('monitor').setAttribute('aria-pressed','false');
        return result;
    }
    function command(command,track=0,value=0){if(!token)return Promise.reject(new Error('Start native audio first.'));return request('engine_control',{token,command,track,value});}
    async function close(){const old=token;token=0;generation++;try{if(old)await request('engine_close',{token:old});}finally{monitor=false;controls(false);get('monitor').textContent='MON OFF';get('monitor').setAttribute('aria-pressed','false');get('input-meter').value=0;get('output-meter').value=0;get('status').textContent='Audio closed. Unsaved native loops have been released.';draw([]);}}
    function draw(tracks){
        for(let i=0;i<10;i++){
            const t=tracks[i]||{state:'empty',length:0,position:0,muted:false},card=get(`loop-${i}`);
            card.className=`loop ${t.state}`;card.querySelector('.track-status').textContent=`${t.state.toUpperCase()} · ${(t.length/(info?.sample_rate||48000)).toFixed(2)} s`;
            const writing=['recording','overdubbing','substituting'].includes(t.state);
            for(const button of card.querySelectorAll('button')){
                const action=button.dataset.command;
                const available=action==='record'?t.state==='empty':action==='play'?t.state==='stopped':action==='finish'?writing:action==='stop'?t.state==='playing':action==='overdub'||action==='substitute'?t.state==='playing':action==='clear'?t.state!=='empty':t.length>0;
                button.disabled=!token||i>=tracks.length||!available;
                if(action==='mute'){button.textContent=t.muted?'UNMUTE':'MUTE';button.dataset.value=t.muted?'0':'1';button.setAttribute('aria-pressed',String(t.muted));}
            }
            for(const slider of card.querySelectorAll('input'))slider.disabled=!token||i>=tracks.length;
        }
    }
    async function status(){if(!token)return null;const current=token,epoch=generation;const value=await request('engine_status',{token:current});if(current!==token||epoch!==generation)return null;
        draw(value.tracks);get('input-meter').value=value.input_peak;get('output-meter').value=value.output_peak;
        get('status').textContent=`${value.info.host} / ${value.info.device} · ${value.info.sample_rate} Hz · ${value.callback_frames} frames/callback\nClock ${value.frames} · input underrun frames ${value.underruns} · dropped ${value.capture.dropped_frames} · stream errors ${value.stream_errors+value.capture.errors} · command errors ${value.command_errors}`;
        if(value.underruns||value.capture.dropped_frames||value.stream_errors||value.capture.errors||value.discontinuities)get('error').textContent='Native audio reported a gap or device error. Close audio and check the interface settings.';
        return value;
    }
    for(let i=0;i<10;i++){
        const card=document.createElement('section');card.id=`loop-${i}`;card.className='loop';
        card.innerHTML=`<h2>LOOP ${i+1}</h2><div class="track-status">EMPTY</div><div class="buttons">${[['record','REC'],['finish','FINISH'],['play','PLAY'],['stop','STOP'],['overdub','OVERDUB'],['substitute','SUBSTITUTE'],['mute','MUTE'],['clear','CLEAR']].map(([command,label])=>`<button data-command="${command}" disabled>${label}</button>`).join('')}</div><label>LEVEL <input data-control="gain" aria-label="Loop ${i+1} level" type="range" min="0" max="2" step="0.01" value="1" disabled></label><label>FEEDBACK <input data-control="feedback" aria-label="Loop ${i+1} feedback" type="range" min="0" max="1" step="0.01" value="0.8" disabled></label>`;
        card.addEventListener('click',event=>{const button=event.target.closest('button[data-command]');if(button&&!button.disabled)command(button.dataset.command,i,Number(button.dataset.value||0)).then(status).catch(error);});
        card.addEventListener('change',event=>{if(event.target.dataset.control)command(event.target.dataset.control,i,Number(event.target.value)).catch(error);});get('loops').appendChild(card);
    }
    get('start').onclick=()=>open().then(status).catch(error);get('close').onclick=()=>close().catch(error);get('refresh').onclick=()=>devices().catch(error);
    get('monitor').onclick=()=>{const next=!monitor;command('monitor',0,Number(next)).then(()=>{monitor=next;get('monitor').textContent=next?'MON ON':'MON OFF';get('monitor').setAttribute('aria-pressed',String(next));}).catch(error);};
    get('master').onchange=()=>{if(token)command('master',0,Number(get('master').value)).catch(error);};
    get('input-gain').onchange=()=>{if(token)command('input_gain',0,Number(get('input-gain').value)).catch(error);};
    setInterval(async()=>{if(!pollEnabled||polling||!token)return;polling=true;try{await status();}catch(reason){error(reason);}finally{polling=false;}},100);
    window.addEventListener('pagehide',()=>{if(token)request('engine_close',{token}).catch(()=>{});});
    window.AionNative={open,close,command,status,devices,request,setPolling(enabled){pollEnabled=enabled;}};
    devices().catch(error);
    window.dispatchEvent(new Event('aion-native-ready'));
})();
