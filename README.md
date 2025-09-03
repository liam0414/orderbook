# Order Book Engine

A C++ implementation of a financial order book with price-time priority matching.

## Features
- Price-time priority matching algorithm
- Support for limit and market orders
- Thread-safe operations
- Comprehensive test suite
- Real-time market data queries

## Building
```bash
cmake -B build
cd build
make
```
## Running
```bash
# Run demo
./orderbook_client

# Run tests
ctest --verbose
```