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
             ' '.join('binding%d="0"'%i for i in range(93))+'/></AionState>').encode()
        return b'opaque-prefix'+b'VC2!'+struct.pack('<I',len(xml))+xml+b'\0opaque-suffix'

    def test_equal_length_fixture_and_opaque_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            run=Path(tmp);raw=self.state()
            (run/'midi-source.b64').write_text(base64.b64encode(raw).decode())
            runner.midi_fixtures(run)
            for kind in (1,2,3):
                generated=base64.b64decode((run/('midi-%d.b64'%kind)).read_text())
                self.assertEqual(len(raw),len(generated))
                self.assertTrue(generated.startswith(b'opaque-prefixVC2!'))
                self.assertTrue(generated.endswith(b'\0opaque-suffix'))
                expected=raw.replace(b'enabled="0"',b'enabled="1"').replace(
                    b'binding73="0"',b'binding73="%d"'%kind).replace(
                    b'id="inputMonitor" value="1.0"',b'id="inputMonitor" value="0.0"').replace(
                    b'id="syncMode" value="2.0"',b'id="syncMode" value="0.0"')
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
        raw=self.state().replace(b'enabled="0"',b'enabled="1"').replace(b'binding73="0"',b'binding73="3"')
        if mode=='wrong_mapping':raw=raw.replace(b'binding73="3"',b'binding73="1"')
        (run/'midi-restored.b64').write_text(base64.b64encode(raw).decode())
        for kind in (1,2,3,'project_reload'):
            data=[]
            for i in range(48000):
                phase=i+(17 if mode=='phase_shift' and i>=33600 else 0)
                value=3000+round(1000*math.sin(phase*2*math.pi/7200))
                if mode=='silent' or (mode!='monitor_leak' and (i<9600 or 20160<=i<33600)):value=0
                data.extend((value,value//2 if mode=='wrong_pan' else value))
            with wave.open(str(run/('midi-%s.wav'%kind)),'wb') as f:
                f.setparams((2,2,48000,0,'NONE','not compressed'))
                f.writeframes(struct.pack('<'+'h'*len(data),*data))

    def test_audio_positive_and_negative_controls(self):
        for mode in ['correct','silent','monitor_leak','phase_shift','wrong_pan','wrong_mapping']:
            with self.subTest(mode=mode),tempfile.TemporaryDirectory() as tmp:
                run=Path(tmp);self.wav(run,mode)
                if mode=='correct':
                    self.assertEqual(len(runner.verify_midi(run)),4)
                else:
                    with self.assertRaises(AssertionError):runner.verify_midi(run)

if __name__=='__main__':unittest.main()
