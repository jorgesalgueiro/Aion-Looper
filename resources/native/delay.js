// SPDX-License-Identifier: AGPL-3.0-only
// Original Delay panel/controls, sharing the native command and receipt owner.
(()=>{
 const fields=[['time','Time',.01,4,.01],['repetitions','Reps',0,10,1],['damp','Damp',200,20000,100],['mix','Mix',0,1,.01],['panSpeed','Pan Spd',0,5,.1],['panDepth','Pan Dpt',0,1,.1]];
 const presets=['default','slapback','dotted8th','dub','infinite','ping-pong','short-ambience','tape-echo','rhythmic'];
 window.AionDelayUI=({current,enqueue,equalizer})=>{
  let alive=true;const controls=[],toggles=[];const module=document.createElement('div');module.className='retro-module';module.style.borderColor='#fa0';
  module.innerHTML='<div class="module-header" style="color:#fa0"><span id="native-delay-title">DELAY (D)</span><label><input type="checkbox" id="native-delay-enabled"> Dly</label></div><div class="module-content"><div class="preset-row effect-preset-row"><label>Preset:</label><select class="effect-preset-select" id="sel_delay" aria-label="Delay Preset"><option value="">-- Select --</option></select></div><div style="margin:5px 0"><label><input type="checkbox" id="delaySync"> Sync to Tempo</label></div><div class="preset-row effect-preset-row"><label for="native-delay-mode">Repeats:</label><select id="native-delay-mode" class="effect-preset-select"><option value="feedback">Feedback</option><option value="exact">Exact</option></select><select id="native-delay-count" class="effect-preset-select" aria-label="Exact Delay Repeats"></select></div><div id="native-delay-controls"></div></div>';
  document.getElementById('fx-controls-container').append(module);
  const preset=module.querySelector('#sel_delay'),enabled=module.querySelector('#native-delay-enabled'),sync=module.querySelector('#delaySync'),mode=module.querySelector('#native-delay-mode'),count=module.querySelector('#native-delay-count');
  const send=(action,data={},key)=>enqueue(equalizer.selectedTrack(),{action,...data},key);
  for(const p of presets)preset.add(new Option(p,p));for(let i=1;i<=10;++i)count.add(new Option(String(i),String(i)));
  const reset=document.createElement('button');reset.type='button';reset.textContent='Reset';reset.id='native-delay-reset';reset.addEventListener('click',()=>send('reset'));preset.after(reset);
  preset.addEventListener('change',()=>{if(preset.value)send('preset',{preset:preset.value});});enabled.addEventListener('change',()=>send('toggle'));
  sync.addEventListener('change',()=>send('set',{parameter:'sync',value:sync.checked?1:0}));
  const repeats=()=>send('repeats',{repeatMode:mode.value,repeatCount:Number(count.value)},'delay:repeats');
  mode.addEventListener('change',repeats);count.addEventListener('change',repeats);
  for(const [name,label,min,max,step] of fields){const row=document.createElement('div');row.className='slider-container';row.style.cssText='display:flex;align-items:center;gap:4px;margin-bottom:20px';
   const text=document.createElement('label'),input=document.createElement('input'),value=document.createElement('span');input.id='delay'+name[0].toUpperCase()+name.slice(1);text.htmlFor=input.id;text.textContent=label;text.style.cssText='font-size:10px;width:54px';
   input.type='range';input.min=min;input.max=max;input.step=step;input.style.cssText='flex:1;min-width:20px;margin:8px 4px';value.id=input.id+'Val';value.style.cssText='font-size:10px;width:60px;text-align:right';
   const edit=new window.AionMidiSliderEdit(input);input.addEventListener('input',()=>{value.textContent=input.value;send('set',{parameter:name,value:Number(input.value)},`delay:${name}`);});
   controls.push({name,input,value,edit});row.append(text,input,value);module.querySelector('#native-delay-controls').append(row);
  }
  module.addEventListener('keydown',e=>e.stopPropagation());window.addEventListener('pagehide',()=>{alive=false;});
  const reveal=()=>{module.style.scrollMarginTop=`${Math.ceil(document.querySelector('.sticky-top')?.getBoundingClientRect().height||0)+16}px`;module.scrollIntoView({block:'start',behavior:'instant'});};
  return {
   bindRow(row,id){const input=row.querySelector(`input[id$="-${id}-delay"]`);if(!input)return;input.disabled=false;input.dataset.nativeControl='true';input.title='Toggle Delay';
    input.addEventListener('change',e=>{e.stopPropagation();enqueue(id,{action:'toggle'});equalizer.select(id);});const label=input.nextElementSibling;if(label)label.addEventListener('click',e=>{e.stopPropagation();equalizer.select(id);reveal();});toggles.push({input,track:id});
   },
   update(){const s=current(),track=equalizer.selectedTrack(),settings=s?.tracks[track]?.delay;if(!alive||!settings)return;
    module.querySelector('#native-delay-title').textContent=`Loop ${track+1} — DELAY (D)`;enabled.checked=settings.enabled;preset.value=settings.preset;sync.checked=!!settings.values.sync;
    if(document.activeElement!==mode)mode.value=settings.repeatMode;if(document.activeElement!==count)count.value=String(settings.repeatCount);count.disabled=settings.repeatMode!=='exact';
    for(const c of controls){c.edit.update(settings.values[c.name],false);c.input.disabled=c.name==='repetitions'&&settings.repeatMode==='exact';c.value.textContent=settings.values[c.name].toFixed(2);}
    for(const t of toggles){t.input.checked=!!s.tracks[t.track].delay?.enabled;t.input.disabled=!s.audioReady;}
   }
  };
 };
})();
