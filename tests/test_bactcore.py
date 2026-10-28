#!/usr/bin/env python3
"""Black-box correctness and large-row regression tests for BactCore."""

import math
import random
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BINARY = ROOT / "BactCore"
VALID = set("ACGTacgt")


def normalized(sequence: str) -> str:
    return "".join(c.upper() if c in VALID else "-" for c in sequence)


def expected(records, threshold=0.95, snps=False):
    count = len(records)
    length = len(records[0][1])
    required = math.ceil(threshold * count)
    keep = []
    for column in range(length):
        chars = [sequence[column] for _, sequence in records]
        valid = [char.upper() for char in chars if char in VALID]
        retain = len(valid) >= required
        if snps:
            retain = retain and len(set(valid)) > 1
        if retain:
            keep.append(column)
    normalized_records = [
        (header, normalized(sequence)) for header, sequence in records]
    return [
        (header, "".join(sequence[i] for i in keep))
        for header, sequence in normalized_records
    ]


def write_fasta(path: Path, records, wrap=7):
    with path.open("w", newline="") as output:
        for index, (header, sequence) in enumerate(records):
            output.write(f">{header}\r\n" if index & 1 else f">{header}\n")
            width = wrap or max(1, len(sequence))
            for start in range(0, len(sequence), width):
                ending = "\r\n" if (start // width) & 1 else "\n"
                output.write(sequence[start : start + width] + ending)


def read_fasta(path: Path):
    records = []
    header = None
    pieces = []
    with path.open(newline=None) as source:
        for line in source:
            if line.startswith(">"):
                if header is not None:
                    records.append((header, "".join(pieces)))
                header = line[1:].rstrip("\r\n")
                pieces = []
            else:
                pieces.append(line.strip())
    if header is not None:
        records.append((header, "".join(pieces)))
    return records


class BactCoreTests(unittest.TestCase):
    def run_bactcore(self, records, *options, wrap=7):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / "input.fa"
            output = directory / "output.fa"
            write_fasta(source, records, wrap=wrap)
            completed = subprocess.run(
                [BINARY, *options, source, output],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            return read_fasta(output)

    def test_randomized_thresholds_and_modes(self):
        rng = random.Random(20261022)
        alphabet = "ACGTacgtN-RY?"
        records = [
            (f"sample {sample}",
             "".join(rng.choice(alphabet) for _ in range(257)))
            for sample in range(37)
        ]
        for wrap in (0, 7):
            for threshold in (0.0, 0.5, 0.95, 1.0):
                with self.subTest(wrap=wrap, threshold=threshold):
                    actual = self.run_bactcore(
                        records, f"--threshold={threshold}", wrap=wrap)
                    self.assertEqual(actual, expected(records, threshold))

            actual = self.run_bactcore(
                records, "--strict", "--snps", wrap=wrap)
            self.assertEqual(actual, expected(records, 1.0, snps=True))

    def test_output_buffer_boundary(self):
        length = (1 << 22) + 64
        first = "A" * length
        second = first[: (1 << 22) + 5] + "N" + first[(1 << 22) + 6 :]
        records = [("first", first), ("second", second)]
        actual = self.run_bactcore(records, "--strict", wrap=0)
        retained = first[: (1 << 22) + 5] + first[(1 << 22) + 6 :]
        self.assertEqual(actual, [("first", retained), ("second", retained)])

    def test_more_than_65535_rows(self):
        # The original uint16 valid-count implementation wrapped here.
        records = [(f"s{i}", "ACGTACGT") for i in range(65_536)]
        actual = self.run_bactcore(records)
        self.assertEqual(len(actual), len(records))
        self.assertTrue(all(sequence == "ACGTACGT" for _, sequence in actual))

    def test_rejects_bad_threshold_and_unequal_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            source = directory / "input.fa"
            output = directory / "output.fa"
            source.write_text(">a\nACGT\n>b\nACG\n")
            bad_shape = subprocess.run(
                [BINARY, source, output], capture_output=True, text=True)
            self.assertNotEqual(bad_shape.returncode, 0)

            bad_threshold = subprocess.run(
                [BINARY, "--threshold=nan", source, output],
                capture_output=True,
                text=True,
            )
            self.assertEqual(bad_threshold.returncode, 64)


if __name__ == "__main__":
    unittest.main()
