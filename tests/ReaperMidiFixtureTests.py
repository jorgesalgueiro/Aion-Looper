"""Portable negative controls for the isolated REAPER MIDI gate (no DAW)."""
import base64
import importlib.util
import math
from pathlib import Path
import struct
import tempfile
import unittest
import wave

spec=importlib.util.spec_from_file_location('runner',Path(__file__).parents[1]/'tools/TestReaper.py')
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)

class Fixtures(unittest.TestCase):
    def state(self):
        xml=('<AionState><PARAM id="masterGain" value="1.0"/>'
             '<PARAM id="inputMonitor" value="1.0"/>'
             '<PARAM id="syncMode" value="2.0"/>'
             '<MidiSettings enabled="0" version="3" '+
             ' '.join('binding%d="%s"'%(i,'145' if i==0 else '0') for i in range(93))+'/></AionState>').encode()
        return b'opaque-prefix'+b'VC2!'+struct.pack('<I',len(xml))+xml+b'\0opaque-suffix'

    def test_equal_length_fixture_and_opaque_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            run=Path(tmp);raw=self.state()
            (run/'midi-source.b64').write_text(base64.b64encode(raw).decode())
            runner.midi_fixtures(run)
            for label,kind,channel in runner.midi_specs():
                generated=base64.b64decode((run/('midi-%s.b64'%label)).read_text())
                self.assertEqual(len(raw),len(generated))
                self.assertTrue(generated.startswith(b'opaque-prefixVC2!'))
                self.assertTrue(generated.endswith(b'\0opaque-suffix'))
                expected=raw.replace(b'enabled="0"',b'enabled="1"').replace(
                    b'binding73="0"',b'binding73="%d"'%(kind|(channel<<9))).replace(
                    b'id="inputMonitor" value="1.0"',b'id="inputMonitor" value="0"').replace(
                    b'id="syncMode" value="2.0"',b'id="syncMode" value="0"')
                if kind==3 and label!='pc-repeat':expected=expected.replace(b'binding0="145"',b'binding0="%d"'%(3|(127<<2)|(channel<<9)))
                expected=expected.replace(b'</AionState>',b' '*(len(raw)-len(expected))+b'</AionState>')
                self.assertEqual(generated,expected)
            # A repeated poll must not rewrite a published response.
            (run/'midi-source.b64').write_text('invalid')
            runner.midi_fixtures(run)

    def test_reject_wrong_truncated_ambiguous_or_assigned_state(self):
        raw=self.state()
        for bad in [b'wrong',raw[:-20],raw+raw,raw.replace(b'version="3"',b'version="4"'),
                    raw.replace(b'binding73="0"',b'binding73="1"'),
                    raw.replace(b'enabled="0"',b'enabled="1"'),
                    raw.replace(b'id="masterGain" value="1.0"',b'id="masterGain" value="0.0"')]:
            with self.subTest(bad=bad[:20]),tempfile.TemporaryDirectory() as tmp:
                run=Path(tmp);(run/'midi-source.b64').write_text(base64.b64encode(bad).decode())
                with self.assertRaises((AssertionError,runner.ET.ParseError)):
                    runner.midi_fixtures(run)
                self.assertFalse((run/'midi-1.b64').exists())

    def wav(self,run,mode):
        raw=self.state().replace(b'binding0="145"',b'binding0="8703"').replace(b'enabled="0"',b'enabled="1"').replace(b'binding73="0"',b'binding73="8195"')
        if mode=='wrong_mapping':raw=raw.replace(b'binding73="8195"',b'binding73="1"')
        pos=raw.index(b'VC2!');size=raw.index(b'\0',pos+8)-(pos+8)
        raw=raw[:pos+4]+struct.pack('<I',size)+raw[pos+8:]
        (run/'midi-restored.b64').write_text(base64.b64encode(raw).decode())
        for kind in [label for label,_,_ in runner.midi_specs()]+['project_reload']:
            data=[]
            for i in range(48000):
                phase=i+(17 if mode=='phase_shift' and i>=33600 else 0)
                value=3000 if mode=='ambiguous_period' else 3000+round(1000*math.sin(phase*2*math.pi*31/7200)+500*math.cos(phase*2*math.pi*47/7200))
                if mode=='silent' or (mode!='monitor_leak' and (i<9600 or 20160<=i<33600)):value=0
                data.extend((value,value//2 if mode=='wrong_pan' else value))
            with wave.open(str(run/('midi-%s.wav'%kind)),'wb') as f:
                f.setparams((2,2,48000,0,'NONE','not compressed'))
                f.writeframes(struct.pack('<'+'h'*len(data),*data))

    def test_audio_positive_and_negative_controls(self):
        for mode in ['correct','silent','monitor_leak','phase_shift','wrong_pan','wrong_mapping','ambiguous_period']:
            with self.subTest(mode=mode),tempfile.TemporaryDirectory() as tmp:
                run=Path(tmp);self.wav(run,mode)
                if mode=='correct':
                    self.assertEqual(len(runner.verify_midi(run)),20)
                else:
                    with self.assertRaises(AssertionError):runner.verify_midi(run)

if __name__=='__main__':unittest.main()
