// SPDX-License-Identifier: AGPL-3.0-only
// Paired component observation. Does not modify product/reference sources.
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'..');
const git=(...args)=>cp.execFileSync('git',args,{cwd:root,encoding:'utf8'});
const baseline=git('rev-parse','--verify',(process.argv[2]||'fcef1a9')+'^{commit}').trim();
if(!/^[0-9a-f]{40}$/.test(baseline))throw Error('Expected resolved commit');
const directory=path.join(root,'build','drive-kind-paired-'+baseline.slice(0,12));
const source=path.join(directory,'source'),build=path.join(directory,'cmake');
fs.mkdirSync(source,{recursive:true});fs.mkdirSync(build,{recursive:true});
function immutable(file,text){
 if(fs.existsSync(file)){if(fs.readFileSync(file,'utf8')!==text)throw Error('Preserve modified generated source: '+file);}
 else fs.writeFileSync(file,text);
}
for(const ext of ['h','cpp']){
 const text=git('show',baseline+':source/core/Drive.'+ext).replace(/\bDrive\b/g,'BaselineDrive');
 immutable(path.join(source,'BaselineDrive.'+ext),text);
}
const portable=p=>p.replaceAll('\\','/');
immutable(path.join(source,'CMakeLists.txt'),`cmake_minimum_required(VERSION 3.22)
project(DrivePaired LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_executable(drive_paired "${portable(root)}/tests/DriveKindBenchmark.cpp" BaselineDrive.cpp "${portable(root)}/source/core/Drive.cpp" "${portable(root)}/source/core/DriveSettings.cpp" "${portable(root)}/source/core/DistortionSettings.cpp")
target_include_directories(drive_paired PRIVATE "${portable(root)}/source" "${portable(root)}/source/core" "${portable(source)}")
`);
const win=process.platform==='win32';
const cmake=win?path.join(root,'.toolchain/vs2022/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'):'cmake';
const run=crypto.randomUUID(),logFile=path.join(directory,'build-'+run+'.log');
function command(exe,args){
 const result=cp.spawnSync(exe,args,{cwd:root,encoding:'utf8',maxBuffer:16*1024*1024});
 fs.appendFileSync(logFile,(result.stdout||'')+(result.stderr||''));
 if(result.error)throw result.error;if(result.status!==0)throw Error(exe+' failed; see '+logFile);
 return result.stdout;
}
command(cmake,['-S',source,'-B',build,...(win?['-G','Visual Studio 17 2022','-A','x64','-DCMAKE_GENERATOR_INSTANCE='+portable(path.join(root,'.toolchain/vs2022'))]:['-G','Ninja','-DCMAKE_BUILD_TYPE=Release'])]);
command(cmake,['--build',build,'--config','Release','--parallel','2']);
const report=command(path.join(build,win?'Release/drive_paired.exe':'drive_paired'),[]);
const rows=report.trim().split(/\r?\n/).map(JSON.parse);
if(rows.length!==9||rows.some(r=>!Number.isFinite(r.oldMean)||!Number.isFinite(r.newMean)))throw Error('Incomplete paired result');
const output=path.join(directory,'results-'+run+'.jsonl');fs.writeFileSync(output,report);
fs.writeFileSync(path.join(directory,'metadata-'+run+'.json'),JSON.stringify({baseline,currentHead:git('rev-parse','HEAD').trim(),workingTreeModified:!!git('status','--porcelain').trim(),sourceHashes:Object.fromEntries(['source/core/Drive.h','source/core/Drive.cpp','tests/DriveKindBenchmark.cpp'].map(p=>[p,crypto.createHash('sha256').update(fs.readFileSync(path.join(root,p))).digest('hex')])),output,logFile},null,2));
process.stdout.write(report);process.stderr.write('Saved paired observation: '+output+'\n');
