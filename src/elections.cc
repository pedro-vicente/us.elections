#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WCompositeWidget.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WVBoxLayout.h>
#include <Wt/WText.h>
#include <Wt/WComboBox.h>
#include <Wt/WTable.h>
#include <Wt/WTableCell.h>
#include <Wt/WCssStyleSheet.h>
#include <sstream>
#include <memory>
#include <iomanip>
#include "data.hh"
#include "map.hh"

/////////////////////////////////////////////////////////////////////////////////////////////////////
// globals
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<database_t> db;

/////////////////////////////////////////////////////////////////////////////////////////////////////
// format_number
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::string format_number(int64_t num)
{
  std::string s = std::to_string(num);
  int insert_pos = s.length() - 3;
  while (insert_pos > 0)
  {
    s.insert(insert_pos, ",");
    insert_pos -= 3;
  }
  return s;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// ApplicationElections
/////////////////////////////////////////////////////////////////////////////////////////////////////

class ApplicationElections : public Wt::WApplication
{
public:
  ApplicationElections(const Wt::WEnvironment& env);

private:
  int current_year;
  std::vector<county_record> counties;
  std::vector<state_record> states;

  Wt::WMapLibre* map;
  Wt::WComboBox* year_combo;
  Wt::WText* stats_text;
  Wt::WTable* results_table;

  void on_year_changed();
  void update_stats();
  void update_table();
};

ApplicationElections::ApplicationElections(const Wt::WEnvironment& env)
  : Wt::WApplication(env), current_year(2024)
{
  setTitle("US Elections (DuckDB + Spatial)");

  if (db)
  {
    std::vector<int> years = db->get_years();
    if (!years.empty()) current_year = years[0];
    counties = db->get_counties(current_year);
    states = db->get_states(current_year);
  }

  std::unique_ptr<Wt::WHBoxLayout> layout = std::make_unique<Wt::WHBoxLayout>();
  layout->setContentsMargins(0, 0, 0, 0);

  std::unique_ptr<Wt::WContainerWidget> sidebar = std::make_unique<Wt::WContainerWidget>();
  std::string sidebar_id = sidebar->id();
  styleSheet().addRule("#" + sidebar_id,
    "background:#1a1a2e;color:#eee;padding:15px;overflow-y:auto;max-height:100vh;");
  sidebar->setWidth(280);

  std::unique_ptr<Wt::WVBoxLayout> layout_sidebar = std::make_unique<Wt::WVBoxLayout>();
  layout_sidebar->setContentsMargins(0, 0, 0, 0);

  layout_sidebar->addWidget(std::make_unique<Wt::WText>("<h3 style='margin:0 0 15px 0;'>US Elections</h3>"));
  layout_sidebar->addWidget(std::make_unique<Wt::WText>("<b>Election Year</b>"));

  year_combo = layout_sidebar->addWidget(std::make_unique<Wt::WComboBox>());
  std::string combo_id = year_combo->id();
  styleSheet().addRule("#" + combo_id,
    "width:100%;padding:8px;margin:5px 0 15px 0;background:#16213e;color:#fff;border:1px solid #0f3460;border-radius:4px;");

  if (db)
  {
    std::vector<int> years = db->get_years();
    for (size_t i = 0; i < years.size(); i++)
    {
      year_combo->addItem(std::to_string(years[i]));
    }
  }
  else
  {
    year_combo->addItem("2024");
  }
  year_combo->changed().connect(this, &ApplicationElections::on_year_changed);

  layout_sidebar->addWidget(std::make_unique<Wt::WText>("<b>Legend</b>"));
  layout_sidebar->addWidget(std::make_unique<Wt::WText>(
    "<div style='font-size:11px;margin:10px 0;'>"
    "<div style='margin:3px 0;'><span style='background:#B82D35;padding:2px 12px;'></span> Strong GOP</div>"
    "<div style='margin:3px 0;'><span style='background:#E48268;padding:2px 12px;'></span> Lean GOP</div>"
    "<div style='margin:3px 0;'><span style='background:#FACCB4;padding:2px 12px;'></span> Slight GOP</div>"
    "<div style='margin:3px 0;'><span style='background:#BFDCEB;padding:2px 12px;'></span> Slight DEM</div>"
    "<div style='margin:3px 0;'><span style='background:#6BACD0;padding:2px 12px;'></span> Lean DEM</div>"
    "<div style='margin:3px 0;'><span style='background:#2A71AE;padding:2px 12px;'></span> Strong DEM</div>"
    "</div>"));

  layout_sidebar->addWidget(std::make_unique<Wt::WText>("<b>National Results</b>"));
  stats_text = layout_sidebar->addWidget(std::make_unique<Wt::WText>());
  update_stats();

  layout_sidebar->addWidget(std::make_unique<Wt::WText>("<b>State Results</b>"));
  results_table = layout_sidebar->addWidget(std::make_unique<Wt::WTable>());
  std::string table_id = results_table->id();
  styleSheet().addRule("#" + table_id,
    "width:100%;font-size:11px;margin-top:10px;border-collapse:collapse;");
  update_table();

  layout_sidebar->addStretch(1);
  sidebar->setLayout(std::move(layout_sidebar));
  layout->addWidget(std::move(sidebar), 0);

  std::unique_ptr<Wt::WContainerWidget> container_map = std::make_unique<Wt::WContainerWidget>();
  map = container_map->addWidget(std::make_unique<Wt::WMapLibre>());
  map->resize(Wt::WLength::Auto, Wt::WLength::Auto);
  map->counties_ptr = &counties;
  map->current_year = current_year;

  layout->addWidget(std::move(container_map), 1);
  root()->setLayout(std::move(layout));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// on_year_changed
/////////////////////////////////////////////////////////////////////////////////////////////////////

void ApplicationElections::on_year_changed()
{
  if (!db) return;

  current_year = std::stoi(year_combo->currentText().toUTF8());
  counties = db->get_counties(current_year);
  states = db->get_states(current_year);

  map->current_year = current_year;
  map->counties_ptr = &counties;
  map->refresh_data();

  update_stats();
  update_table();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// update_stats
/////////////////////////////////////////////////////////////////////////////////////////////////////

void ApplicationElections::update_stats()
{
  if (!db)
  {
    return;
  }

  int64_t total = db->get_total_votes(current_year);
  if (total == 0)
  {
    return;
  }

  int64_t gop = 0, dem = 0;
  for (size_t i = 0; i < states.size(); i++)
  {
    gop += states[i].votes_gop;
    dem += states[i].votes_dem;
  }

  std::stringstream ss;
  ss << std::fixed << std::setprecision(1);
  ss << "<div style='margin:10px 0;'>";
  ss << "<div style='color:#B82D35;'>GOP: " << format_number(gop) << " (" << (100.0 * gop / total) << "%)</div>";
  ss << "<div style='color:#6BACD0;'>DEM: " << format_number(dem) << " (" << (100.0 * dem / total) << "%)</div>";
  ss << "<div style='color:#888;margin-top:5px;'>Total: " << format_number(total) << "</div>";
  ss << "</div>";

  stats_text->setText(ss.str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// update_table
/////////////////////////////////////////////////////////////////////////////////////////////////////

void ApplicationElections::update_table()
{
  results_table->clear();

  results_table->elementAt(0, 0)->addWidget(std::make_unique<Wt::WText>("State"));
  results_table->elementAt(0, 1)->addWidget(std::make_unique<Wt::WText>("Winner"));
  results_table->elementAt(0, 2)->addWidget(std::make_unique<Wt::WText>("Margin"));

  for (size_t i = 0; i < states.size(); i++)
  {
    const state_record& s = states[i];
    int row = i + 1;

    results_table->elementAt(row, 0)->addWidget(
      std::make_unique<Wt::WText>(s.name.substr(0, 12)));

    std::string color = (s.winner == "GOP") ? "#B82D35" : "#2A71AE";
    results_table->elementAt(row, 1)->addWidget(
      std::make_unique<Wt::WText>("<span style='color:" + color + ";'>" + s.winner + "</span>"));

    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << std::abs(s.per_gop - s.per_dem) * 100 << "%";
    results_table->elementAt(row, 2)->addWidget(std::make_unique<Wt::WText>(ss.str()));
  }
}

std::unique_ptr<Wt::WApplication> create_application(const Wt::WEnvironment& env)
{
  return std::make_unique<ApplicationElections>(env);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// main
/////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[])
{
  try
  {
    db = std::make_unique<database_t>("elections.duckdb");
    db->print_counties_info();
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return Wt::WRun(argc, argv, &create_application);
}
