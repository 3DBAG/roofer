"""Exercise the completion writer over a pipe and the CLI on a tiled dataset."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def check_live_delivery(fixture):
    with subprocess.Popen(
        [fixture], stdin=subprocess.PIPE, stdout=subprocess.PIPE, encoding="utf-8"
    ) as process:
        try:
            assert json.loads(process.stdout.readline())["type"] == "CityJSON"
            ready = json.loads(process.stdout.readline())
            assert ready["id"] == "2"
            attrs = ready["CityObjects"]["2"]["attributes"]
            assert attrs["source"] == "second"
            assert attrs["rf_success"] is False
            assert attrs["rf_extrusion_mode"] == "fail"
            assert len(ready["CityObjects"]["2"]["geometry"]) == 1
            # Only now may the other building in the same tile finish.
            output, _ = process.communicate("\n", timeout=10)
            last = json.loads(output)
            assert last["id"] == "1"
            attrs = last["CityObjects"]["1"]["attributes"]
            assert attrs["source"] == "first"
            assert attrs["rf_success"] and attrs["rf_t_run"] == 7
            assert process.returncode == 0
        finally:
            process.kill()


def records(text):
    if text:
        assert text.endswith("\n")
    return [json.loads(line) for line in text.splitlines()]


def check_cli(roofer, data, work):
    points = data / "ahn5/points.laz"
    footprints = data / "bag/panden.geojson"
    config = work / "stream.toml"
    # A legacy key exercises warnings emitted before the destination is resolved.
    config.write_text(f'''split-cjseq = false
[input]
polygon-source = {json.dumps(str(footprints))}
id-attribute = "identificatie"
[[input.pointclouds]]
name = "AHN5"
source = [{json.dumps(str(points))}]
force_lod11 = true
[output]
output-directory = "-"
tiling = true
tilesize = [30, 30]
terrain = true
''')

    def run(*args, success=True):
        result = subprocess.run([roofer, "-j", "4", "--loglevel", "debug", *args],
                                cwd=work, capture_output=True, encoding="utf-8", timeout=30)
        assert (result.returncode == 0) == success, result.stderr
        return result

    stream = run("-c", config)
    assert "deprecated" in stream.stderr and "[reconstructor]" in stream.stderr
    header, *features = records(stream.stdout)
    assert header["type"] == "CityJSON" and not header["vertices"]
    assert header["CityObjects"] == {}
    assert "geographicalExtent" not in header["metadata"]
    assert "referenceSystem" in header["metadata"]
    assert all(f["type"] == "CityJSONFeature" for f in features)
    by_id = {feature["id"]: feature for feature in features}
    assert len(by_id) == len(features)
    inputs = json.loads(footprints.read_text(encoding="utf-8"))["features"]
    input_ids = {f["properties"]["identificatie"] for f in inputs}
    assert {key for key in by_id if not key.startswith("terrain-")} == input_ids

    directory = work / "tiles"
    run("-c", config, directory)
    files = list(directory.glob("*.city.jsonl"))
    assert len(files) > 1
    expected_ids = set()
    transforms = set()
    for path in files:
        tile_header, *tile_features = records(path.read_text(encoding="utf-8"))
        transforms.add(tuple(tile_header["transform"]["translate"]))
        terrain = [f for f in tile_features if f["id"].startswith("terrain-")]
        assert len(terrain) == 1
        for feature in tile_features:
            feature_id = feature["id"]
            assert feature_id not in expected_ids
            expected_ids.add(feature_id)
            streamed = by_id[feature_id]
            for obj in (feature, streamed):
                obj["CityObjects"][feature_id]["attributes"].pop("rf_t_run", None)
            assert feature["CityObjects"] == streamed["CityObjects"]
            assert len(feature["vertices"]) == len(streamed["vertices"])
            for local, shared in zip(feature["vertices"], streamed["vertices"]):
                for axis in range(3):
                    scale = header["transform"]["scale"][axis]
                    a = local[axis] * scale + tile_header["transform"]["translate"][axis]
                    b = shared[axis] * scale + header["transform"]["translate"][axis]
                    # Each transform truncates independently, possibly on opposite sides of zero.
                    assert abs(a - b) <= 2 * scale + 1e-9, (a, b)
    assert len(transforms) > 1 and expected_ids == set(by_id)

    split = work / "split"
    run("-c", config, "--no-tiling", "--no-terrain", "--split-cjseq", split)
    assert json.loads((split / "metadata.json").read_text())["type"] == "CityJSON"
    split_features = [json.loads(path.read_text(encoding="utf-8"))
                      for path in split.rglob("*.city.jsonl")]
    assert len(split_features) == len(input_ids)
    assert {f["id"] for f in split_features} == input_ids

    explicit = run("-c", config, "--cj-translate", "0", "0", "0", "--omit-metadata", "-")
    rows = records(explicit.stdout)
    assert all(row["type"] == "CityJSONFeature" for row in rows)
    assert any(v[1] > 2**31 for row in rows for v in row["vertices"])
    empty = records(run("--filter", "1 = 0", points, footprints, "-").stdout)
    assert len(empty) == 1 and empty[0]["transform"]["translate"] == [0, 0, 0]
    assert not run("-c", config, "--omit-metadata", "--filter", "1 = 0").stdout
    for option in ("--split-cjseq", "--crop-only", "--crop-output", "--crop-output-all",
                   "--crop-rasters", "--index"):
        rejected = run(option, points, footprints, "-", success=False)
        assert not rejected.stdout and "require an output directory" in rejected.stderr
    assert not (work / "-").exists()
    overflow = run("-c", config, "--cj-scale", "1e-20", "1e-20", "1e-20",
                   "--cj-translate", "0", "0", "0", success=False)
    assert "64-bit" in overflow.stderr and len(records(overflow.stdout)) == 1

    # Close the read end before the header to avoid a race with a fast producer.
    # An inherited, ignored SIGPIPE also exercises the write-error path on Unix.
    reader, writer = os.pipe()
    os.close(reader)
    with os.fdopen(writer, "wb") as broken_pipe:
        result = subprocess.run([roofer, "-j", "2", "-c", config, "--trace-interval", "120", "-"],
                                cwd=work, stdout=broken_pipe, stderr=subprocess.PIPE,
                                timeout=10, restore_signals=False)
    assert result.returncode != 0 and b"Failed to stream CityJSON" in result.stderr


if __name__ == "__main__":
    executable, fixture, input_dir = map(Path, sys.argv[1:])
    check_live_delivery(fixture)
    with tempfile.TemporaryDirectory() as tmp:
        check_cli(executable, input_dir, Path(tmp))
