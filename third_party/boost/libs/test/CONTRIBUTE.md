# How to contribute to Boost.Test

## Ticket
We like having a ticket stating the bug you are experiencing or the feature you want to implement.
We use the [GitHub issues](https://github.com/boostorg/test/issues) for raising bugs and feature requests, 
while older tickets may be found in our former bug tracking system at https://svn.boost.org/ 
(`test` component).

## Pull requests
We welcome any contribution in the form of a pull request. Each PR is never integrated exactly as submitted,
we first run our internal unit tests on several platforms, and work the PR if needed.

To ease the work of the maintainer and make the integration of your changes faster, please

- base all your PR on the latest develop, rebase if develop changed since you forked the library
- ensure that your changes are not creating any regression in the current test bed (see below on how to run
  the unit tests)
- provide a test case that reproduces the problem you encountered
- integrate your unit test into the `Jamfile.v2` of the test folder

# Running the unit tests
Please make sure that the current set of tests pass for the changes that you submit. 
To run the tests, see [this document](test/README.md).

# Compile the documentation
The instructions for compiling the documentation are provided in [this document](doc/README.md). 
