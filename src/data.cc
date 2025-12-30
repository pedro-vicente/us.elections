#include "data.hh"
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>

/////////////////////////////////////////////////////////////////////////////////////////////////////
// database_t
/////////////////////////////////////////////////////////////////////////////////////////////////////

database_t::database_t(const std::string& path) : db_path(path)
{
  duckdb::DBConfig config;
  db = std::make_unique<duckdb::DuckDB>(db_path, &config);
  conn = std::make_unique<duckdb::Connection>(*db);

  conn->Query("INSTALL spatial;");
  conn->Query("LOAD spatial;");

  conn->Query(R"(
    CREATE TABLE IF NOT EXISTS counties (
      fips VARCHAR PRIMARY KEY,
      name VARCHAR,
      state_fips VARCHAR,
      geometry GEOMETRY
    );
  )");

  conn->Query(R"(
    CREATE TABLE IF NOT EXISTS states (
      fips VARCHAR PRIMARY KEY,
      name VARCHAR,
      geometry GEOMETRY
    );
  )");

  conn->Query(R"(
    CREATE TABLE IF NOT EXISTS state_names (
      fips VARCHAR PRIMARY KEY,
      name VARCHAR NOT NULL
    );
  )");

  const char* state_data[] = {
    "01", "Alabama", "02", "Alaska", "04", "Arizona", "05", "Arkansas",
    "06", "California", "08", "Colorado", "09", "Connecticut", "10", "Delaware",
    "11", "District of Columbia", "12", "Florida", "13", "Georgia", "15", "Hawaii",
    "16", "Idaho", "17", "Illinois", "18", "Indiana", "19", "Iowa",
    "20", "Kansas", "21", "Kentucky", "22", "Louisiana", "23", "Maine",
    "24", "Maryland", "25", "Massachusetts", "26", "Michigan", "27", "Minnesota",
    "28", "Mississippi", "29", "Missouri", "30", "Montana", "31", "Nebraska",
    "32", "Nevada", "33", "New Hampshire", "34", "New Jersey", "35", "New Mexico",
    "36", "New York", "37", "North Carolina", "38", "North Dakota", "39", "Ohio",
    "40", "Oklahoma", "41", "Oregon", "42", "Pennsylvania", "44", "Rhode Island",
    "45", "South Carolina", "46", "South Dakota", "47", "Tennessee", "48", "Texas",
    "49", "Utah", "50", "Vermont", "51", "Virginia", "53", "Washington",
    "54", "West Virginia", "55", "Wisconsin", "56", "Wyoming", nullptr, nullptr
  };

  for (int idx = 0; state_data[idx] != nullptr; idx += 2)
  {
    conn->Query("INSERT OR IGNORE INTO state_names VALUES ('" +
      std::string(state_data[idx]) + "', '" + std::string(state_data[idx + 1]) + "');");
  }

  conn->Query(R"(
    CREATE TABLE IF NOT EXISTS results (
      year INTEGER NOT NULL,
      county_fips VARCHAR NOT NULL,
      county_name VARCHAR,
      votes_gop BIGINT NOT NULL,
      votes_dem BIGINT NOT NULL,
      votes_total BIGINT NOT NULL,
      per_gop DOUBLE NOT NULL,
      per_dem DOUBLE NOT NULL,
      margin DOUBLE NOT NULL,
      PRIMARY KEY (year, county_fips)
    );
  )");

  // add county_name column if it doesn't exist (for existing databases)
  std::unique_ptr<duckdb::MaterializedQueryResult> check_result = conn->Query(
    "SELECT column_name FROM information_schema.columns WHERE table_name = 'results' AND column_name = 'county_name'");
  duckdb::unique_ptr<duckdb::DataChunk> check_chunk = check_result->Fetch();
  if (!check_chunk || check_chunk->size() == 0)
  {
    conn->Query("ALTER TABLE results ADD COLUMN county_name VARCHAR;");
    std::cout << "Added county_name column to results table" << std::endl;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// load_topojson
/////////////////////////////////////////////////////////////////////////////////////////////////////

int database_t::load_topojson(const std::string& json_path)
{
  std::ifstream file(json_path);
  if (!file.is_open())
  {
    return -1;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();
  std::string json_str = buffer.str();
  file.close();

  bool is_topojson = json_str.find("\"type\":\"Topology\"") != std::string::npos ||
    json_str.find("\"type\": \"Topology\"") != std::string::npos;

  if (is_topojson)
  {
    conn->Query("DELETE FROM counties;");
    conn->Query("DELETE FROM states;");

    std::unique_ptr<duckdb::MaterializedQueryResult> result = conn->Query(
      "SELECT * FROM ST_Read('" + json_path + "', layer='counties') LIMIT 1;"
    );

    if (result->HasError())
    {
      std::cerr << result->GetError() << std::endl;
      return -1;
    }

    std::unique_ptr<duckdb::MaterializedQueryResult> counties_result = conn->Query(R"(
      INSERT INTO counties (fips, name, state_fips, geometry)
      SELECT 
        LPAD(CAST(id AS VARCHAR), 5, '0') as fips,
        '' as name,
        SUBSTR(LPAD(CAST(id AS VARCHAR), 5, '0'), 1, 2) as state_fips,
        geom as geometry
      FROM ST_Read(')" + json_path + R"(', layer='counties')
      ON CONFLICT (fips) DO UPDATE SET geometry = EXCLUDED.geometry;
    )");

    if (counties_result->HasError())
    {
      std::cerr << counties_result->GetError() << std::endl;
    }

    std::unique_ptr<duckdb::MaterializedQueryResult> states_result = conn->Query(R"(
      INSERT INTO states (fips, name, geometry)
      SELECT 
        LPAD(CAST(id AS VARCHAR), 2, '0') as fips,
        '' as name,
        geom as geometry
      FROM ST_Read(')" + json_path + R"(', layer='states')
      ON CONFLICT (fips) DO UPDATE SET geometry = EXCLUDED.geometry;
    )");

    if (states_result->HasError())
    {
      std::cerr << states_result->GetError() << std::endl;
    }
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  // geojson
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  else
  {

    conn->Query("DELETE FROM counties;");

    std::unique_ptr<duckdb::MaterializedQueryResult> result = conn->Query(R"(
      INSERT INTO counties (fips, name, state_fips, geometry)
      SELECT 
        LPAD(CAST(id AS VARCHAR), 5, '0') as fips,
        '' as name,
        SUBSTR(LPAD(CAST(id AS VARCHAR), 5, '0'), 1, 2) as state_fips,
        geom as geometry
      FROM ST_Read(')" + json_path + R"(')
      ON CONFLICT (fips) DO UPDATE SET geometry = EXCLUDED.geometry;
    )");

    if (result->HasError())
    {
      std::cerr << result->GetError() << std::endl;
      return -1;
    }
  }

  std::unique_ptr<duckdb::MaterializedQueryResult> count_result = conn->Query("SELECT COUNT(*) FROM counties;");
  if (!count_result->HasError())
  {
    duckdb::unique_ptr<duckdb::DataChunk> chunk = count_result->Fetch();
    if (chunk && chunk->size() > 0)
    {
      int64_t count = chunk->GetValue(0, 0).GetValue<int64_t>();
      std::cout << "Loaded " << count << " counties" << std::endl;
      return static_cast<int>(count);
    }
  }

  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// load_election_csv (2024 format only)
/////////////////////////////////////////////////////////////////////////////////////////////////////

int database_t::load_election_csv(const std::string& csv_path, int year)
{
  conn->Query("DELETE FROM results WHERE year = " + std::to_string(year));

  std::string sql = R"(
    INSERT INTO results (year, county_fips, county_name, votes_gop, votes_dem, votes_total, per_gop, per_dem, margin)
    SELECT 
      )" + std::to_string(year) + R"( as year,
      LPAD(CAST(county_fips AS VARCHAR), 5, '0') as county_fips,
      "county_name",
      CAST(votes_gop AS BIGINT),
      CAST(votes_dem AS BIGINT),
      CAST(total_votes AS BIGINT),
      per_gop,
      per_dem,
      per_gop - per_dem as margin
    FROM read_csv(')" + csv_path + R"(', header=true)
    WHERE LENGTH(CAST(county_fips AS VARCHAR)) >= 4
  )";

  std::unique_ptr<duckdb::MaterializedQueryResult> result = conn->Query(sql);
  if (result->HasError())
  {
    std::cerr << result->GetError() << std::endl;
    return -1;
  }

  std::unique_ptr<duckdb::MaterializedQueryResult> count_result = conn->Query("SELECT COUNT(*) FROM results WHERE year = " + std::to_string(year));
  if (!count_result->HasError())
  {
    duckdb::unique_ptr<duckdb::DataChunk> chunk = count_result->Fetch();
    if (chunk && chunk->size() > 0)
    {
      int64_t count = chunk->GetValue(0, 0).GetValue<int64_t>();
      std::cout << "Loaded " << count << " results for " << year << std::endl;
      return static_cast<int>(count);
    }
  }

  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// get_years
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<int> database_t::get_years()
{
  std::vector<int> years;

  std::unique_ptr<duckdb::MaterializedQueryResult> result = conn->Query("SELECT DISTINCT year FROM results ORDER BY year DESC");
  if (result->HasError())
  {
    return years;
  }

  duckdb::unique_ptr<duckdb::DataChunk> chunk;
  while ((chunk = result->Fetch()) != nullptr)
  {
    for (size_t idx = 0; idx < chunk->size(); idx++)
    {
      years.push_back(chunk->GetValue(0, idx).GetValue<int>());
    }
  }

  return years;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// get_counties
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<county_record> database_t::get_counties(int year)
{
  std::vector<county_record> records;

  std::string sql = R"(
    SELECT 
      c.fips,
      COALESCE(r.county_name, c.name) as name,
      COALESCE(s.name, '') as state_name,
      c.state_fips,
      COALESCE(r.votes_gop, 0) as votes_gop,
      COALESCE(r.votes_dem, 0) as votes_dem,
      COALESCE(r.votes_total, 0) as votes_total,
      COALESCE(r.per_gop, 0) as per_gop,
      COALESCE(r.per_dem, 0) as per_dem,
      COALESCE(r.margin, 0) as margin,
      ST_AsGeoJSON(c.geometry) as geojson
    FROM counties c
    LEFT JOIN results r ON c.fips = r.county_fips AND r.year = )" + std::to_string(year) + R"(
    LEFT JOIN state_names s ON c.state_fips = s.fips
    ORDER BY c.fips
  )";

  std::unique_ptr<duckdb::MaterializedQueryResult> result = conn->Query(sql);
  if (result->HasError())
  {
    std::cerr << result->GetError() << std::endl;
    return records;
  }

  duckdb::unique_ptr<duckdb::DataChunk> chunk;
  while ((chunk = result->Fetch()) != nullptr)
  {
    for (size_t idx = 0; idx < chunk->size(); idx++)
    {
      county_record rec;
      rec.fips = chunk->GetValue(0, idx).ToString();

      duckdb::Value name_val = chunk->GetValue(1, idx);
      if (name_val.IsNull())
      {
        rec.name = "";
      }
      else
      {
        rec.name = name_val.ToString();
      }

      duckdb::Value state_val = chunk->GetValue(2, idx);
      if (state_val.IsNull())
      {
        rec.state_name = "";
      }
      else
      {
        rec.state_name = state_val.ToString();
      }

      rec.state_fips = chunk->GetValue(3, idx).ToString();
      rec.votes_gop = chunk->GetValue(4, idx).GetValue<int64_t>();
      rec.votes_dem = chunk->GetValue(5, idx).GetValue<int64_t>();
      rec.votes_total = chunk->GetValue(6, idx).GetValue<int64_t>();
      rec.per_gop = chunk->GetValue(7, idx).GetValue<double>();
      rec.per_dem = chunk->GetValue(8, idx).GetValue<double>();
      rec.margin = chunk->GetValue(9, idx).GetValue<double>();
      rec.geojson = chunk->GetValue(10, idx).ToString();
      records.push_back(rec);
    }
  }

  if (records.size() > 0)
  {
    std::cout << "First 3 counties for year " << year << ":" << std::endl;
    for (size_t idx = 0; idx < 3 && idx < records.size(); idx++)
    {
      std::cout << "  FIPS: " << records[idx].fips
        << ", Name: [" << records[idx].name << "]"
        << ", State: [" << records[idx].state_name << "]" << std::endl;
    }
  }

  return records;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// get_states
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::vector<state_record> database_t::get_states(int year)
{
  std::vector<state_record> records;

  std::string sql = R"(
    SELECT 
      c.state_fips as fips,
      s.name as state_name,
      SUM(COALESCE(r.votes_gop, 0)) as votes_gop,
      SUM(COALESCE(r.votes_dem, 0)) as votes_dem,
      SUM(COALESCE(r.votes_total, 0)) as votes_total
    FROM counties c
    LEFT JOIN results r ON c.fips = r.county_fips AND r.year = )" + std::to_string(year) + R"(
    LEFT JOIN state_names s ON c.state_fips = s.fips
    GROUP BY c.state_fips, s.name
    ORDER BY s.name
  )";

  std::unique_ptr<duckdb::MaterializedQueryResult> result = conn->Query(sql);
  if (result->HasError())
  {
    std::cerr << result->GetError() << std::endl;
    return records;
  }

  duckdb::unique_ptr<duckdb::DataChunk> chunk;
  while ((chunk = result->Fetch()) != nullptr)
  {
    for (size_t idx = 0; idx < chunk->size(); idx++)
    {
      state_record rec;
      rec.fips = chunk->GetValue(0, idx).ToString();
      rec.name = chunk->GetValue(1, idx).ToString();
      rec.votes_gop = chunk->GetValue(2, idx).GetValue<int64_t>();
      rec.votes_dem = chunk->GetValue(3, idx).GetValue<int64_t>();
      rec.votes_total = chunk->GetValue(4, idx).GetValue<int64_t>();
      rec.per_gop = (rec.votes_total > 0) ? static_cast<double>(rec.votes_gop) / rec.votes_total : 0.0;
      rec.per_dem = (rec.votes_total > 0) ? static_cast<double>(rec.votes_dem) / rec.votes_total : 0.0;
      rec.winner = (rec.votes_gop > rec.votes_dem) ? "GOP" : "DEM";
      records.push_back(rec);
    }
  }

  return records;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// get_total_votes
/////////////////////////////////////////////////////////////////////////////////////////////////////

int64_t database_t::get_total_votes(int year)
{
  std::unique_ptr<duckdb::MaterializedQueryResult> result = conn->Query("SELECT SUM(votes_total) FROM results WHERE year = " + std::to_string(year));
  if (result->HasError()) return 0;

  duckdb::unique_ptr<duckdb::DataChunk> chunk = result->Fetch();
  if (chunk && chunk->size() > 0)
  {
    return chunk->GetValue(0, 0).GetValue<int64_t>();
  }
  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// export_geojson
/////////////////////////////////////////////////////////////////////////////////////////////////////

int database_t::export_geojson(int year, const std::string& output_path)
{
  std::ofstream file(output_path);
  if (!file.is_open())
  {
    return -1;
  }

  std::vector<county_record> counties = get_counties(year);

  file << "{\"type\":\"FeatureCollection\",\"features\":[\n";

  bool first = true;
  for (size_t idx = 0; idx < counties.size(); idx++)
  {
    const county_record& c = counties[idx];
    if (c.geojson.empty() || c.geojson == "null") continue;

    if (!first) file << ",\n";
    first = false;

    std::string name_escaped = c.name;
    size_t pos = 0;
    while ((pos = name_escaped.find('"', pos)) != std::string::npos)
    {
      name_escaped.replace(pos, 1, "\\\"");
      pos += 2;
    }

    file << "{\"type\":\"Feature\","
      << "\"id\":\"" << c.fips << "\","
      << "\"properties\":{"
      << "\"fips\":\"" << c.fips << "\","
      << "\"name\":\"" << name_escaped << "\","
      << "\"state\":\"" << c.state_name << "\","
      << "\"gop\":" << c.votes_gop << ","
      << "\"dem\":" << c.votes_dem << ","
      << "\"total\":" << c.votes_total << ","
      << "\"per_gop\":" << std::fixed << std::setprecision(6) << c.per_gop << ","
      << "\"per_dem\":" << c.per_dem << ","
      << "\"margin\":" << c.margin
      << "},"
      << "\"geometry\":" << c.geojson
      << "}";
  }

  file << "\n]}\n";
  file.close();

  std::cout << "Exported " << counties.size() << " counties to " << output_path << std::endl;
  return static_cast<int>(counties.size());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// print_summary
/////////////////////////////////////////////////////////////////////////////////////////////////////

void database_t::print_summary(int year)
{
  std::vector<state_record> states = get_states(year);

  std::cout << "\nElection results for " << year << ":\n";
  std::cout << std::left << std::setw(20) << "State"
    << std::right << std::setw(12) << "GOP"
    << std::setw(12) << "DEM"
    << std::setw(8) << "Winner" << "\n";
  std::cout << std::string(52, '-') << "\n";

  int64_t total_gop = 0, total_dem = 0;

  for (size_t idx = 0; idx < states.size(); idx++)
  {
    const state_record& s = states[idx];
    total_gop += s.votes_gop;
    total_dem += s.votes_dem;

    std::cout << std::left << std::setw(20) << s.name
      << std::right << std::setw(12) << s.votes_gop
      << std::setw(12) << s.votes_dem
      << std::setw(8) << s.winner << "\n";
  }

  std::cout << std::string(52, '-') << "\n";
  std::cout << std::left << std::setw(20) << "TOTAL"
    << std::right << std::setw(12) << total_gop
    << std::setw(12) << total_dem
    << std::setw(8) << (total_gop > total_dem ? "GOP" : "DEM") << "\n";
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// print_counties_info
/////////////////////////////////////////////////////////////////////////////////////////////////////

void database_t::print_counties_info()
{
  std::unique_ptr<duckdb::MaterializedQueryResult> result = conn->Query("SELECT COUNT(*) FROM counties");
  if (!result->HasError())
  {
    duckdb::unique_ptr<duckdb::DataChunk> chunk = result->Fetch();
    if (chunk && chunk->size() > 0)
    {
      std::cout << "Counties in database: " << chunk->GetValue(0, 0).GetValue<int64_t>() << std::endl;
    }
  }

  std::vector<int> years = get_years();
  std::cout << "Election years: ";
  for (size_t idx = 0; idx < years.size(); idx++)
  {
    if (idx > 0) std::cout << ", ";
    std::cout << years[idx];
  }
  std::cout << std::endl;
}
