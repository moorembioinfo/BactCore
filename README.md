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


## Benchmarks
Maximum memory usage is approximately that of a single sequence, independent of the number of sequences in the alignment. As such with testing performed on _Salmonella enterica,_ _Mycobacterium tuberculosis_, _Streptococcus pyogenes_ and _Escherichia coli_ **maximum memory usage was never greater than 0.25Gb**. 

**Runtime for 5,000, 10,000, 20,000 and 40,000 genomes was 6.4, 14, 26 and 52 minutes**

## Input

Multi-fasta whole genome alignment derived from mapping to a reference and variant calling such as from [snippy](https://github.com/tseemann/snippy), `snippy-core` and `snippy-clean_full_aln`:

```shell
snippy-core --ref ref.fa snippyoutfiles 
snippy-clean_full_aln core.full.aln > clean.full.aln
```

Use `utils/clean-alignment.py` if the alignment contains sites other than {ATCGN-}


## Rationale
Strict core extraction is extremely sensitive to a minority of positions with gaps. Salmonella Typhimurium genomes (n= 5,379). The strict approach reduced the original 4,685,848bp alignment by 3,089,082 sites. After strict site exclusion 34.08% of sites remained (1,596,766). Conversely, with a relaxed site exclusion, 99.22% were retained (4,555,408). To avoid minority gaps biasing against strict extraction, below shows the rarefaction sampling from between 300 to 3100 randomly sampled genomes with a step of 100 and sampling repeated 50 times per sample size. 


![Project Logo](https://github.com/moorembioinfo/BactCore/blob/main/image/rarefaction.png)


