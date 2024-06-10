# Grafite

<p align="center">Grafite is a data structure that enables fast range emptiness
queries using optimal space and time. It provides strong guarantees on the expected false positive rate under any kind
of workloads.</p>

<p align="center">
    <a href="https://dl.acm.org/doi/abs/10.1145/3639258">Paper</a>
    | <a href="http://acube.di.unipi.it">A³ Lab</a>
</p>

<div align="center">
<a href="https://github.com/marcocosta97/grafite/actions/workflows/build.yml"><img src="https://github.com/marcocosta97/grafite/actions/workflows/build.yml/badge.svg"></a>
</div>
## Quickstart

This is a header-only library. It does not need to be installed. Just clone the repo with

```bash
git clone https://github.com/marcocosta97/grafite.git
cd grafite
git submodule update --init lib/sux
```

and copy the `include/grafite` and `lib/sux` directories to your system's or project's include path.

The `examples/simple.cpp` file shows how to index and query a vector of random integers with Grafite:

```cpp
#include <iostream>
#include <vector>
#include <algorithm>
#include "grafite/grafite.hpp"

int main()
{
    std::vector<int> keys (1000000);
    std::generate(keys.begin(), keys.end(), std::rand);
    keys.push_back(42);

    // Construct the Grafite range filter
    auto bpk = 12.0;
    auto g = grafite::filter(keys.begin(), keys.end(), bpk);

    // Query the Grafite range filter
    auto left = 40, right = 44;
    auto result = g.query(left, right);
    std::cout << "Query result: " << result << std::endl;

    return 0;
}
```

[Run and edit this and other examples on Repl.it](https://repl.it/github/marcocosta97/grafite). Or run it locally via:

```bash
g++ examples/simple.cpp -std=c++17 -I./include -I./lib/sux -DSUCCINCT_LIB_SUX -o simple
./simple
```

## Classes overview

Other than the `grafite::filter` class in the example above, this library provides the following classes:

- `grafite::bucket` an heuristic range filter which provides very fast lookups in small space without any guarantee on the false positive rate.
- `grafite::ef_sux_vector` a wrapper for the Elias-Fano implementation of the [sux](https://sux.di.unimi.it) library. _This implementation is used as default for Grafite_.
- `grafite::ef_sdsl_vector` a wrapper for the Elias-Fano implementation of the [sdsl](https://github.com/simongog/sdsl-lite) library.

## Compile the tests and the benchmarks

After cloning the repository and all its submodules with
```bash
git clone --recurse-submodules -j8 https://github.com/marcocosta97/grafite.git
cd grafite
```

build the project with CMAKE
```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j8
```

The benchmarks will be placed in `build/bench/`, see [reproducibility.md](bench/reproducibility.md) for details on how to reproduce 
the tests in the paper.

## License

This project is licensed under the GPLv3 License - see the [LICENSE](LICENSE) file for details.

If you use the library please cite the following paper:

> Costa, Marco, Paolo Ferragina, and Giorgio Vinciguerra. "Grafite: Taming Adversarial Queries with Optimal Range Filters." Proceedings of the ACM on Management of Data 2.1 (2024): 1-23.

```tex
@article{costa2024grafite,
  title={Grafite: Taming Adversarial Queries with Optimal Range Filters},
  author={Costa, Marco and Ferragina, Paolo and Vinciguerra, Giorgio},
  journal={Proceedings of the ACM on Management of Data},
  volume={2},
  number={1},
  pages={1--23},
  year={2024},
  publisher={ACM New York, NY, USA}
}
```
