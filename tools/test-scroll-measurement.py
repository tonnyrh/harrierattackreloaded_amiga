"""Fault injection for the content detector. Never modifies game assets."""
import importlib.util
from pathlib import Path
import random
import tempfile
import unittest
from PIL import Image

spec=importlib.util.spec_from_file_location('scroll',Path(__file__).with_name('check-scroll-frames.py'))
scroll=importlib.util.module_from_spec(spec)
spec.loader.exec_module(scroll)


class ScrollMeasurementTests(unittest.TestCase):
    def measure(self, positions):
        randomizer=random.Random(145)
        world=Image.frombytes('RGB',(600,30),randomizer.randbytes(600*30*3))
        with tempfile.TemporaryDirectory() as directory:
            files=[]
            for i,x in enumerate(positions):
                path=Path(directory)/f'{i:04}.png'
                world.crop((x,0,x+200,30)).save(path)
                files.append(path)
            return scroll.analyze(files,(20,0,160,30),6,24)

    def test_clean_sequence(self):
        summary,events=self.measure([0,6,12,18,24])
        self.assertEqual(summary['result'],'pass')

    def test_duplicate_and_catchup_at_exact_frames(self):
        summary,events=self.measure([0,6,6,18,24])
        self.assertEqual(summary['faults'],2)
        self.assertEqual([(e['transition'],e['kind'],e['step']) for e in events if e['kind']!='ok'],
                         [(2,'repeated_image',0),(3,'wrong_step',12)])

    def test_backward_and_large_step(self):
        summary,events=self.measure([0,6,3,21,27])
        self.assertEqual(events[1]['kind'],'reverse_step')
        self.assertEqual(events[2]['kind'],'wrong_step')

    def test_uniform_background_is_not_a_pass(self):
        blank=Image.new('RGB',(200,30),'blue')
        result=scroll.measure_pair(blank,blank,(20,0,160,30),24)
        self.assertEqual(result['status'],'ambiguous_texture')

    def test_corruption_is_inconclusive_not_a_fake_step(self):
        r=random.Random(98)
        a=Image.frombytes('RGB',(200,30),r.randbytes(18000))
        b=Image.frombytes('RGB',(200,30),r.randbytes(18000))
        self.assertEqual(scroll.measure_pair(a,b,(20,0,160,30),24)['status'],
                         'unreliable_content_change')

    def test_invalid_crop_rejected(self):
        a=Image.new('RGB',(100,30))
        with self.assertRaises(ValueError):
            scroll.measure_pair(a,a,(50,0,100,30),24)

    def test_missing_capture_is_not_reported_as_game_jump(self):
        with tempfile.TemporaryDirectory() as directory:
            files=[Path(directory)/'0001.png',Path(directory)/'0003.png']
            for path in files:
                Image.new('RGB',(200,30),'blue').save(path)
            summary,events=scroll.analyze(files,(20,0,160,30),6,24)
            self.assertEqual(events[0]['kind'],'capture_sequence_gap')
            self.assertEqual(summary['result'],'inconclusive')


if __name__=='__main__':
    unittest.main()
