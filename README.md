# BactCore
Extract per-site flexible (≥95%) bacterial core genome alignments from whole-genome alignments, such as from read mapping. 

#### With 6 processors and 5Mbp sequences and 500,000 sites for removal:  
- 2,500 sequences in 115 seconds  
- 3,500 sequences in 198 seconds  
#### With 1 processor and 5Mbp sequences and 500,000 sites for removal:  
- 2,500 sequences in 189 seconds  


Output includes invariant sites in the output for detection of recombinant sites with [ClonalFrameML](https://github.com/xavierdidelot/ClonalFrameML)




## Usage
Compile with:
```shell
g++ -std=c++11 -O3 -fopenmp BactCore.cpp -o BactCore
```

Run by specifying the number of threads, input filename and output filename. For example, with 10 threads:
```shell
BactCore -t 10 input.fasta > output.fasta
```

You may need to install openMP first, there are a number of ways of doing this I reccomend conda:

```shell
conda install -c conda-forge openmp
```


## Input

Multi-fasta whole genome alignment derived from mapping to a reference and variant calling such as from [snippy](https://github.com/tseemann/snippy), `snippy-core` and `snippy-clean_full_aln`:

```shell
snippy-core --ref ref.fa snippyoutfiles 
snippy-clean_full_aln core.full.aln > clean.full.aln
```
