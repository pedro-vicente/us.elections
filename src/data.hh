#ifndef ELECTIONS_DB_HH
#define ELECTIONS_DB_HH

#include <string>
#include <vector>
#include <memory>
#include "duckdb.hpp"

/////////////////////////////////////////////////////////////////////////////////////////////////////
// county_record 
// combined geometry and election data
/////////////////////////////////////////////////////////////////////////////////////////////////////

struct county_record
{
  std::string fips;
  std::string name;
  std::string state_name;
  std::string state_fips;
  int64_t votes_gop = 0;
  int64_t votes_dem = 0;
  int64_t votes_total = 0;
  double per_gop = 0.0;
  double per_dem = 0.0;
  double margin = 0.0;
  std::string geojson;  // ST_AsGeoJSON geometry
};

struct state_record
{
  std::string fips;
  std::string name;
  int64_t votes_gop = 0;
  int64_t votes_dem = 0;
  int64_t votes_total = 0;
  double per_gop = 0.0;
  double per_dem = 0.0;
  std::string winner;
  std::string geojson;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
// database_t
/////////////////////////////////////////////////////////////////////////////////////////////////////

class database_t
{
private:
  std::unique_ptr<duckdb::DuckDB> db;
  std::unique_ptr<duckdb::Connection> conn;
  std::string db_path;

public:
  database_t(const std::string& path);

  int load_topojson(const std::string& json_path);
  int load_election_csv(const std::string& csv_path, int year);
  std::vector<int> get_years();
  std::vector<county_record> get_counties(int year);
  std::vector<state_record> get_states(int year);
  int64_t get_total_votes(int year);
  int export_geojson(int year, const std::string& output_path);
  void print_summary(int year);
  void print_counties_info();
};

#endif
