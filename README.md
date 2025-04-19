# Astra Chess Engine

Astra is a UCI-compliant chess engine written in C++. "Per aspera ad Astra."

## Compiling
Compile the code yourself for better performance.
```
git clone https://github.com/h1me01/Astra.git
cd Astra/src
make pgo
```

If PEXT is supported on your machine:
```
make pgo PEXT=1
```

## NNUE
- Version starting from 5.0 use a self-implemented Neural Network Trainer
- Versions below 5.0 used [CudAD](https://github.com/Luecx/CudAD)
- Versions below 4.0 used [Pytorch-Neural-Network](https://github.com/h1me01/Pytorch-Neural-Network)

## Credits
- [Stockfish](https://github.com/official-stockfish/Stockfish)
- [Koivisto](https://github.com/Luecx/Koivisto)
- [Fathom](https://github.com/jdart1/Fathom) 
- [Chess Programming Wiki](https://www.chessprogramming.org/Main_Page)
