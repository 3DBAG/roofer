# Test data description

## wateringen

A medium area, with four point cloud files.
The data is about 200Mb.


## wateringen-large

A large area, with four point cloud files.
The region of interest [ 78421.161 447315.091 79983.606 449303.658 ] overlaps all four files.
The data is about 1.5GB.
Includes greenhouses in the vicinity of Den Haag.

## wippolder

A small, representative area in Delft, Netherlands. Mostly used for basic tests.

## pc-fusion

The Kadaster point cloud is from 2020.

**yoc-2019-completely-new**

Polygon ((95884.55 462674.86, 95884.55 462717.79, 95910.04 462717.79, 95910.04 462674.86, 95884.55 462674.86))

- NL.IMBAG.Pand.0547100000326960 and NL.IMBAG.Pand.0547100000326963 are completely new buildings built in 2019.
- NL.IMBAG.Pand.0547100000326967 and NL.IMBAG.Pand.0547100000326966 are two small garden sheds built in 2019.
- NL.IMBAG.Pand.0547100000290141 is a long, narrow building, sort of part of another building which is likely to cause incorrect results due to the small footprint area and relative thresholds.

**yoc-2010-ahn**

Polygon ((93277.08 466722.33, 93277.08 466773.35, 93330.13 466773.35, 93330.13 466722.33, 93277.08 466722.33))

- All three should be AHN, but NL.IMBAG.Pand.0579100000011630 got Kadaster, NL.IMBAG.Pand.0579100000011631 got None, because insufficient coverage in all point clouds.


**yoc-2022-low-coverage**

Polygon ((93219.28 466645.92, 93219.28 466696.20, 93268.16 466696.20, 93268.16 466645.92, 93219.28 466645.92))

- NL.IMBAG.Pand.0579100000016039 is under construction in 2022.
- NL.IMBAG.Pand.0579100000001517 is questionable. It was marked for insufficient coverage for AHN4, so AHN3 was selected, but the AHN4 data seem to have good-enough distribution of points, but less density because of many small gaps.


**sheds-garage**

Polygon ((92041.58 466818.54, 92041.58 466862.51, 92066.90 466862.51, 92066.90 466818.54, 92041.58 466818.54))

- Many small garden sheds that were incorrectly classified for insufficient coverage for all point clouds.
- NL.IMBAG.Pand.0579100000016054 is a garage built in 2021, but I'm not sure if there is actually a new construction.

**all-ahn**

Polygon ((93853.70 466158.04, 93853.70 466226.57, 93920.38 466226.57, 93920.38 466158.04, 93853.70 466158.04))

- AHN should be selected for all, but these got Kadaster:
	- NL.IMBAG.Pand.0579100000005171, NL.IMBAG.Pand.0579100000005282 (I think, because both of them have a small part of their footprint, sharing a wall, that is not covered with points at all in AHN4). Same situation for NL.IMBAG.Pand.0579100000005170, but this got AHN3.
	- NL.IMBAG.Pand.0579100000005289, NL.IMBAG.Pand.0579100000005406 (usual shed-situation).


**bag-underground**

Polygon ((93811.17 466162.01, 93811.17 466305.48, 93877.68 466305.48, 93877.68 466162.01, 93811.17 466162.01))

- Building block with separate "towers", but underground BAG object covering them all.
