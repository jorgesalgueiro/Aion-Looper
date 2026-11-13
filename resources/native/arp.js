// SPDX-License-Identifier: AGPL-3.0-only
// Original Arp controls; audio, originals and exact completion live in C++.
(()=>{
 const fields=[['time','Time',.01,2,.01],['repetitions','Reps',0,10,1],['mix','Mix',0,1,.01],['amplitude','Note Vol',0,1.5,.01],['range','Oct Range',.25,3,.25],['humanize','Feel',0,1,.01],['scale','Scale',0,19,1],['panSpeed','Pan Spd',0,5,.1],['panDepth','Pan Dpt',0,1,.1]];
 const scales=['Octaves','Major','Minor','PentMaj','PentMin','WholeTone','Diminish','Octatonic','Aeolian','Ionian','Maj7','Min7','4ths/5ths','SuperOct','FifthsUp','Chrom','Micro1','Micro2','Wide','Tritones'];
 const presets=['maj-chord','min-chord','angel-oct','frozen','swirling','sync-trip','pentatonic-rise','chromatic-descent'];
 window.AionArpUI=({current,enqueue,equalizer})=>{
  let alive=true;const controls=[],toggles=[];
  const module=document.createElement('div');module.className='retro-module';module.style.borderColor='#e0f';
  module.innerHTML='<div class="module-header" style="color:#e0f"><span id="native-arp-title">ARP DELAY (A)</span><label><input type="checkbox" id="native-arp-enabled"> Arpd</label></div><div class="module-content"><div class="preset-row effect-preset-row"><label>Preset:</label><select class="effect-preset-select" id="sel_arpDelay" aria-label="Arp Delay Preset"><option value="">-- Select --</option></select></div><div style="margin:5px 0"><label><input type="checkbox" id="arpDelaySync"> Sync to Tempo</label></div><div id="native-arp-controls"></div></div>';
  document.getElementById('fx-controls-container').append(module);
  const preset=module.querySelector('select'),enabled=module.querySelector('#native-arp-enabled');
  const send=(action,data={},key)=>enqueue(equalizer.selectedTrack(),{action,...data},key);
  for(const p of presets)preset.add(new Option(p,p));
  preset.addEventListener('change',()=>{if(preset.value)send('preset',{preset:preset.value});});enabled.addEventListener('change',()=>send('toggle'));
  const flag=(name,id)=>{const input=module.querySelector('#'+id);input.addEventListener('change',()=>send('set',{parameter:name,value:input.checked?1:0}));return input;};
  const sync=flag('sync','arpDelaySync');
  for(const [name,label,min,max,step] of fields){
   const row=document.createElement('div');row.className='slider-container';row.style.cssText='display:flex;align-items:center;gap:4px;margin-bottom:20px';
   const text=document.createElement('label'),input=document.createElement('input'),value=document.createElement('span');
   input.id='arpDelay'+name[0].toUpperCase()+name.slice(1);text.htmlFor=input.id;text.textContent=label;text.style.cssText='font-size:10px;width:54px';
   input.type='range';input.min=min;input.max=max;input.step=step;input.style.cssText='flex:1;min-width:20px;margin:8px 4px';
   value.id=input.id+'Val';value.style.cssText='font-size:10px;width:60px;text-align:right';
   const edit=new window.AionMidiSliderEdit(input);
   input.addEventListener('input',()=>{value.textContent=name==='scale'?scales[Math.floor(input.value)]:input.value;send('set',{parameter:name,value:Number(input.value)},`arp:${name}`);});
   controls.push({name,input,value,edit});row.append(text,input,value);module.querySelector('#native-arp-controls').append(row);
   if(name==='scale'){const hold=document.createElement('div');hold.style.margin='5px 0';hold.innerHTML='<label><input type="checkbox" id="arpDelayStay"> STAY (Hold)</label>';row.after(hold);}
  }
  const stay=flag('stay','arpDelayStay');module.addEventListener('keydown',e=>e.stopPropagation());
  window.addEventListener('pagehide',()=>{alive=false;});
  const reveal=()=>{module.style.scrollMarginTop=`${Math.ceil(document.querySelector('.sticky-top')?.getBoundingClientRect().height||0)+16}px`;module.scrollIntoView({block:'start',behavior:'instant'});};
  return {
   bindRow(row,id){const input=row.querySelector(`input[id$="-${id}-arpDelay"]`);if(!input)return;
    input.disabled=false;input.dataset.nativeControl='true';input.title='Toggle Arp Delay';
    input.addEventListener('change',e=>{e.stopPropagation();enqueue(id,{action:'toggle'});equalizer.select(id);});
    const label=input.nextElementSibling;if(label)label.addEventListener('click',e=>{e.stopPropagation();equalizer.select(id);reveal();});toggles.push({input,track:id});
   },
   update(){const s=current(),track=equalizer.selectedTrack(),settings=s?.tracks[track]?.arp;if(!alive||!settings)return;
    module.querySelector('#native-arp-title').textContent=`Loop ${track+1} — ARP DELAY (A)`;enabled.checked=settings.enabled;preset.value=settings.preset;sync.checked=!!settings.values.sync;stay.checked=!!settings.values.stay;
    for(const c of controls){const index=window.AionMidiArpParameters.indexOf(c.name);c.edit.update(settings.values[c.name],!!s.midi?.bindings[124+index]);c.value.textContent=c.name==='scale'?scales[Math.floor(settings.values.scale)]:String(settings.values[c.name]);}
    for(const t of toggles){t.input.checked=!!s.tracks[t.track].arp?.enabled;t.input.disabled=!s.audioReady;}
   }
  };
 };
})();
