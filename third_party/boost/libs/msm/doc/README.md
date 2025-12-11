## Building the documentation locally

Run the following commands once for the initial setup:

- Make sure nodejs (>= 16.0.0) & npm are installed: `sudo apt install nodejs npm`
- Make sure you are in the doc folder: `cd doc`
- Install the required packages: `npm install`
- Set up a pseudo-repository: `git init && git commit --allow-empty -m init`

Antora requires the doc sources to be located within a git repository, but it cannot recognize git submodules. By setting up a pseudo-repository in the doc folder, the local documentation build works when MSM is used as a standalone repo as well as when MSM is opened from a submodule path within the Boost super-project.

After the initial setup is done, the documentation can be built by running `npx antora --fetch local-playbook.yml` from the doc folder.
