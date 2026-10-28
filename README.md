# BactCore
Extract per-site flexible (≥95%) bacterial core genome alignments from bacterial whole-genome, reference-anchored alignments, such as from read mapping. 

Output includes invariant sites in the output for detection of recombinant sites with [ClonalFrameML](https://github.com/xavierdidelot/ClonalFrameML)




## Installation
Compile with:
```shell
git clone https://github.com/moorembioinfo/BactCore.git
cd BactCore/
make
```
## Usage

Run `BactCore`

```shell
BactCore <input.fasta> <output>
```


And for strict cores (sites with no gaps):
```shell
BactCore --strict <input.fasta> <output.fasta>
```
Or SNP-sites only from the strict core sites:
```shell
BactCore --strict --snps <input.fasta> <output.fasta>
```

## Input

Multi-fasta whole genome alignment derived from mapping to a reference and variant calling  

---




## Benchmarks

BactCore was tested on synthetic six-megabase alignments containing up to
10,000 sequences (60 billion characters).

Benchmarks used one logical CPU on an AMD Ryzen 7 7730U. The alignments
contained defined SNPs plus gap-only, `N`-only, and mixed-invalid columns above,
below, and exactly at the 95% core threshold.

#### Flexible 95% core

These measurements include both input passes and complete FASTA serialization
to `/dev/null`. They exclude the time required to persist the nearly
input-sized output to storage (lack of storage space on my laptop)

| Sequences | Alignment length | Polymorphic sites | Input size | Time | Peak RSS |
|---:|---:|---:|---:|---:|---:|
| 2,000 | 6,000,000 bp | 30,000 | 12 GB | 17.4 s | 21.4 MiB |
| 5,000 | 6,000,000 bp | 30,000 | 30 GB | 57.0 s | 21.4 MiB |
| 10,000 | 6,000,000 bp | 30,000 | 60 GB | 116.6 s | 21.4 MiB |
| 5,000 | 6,000,000 bp | 50,000 | 30 GB | 50.5 s | 21.4 MiB |
| 5,000 | 6,000,000 bp | 100,000 | 30 GB | 56.8 s | 21.4 MiB |

All runs retained the expected 5,991,000 columns. Columns with exactly 95%
valid calls were retained, while columns immediately below the threshold were
removed.

#### Strict SNP alignment

These measurements include writing the SNP alignment to a real file.

| Sequences | Input length | Polymorphic sites | Time | Peak RSS |
|---:|---:|---:|---:|---:|
| 2,000 | 6,000,000 bp | 30,000 | 18.1 s | 15.2 MiB |
| 5,000 | 6,000,000 bp | 30,000 | 57.5 s | 15.2 MiB |
| 10,000 | 6,000,000 bp | 30,000 | 115.5 s | 15.2 MiB |
| 5,000 | 6,000,000 bp | 50,000 | 51.4 s | 15.2 MiB |
| 5,000 | 6,000,000 bp | 100,000 | 58.0 s | 15.2 MiB |

Every output contained exactly the designed number of SNP columns. No
benchmark process swapped.

BactCore's memory consumption depends primarily on **alignment width** rather than sequence count. Increasing the number of sequences primarily increases sequential I/O time: even the 10,000 × 6,000,000 alignment required only about 21 MiB for flexible filtering and 15 MiB for strict SNP extraction.

