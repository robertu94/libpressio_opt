#include <algorithm>
#include <limits>
#include <chrono>
#include "pressio_search.h"
#include "pressio_search_defines.h"
#include <libdistributed_work_queue.h>
#include <std_compat/memory.h>
#include <libpressio_ext/cpp/distributed_manager.h>

struct nrt_search: public pressio_search_plugin {
  private:
      using task_request_t = std::tuple<size_t, std::vector<double>>;
      using task_response_t = std::tuple<size_t, std::vector<double>, std::vector<double>>;

  public:
    pressio_search_results search(
        std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)> compress_fn,
        distributed::queue::StopToken& token
        ) override {
      pressio_search_results best_results;
      double best_objective;
      switch(mode){
        case pressio_search_mode_min:
          best_objective = std::numeric_limits<pressio_search_results::output_type::value_type>::max();
          break;
        default:
          best_results.status = -1;
          best_results.msg = "only minimization supported for now";
          return best_results;
      }
      if(max_iterations < 3) {
        best_results.status = -2;
        best_results.msg = "at least 3 iterations are required";
        return best_results;
      }

      std::vector<task_request_t> inital_points;
      //TODO modify the following to set your set of initial points
      inital_points.reserve(3);
      inital_points.emplace_back(0, lower_bound);
      inital_points.emplace_back(1, upper_bound);
      inital_points.emplace_back(2, prediction);
      size_t idx = 3;

      auto start_time = std::chrono::system_clock::now();
      auto should_stop = [this, &idx, &token, start_time](){
        auto current_time = std::chrono::system_clock::now();
        return token.stop_requested() ||
          std::chrono::duration_cast<std::chrono::seconds>(current_time-start_time).count() > max_seconds ||
          idx > max_iterations;
      };

      manager.
        work_queue(
            std::begin(inital_points), std::end(inital_points),
            [ &compress_fn](task_request_t const& request) {
              auto const& inputs = std::get<1>(request);
              auto const& id = std::get<0>(request);
              pressio_search_results::output_type result = compress_fn(inputs);
              return task_response_t{id, inputs, result};
            },
            //any variables you want to have preserved from call to call must be declared here like best_results and idx
            [&best_results,&best_objective,&idx,&token, &should_stop, this](
                task_response_t response,
                distributed::queue::TaskManager<task_request_t, MPI_Comm>& task_manager
              ) {
              const auto& id = std::get<0>(response);
              (void)id;
              const auto& inputs = std::get<1>(response);
              const auto& objective = std::get<2>(response).front();
              //update best_results if needed to updated the min
              if(objective < best_objective) {
                best_objective = objective;
                best_results.inputs = inputs;
              }

              //check if we:
              //  - are asked to stop and forward the message to our children
              //  - have gone past our elapsed time
              //  - have evaluated more points than we are allowed to evaluate
              //and stop accordingly, defined on line 45
              if(should_stop()) {
                task_manager.request_stop();
                return;
              }

              //TODO check if we have surpassed our target and can stop
              if(objective < target) {
                task_manager.request_stop();
                token.request_stop();
                return;
              }
              
              //TODO write code to get the next point here

              //if you need to to enqueue a point, always increment idx
              task_request_t request {idx++, inputs};
              task_manager.push(request);
            });

      manager.bcast(best_results.inputs);
      manager.bcast(best_results.output);
      manager.bcast(best_results.status);
      manager.bcast(best_results.msg);
      return best_results;
    }

    //configuration
    pressio_options get_options() const override {
      pressio_options opts;
      
      //need to reconfigure because input size has changed
      set(opts, "opt:prediction", pressio_data(std::begin(prediction), std::end(prediction)));
      set(opts, "opt:lower_bound", pressio_data(std::begin(lower_bound), std::end(lower_bound)));
      set(opts, "opt:upper_bound", pressio_data(std::begin(upper_bound), std::end(upper_bound)));
      set(opts, "opt:max_iterations", max_iterations);
      set(opts, "opt:max_seconds", max_seconds);
      set(opts, "opt:target", target);
      set(opts, "opt:objective_mode", mode);
      opts.copy_from(manager.get_options());
      return opts;
    }
    int set_options(pressio_options const& options) override {
      pressio_data data;
      if(options.get("opt:prediction", &data) == pressio_options_key_set) {
        prediction = data.to_vector<pressio_search_results::input_element_type>();
        if(prediction.size() > 1) return 1;
      }
      if(options.get("opt:lower_bound", &data) == pressio_options_key_set) {
        lower_bound = data.to_vector<pressio_search_results::input_element_type>();
      }
      if(options.get("opt:upper_bound", &data) == pressio_options_key_set) {
        upper_bound = data.to_vector<pressio_search_results::input_element_type>();
      }
      options.get("opt:max_iterations", &max_iterations);
      options.get("opt:max_seconds", &max_seconds);
      options.get("opt:target", &target);
      options.get("opt:objective_mode", &mode);
      manager.set_options(options);
      return 0;
    }

    void set_name_impl(std::string const& new_name) override {
      manager.set_name(new_name);
    }


    
    //meta-data
    /** get the prefix used by this compressor for options */
    const char* prefix() const override {
      return "nrt";
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
      return compat::make_unique<nrt_search>(*this);
    }

private:
    pressio_search_results::input_type prediction;
    pressio_search_results::input_type lower_bound;
    pressio_search_results::input_type upper_bound;
    pressio_search_results::output_type::value_type target;
    unsigned int max_iterations = 100;
    unsigned int max_seconds = std::numeric_limits<unsigned int>::max();
    unsigned int mode = pressio_search_mode_target;
    pressio_distributed_manager manager = pressio_distributed_manager(
        /*max_masters*/1,
        /*max_ranks_per_worker*/1
        );
};


static pressio_register guess_nrt_register(search_plugins(), "nrt", [](){ return compat::make_unique<nrt_search>();});
