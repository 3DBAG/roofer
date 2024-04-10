# Customized logger for roofer

## Things we want to log from roofer lib/app?

- Context
  - all config values
  - OS/system?
- Progress (e.g. "read input ...", "done with ...")
- In case an exception is thrown, log the input and config to a file so that it is possible to fully reproduce the error only from the logs.

## Requirements:

- log from multiple threads
- log into:
  - console
  - file
- logger takes care of converting/formatting what we pass to it, so that we can use the same logging methods to log anything without preprocessing the content
  - e.g. log the WKT of the geometry that is passed to the logger (except point cloud)
- set the log level from the configuration

## High level design

The logger abstracts away the logging backend, which makes it possible to swap the logging library.

*spdlog* might not be the best backend, because *rerun* can log plain messages too.
But if we have swappable backends, we can try both.

The logger maintains a global state and the functions request an instance from the global context.
Threads request a logger from the global logger pool.

The API functions do not log, but they throw exceptions.
The rationale is that the library user, who integrate the "reconstruct" function into their code, is not interested in log messages, but only wants to run the function and get the output.
If the function breaks, then the exception should inform the caller what exactly went wrong.
Then the caller decides what to do.
Therefore, the exceptions need to be implemented and documented in a way that is straightforward to handle them.
If we don't log from the API functions, then we also don't need to bother about managing a roofer logger instance.

Ideally, the library user shouldn't be required to install and use spdlog/rerun, but there would be some simple logger implementation that logs to the console.

If the reconstruction crashes, we want to be able to reproduce the crash just from the logs.
Even if roofer was run in release mode, with high (eg error) log level setting.
This means that we wouldn't need the original, full input files to reproduce the issue, but the log contains the input data of the object that caused the crash.
Then, we could rerun the reconstruction of only the problematic object even from an intermediate state.
There is a Rust library, [bincode](https://crates.io/crates/bincode), which can de/serialize Rust objects to binary files, using "zero-fluff" encoding.
If there is a C++ equivalent, we could use that to dump the inputs to files.
Then write a converter from the binary to GIS formats.
