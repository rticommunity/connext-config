# Testing connext-config

This directory contains the scripts and material required for testing `connext-config`.

The following are the tests available:

* Dump of all settings



### Dump of all settings

The script `dump-all.sh` takes as the only input argument the `connext-config` to use for the test. For example:

```sh
./dump-all.sh ../src/connext-config
```



This script does the following operation:

* Runs `connext-config --list-all` to print all the architectures

* Runs `connext-config --dump-all` to print all the architectures and parameters
* Runs the tool multiple times with all the arguments and modifiers for each architectures

The output can be redirected to a file and compared with a particular baseline.

Current baselines:

| File                   | Description                                                  |
| ---------------------- | ------------------------------------------------------------ |
| `reference-601.txt.gz` | Compressed output using Connext 6.0.1 and `connext-config` version 1.0 |

