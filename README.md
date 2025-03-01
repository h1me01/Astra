# Astra Chess Engine

Astra is a UCI-compliant chess engine written in C++. "Per aspera ad Astra."

## Compiling
Compile the code yourself for better performance.
```
git clone https://github.com/h1me01/Astra.git
cd Astra/src
make pgo
```

## NNUE
Starting from version 5.0, Astra features a self-implemented Neural Network for position evaluation.
- Versions below 4.0 used [Pytorch-Neural-Network](https://github.com/h1me01/Pytorch-Neural-Network)
- Versions below 5.0 used [CudAD](https://github.com/Luecx/CudAD)

## Credits
- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Koivisto](https://github.com/Luecx/Koivisto)
- [Fathom](https://github.com/jdart1/Fathom) 
- [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page)
