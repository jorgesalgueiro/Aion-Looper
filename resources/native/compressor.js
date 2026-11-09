// SPDX-License-Identifier: AGPL-3.0-only
// Original C presentation, shared selected-loop tab and native command owner.
// No AudioContext, compressor DSP, PCM traffic or browser-side settings owner.
(()=>{
 const names=['threshold','ratio','knee','attack','release','gain','mix'];
 const fields=[['Thr',-100,0,1],['Rat',1,20,.1],['Knee',0,40,1],['Att',0,1,.001],['Rel',.01,1,.01],['Gain',0,20,.1],['Mix',0,1,.01]];
 const defaults=[-16,4,10,.01,.15,2,1];
 const presets=['default','modern','drums','guitar','vocals','soft','hard','sustain','parallel-crush','gentle-glide','snap'];
 window.AionCompressorUI=({rpc,enqueue,current,notify,equalizer})=>{
  let alive=true,busy=false,key='',points=null,changed=true,drawAt=0,selection='';
  const controls=[],toggles=[],container=document.getElementById('fx-controls-container');
  const module=document.createElement('div');module.className='retro-module';module.style.borderColor='lightgreen';
  module.innerHTML='<div class="module-header" style="color:lightgreen"><span id="native-compressor-title">COMPRESSOR (C)</span><label><input type="checkbox" id="native-compressor-enabled"> Comp</label></div><div class="module-content"><div class="preset-row effect-preset-row"><label>Preset:</label><select class="effect-preset-select" id="sel_compressor" aria-label="Compressor Preset"><option value="">-- Select Preset --</option></select></div><div style="text-align:center"><canvas id="compCurveCanvas" width="320" height="120" style="background:#111;border:1px solid #444;margin-bottom:5px" aria-label="Compressor input-output display curve"></canvas></div><div id="native-compressor-controls"></div></div>';
  container.append(module);
  const preset=module.querySelector('#sel_compressor'),enabled=module.querySelector('#native-compressor-enabled'),canvas=module.querySelector('canvas'),ctx=canvas.getContext('2d');
  const send=(action,data={},coalesce)=>enqueue(equalizer.selectedTrack(),{action,...data},coalesce);
  for(const title of presets)preset.add(new Option(title,title));
  preset.addEventListener('change',()=>{if(preset.value)send('preset',{preset:preset.value});});
  enabled.addEventListener('change',()=>send('toggle'));
  for(const [p,name]of names.entries()){
   const [label,min,max,step]=fields[p],row=document.createElement('div');row.className='slider-container';row.style.cssText='display:flex;align-items:center;gap:4px;margin-bottom:20px';
   const text=document.createElement('label'),input=document.createElement('input'),value=document.createElement('span');
   input.id='compressor'+name[0].toUpperCase()+name.slice(1);text.htmlFor=input.id;text.textContent=label;text.style.cssText='font-size:10px;width:32px';
   input.type='range';input.min=min;input.max=max;input.step=step;input.value=defaults[p];input.style.cssText='flex:1;min-width:20px;margin:8px 4px';
   value.id=input.id+'Val';value.style.cssText='font-size:10px;width:44px;text-align:right';
   const edit=new window.AionMidiSliderEdit(input);
   input.addEventListener('input',()=>{value.textContent=input.value;send('set',{values:{[name]:Number(input.value)}},`compressor:${name}`);});
   input.addEventListener('dblclick',()=>{input.value=defaults[p];send('set',{values:{[name]:defaults[p]}},`compressor:${name}`);});
   controls.push({name,input,value,edit});row.append(text,input,value);module.querySelector('#native-compressor-controls').append(row);
  }
  module.addEventListener('keydown',e=>e.stopPropagation());
  function draw(){
   ctx.clearRect(0,0,320,120);ctx.fillStyle='#666';ctx.font='8px monospace';ctx.fillText('OUT',2,8);ctx.fillText('IN',308,118);
   ctx.strokeStyle='#333';ctx.lineWidth=1;ctx.beginPath();
   for(let db=-60;db<=0;db+=12){const x=320+(db/60)*320,y=-(db/60)*120;ctx.moveTo(x,0);ctx.lineTo(x,120);ctx.moveTo(0,y);ctx.lineTo(320,y);}ctx.stroke();
   if(points){ctx.strokeStyle='#0f0';ctx.lineWidth=2;ctx.beginPath();ctx.moveTo(0,120);points.forEach((db,i)=>ctx.lineTo(i,120-((db+60)/60)*120));ctx.stroke();}
  }
  const reveal=()=>{module.style.scrollMarginTop=`${Math.ceil(document.querySelector('.sticky-top')?.getBoundingClientRect().height||0)+16}px`;module.scrollIntoView({block:'start',behavior:'instant'});};
  window.addEventListener('blur',()=>{for(const c of controls)c.input.blur();});
  window.addEventListener('pagehide',()=>{alive=false;points=null;});
  return {
   bindRow(row,id){
    const input=row.querySelector(`input[id$="-${id}-compressor"]`);if(!input)return;
    input.disabled=false;input.dataset.nativeControl='true';input.title='Toggle Comp';
    input.addEventListener('change',e=>{e.stopPropagation();enqueue(id,{action:'toggle'});equalizer.select(id);});
    const label=input.nextElementSibling;if(label){label.title='Compressor controls';label.addEventListener('click',e=>{e.stopPropagation();equalizer.select(id);reveal();});}
    toggles.push({input,track:id});
   },
   update(){
    const s=current(),track=equalizer.selectedTrack(),settings=s?.tracks[track]?.compressor;if(!alive||!settings)return;
    const owner=`${s.generation}:${track}`;if(owner!==selection){selection=owner;points=null;changed=true;key='';}
    module.querySelector('#native-compressor-title').textContent=`Loop ${track+1} — COMPRESSOR (C)`;
    enabled.checked=settings.enabled;preset.value=settings.preset;
    for(const c of controls){c.edit.update(settings.values[c.name],true);c.value.textContent=Number(settings.values[c.name]).toFixed(c.name==='attack'?3:2);}
    for(const t of toggles){t.input.checked=!!s.tracks[t.track].compressor?.enabled;t.input.disabled=!s.audioReady;}
    const next=JSON.stringify([s.generation,track,settings]);
    if(!busy&&next!==key&&Date.now()-drawAt>=100){
     busy=true;drawAt=Date.now();
     rpc({operation:'compressor',action:'curve',track}).then(reply=>{
      if(!alive||track!==equalizer.selectedTrack()||next!==JSON.stringify([current()?.generation,track,current()?.tracks[track]?.compressor]))return;
      if(!reply.ok)throw Error(reply.error);points=reply.points;key=next;changed=true;
     }).catch(e=>{if(alive)notify(e.message);}).finally(()=>{busy=false;});
    }
    if(changed){draw();changed=false;}
   }
  };
 };
})();
