#include "data.hh"
#include <iostream>

/////////////////////////////////////////////////////////////////////////////////////////////////////
// main
// ./loader <topojson> <csv_file> <year> [db]
// ./loader counties-10m.json 2024_US_County_Level_Presidential_Results.csv 2024 elections.duckdb
/////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
  if (argc < 4)
  {
    std::cout << "Usage: " << argv[0] << " <topojson> <csv_file> <year> [db]\n";
    return 1;
  }

  std::string json_path = argv[1];
  std::string csv_path = argv[2];
  int year = std::stoi(argv[3]);
  std::string db_path = (argc > 4) ? argv[4] : "elections.duckdb";

  database_t db(db_path);

  int geo_count = db.load_topojson(json_path);
  if (geo_count <= 0)
  {
    return 1;
  }

  std::cout << "Counties loaded: " << geo_count << std::endl;

  int csv_count = db.load_election_csv(csv_path, year);
  if (csv_count <= 0)
  {
    return 1;
  }

  std::cout << "Results loaded: " << csv_count << std::endl;

  db.print_summary(year);

  return 0;
}
