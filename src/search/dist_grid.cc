#include "pressio_search.h"
#include "pressio_search_results.h"
#include "pressio_search_defines.h"
#include "libpressio_opt_ext/impl/pressio_data_utilities.h"
#include <libdistributed_work_queue.h>

namespace {
  auto loss(pressio_search_results::objective_type target, pressio_search_results::objective_type actual) {
    return std::abs(target-actual);
  }
}

struct dist_gridsearch_search: public pressio_search_plugin {
  public:

    dist_gridsearch_search() {
      search_method = search_plugins().build(search_method_str);
    }

    pressio_search_results search(std::function<pressio_search_results::objective_type(pressio_search_results::input_type const&)> compress_fn) override {
      pressio_search_results results;
      switch(mode){
        case pressio_search_mode_max:
          results.objective = std::numeric_limits<pressio_search_results::objective_type>::lowest();
          break;
        case pressio_search_mode_min:
          results.objective = std::numeric_limits<pressio_search_results::objective_type>::max();
          break;
        case pressio_search_mode_target:
          results.objective =
            std::numeric_limits<pressio_search_results::objective_type>::max();
          break;
      }

      auto tasks = build_task_list();

      distributed::queue::work_queue(
          parent_comm,
          std::begin(tasks), std::end(tasks),
          [this, compress_fn](task_request_t const& task) {
            //set lower and upper bounds
            auto grid_lower = lower_bound;
            auto grid_upper = upper_bound;
            grid_lower.front() = std::get<0>(task).front();
            grid_upper.front() = std::get<1>(task).front();

            pressio_options options;
            options.set("opt:lower_bound", vector_to_owning_pressio_data(grid_lower));
            options.set("opt:upper_bound", vector_to_owning_pressio_data(grid_upper));
            search_method->set_options(options);

            auto grid_result = search_method->search(compress_fn);
            return task_response_t{grid_result.objective, grid_result.status, grid_result.inputs};
          },
          [this, &results](task_response_t response) {
            auto const& actual = std::get<0>(response);
            auto const& status = std::get<1>(response);
            auto const& inputs = std::get<2>(response);
            if(status == 0) {
              switch(mode){
                case pressio_search_mode_max:
                  results.objective = std::max(results.objective, actual);
                  results.inputs = inputs;
                case pressio_search_mode_min:
                  results.objective = std::min(results.objective, actual);
                  results.inputs = inputs;
                case pressio_search_mode_target:
                  if(loss(target, actual) < loss(target, results.objective)) {
                    results.objective = actual;
                    results.inputs = inputs;
                  }
              }
            }

          }
          );
      return results;
    }

    //configuration
    pressio_options get_options(pressio_options const& opt_module_settings) const override {
      pressio_options opts;
      std::vector<std::string> inputs;
      opt_module_settings.get("opt:inputs", &inputs);
      
      //need to reconfigure because input size has changed
      if(inputs.size() != lower_bound.size()) {
        opts.set("opt:lower_bound",  pressio_data::empty(pressio_double_dtype, {inputs.size()}));
        opts.set("opt:upper_bound",  pressio_data::empty(pressio_double_dtype, {inputs.size()}));
        opts.set("dist_gridsearch:num_bins",  pressio_data::empty(pressio_int32_dtype, {inputs.size()}));
        opts.set("dist_gridsearch:overlap_percentage",  pressio_data::empty(pressio_double_dtype, {inputs.size()}));
      } else {
        opts.set("opt:lower_bound", vector_to_owning_pressio_data(lower_bound));
        opts.set("opt:upper_bound", vector_to_owning_pressio_data(upper_bound));
        opts.set("dist_gridsearch:num_bins", vector_to_owning_pressio_data(num_bins));
        opts.set("dist_gridsearch:overlap_percentage", vector_to_owning_pressio_data(overlap_percentage));
      }
      opts.set("opt:target", target);
      opts.set("dist_gridsearch:comm", (void*)parent_comm);
      opts.set("dist_gridsearch:search", search_method_str);

      //get options from child search_method
      auto method_options = search_method->get_options(opt_module_settings);
      for (auto const& options : method_options) {
        opts.set(options.first, options.second);
      }
      return opts;
    }

    int set_options(pressio_options const& options) override {
      pressio_data data;
      if(options.get("opt:lower_bound", &data) == pressio_options_key_set) {
        lower_bound = pressio_data_to_vector<pressio_search_results::input_element_type>(data);
      }
      if(options.get("opt:upper_bound", &data) == pressio_options_key_set) {
        upper_bound = pressio_data_to_vector<pressio_search_results::input_element_type>(data);
      }
      if(options.get("dist_gridsearch:num_bins", &data) == pressio_options_key_set) {
        num_bins = pressio_data_to_vector<size_t>(data);
      }
      if(options.get("dist_gridsearch:overlap_percentage", &data) == pressio_options_key_set) {
        overlap_percentage = pressio_data_to_vector<double>(data);
      }
      options.get("opt:target", &target);
      options.get("dist_gridsearch:comm", (void**)&parent_comm);
      std::string tmp_search_method;
      if(options.get("dist_gridsearch:search", &tmp_search_method) == pressio_options_key_set) {
        if(tmp_search_method != search_method_str) {
          search_method_str = tmp_search_method;
          search_method = search_plugins().build(search_method_str);
        }
      }
      search_method->set_options(options);
      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    const char* prefix() const override {
      return "dist_gridsearch";
    }

    /** get a version string for the compressor
     * \see pressio_compressor_version for the semantics this function should obey
     */
    const char* version() const override {
      return "0.0.1";
    }
    /** get the major version, default version returns 0
     * \see pressio_compressor_major_version for the semantics this function should obey
     */
    int major_version() const override { return 0; }
    /** get the minor version, default version returns 0
     * \see pressio_compressor_minor_version for the semantics this function should obey
     */
    int minor_version() const override { return 0; }
    /** get the patch version, default version returns 0
     * \see pressio_compressor_patch_version for the semantics this function should obey
     */
    int patch_version() const override { return 1; }

    std::shared_ptr<pressio_search_plugin> clone() override {
      auto tmp = compat::make_unique<dist_gridsearch_search>();
      tmp->lower_bound = lower_bound;
      tmp->upper_bound = upper_bound;
      tmp->search_method_str = search_method_str;
      tmp->search_method = search_method->clone();
      return tmp;
    }

private:
    using task_request_t = std::tuple<std::vector<double>, std::vector<double>>; //<0>=lower_bound, <1>=upper_bound
    using task_response_t  = std::tuple<double, int, std::vector<double>>; //<0> multi-objective <1> status <2> best_input

    std::vector<task_request_t> build_task_list() {
      std::vector<task_request_t> tasks;
      //for now just build the task list based on the first element.
      //we need improvements to LibDistributed before we can do more than this
      //in a general way.
      std::vector<double> step(lower_bound.size());
      std::vector<double> overlap(lower_bound.size());
      for (size_t dim = 0; dim < lower_bound.size(); ++dim) {
        step[dim] = (upper_bound[dim] - lower_bound[dim]) / static_cast<double>(num_bins[dim]);
        overlap[dim] = overlap_percentage[dim] * step[dim];
      }

      //be sure to include overlap_percentage % overlap between the bins
      //to ensure sufficient stationary points at the end point

      std::vector<size_t> bin(lower_bound.size(), 0);
      bool done = false;
      size_t idx = 0;
      size_t configs = 0;
      int rank;
      while(!done) {

        std::vector<double> grid_lower(lower_bound.size());
        std::vector<double> grid_upper(lower_bound.size());
        for (size_t dim = 0; dim < lower_bound.size(); ++dim) {
          grid_lower[dim] = std::max(lower_bound[dim], lower_bound[dim] + step[dim] * bin[dim] - overlap[dim]);
          grid_upper[dim] = std::min(upper_bound[dim], lower_bound[dim] + step[dim] * static_cast<double>(bin[dim] + 1) + overlap[dim]);
        }
        tasks.emplace_back(
            grid_lower,
            grid_upper
        );

        bool updating = true;
        while(updating) {
          ++bin[idx];
          if(bin[idx] == num_bins[idx]) {
            bin[idx] = 0;
            idx++;
            if(idx == num_bins.size()) {
              done = true;
            }
            updating = true;
          } else {
            updating = false;
          }
        }
        idx = 0;
      }
      return tasks;
    }


    std::vector<double> overlap_percentage;
    pressio_search_results::input_type lower_bound{};
    pressio_search_results::input_type upper_bound{};
    std::vector<size_t> num_bins{};
    std::string search_method_str = "guess";
    pressio_search search_method;
    MPI_Comm parent_comm = MPI_COMM_WORLD;
    unsigned int mode = pressio_search_mode_target;
    pressio_search_results::objective_type target;
};


static pressio_register X(search_plugins(), "dist_gridsearch", [](){ return compat::make_unique<dist_gridsearch_search>();});
