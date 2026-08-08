# Building the documentation locally

Run the following commands once for the initial setup:

- Make sure nodejs (LTS version >= 16.0.0) & npm are installed: `sudo apt install nodejs npm`
- Make sure you are in the doc folder: `cd doc`
- Run the Make target for the setup: `make setup`

Antora requires the doc sources to be located within a git repository, but it cannot recognize git submodules.
You can set up a pseudo-repository with `git init && git commit --allow-empty -m init` to make the local documentation build work when MSM is opened from a submodule path within the Boost super-project.

After the initial setup is done, build the the documentation with `make build`.

If you are not interested in viewing the generated API reference, you can speed up the documenation build with the ENV `ANTORA_SKIP_CPP_REFERENCE=1`.

The Antora Cpp reference extension will clone the complete Boost repo to a cache folder. If you want to use an existing folder instead, set up an ENV `BOOST_SRC_DIR` to point to it.
