#include "phlex/module.hpp"
#include "edep_arrow/Segments.hpp"
#include "edep_sim_phlex/Data.hpp"

namespace inference {

    
    std::vector<std::vector<double>> rasterize(const phlex_arrow::TableGroup & group) {
        auto segments_table = group.at(edep_sim_phlex::kSegmentsMember);


        
    }

}