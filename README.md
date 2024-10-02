# Astra Chess Engine

Astra is a UCI chess engine written in C++.

## Strength
- Unknown

## Board Representation
- Bitboards
- Magic Bitboards

## Search
- Principal Variation Search (PVS)
- Quiescence Search
- Iterative Deepening
- Transposition Table
- Aspiration Windows
- Internal Iterative Reductions (IIR)
- Delta Pruning
- Reverse Futility Pruning
- Mate Pruning
- Null Move Pruning
- Razoring
- Static Exchange Evaluation (SEE)
- Late Move Reductions (LMR)
- History Heuristic
- Killer Heuristic
- Counter Heuristic
- Singular Extensions
- One Reply Extension

## Evaluation
- NNUE (Efficiently Updatable Neural Network)
- Currently trained on ~38 mil. positions
- [Pytorch Neural Network](https://github.com/h1me01/Pytorch-Neural-Network)

## Build
Build the code yourself for better performance.
```
git clone https://github.com/h1me01/Astra.git
cd Astra/src
make -j
```

## Credits
- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Koivisto](https://github.com/Luecx/Koivisto)
- [Smallbrain](https://github.com/Disservin/Smallbrain)
- [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page)
