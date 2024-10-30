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

Multi-fasta whole genome alignment derived from mapping to a reference and variant calling such as from [snippy](https://github.com/tseemann/snippy), `snippy-core` and `snippy-clean_full_aln`:

```shell
snippy-core --ref ref.fa snippyoutfiles 
snippy-clean_full_aln core.full.aln > clean.full.aln
```

Use `utils/clean-alignment.py` if the alignment contains sites other than {ATCGN-}

