// MIDI configuration only. Performance dispatch and timers stay in C++.
(()=>{
window.AionMidiMode = class {
    constructor(){this.sequence=0;this.pending=false;this.fence=null;this.last=-1;}
    begin(){this.pending=true;this.fence=null;return ++this.sequence;}
    ack(sequence,revision){if(sequence===this.sequence)this.fence=revision;}
    fail(sequence){if(sequence===this.sequence){this.pending=false;this.last=-1;}}
    observe(view){if(this.pending&&(this.fence===null||view.revision<this.fence))return null;this.pending=false;if(view.modeRevision===this.last)return null;this.last=view.modeRevision;return ['normal','overdub','substitute'][view.mode];}
};
window.AionMidiSliderTarget=id=>{const m=/^loop-(vol|pan)-slider-(\d+)$/.exec(id);return m&&Number(m[2])<20?(m[1]==='vol'?33:53)+Number(m[2]):-1;};
window.AionMidiSetup=({rpc,notify,setMode})=>{
    window.AionMidiMarkup();let current=null;const modeSync=new window.AionMidiMode();
    const button=document.getElementById('enableMidiBtn'),status=document.getElementById('midiStatus'),mappingStatus=document.getElementById('mapping-status');
    const send=async request=>{const reply=await rpc({operation:'midiConfig',...request});if(!reply.ok)throw Error(reply.error);return reply;};
    const route=document.createElement('div');route.id='native-midi-routing';route.className='module-content';
    const hint=document.createElement('p');hint.textContent='Click a MIDI field, then send a Note, CC or Program Change. Right-click to clear; Escape cancels. PC = one tap; CC = rising edge. Only held notes can cut/delete (600 ms / 4 s).';
    const manual={en:['Native MIDI looper controls','Use INPUT MAPPING & CONTROLS → MIDI. Click a field and send a Note, CC or Program Change; right-click clears it and Escape cancels learning. Save Mappings stores bindings in native app/plugin state. PC sends one tap; a CC must return to zero before its next positive edge. Only a NOTE held from STOP can delete after 4 s; PC/CC triggers never hold-delete. Route native inputs here in Standalone, or through your DAW in plugins. Right-click a loop volume or pan slider to learn an expression/fader CC (including zero); Shift+right-click clears it. Escape or 10 s cancels learning. Slider CC values are continuous and never trigger loop actions. Other expression targets, synth MIDI, clock sync and gamepad mapping are still being migrated.'],es:['Controles MIDI nativos del looper','Usa INPUT MAPPING & CONTROLS → MIDI. Haz clic en un campo y envía Note, CC o Program Change; clic derecho borra y Escape cancela el aprendizaje. Save Mappings guarda las asignaciones en el estado nativo. PC es una pulsación; un CC debe volver a cero antes del siguiente valor positivo. Solo una NOTE mantenida desde STOP borra después de 4 s; PC/CC nunca borran por mantener. Selecciona entradas aquí en Standalone o en tu DAW para plugins. Clic derecho en volumen o panorama de un loop aprende CC continuo; Mayús+clic derecho borra. Escape o 10 s cancela. Otros parámetros, sintetizador MIDI, reloj y gamepad siguen en migración.'],pt:['Controlos MIDI nativos do looper','Usa INPUT MAPPING & CONTROLS → MIDI. Clica num campo e envia Note, CC ou Program Change; botão direito limpa e Escape cancela a aprendizagem. Save Mappings guarda as atribuições no estado nativo. PC é um toque; um CC tem de voltar a zero antes do próximo valor positivo. Só uma NOTE mantida desde STOP apaga após 4 s; PC/CC nunca apagam por manter. Escolhe entradas aqui em Standalone ou na DAW para plugins. Botão direito no volume ou panorama de um loop aprende CC contínuo; Shift+botão direito limpa. Escape ou 10 s cancela. Outros parâmetros, sintetizador MIDI, relógio e gamepad continuam em migração.']};
    for(const [language,[title,text]] of Object.entries(manual)){const panel=document.getElementById('manual-'+language);if(panel){const note=document.createElement('section');note.className='native-midi-manual';const heading=document.createElement('h4'),paragraph=document.createElement('p');heading.textContent=title;paragraph.textContent=text;note.append(heading,paragraph);panel.prepend(note);}}
    const refresh=document.createElement('button');refresh.className='std-btn small';refresh.textContent='REFRESH MIDI INPUTS';refresh.disabled=false;
    const devices=document.createElement('div');devices.id='native-midi-devices';route.append(hint,refresh,devices);document.getElementById('keyMappingUI').append(route);
    const listDevices=async()=>{try{const reply=await rpc({operation:'midiDevices'});if(!reply.ok)throw Error(reply.error);devices.replaceChildren();
        if(!reply.standalone){devices.textContent='Plugin: route controller MIDI through the host.';return;}
        if(!reply.devices.length){devices.textContent='No native MIDI inputs found. Connect the controller and refresh.';return;}
        for(const device of reply.devices){const label=document.createElement('label'),check=document.createElement('input');check.type='checkbox';check.checked=device.enabled;check.disabled=false;label.append(check,document.createTextNode(' '+device.name+' '));devices.append(label);
            check.addEventListener('change',async()=>{try{const r=await rpc({operation:'setMidiDevice',id:device.id,enabled:check.checked});if(!r.ok)throw Error(r.error);}catch(e){notify(e.message);}await listDevices();});}
    }catch(e){notify(e.message);}};
    refresh.addEventListener('click',listDevices);
    const tabs=[...document.querySelectorAll('#keyMappingUI .tab-btn')];for(const tab of tabs){tab.disabled=tab.dataset.tab==='gamepad';if(!tab.disabled)tab.addEventListener('click',()=>{for(const t of tabs)t.classList.toggle('active',t===tab);for(const panel of document.querySelectorAll('#keyMappingUI .tab-content'))panel.classList.toggle('active',panel.id==='tab-'+tab.dataset.tab);});}
    tabs.find(t=>t.dataset.tab==='midi').click();
    for(let i=0;i<33;++i){const input=document.getElementById('map-midi-'+i);if(!input)continue;input.disabled=false;input.title='Learn Note / CC / Program Change; right-click clears';
        input.addEventListener('click',()=>send({action:'learn',target:i}).then(listDevices).catch(e=>notify(e.message)));
        input.addEventListener('contextmenu',e=>{e.preventDefault();send({action:'clear',target:i}).catch(e=>notify(e.message));});
    }
    const save=document.getElementById('saveMappingsBtn');save.disabled=false;save.addEventListener('click',()=>send({action:'save'}).catch(e=>notify(e.message)));
    button.disabled=false;button.addEventListener('click',()=>send({action:'toggle'}).then(listDevices).catch(e=>notify(e.message)));
    document.addEventListener('keydown',e=>{if(e.code==='Escape'&&current?.learnTarget>=0){e.preventDefault();send({action:'cancelLearn'}).catch(e=>notify(e.message));}});
    document.addEventListener('contextmenu',e=>{const target=window.AionMidiSliderTarget(e.target.id||'');if(target<0||e.target.disabled)return;
        e.preventDefault();send({action:e.shiftKey?'clear':'learn',target}).then(listDevices).catch(error=>notify(error.message));});
    const kbd=['1','2','3','4','5','6','7','8','9','0'];for(let i=0;i<20;++i){const input=document.getElementById('map-kbd-'+i);if(input)input.value=i<10?kbd[i]:'Shift+'+kbd[i-10];}
    listDevices();
    return {mode:value=>{const sequence=modeSync.begin();return send({action:'mode',mode:value}).then(reply=>modeSync.ack(sequence,reply.revision)).catch(e=>{modeSync.fail(sequence);notify(e.message);});},render:midi=>{
        if(!midi)return;current=midi;
        const next=modeSync.observe(midi);if(next!==null)setMode(next);
        button.textContent=midi.enabled?'Disable Midi':'Enable Midi';
        document.getElementById('susModeIndicator').style.display=midi.sustain?'':'none';
        status.textContent=`${midi.enabled?'Native MIDI enabled':'Native MIDI disabled'} | ${midi.messages} messages | ${midi.actions} actions${midi.overflows?' | input overflow: held actions cancelled':''}${midi.rejected?' | rejected actions: '+midi.rejected:''}`;
        mappingStatus.textContent=midi.status||'';
        for(let i=0;i<20;++i)for(const [kind,base] of [['vol',33],['pan',53]]){const slider=document.getElementById(`loop-${kind}-slider-${i}`);if(!slider)continue;const target=base+i,b=midi.bindings[target]||0;
            slider.style.outline=midi.learnTarget===target?'2px solid #f0f':'';
            slider.title=`Right-click: learn MIDI CC; Shift+right-click: clear${b?` | CC ${(b>>2)&127}${b>>9?' CH '+(b>>9):' ANY'}`:''}`;}
        for(let i=0;i<33;++i){const input=document.getElementById('map-midi-'+i);if(!input)continue;const b=midi.bindings[i]||0;input.value=b?`${['','NOTE','CC','PC'][b&3]} ${(b>>2)&127}${b>>9?' CH '+(b>>9):' ANY'}`:'';input.style.outline=midi.learnTarget===i?'2px solid #ff0':'';}
    }};
};})();
