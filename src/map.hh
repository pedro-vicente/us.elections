#ifndef MAP_HH
#define MAP_HH

#include <Wt/WCompositeWidget.h>
#include <Wt/WWebWidget.h>
#include <Wt/WApplication.h>
#include <string>
#include <vector>
#include <map>
#include "data.hh"

/////////////////////////////////////////////////////////////////////////////////////////////////////
// WMapLibre
/////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Wt
{
  class WT_API WMapLibre : public WCompositeWidget
  {
    class Impl;

  public:
    WMapLibre();
    ~WMapLibre();

    void set_year(int year);
    void set_view_mode(const std::string& mode);
    void refresh_data();

    int current_year;
    std::string view_mode;
    std::vector<county_record>* counties;
    std::vector<state_record>* states;

  protected:
    Impl* impl;
    virtual void render(WFlags<RenderFlag> flags) override;
  };
}

#endif
