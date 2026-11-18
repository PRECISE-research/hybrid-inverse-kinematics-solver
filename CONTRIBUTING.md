# Contributing

Bug reports, questions and pull requests are welcome.

1. Create a branch from `main` and open a pull request.
2. Run the tests before pushing (`ctest --test-dir build -C Release`). CI runs
   them again on every push.
3. If a change moves the solver's output on purpose, regenerate the references
   (see [Tests](README.md#tests)) and say in the pull request why the numbers
   changed.
4. Add a line to [CHANGELOG.md](CHANGELOG.md) for anything a user would notice.
5. Do not commit local absolute paths or participant data beyond the short,
   deidentified excerpts in `examples/`. Record the source and licence of any
   new third-party file or model in [NOTICE](NOTICE).
