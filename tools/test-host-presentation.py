"""Regression checks for measurement classification, requiring no emulator."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("host", Path(__file__).with_name("check-host-presentation.py"))
host = importlib.util.module_from_spec(spec)
spec.loader.exec_module(host)


def row(t, display, chain="a", present=20):
    return {"ProcessID": "1", "SwapChainAddress": chain,
            "CPUStartQPCTime": str(t), "DisplayedTime": str(display),
            "MsBetweenPresents": str(present), "PresentMode": "Composed: Flip",
            "MsCPUBusy": "0", "MsUntilDisplayed": "0" if display != "NA" else "NA",
            "SyncInterval": "0"}


class HostTimingTests(unittest.TestCase):
    def test_regular_submission_can_hide_display_judder(self):
        data = [row(1000+i*20, 33.33 if i % 5 == 4 else 16.67) for i in range(20)]
        summary, events = host.analyze(data)
        self.assertEqual(summary[0]["between_presents"]["max_ms"], 20)
        self.assertEqual(summary[0]["long_display_recurrence"]["median_ms"], 100)
        self.assertEqual(sum("long_display" in e["flags"] for e in events), 4)

    def test_swapchains_and_missing_display_are_not_mixed(self):
        summary, events = host.analyze([row(100, 20), row(10, "NA", "b"), row(120, 20)])
        self.assertEqual(len(summary), 2)
        self.assertIsNone(summary[1]["display_intervals"])
        self.assertEqual(summary[1]["not_displayed_or_zero_or_unavailable"], 1)

    def test_reject_non_qpc_or_reordered_trace(self):
        with self.assertRaises(ValueError):
            host.analyze([{}])
        with self.assertRaises(ValueError):
            host.analyze([row(120,20),row(100,20)])

    def test_presentmon_26_schema(self):
        sample = row(1000, 33.3)
        sample['CPUStartQPCTimeInMs'] = sample.pop('CPUStartQPCTime')
        sample['MsBetweenDisplayChange'] = sample.pop('DisplayedTime')
        summary, events = host.analyze([sample])
        self.assertEqual(summary[0]['display_interval_metric'], 'MsBetweenDisplayChange')
        self.assertEqual(events[0]['qpc_ms'], 1000)
        self.assertEqual(events[0]['displayed_ms'], 33.3)

    def test_display_clock_reconstruction_and_bad_timestamp(self):
        samples = [row(1000,20),row(1020,20),row(1040,20)]
        for sample in samples:
            sample['MsCPUBusy']='18'
            sample['MsUntilDisplayed']='7'
            sample['MsBetweenDisplayChange']='20'
        summary, events = host.analyze(samples)
        self.assertEqual(events[0]['display_qpc_ms'],1025)
        self.assertEqual(summary[0]['display_clock_max_residual_ms'],0)
        samples[2]['MsUntilDisplayed']='47'
        summary, events=host.analyze(samples)
        self.assertIn('display_clock_inconsistent',events[2]['flags'])


if __name__ == '__main__':
    unittest.main()
