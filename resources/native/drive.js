// SPDX-License-Identifier: AGPL-3.0-only
// T/O/F presentation only. One native settings/DSP owner serves GUI/CLI/MIDI.
(()=>{
 window.AionDriveUI=({rpc,current,enqueue,notify,equalizer})=>{
  let alive=true;const panels=[];
  const container=document.getElementById('fx-controls-container');
  for(const [name,code,color]of [['distortion','T','yellow'],['overdrive','O','red'],['fuzz','F','orange']]){
   const module=document.createElement('div');module.className='retro-module';module.style.borderColor=color;
   module.innerHTML=`<div class="module-header" style="color:${color}"><span id="native-${name}-title">${name.toUpperCase()} (${code})</span><label><input id="native-${name}-enabled" type="checkbox" disabled> ${name}</label></div><div class="module-content"><div class="preset-row effect-preset-row"><label>Preset:</label><select class="effect-preset-select" id="sel_${name}" aria-label="${name} Preset" disabled><option value="">-- Select Preset --</option></select></div><div id="native-${name}-controls"></div></div>`;
   container.append(module);const enabled=module.querySelector('input'),preset=module.querySelector('select');
   const panel={name,code,module,enabled,preset,controls:[],toggles:[],ready:false};panels.push(panel);
   const send=(action,data={},key)=>enqueue(equalizer.selectedTrack(),{effect:name,action,...data},key);
   enabled.addEventListener('change',()=>send('toggle'));preset.addEventListener('change',()=>{if(preset.value)send('preset',{preset:preset.value});});
   module.addEventListener('keydown',e=>e.stopPropagation());
   rpc({operation:'drive',track:0,effect:name,action:'get'}).then(reply=>{
    if(!alive)return;if(!reply.ok||reply.schema!==1||!Array.isArray(reply.controls)||reply.controls.length>5||!Array.isArray(reply.presets)||reply.presets.length>17)throw Error(reply.error||'Invalid drive control schema');
    for(const title of reply.presets)preset.add(new Option(title,title));
    for(const control of reply.controls){
     const p=control.name;if(typeof p!=='string'||!['initial','min','max','step'].every(k=>Number.isFinite(control[k])))throw Error('Invalid drive control');
     const row=document.createElement('div');row.className='slider-container';row.style.cssText='display:flex;align-items:center;gap:4px;margin-bottom:20px';
     const label=document.createElement('label'),input=document.createElement('input'),value=document.createElement('span');
     input.id=name+p[0].toUpperCase()+p.slice(1);input.type='range';input.min=control.min;input.max=control.max;input.step=control.step;input.value=control.initial;input.style.cssText='flex:1;min-width:20px;margin:8px 4px';
     label.htmlFor=input.id;label.textContent=p==='volume'?'Vol':p[0].toUpperCase()+p.slice(1);label.style.cssText='font-size:10px;width:44px';value.id=input.id+'Val';value.style.cssText='font-size:10px;width:54px;text-align:right';
     const edit=new window.AionMidiSliderEdit(input);
     input.addEventListener('input',()=>{value.textContent=input.value;send('set',{values:{[p]:Number(input.value)}},`${name}:${p}`);});
     input.addEventListener('dblclick',()=>send('set',{values:{[p]:control.initial}},`${name}:${p}`));
     row.append(label,input,value);module.querySelector(`#native-${name}-controls`).append(row);panel.controls.push({name:p,input,value,edit});
    }panel.ready=true;
   }).catch(e=>{if(alive)notify(e.message);});
  }
  window.addEventListener('blur',()=>{for(const p of panels)for(const c of p.controls)c.input.blur();});
  window.addEventListener('pagehide',()=>alive=false);
  return {
   bindRow(row,track){for(const p of panels){const input=row.querySelector(`input[id$="-${track}-${p.name}"]`);if(!input)continue;
    input.dataset.nativeControl='true';input.addEventListener('change',e=>{e.stopPropagation();enqueue(track,{effect:p.name,action:'toggle'});equalizer.select(track);});
    const label=input.nextElementSibling;if(label)label.addEventListener('click',e=>{e.stopPropagation();equalizer.select(track);p.module.style.scrollMarginTop=`${Math.ceil(document.querySelector('.sticky-top')?.getBoundingClientRect().height||0)+16}px`;p.module.scrollIntoView({block:'start',behavior:'instant'});});
    p.toggles.push({input,track});
   }},
   update(){const s=current(),track=equalizer.selectedTrack();if(!alive||!s)return;
    for(const p of panels){const settings=s.tracks[track]?.drives?.[p.name];if(!settings)continue;
     p.module.querySelector(`#native-${p.name}-title`).textContent=`Loop ${track+1} — ${p.name.toUpperCase()} (${p.code})`;
     p.enabled.disabled=p.preset.disabled=!p.ready||!s.audioReady;p.enabled.checked=settings.enabled;p.preset.value=settings.preset;
     for(const c of p.controls){c.input.disabled=!s.audioReady;c.edit.update(settings.values[c.name],true);c.value.textContent=String(settings.values[c.name]);}
     for(const t of p.toggles){t.input.disabled=!p.ready||!s.audioReady;t.input.checked=!!s.tracks[t.track]?.drives?.[p.name]?.enabled;}
    }
   }
  };
 };
})();
