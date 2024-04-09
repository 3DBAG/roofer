# Customized logger for roofer

Based on *fmt* and *spdlog*.

## Things we want to log from roofer lib/app?

- Context
  - all config values
  - OS/system?
- Progress (e.g. "read input ...", "done with ...")
- In case of an error, log enough info so that we can reproduce the error
  - input polygon?
  - input pointcloud?
  - context

## Requirements:

- log from multiple threads
- log into:
  - console
  - file
- logger takes care of converting/formatting what we pass to it, so that we can use the same logging methods to log anything without preprocessing the content
  - log the WKT of the geometry that is passed to the logger (except point cloud)
- set the log level from the configuration

## Notes

Does roofer maintain a global context for the logger to live in?

I use the roofer reconstruct API in my own code, where does the logger log?
Can I pass in my own logger?
How does the reconstruct API function get the logger?
