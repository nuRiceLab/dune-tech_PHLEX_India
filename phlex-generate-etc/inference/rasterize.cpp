#include "phlex/module.hpp"
#include "edep_sim_phlex/Data.hpp"
#include "phlex_arrow_common/TableGroup.hpp"

#include <arrow/api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace inference {

    // A cubic voxel grid in edep-sim units (mm).  Axis 0, 1, 2 = x, y, z.
    struct Grid {
        std::array<double, 3> lo{};
        std::array<std::size_t, 3> n{};
        double voxel_size = 0.0;
    };

    // Sparse 3D voxels: flattened voxel index -> summed weight.  Only voxels that
    // receive a deposit are stored, so a fine grid over the whole detector is cheap.
    using Voxels = std::unordered_map<std::uint64_t, double>;

    Grid make_grid(std::array<double, 3> const& lo, std::array<double, 3> const& hi,
                   double voxel_size)
    {
        if (!(voxel_size > 0)) {
            throw std::runtime_error("rasterize: voxel_size must be > 0");
        }
        Grid grid{lo, {}, voxel_size};
        for (std::size_t a = 0; a < 3; ++a) {
            if (!(hi[a] > lo[a])) {
                throw std::runtime_error("rasterize: each axis needs max > min");
            }
            // The tolerance stops rounding error adding a voxel, eg voxel_size 100.0 / 29
            // over 100 mm gives 29.000000000000004.
            auto const ratio = (hi[a] - lo[a]) / voxel_size;
            grid.n[a] = static_cast<std::size_t>(std::ceil(ratio * (1 - 1e-9)));
        }
        return grid;
    }

    // A double column of the segments table, checked by name and type.
    std::shared_ptr<arrow::DoubleArray> double_column(arrow::RecordBatch const& batch,
                                                      std::string const& name)
    {
        auto column = batch.GetColumnByName(name);
        if (!column) {
            throw std::runtime_error("rasterize: segments table has no column '" + name + "'");
        }
        if (column->type_id() != arrow::Type::DOUBLE || column->null_count() != 0) {
            throw std::runtime_error("rasterize: column '" + name + "' is " +
                                     column->type()->ToString() + " with " +
                                     std::to_string(column->null_count()) +
                                     " nulls; expected non-null double");
        }
        return std::static_pointer_cast<arrow::DoubleArray>(column);
    }

    // Deposit `weight` along the straight segment start -> stop.  The segment is cut
    // at every voxel boundary it crosses and each piece gets a share of the weight
    // proportional to its length; pieces outside the grid are dropped.
    void deposit(Grid const& grid, std::array<double, 3> const& start,
                 std::array<double, 3> const& stop, double weight, Voxels& voxels)
    {
        // Cut points as fractions t of the way from start to stop.
        std::vector<double> cuts{0.0, 1.0};
        for (std::size_t a = 0; a < 3; ++a) {
            double const d = stop[a] - start[a];
            if (d == 0) {
                continue;
            }
            auto const [a0, a1] = std::minmax(start[a], stop[a]);
            // Boundary k sits at lo + k*voxel_size, for k = 0 .. n.
            auto const first = std::max(0.0, std::ceil((a0 - grid.lo[a]) / grid.voxel_size));
            auto const last = std::min(static_cast<double>(grid.n[a]),
                                       std::floor((a1 - grid.lo[a]) / grid.voxel_size));
            for (double k = first; k <= last; k += 1) {
                double const t = (grid.lo[a] + k * grid.voxel_size - start[a]) / d;
                if (t > 0 && t < 1) {
                    cuts.push_back(t);
                }
            }
        }
        std::ranges::sort(cuts);

        // Each piece lies inside one voxel: find it from the piece's midpoint.
        for (std::size_t i = 0; i + 1 < cuts.size(); ++i) {
            double const fraction = cuts[i + 1] - cuts[i];
            if (fraction <= 0) {
                continue; // repeated cut (a corner crossing)
            }
            double const t = 0.5 * (cuts[i] + cuts[i + 1]);
            std::uint64_t key = 0;
            bool inside = true;
            for (std::size_t a = 0; a < 3; ++a) {
                double const x = start[a] + t * (stop[a] - start[a]);
                double const v = std::floor((x - grid.lo[a]) / grid.voxel_size);
                if (!(v >= 0 && v < static_cast<double>(grid.n[a]))) {
                    inside = false;
                    break;
                }
                key = key * grid.n[a] + static_cast<std::uint64_t>(v);
            }
            if (inside) {
                voxels[key] += weight * fraction;
            }
        }
    }

    // Voxelize the segments' `weight` column (eg energy_deposit in MeV) in 3D, then
    // sum along y.  Returns the n_x (drift) by n_z (beam) image flattened row-major:
    // pixel (ix, iz) is element ix * n_z + iz.
    std::vector<double> rasterize(phlex_arrow::TableGroup const& group, Grid const& grid,
                                  std::string const& weight)
    {
        auto const member = group.members.find(edep_sim_phlex::kSegmentsMember);
        if (member == group.members.end() || !member->second) {
            throw std::runtime_error("rasterize: '" + group.type + "' group has no '" +
                                     edep_sim_phlex::kSegmentsMember + "' table");
        }
        auto const& segments_table = member->second;
        edep_arrow::require_readable(
            *segments_table->schema(), edep_arrow::kSegmentsSchema, "rasterize");

        // A table may be split into several chunks; combine them into one record
        // batch so every column is a single contiguous array.
        auto combined = segments_table->CombineChunksToBatch();
        if (!combined.ok()) {
            throw std::runtime_error("rasterize: " + combined.status().ToString());
        }
        auto const& batch = **combined;

        auto const w = double_column(batch, weight);
        std::array const starts{double_column(batch, "start_x"),
                                double_column(batch, "start_y"),
                                double_column(batch, "start_z")};
        std::array const stops{double_column(batch, "stop_x"),
                               double_column(batch, "stop_y"),
                               double_column(batch, "stop_z")};

        Voxels voxels;
        for (std::int64_t row = 0; row < batch.num_rows(); ++row) {
            std::array<double, 3> start{}, stop{};
            for (std::size_t a = 0; a < 3; ++a) {
                start[a] = starts[a]->Value(row);
                stop[a] = stops[a]->Value(row);
            }
            bool const finite = std::ranges::all_of(start, [](double x) { return std::isfinite(x); }) &&
                                std::ranges::all_of(stop, [](double x) { return std::isfinite(x); }) &&
                                std::isfinite(w->Value(row));
            if (!finite) {
                throw std::runtime_error("rasterize: segment " + std::to_string(row) +
                                         " has a non-finite position or " + weight);
            }
            deposit(grid, start, stop, w->Value(row), voxels);
        }

        // Collapse: sum the y voxels of each (x, z) column.
        auto const [nx, ny, nz] = grid.n;
        std::vector<double> image(nx * nz, 0.0);
        for (auto const& [key, value] : voxels) {
            image[key / (ny * nz) * nz + key % nz] += value;
        }
        return image;
    }
    
    
}




PHLEX_REGISTER_ALGORITHMS(m, config)
{
    using namespace phlex;

    // Grid bounds in mm; the defaults are the active volume of pdhd_like.gdml.
    auto const grid = inference::make_grid(
        {config.get<double>("x_min", -3520.9), config.get<double>("y_min", 76.1),
         config.get<double>("z_min", -1.0)},
        {config.get<double>("x_max", 3520.9), config.get<double>("y_max", 6066.7),
         config.get<double>("z_max", 4626.3)},
        config.get<double>("voxel_size", 5.0));
    auto const weight = config.get<std::string>("weight", std::string{"energy_deposit"});

    m.transform(
         "rasterize",
         [grid, weight](phlex_arrow::TableGroup const& group) {
             return inference::rasterize(group, grid, weight);
         },
         concurrency::unlimited)
        .input_family(product_selector{
            .creator = config.get<std::string>("input_creator", std::string{"edep_observables"}),
            .layer = config.get<std::string>("input_layer"),
            .suffix = experimental::identifier{
                config.get<std::string>("input_suffix", std::string{"observables"})}})
        .output_product_suffixes(config.get<std::string>("output_suffix", std::string{"image"}));

    m.transform(
         "image_for_python",
         [](std::vector<double> const& image) {
             return std::make_shared<std::vector<double>>(image);
         },
         concurrency::unlimited)
        .input_family(product_selector{.creator = "rasterize",
                                       .layer = config.get<std::string>("input_layer"),
                                       .suffix = experimental::identifier{"image"}})
        .output_product_suffixes("py_image");
}

