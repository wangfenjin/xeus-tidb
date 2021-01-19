# This repo is archived as I had contribute the logic back to [xeus-sql](https://github.com/jupyter-xeus/xeus-sql/issues/17)

----

# xeus-tidb

A Jupyter kernel for TiDB

- Author(s):     [wangfenjin](https://github.com/wangfenjin) (Wang Fenjin)
- Last updated:  2021-01-15

## Abstract

Jupyter with Python kernel is widely used in scientific computing community, But another important tool for data science is the SQL family of programming languages.

This new kernel allows the user to use the complete TiDB syntax as well as some extra operations such as opening or closing a database connection, or visualizing the data in different ways using Jupyter magics.

## Usage

Launch the Jupyter notebook with `jupyter notebook` or Jupyter lab with `jupyter lab` and launch a new SQL notebook by selecting the **xtidb** kernel.

## Installation

`xeus-tidb` has been packaged for the conda package manager.

To ensure that the installation works, it is preferable to install `xeus-tidb` in a fresh conda environment.

To ensure that the installation works, it is preferable to install `xeus` in a fresh conda environment. It is also needed to use
a [miniconda](https://conda.io/miniconda.html) installation because with the full [anaconda](https://www.anaconda.com/)
you may have a conflict.

The safest usage is to create an environment named `xeus-tidb` with your miniconda installation

```
conda create -n xeus-tidb
conda activate xeus-tidb
```

### Installing from source

To install the xeus-tidb dependencies:

```bash
conda install nlohmann_json xtl cppzmq xeus xvega xproperty jupyterlab soci-mysql mysqlclient compilers cmake -c conda-forge
```

Then you can compile the sources

```bash
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=${CONDA_PREFIX} -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
make -j 12 install
```

## Credits

This project is inspired by [xeus-sqlite](https://github.com/jupyter-xeus/xeus-sqlite) and [xeus-sql](https://github.com/jupyter-xeus/xeus-sql), and reuse some code/doc from the project! Thanks!

## License

We use a shared copyright model that enables all contributors to maintain the
copyright on their contributions.

This software is licensed under the BSD-3-Clause license. See the [LICENSE](LICENSE) file for details.
