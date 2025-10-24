# Astra Chess Engine

Astra is a UCI-compliant chess engine written in C++.

## Compiling
```
git clone https://github.com/h1me01/Astra.git
cd Astra/src
make -j
```

## NNUE
- Versions starting from 5.0 use [Astra-Trainer](https://github.com/h1me01/Astra-Trainer)
- Versions below 5.0 used [CudAD](https://github.com/Luecx/CudAD)
- Versions below 4.0 used a simple Pytorch Neural Network

## Credits
- Engine Inspiration: [Stockfish](https://github.com/official-stockfish/Stockfish), [Koivisto](https://github.com/Luecx/Koivisto), [Berserk](https://github.com/jhonnold/berserk)
- Tablebase Probing: [Fathom](https://github.com/jdart1/Fathom)
- Binary Embedding: [incbin](https://github.com/graphitemaster/incbin)
- Learning Resource: [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page)
