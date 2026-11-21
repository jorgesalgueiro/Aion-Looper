// SPDX-License-Identifier: AGPL-3.0-only
// Offline attribution of the first primary-executable histogram produced by
// our Linux/glibc ELF64 little-endian profile target. Never runs on audio.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),cp=require('node:child_process'),assert=require('node:assert/strict');
function histogram(b){
 assert(b.length>=61&&b.length<=64*1024*1024,'Profile file size is outside supported bounds');
 assert.equal(b.toString('ascii',0,4),'gmon','Not a GNU profile');assert.equal(b.readUInt32LE(4),1,'Unsupported GNU profile version');
 assert.equal(b[20],0,'Expected primary time histogram as first record');
 const low=Number(b.readBigUInt64LE(21)),high=Number(b.readBigUInt64LE(29)),size=b.readUInt32LE(37),rate=b.readUInt32LE(41);
 assert(Number.isSafeInteger(low)&&Number.isSafeInteger(high)&&high>low,'Unsupported address range');
 assert(size>0&&size<=16*1024*1024&&61+size*2<=b.length,'Invalid histogram length');
 assert(rate>0&&rate<=1000000,'Invalid sample rate');
 const bins=[];let total=0;
 for(let i=0;i<size;++i){const count=b.readUInt16LE(61+i*2);if(count){assert(bins.length<8192,'Too many nonzero buckets');const address=low+Math.floor(i*(high-low)/size);assert(Number.isSafeInteger(address),'Unsupported bucket address');bins.push({address,count});total+=count;}}
 assert(total>0,'No recorded samples');return {low,high,size,rate,total,bins};
}
function locations(text,addresses){
 const groups=text.trim().split(/(?=^0x[0-9a-f]+\s*$)/mi).filter(s=>s.trim());assert.equal(groups.length,addresses.length,'Incomplete address resolution');
 return groups.map((group,i)=>{
  const lines=group.trim().split(/\r?\n/);assert.equal(Number.parseInt(lines.shift(),16),addresses[i],'Resolver changed address order');
  assert(lines.length>0&&lines.length%2===0&&lines.length<=128,'Invalid inline stack');
  const frames=[];for(let n=0;n<lines.length;n+=2){const match=/^(.*):(\d+|\?)(?: \(discriminator \d+\))?$/.exec(lines[n+1]);assert(match,'Invalid source location');frames.push({name:lines[n],file:match[1],line:match[2]==='?'?0:Number(match[2])});}return frames;
 });
}
function sourceRanges(text){
 const lines=text.split(/\r?\n/),one=needle=>{const matches=lines.flatMap((s,i)=>s.includes(needle)?[i+1]:[]);assert.equal(matches.length,1,'Changed/ambiguous source anchor: '+needle);return matches[0];};
 return {toneStart:one('const auto dc=w-c.dcInput+'),toneEnd:one('w=current[3]*v2;return'),controlsStart:one('for(unsigned p=0;p<5;++p){current[p]+='),controlsEnd:one('bypass+=smoothing*(bypassTarget-bypass);')};
}
function classify(frames,r){
 const source=f=>f.file.replaceAll('\\','/').endsWith('/source/core/Drive.cpp');
 const drive=frames.some(f=>source(f)||f.name.includes('aion::Drive::'));
 if(!drive)return frames.every(f=>f.name==='??'&&f.file==='??')?'unresolved_executable':'other_executable';
 if(frames.some(f=>/::(aa|antialiased|integral|integrated|transfer|root)\(/.test(f.name)))return 'nonlinear';
 if(frames.some(f=>f.name.includes('Drive::Up<')))return 'upsampling';
 if(frames.some(f=>f.name.includes('Drive::Down<')))return 'downsampling_wet_and_dry';
 if(frames.some(f=>f.name.includes('::branch<')))return 'resampling_unresolved';
 if(frames.some(f=>source(f)&&f.line>=r.toneStart&&f.line<=r.toneEnd))return 'dc_tone_and_mix';
 if(frames.some(f=>source(f)&&f.line>=r.controlsStart&&f.line<=r.controlsEnd))return 'control_smoothing';
 return 'drive_other';
}
function main(){
 assert.equal(process.argv.length,5,'Usage: node tools/AttributeDriveProfile.cjs EXECUTABLE GMON EXPECTED_DRIVE_CPP_SHA256');
 const [executable,profile,expected]=process.argv.slice(2);assert(/^[0-9a-f]{64}$/i.test(expected),'Expected source SHA256');
 const source=fs.readFileSync(path.resolve(__dirname,'../source/core/Drive.cpp')),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
 assert.equal(hash(source),expected.toLowerCase(),'Source changed since measured snapshot; refusing current line anchors');
 const elf=fs.readFileSync(executable);assert(elf.toString('hex',0,4)==='7f454c46'&&elf[4]===2&&elf[5]===1,'Only ELF64 little-endian is supported');
 const raw=fs.readFileSync(profile),h=histogram(raw),addresses=h.bins.map(b=>b.address);
 const result=cp.spawnSync('addr2line',['-a','-f','-C','-i','-e',executable],{input:addresses.map(a=>'0x'+a.toString(16)).join('\n')+'\n',encoding:'utf8',maxBuffer:16*1024*1024,timeout:60000});
 if(result.error)throw result.error;assert.equal(result.status,0,result.stderr);
 const stacks=locations(result.stdout,addresses),ranges=sourceRanges(source.toString('utf8')),counts={};
 const samples=h.bins.map((b,i)=>{const stage=classify(stacks[i],ranges);counts[stage]=(counts[stage]||0)+b.count;return {...b,stage,frames:stacks[i]};});
 const outside=stage=>stage==='other_executable'||stage==='unresolved_executable';
 const driveTotal=h.total-(counts.other_executable||0)-(counts.unresolved_executable||0);
 const stages=Object.entries(counts).map(([stage,count])=>({stage,count,percentExecutable:100*count/h.total,percentDrive:outside(stage)||!driveTotal?null:100*count/driveTotal})).sort((a,b)=>b.count-a.count);
 console.log(JSON.stringify({scope:'Coarse instrumented executable samples, inline/source attribution; not native latency or exact instruction cost; shared-library time excluded; wet/dry decimation aggregated',binarySha256:hash(elf),profileSha256:hash(raw),sourceSha256:hash(source),rate:h.rate,totalSamples:h.total,driveSamples:driveTotal,stages,samples},null,2));
}
module.exports={histogram,locations,sourceRanges,classify};
if(require.main===module){try{main();}catch(e){console.error(e.message);process.exitCode=1;}}
