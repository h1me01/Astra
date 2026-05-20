# Astra Chess Engine

Astra is a UCI-compliant chess engine written in C++.

## Compiling
```
git clone https://github.com/h1me01/Astra.git
cd Astra/src
make -j
```

## NNUE
Astra versions 6.1.1 and below were trained with lc0-generated games.

Astra began training its neural network on self-play games, starting from randomly initialized weights. In total, 21 networks were trained, and the full process can be seen in the selfgen branch. The last network to beat master was renamed to weights-v3.nnue.

The games were converted into Stockfish's binpack format and trained with [Sorei](https://github.com/h1me01/Sorei) using the [astra-selfplay dataset](https://huggingface.co/datasets/h1me01/astra-selfplay) with roughly 12 billion positions.

## Credits
- Engine Inspiration: [Stockfish](https://github.com/official-stockfish/Stockfish), [Koivisto](https://github.com/Luecx/Koivisto), [Berserk](https://github.com/jhonnold/berserk)
- Testing Framework: [OpenBench](https://github.com/AndyGrant/OpenBench)
- Tablebase Probing: [Fathom](https://github.com/jdart1/Fathom)
- Weights Embedding: [incbin](https://github.com/graphitemaster/incbin)
- Learning Resource: [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page)
