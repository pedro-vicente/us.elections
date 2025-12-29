# us.elections 

U.S. elections data by county and state. SQL database backend with DuckDB spatial extension, frontend with C++ Wt web framework and MapLibre GL JS. 

**Live Demo:** https://pedro-vicente.net:9441/

![US Elections Map](https://github.com/user-attachments/assets/67af63c3-8660-4840-8d5f-b32c901f8bae)

## Architecture

```
TopoJSON ──┐
           ├──> DuckDB (spatial) ──> Wt + MapLibre GL JS
CSV Data ──┘
```

## Dependencies

| Library | Version | Description |
|---------|---------|-------------|
| DuckDB | 1.4.3 | In-process SQL database |
| DuckDB Spatial | - | Spatial extension for geometry |
| duck-spatial | - | C++ spatial client wrapper for DuckDB |
| Wt | 4.12.1 | C++ web framework |
| Boost | 1.88.0 | C++ libraries |
| MapLibre GL JS | 4.7.1 | Web map rendering (CDN) |

## duck-spatial API

C++ wrapper for DuckDB spatial extension providing geometry operations:

| Category | Functions |
|----------|-----------|
| **Create** | `st_point`, `st_geom_from_text`, `st_makeline`, `st_makepolygon`, `st_make_envelope` |
| **Properties** | `st_x`, `st_y`, `st_area`, `st_length`, `st_npoints`, `st_isvalid`, `st_centroid`, `st_extent` |
| **Relationships** | `st_intersects`, `st_contains`, `st_within`, `st_distance` |
| **Operations** | `st_intersection`, `st_union`, `st_buffer`, `st_convexhull` |
| **Export** | `st_asgeojson` |

## Build

```bash
./build_duckdb.sh
./build_boost.sh
./build_wt.sh
./build_cmake.sh
```

## Executables

| Target | Description |
|--------|-------------|
| loader | Load TopoJSON and election data into DuckDB, create tables |
| elections | Web application displaying U.S elections |

## Usage

### 1. Generate Database

```bash
./loader counties-10m.json 2024_US_County_Level_Presidential_Results.csv 2024 elections.duckdb
```

Creates `elections.duckdb` with election data, counties and state boundaries

### 2. Run Web Application

```bash
./elections --http-address=0.0.0.0 --http-port=8080 --docroot=.
```

Open http://localhost:8080 in browser.

## DuckDB Tables

```sql
-- County geometries (from TopoJSON)
CREATE TABLE counties (
  fips VARCHAR PRIMARY KEY,
  name VARCHAR,
  state_fips VARCHAR,
  geometry GEOMETRY
);

-- Election results
CREATE TABLE results (
  year INTEGER,
  county_fips VARCHAR,
  votes_gop BIGINT,
  votes_dem BIGINT,
  votes_total BIGINT,
  per_gop DOUBLE,
  per_dem DOUBLE,
  margin DOUBLE,
  PRIMARY KEY (year, county_fips)
);

-- Query with geometry
SELECT c.fips, c.name, r.margin, ST_AsGeoJSON(c.geometry)
FROM counties c
JOIN results r ON c.fips = r.county_fips
WHERE r.year = 2024;
```

## Data Sources

### Election Results CSV

[tonmcg/US_County_Level_Election_Results_08-24](https://github.com/tonmcg/US_County_Level_Election_Results_08-24)

### TopoJSON County Boundaries

[topojson/us-atlas](https://github.com/topojson/us-atlas)

## Dependencies

- [DuckDB](https://duckdb.org/) with spatial extension
- [Wt](https://www.webtoolkit.eu/wt) (web app)
- [MapLibre GL JS](https://maplibre.org/) (CDN)





