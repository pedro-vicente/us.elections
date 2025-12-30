#pragma once

#include <string>
#include <vector>

namespace duckdb
{
  class DuckDB;
  class Connection;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// Point2D
/////////////////////////////////////////////////////////////////////////////////////////////////////

struct Point2D 
{
  double x;
  double y;

  Point2D();
  Point2D(double x_, double y_);
  std::string to_wkt() const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
// BoundingBox
/////////////////////////////////////////////////////////////////////////////////////////////////////

struct BoundingBox 
{
  double min_x, min_y, max_x, max_y;

  BoundingBox();
  BoundingBox(double minx, double miny, double maxx, double maxy);
  double area() const;
  bool contains(const Point2D& p) const;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
// SpatialClient
/////////////////////////////////////////////////////////////////////////////////////////////////////

class SpatialClient 
{
public:
  SpatialClient();
  explicit SpatialClient(const std::string& db_path);
  ~SpatialClient();

  bool init_spatial();

  void query(const std::string& sql);
  bool execute(const std::string& sql);
  std::string query_string(const std::string& sql);
  double query_double(const std::string& sql);
  bool query_bool(const std::string& sql);
  int query_int(const std::string& sql);

  // geometry creation
  std::string st_point(double x, double y);
  std::string st_geom_from_text(const std::string& wkt);
  std::string st_makeline(const std::vector<Point2D>& points);
  std::string st_makepolygon(const std::vector<Point2D>& ring);
  std::string st_make_envelope(double min_x, double min_y, double max_x, double max_y);

  // properties
  double st_x(const std::string& geom);
  double st_y(const std::string& geom);
  double st_area(const std::string& geom);
  double st_length(const std::string& geom);
  int st_npoints(const std::string& geom);
  bool st_isvalid(const std::string& geom);
  Point2D st_centroid(const std::string& geom);
  BoundingBox st_extent(const std::string& geom);

  // relationships
  bool st_intersects(const std::string& geom1, const std::string& geom2);
  bool st_contains(const std::string& geom1, const std::string& geom2);
  bool st_within(const std::string& geom1, const std::string& geom2);
  double st_distance(const std::string& geom1, const std::string& geom2);

  // operations
  std::string st_intersection(const std::string& geom1, const std::string& geom2);
  std::string st_union(const std::string& geom1, const std::string& geom2);
  std::string st_buffer(const std::string& geom, double distance);
  std::string st_convexhull(const std::string& geom);

  // export
  std::string st_asgeojson(const std::string& geom);

private:
  duckdb::DuckDB* db;
  duckdb::Connection* conn;

  std::string escape(const std::string& s);
};

