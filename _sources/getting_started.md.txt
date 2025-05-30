# Getting started

## Installation

The easiest way to get roofer is to download the latest binary as available via [GitHub](https://github.com/3DBAG/roofer/releases/latest). Alternatively you can use the [Docker image](https://hub.docker.com/r/3dgi/roofer/tags) or you can [built roofer from source](#developers).

## Running roofer
To test if roofer is correctly installed you can try to run it with our small [test dataset](https://data.3dbag.nl/testdata/roofer/wippolder.zip). To download this data you can do:

```bash
curl -LO https://data.3dbag.nl/testdata/roofer/wippolder.zip
unzip wippolder.zip
```

Then, assuming you have installed roofer, you can run with the test dataset as follows:

```bash
roofer wippolder/wippolder.las wippolder/wippolder.gpkg output
```

This should give an output like:

```bash
[2025-05-28 10:53:32.420651]    INFO    Region of interest: 85289.898 447042.018, 85466.683 447163.476
[2025-05-28 10:53:32.420827]    INFO    Number of source footprints: 60
[2025-05-28 10:53:32.421427]    INFO    Using 5 threads for the reconstructor pool, 10 threads in total (system offers 10)
[2025-05-28 10:53:32.421755]    INFO    [serializer] Writing output to output
[2025-05-28 10:53:32.424692]    INFO    Simplifying and buffering footprints...
[2025-05-28 10:53:32.428223]    INFO    Cropping pointcloud ...
[2025-05-28 10:53:32.551549]    INFO    Analysing pointcloud ...
```

And an output file `085289_447042.city.jsonl` should have been produced in the `output` folder. This is a [CityJSONSeq](https://www.cityjson.org/cityjsonseq/) file. You can convert it to a regular CityJSON file using [cjseq](https://github.com/cityjson/cjseq), eg:

```bash
cat output/085289_447042.city.jsonl.city.jsonl | cjseq collect > output/085289_447042.city.json
```

The resulting CityJSON file you can inspect in ninja or another [CityJSON compatible software](https://www.cityjson.org/software/).

To learn more about roofer's various configuration check out the [roofer CLI documentation](#cli_application).
