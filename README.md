# BactCore
Extract per-site flexible (≥95%) bacterial core genome alignments from bacterial whole-genome, reference-anchored alignments, such as from read mapping. 

Output includes invariant sites in the output for detection of recombinant sites with [ClonalFrameML](https://github.com/xavierdidelot/ClonalFrameML)




## Installation
Compile with:
```shell
git clone https://github.com/moorembioinfo/BactCore.git
make
```
## Usage

Run `BactCore`, optionally specifying number of threads for extra speed

```shell
BactCore -t 10 input.fasta > output.fasta
```

## Dependencies
You may need to install openMP first. There are a number of ways of doing this, I reccomend conda:

```shell
conda install -c conda-forge openmp
```


## Input

Multi-fasta whole genome alignment derived from mapping to a reference and variant calling such as from [snippy](https://github.com/tseemann/snippy), `snippy-core` and `snippy-clean_full_aln`:

```shell
snippy-core --ref ref.fa snippyoutfiles 
snippy-clean_full_aln core.full.aln > clean.full.aln
```

Use `tools/clean_alignment.fasta` if the alignment contains sites other than {ATCGN-}
