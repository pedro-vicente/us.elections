#include "map.hh"
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <iostream>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////
// to_hex
// convert int to hex string, apply zero padding
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::string to_hex(int n)
{
  std::stringstream ss;
  ss << std::hex << std::setw(2) << std::setfill('0') << n;
  return ss.str();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// rgb_to_hex
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::string rgb_to_hex(int r, int g, int b)
{
  std::string str("#");
  str += to_hex(r);
  str += to_hex(g);
  str += to_hex(b);
  return str;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// load_geojson
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::string load_geojson(const std::string& name)
{
  std::ifstream file(name);
  if (!file.is_open())
  {
    return "";
  }
  std::string str;
  std::string line;
  while (std::getline(file, line))
  {
    str += line;
  }
  file.close();
  return str;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// escape_js_string
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::string escape_js_string(const std::string& input)
{
  std::string output;
  output.reserve(input.size());
  for (size_t idx = 0; idx < input.size(); ++idx)
  {
    char c = input[idx];
    switch (c)
    {
    case '\'': output += "\\'"; break;
    case '\"': output += "\\\""; break;
    case '\\': output += "\\\\"; break;
    case '\n': output += "\\n"; break;
    case '\r': output += "\\r"; break;
    case '\t': output += "\\t"; break;
    default: output += c; break;
    }
  }
  return output;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// margin_to_color
// margin: positive = gop (red), negative = dem (blue)
/////////////////////////////////////////////////////////////////////////////////////////////////////

std::string margin_to_color(double margin)
{
  if (margin > 0.3334) return "#B82D35";      // strong gop
  if (margin > 0.1667) return "#E48268";      // lean gop
  if (margin > 0.0)    return "#FACCB4";      // slight gop
  if (margin > -0.1667) return "#BFDCEB";     // slight dem
  if (margin > -0.3334) return "#6BACD0";     // lean dem
  return "#2A71AE";                           // strong dem
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
// WMapLibre
/////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Wt
{
  class WMapLibre::Impl : public WWebWidget
  {
  public:
    Impl();
    virtual DomElementType domElementType() const override;
  };

  WMapLibre::Impl::Impl()
  {
    setInline(false);
  }

  DomElementType WMapLibre::Impl::domElementType() const
  {
    return DomElementType::DIV;
  }

  WMapLibre::WMapLibre()
    : current_year(2024), view_mode("county"), counties(nullptr), states(nullptr)
  {
    setImplementation(std::unique_ptr<Impl>(impl = new Impl()));
    WApplication* app = WApplication::instance();
    app->styleSheet().addRule("body", "margin: 0; padding: 0;");
    app->styleSheet().addRule("#" + id(), "position: absolute; top: 0; bottom: 0; width: 100%;");
    app->useStyleSheet("https://unpkg.com/maplibre-gl@4.7.1/dist/maplibre-gl.css");
    app->require("https://unpkg.com/maplibre-gl@4.7.1/dist/maplibre-gl.js", "maplibre");
    app->require("https://unpkg.com/@turf/turf@6/turf.min.js", "turf");
  }

  WMapLibre::~WMapLibre()
  {
  }

  void WMapLibre::set_year(int year)
  {
    current_year = year;
  }

  void WMapLibre::set_view_mode(const std::string& mode)
  {
    view_mode = mode;
  }

  void WMapLibre::refresh_data()
  {
    scheduleRender();
  }

  /////////////////////////////////////////////////////////////////////////////////////////////////////
  // render
  /////////////////////////////////////////////////////////////////////////////////////////////////////

  void WMapLibre::render(WFlags<RenderFlag> flags)
  {
    WCompositeWidget::render(flags);

    if (flags.test(RenderFlag::Full))
    {
      std::stringstream js;

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // create map
      /////////////////////////////////////////////////////////////////////////////////////////////////////

      js << "if (window.map) { window.map.remove(); }\n";
      js << "window.map = new maplibregl.Map({\n"
         << "  container: " << jsRef() << ",\n"
         << "  style: 'https://basemaps.cartocdn.com/gl/positron-gl-style/style.json',\n"
         << "  center: [-98, 39],\n"
         << "  zoom: 4\n"
         << "});\n"
         << "window.map.addControl(new maplibregl.NavigationControl());\n";

#ifdef _WIN32
      OutputDebugStringA(js.str().c_str());
#endif

      js << "window.map.on('load', function() {\n";

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // build geojson from database
      /////////////////////////////////////////////////////////////////////////////////////////////////////

      js << "var geojson = {type:'FeatureCollection',features:[\n";

      if (counties && !counties->empty())
      {
        bool first = true;
        for (size_t idx = 0; idx < counties->size(); ++idx)
        {
          const county_record& c = (*counties)[idx];
          if (c.geojson.empty() || c.geojson == "null")
          {
            continue;
          }

          if (!first)
          {
            js << ",\n";
          }
          first = false;

          std::string color = margin_to_color(c.margin);

          js << "{type:'Feature',id:'" << c.fips << "',"
             << "properties:{"
             << "fips:'" << c.fips << "',"
             << "name:'" << escape_js_string(c.name) << "',"
             << "state:'" << escape_js_string(c.state_name) << "',"
             << "gop:" << c.votes_gop << ","
             << "dem:" << c.votes_dem << ","
             << "total:" << c.votes_total << ","
             << "per_gop:" << c.per_gop << ","
             << "per_dem:" << c.per_dem << ","
             << "margin:" << c.margin << ","
             << "color:'" << color << "'"
             << "},geometry:" << c.geojson << "}";
        }
      }

      js << "]};\n";

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // add source and layers
      /////////////////////////////////////////////////////////////////////////////////////////////////////

      js << "window.map.addSource('counties', {type:'geojson', data:geojson});\n";

      js << "window.map.addLayer({\n"
         << "  id:'counties-fill', type:'fill', source:'counties',\n"
         << "  paint:{'fill-color':['get','color'], 'fill-opacity':0.8}\n"
         << "});\n";

      js << "window.map.addLayer({\n"
         << "  id:'counties-line', type:'line', source:'counties',\n"
         << "  paint:{'line-color':'#222', 'line-width':0.3}\n"
         << "});\n";

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // popup on hover
      /////////////////////////////////////////////////////////////////////////////////////////////////////

      js << "var popup = new maplibregl.Popup({closeButton:false, closeOnClick:false});\n";

      js << "window.map.on('mousemove', 'counties-fill', function(e) {\n"
         << "  if (e.features.length > 0) {\n"
         << "    window.map.getCanvas().style.cursor = 'pointer';\n"
         << "    var p = e.features[0].properties;\n"
         << "    var winner = (p.margin > 0) ? 'GOP' : 'DEM';\n"
         << "    var marginPct = Math.abs(p.margin * 100).toFixed(1);\n"
         << "    var html = '<div style=\"font-family:sans-serif;font-size:12px;\">'\n"
         << "      + '<strong>' + p.name + ', ' + p.state + '</strong><br>'\n"
         << "      + '<span style=\"color:#B82D35\">GOP: ' + (p.per_gop * 100).toFixed(1) + '%</span><br>'\n"
         << "      + '<span style=\"color:#2A71AE\">DEM: ' + (p.per_dem * 100).toFixed(1) + '%</span><br>'\n"
         << "      + 'Margin: ' + winner + ' +' + marginPct + '%<br>'\n"
         << "      + 'Total votes: ' + p.total.toLocaleString()\n"
         << "      + '</div>';\n"
         << "    popup.setLngLat(e.lngLat).setHTML(html).addTo(window.map);\n"
         << "  }\n"
         << "});\n";

      js << "window.map.on('mouseleave', 'counties-fill', function() {\n"
         << "  window.map.getCanvas().style.cursor = '';\n"
         << "  popup.remove();\n"
         << "});\n";

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // click to zoom
      /////////////////////////////////////////////////////////////////////////////////////////////////////

      js << "window.map.on('click', 'counties-fill', function(e) {\n"
         << "  var bbox = turf.bbox(e.features[0]);\n"
         << "  window.map.fitBounds(bbox, { padding: 100 });\n"
         << "});\n";

      /////////////////////////////////////////////////////////////////////////////////////////////////////
      // close map.on('load')
      /////////////////////////////////////////////////////////////////////////////////////////////////////

      js << "});\n";

      WApplication* app = WApplication::instance();
      app->doJavaScript(js.str());
    }
  }

} // namespace Wt
