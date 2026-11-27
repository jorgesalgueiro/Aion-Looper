// SPDX-License-Identifier: AGPL-3.0-only
// Original B panel/controls, sharing the native reverb command and receipt owner.
(()=>{
 // Original B control order, ranges, steps and initial values.
 const fields=[
  ['volume','Vol',.1,3,.01,1],['mix','Mix',0,1,.01,.15],['duration','Dur',.1,5,.1,1.2],
  ['decay','Decay',.1,5,.1,1.8],['damp','Damp',500,20000,100,20000],['predelay','PreDly',0,.25,.005,0]
 ];
 // Native catalogue order/identity. Rooms are selected by these presets, as in
 // the preserved panel; the command layer keeps the explicit room action.
 const presets=['small','studio','medium','large','hall','plate','bright-plate','spring','cathedral','washed-out','tight-drum','ambient-cloud','dark-cave','dub-space','vocal-room','garage','shimmer-hall'];
 // Conservative MIDI surface: live controls only (targets 153..156).
 const midiTarget={volume:153,mix:154,damp:155,predelay:156};
 window.AionReverbUI=({current,equalizer,enqueue})=>{
  let alive=true;const controls=[],toggles=[];
  const module=document.createElement('div');module.id='native-reverb-panel';module.className='retro-module';module.style.borderColor='lightblue';
  const header=document.createElement('div');header.className='module-header';header.style.color='lightblue';
  const title=document.createElement('span');title.id='native-reverb-title';title.textContent='CONV. REVERB (B)';
  const enableLabel=document.createElement('label');
  const enabled=document.createElement('input');enabled.type='checkbox';enabled.id='native-reverb-enabled';
  enableLabel.append(enabled,document.createTextNode(' revB'));header.append(title,enableLabel);
  const content=document.createElement('div');content.className='module-content';
  const presetRow=document.createElement('div');presetRow.className='preset-row effect-preset-row';
  const presetLabel=document.createElement('label');presetLabel.textContent='Preset:';
  const preset=document.createElement('select');preset.className='effect-preset-select';preset.id='sel_reverb';preset.setAttribute('aria-label','Reverb Preset');
  const blank=document.createElement('option');blank.value='';blank.textContent='-- Select --';preset.append(blank);
  for(const name of presets){const option=document.createElement('option');option.value=name;option.textContent=name;preset.append(option);}
  const reset=document.createElement('button');reset.type='button';reset.id='native-reverb-reset';reset.textContent='Reset';
  presetRow.append(presetLabel,preset,reset);
  const rows=document.createElement('div');rows.id='native-reverb-controls';
  content.append(presetRow,rows);module.append(header,content);
  document.getElementById('fx-controls-container').append(module);
  const send=(action,data={},key)=>enqueue(equalizer.selectedTrack(),{action,...data},key);
  preset.addEventListener('change',()=>{if(preset.value)send('preset',{preset:preset.value});});
  enabled.addEventListener('change',()=>send('toggle'));
  reset.addEventListener('click',()=>send('reset'));
  preset.disabled=enabled.disabled=reset.disabled=true;
  for(const [name,label,min,max,step,initial] of fields){
   const row=document.createElement('div');row.className='slider-container';row.style.cssText='display:flex;align-items:center;gap:4px;margin-bottom:20px';
   const text=document.createElement('label'),input=document.createElement('input'),value=document.createElement('span');
   input.id='reverb'+name[0].toUpperCase()+name.slice(1);text.htmlFor=input.id;text.textContent=label;text.style.cssText='font-size:10px;width:54px';
   input.type='range';input.min=min;input.max=max;input.step=step;input.setAttribute('aria-label',label);input.style.cssText='flex:1;min-width:20px;margin:8px 4px';
   value.id=input.id+'Val';value.style.cssText='font-size:10px;width:60px;text-align:right';
   input.disabled=true;
   const edit=new window.AionMidiSliderEdit(input);
   input.addEventListener('input',()=>{value.textContent=Number(input.value).toFixed(2);send('set',{parameter:name,value:Number(input.value)},'reverb:'+name);});
   input.addEventListener('dblclick',()=>{input.value=initial;value.textContent=initial.toFixed(2);send('set',{parameter:name,value:initial},'reverb:'+name);});
   controls.push({name,input,value,edit});row.append(text,input,value);rows.append(row);
  }
  module.addEventListener('keydown',e=>e.stopPropagation());window.addEventListener('pagehide',()=>{alive=false;});
  const reveal=()=>{module.style.scrollMarginTop=`${Math.ceil(document.querySelector('.sticky-top')?.getBoundingClientRect().height||0)+16}px`;module.scrollIntoView({block:'start',behavior:'instant'});};
  return {
   bindRow(row,id){const input=row.querySelector(`input[id$="-${id}-reverb"]`);if(!input)return;
    input.disabled=false;input.dataset.nativeControl='true';input.title='Toggle Reverb';
    input.addEventListener('change',e=>{e.stopPropagation();enqueue(id,{action:'toggle'});equalizer.select(id);});
    const label=input.nextElementSibling;if(label)label.addEventListener('click',e=>{e.stopPropagation();equalizer.select(id);reveal();});
    toggles.push({input,track:id});
   },
   update(){const s=current(),track=equalizer.selectedTrack(),settings=s?.tracks?.[track]?.reverb;if(!alive||!s||!settings)return;
    const ready=!!s.audioReady;
    title.textContent=`Loop ${track+1} — CONV. REVERB (B)`;
    enabled.checked=!!settings.enabled;preset.value=settings.preset??'';preset.disabled=enabled.disabled=reset.disabled=!ready;
    for(const c of controls){const number=Number(settings.values[c.name]);c.edit.update(number,midiTarget[c.name]?!!s.midi?.bindings[midiTarget[c.name]]:true);c.value.textContent=number.toFixed(2);c.input.disabled=!ready;}
    for(const t of toggles){const other=s.tracks?.[t.track]?.reverb;t.input.checked=!!other?.enabled;t.input.disabled=!ready;}
   }
  };
 };
})();
