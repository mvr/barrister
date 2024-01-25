Barrister
=========

Usage
-----
To build, run `git submodule update --init` to fetch the TOML library,
and then run `make`.

```
./Barrister inputs/test.toml
```

Input Parameters
----------------

See `inputs/snark.toml` etc.

The `pattern` parameter must be a LifeHistory RLE, where the states
have the following meaning:
* state 1 (green): active pattern
* state 2 (blue): unknown cells where the catalyst to-be-found may lie
* state 3 (pale green): cells that must be ON cells of the catalyst, initially ON
* state 4 (red): cells that must be ON cells of the catalyst, initially OFF
* state 5 (yellow): "stator" ON cells that must not be disturbed, for unrelated catalysts
* state 6 (grey): unused 

Generally, constraints are only applied to cells in the ZOI of the
catalyst. In each generation, the "active" cells are those that are
not equal to the stable state and "changed" cells are those that are
not equal to the state in the previous generation. "Ever-active" cells
are those that are active in at least one generation.

For values of `max-cell-active-window` and `max-cell-active-streak`
other than 0, you will have to edit the corresponding variable at the
top of `Barrister.cpp` and recompile. (These constraints can use much more
memory.)


| Parameter                      | Format                | Description                                                                                            |
|--------------------------------|-----------------------|--------------------------------------------------------------------------------------------------------|
| `pattern`                      | `'''multiline rle'''` | Pattern to search, as described above                                                                  |
| `pattern-center`               | `[x, y]`              | Coordinate in the RLE to be placed at the origin                                                       |
|                                |                       |                                                                                                        |
| `first-active-range`           | `[start, end]`        | Range of generations in which the interaction must start                                               |
| `active-window-range`          | `[min, max]`          | Range of interaction durations that are allowed                                                        |
| `min-stable-interval`          | `n`                   | Duration the catalyst must stay stable after interaction to be accepted                                |
| `max-active-cells`             | `n`                   | Maximum count of active cells in any generation                                                        |
| `max-ever-active-cells`        | `n`                   | Maximum count of ever-active cells                                                                     |
| `max-changes`                  | `n`                   | Maximum count of changed cells in any generation                                                       |
| `active-bounds`                | `[width, height]`     | Maximum bounds the active cells in any generation                                                      |
| `ever-active-bounds`           | `[width, height]`     | Maximum bounds of the ever-active cells                                                                |
| `changes-bounds`               | `[width, height]`     | Maximum bounds the changed cells in any generation                                                     |
|                                |                       |                                                                                                        |
| `max-component-active-cells`   | `n`                   | Versions of the above that separate the relevant cells into components before applying the constraints |
| `max-component-ever-active`    | `n`                   |                                                                                                        |
| `max-component-changes`        | `n`                   |                                                                                                        |
| `component-active-bounds`      | `[width, height]`     |                                                                                                        |
| `component-ever-active-bounds` | `[width, height]`     |                                                                                                        |
| `component-changes-bounds`     | `[width, height]`     |                                                                                                        |
|                                |                       |                                                                                                        |
| `max-cell-active-window`       | `n`                   | Maximum time a cell may remain active after being active the first time                                |
| `max-cell-active-streak`       | `n`                   | Maximum time a cell may remain active in a row                                                         |
| `max-cell-stationary-distance` | `n`                   | The maximum distance that an active, unchanging cell may be changing cells                             |
| `exempt-existing`              | `true` or `false`     | Exempt the parts of the catalyst supplied in the input from the constraints (default `true`)           |
| `print-summary`                | `true` or `false`     | Print all solutions as a single pattern at the end of the search (default `true`)                      |
| `stabilise-results`            | `true` or `false`     | Stabilise each result into a complete still life (default `true`)                                      |
| `minimise-results`             | `true` or `false`     | Try and find the minimal completion or report the first found (default `false`)                        |
| `stabilise-results-timeout`    | `secs`                | How long to spend trying to find a minimal completion (default `3`)                                    |
| `trim-results`                 | `true` or `false`     | Try and collect catalysts that cause different perturbations (default `true`)                          |
| `report-oscillators`           | `true` or `false`     | Only report oscillators (with period > 4) (default `false`)                                            |

### Filters and Forbiddens

Each block headed by `[[filter]]` represents a filter that must be
matched for the solution to be reported. The pattern to be matched
must be provided as a LifeHistory RLE, using states 3 and 4. See
`inputs/snark.toml` for an example. There may be many filters.

Filters of type `EXACT` are passed when the reaction matches the
filter on the generation `filter-gen`. Filters of type `EVER` are
passed when the reaction matches the filter at some point before
`filter-gen`, and after the start of the interaction with the
catalyst.

| Parameter     | Format                | Description        |
|---------------|-----------------------|--------------------|
| `filter`      | `'''multiline rle'''` |                    |
| `filter-type` | `"EXACT"` or `"EVER"` | (default `"EVER"`) |
| `filter-gen`  | `n`                   |                    |
| `filter-pos`  | `[x, y]`              |                    |

Each block headed by `[[forbidden]]` represents a piece of stable
catalyst that may not occur in the solution. The pattern to be matched
must be provided as a LifeHistory RLE, using states 3 and 4. See
`inputs/elbow.toml` for an example.

| Parameter       | Format                | Description |
|-----------------|-----------------------|-------------|
| `forbidden`     | `'''multiline rle'''` |             |
| `forbidden-pos` | `[x, y]`              |             |

### Metasearches

| Parameter                 | Format            | Description       |
|---------------------------|-------------------|-------------------|
| `metasearch`              | `true` or `false` | (default `false`) |
| `metasearch-rounds`       |                   |                   |
| `meta-first-active-range` |                   |                   |

TODO
