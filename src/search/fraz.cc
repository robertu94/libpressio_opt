#include <algorithm>
#include <limits>
#include "dlib/global_optimization/find_max_global.h"
#include "pressio_search.h"
#include "pressio_search_defines.h"
#include "pressio_search_results.h"
#include <std_compat/memory.h>

namespace {
    auto clamp(double value, double low, double high) {
      assert(low < high);
      if(value < low) return low;
      if(high < value) return high;
      return value;
    }
    auto loss(double target, double actual){
      return clamp(pow((target-actual),2),
          std::numeric_limits<double>::min() * 1e-10,
          std::numeric_limits<double>::max() * 1e-10
          );
    }
    auto vector_to_dlib(pressio_search_results::input_type const& input) {
      dlib::matrix<double,0,1> output(input.size());
      std::copy(std::begin(input), std::end(input), std::begin(output));

      return output;
    }
    template <class ForwardIt>
    auto iter_to_dlib(ForwardIt begin, ForwardIt end) {
      dlib::matrix<double,0,1> output(std::distance(begin, end));
      std::copy(begin, end, std::begin(output));
      return output;
    }
    auto dlib_to_vector(dlib::matrix<double,0,1> const& input) {
      pressio_search_results::input_type output(input.begin(), input.end());
      return output;
    }

    std::vector<dlib::function_evaluation> data_to_evaluations(pressio_data const& data, const size_t n_inputs) {
      std::vector<dlib::function_evaluation> evaluations;
      //if there are no evaluations, skip
      if(data.num_dimensions() == 0 || (not data.has_data())) {
        return evaluations;
      }

      if(data.num_dimensions() != 2) {
        std::ostringstream err;
        err << "invalid_dimensions: should be 2d but is " << data.num_dimensions();
        throw std::runtime_error(err.str());
      }
      const size_t width = n_inputs + 1;
      if(data.get_dimension(0) != width) {
        std::ostringstream err;
        err << "invalid_dimensions: dim[0] should be "  << width << " but is " << data.get_dimension(0);
        throw std::runtime_error(err.str());
      }
      const double* ptr = static_cast<double*>(data.data());
      for (size_t i = 0; i < data.get_dimension(1); ++i) {
        evaluations.emplace_back(iter_to_dlib(ptr+(i*width), ptr+(i*width)+n_inputs), ptr[i*width+n_inputs]);
      }


      return evaluations;
    }
}

struct fraz_search: public pressio_search_plugin {
  public:
    pressio_search_results search(
        std::function<pressio_search_results::output_type(pressio_search_results::input_type const&)> compress_fn,
        distributed::queue::StopToken& token
        ) override {

      pressio_search_results results;
      dlib::function_evaluation best_result;
      std::map<pressio_search_results::input_type, pressio_search_results::output_type> cache;
      dlib::thread_pool pool((thread_safe) ? (nthreads): (1));
      std::vector<dlib::function_evaluation> evaluations;
      try{
        evaluations = data_to_evaluations(evaluations_data, lower_bound.size());
      } catch(std::runtime_error const& err) {
        results.msg = err.what();
        results.status = -1;
        return results;
      }

      std::vector<bool> is_integral;
      if(is_integral_config.empty()) {
        //use a default or all double
        is_integral = std::vector<bool>(lower_bound.size(), false);

      } else {
        //use the actual is_integral value
        is_integral = is_integral_config;
      }


      switch(mode) {
        case pressio_search_mode_target:
          {
            auto threshold = std::min(
                loss(*target, *target*(1-global_rel_tolerance)),
                loss(*target, *target*(1+global_rel_tolerance))
            );
            auto should_stop = [threshold,&token,this](double value) {
              bool target_achived = value < threshold;
              if (target_achived) token.request_stop();
              return target_achived || (inter_iteration && token.stop_requested());
            };

            auto fraz = [&cache, &compress_fn, this](dlib::matrix<double,0,1> const& input){
              auto const vec = dlib_to_vector(input);
              auto const result = compress_fn(vec);
              cache[vec] = result;
              return loss(*target, result.front());
            };
            bool skip = false;
            best_result.y = std::numeric_limits<double>::max();
            for (auto& eval : evaluations) {
              //transform to optimization domain
              eval.y = loss(*target, eval.y);

              //cache the value
              cache[dlib_to_vector(eval.x)] = {eval.y};

              //check if we should stop because of evaluations
              if(should_stop(eval.y)) {
                skip = true;
              }

              //record the best result in the evaluations
              if(eval.y < best_result.y) {
                best_result = eval;
              }
            }

            if(!skip) {
              best_result = dlib::find_min_global(
                  pool,
                  fraz,
                  vector_to_dlib(lower_bound),
                  vector_to_dlib(upper_bound),
                  is_integral,
                  dlib::max_function_calls(max_iterations),
                  std::chrono::seconds(max_seconds),
                  local_tolerance,
                  evaluations,
                  dlib::stop_condition(should_stop)
                  );
            }
            break;
          }
        case pressio_search_mode_max:
        case pressio_search_mode_min:
          {
            auto fraz = [&cache, &compress_fn](dlib::matrix<double,0,1> const& input){
              auto const vec = dlib_to_vector(input);
              auto const result = compress_fn(vec);
              cache[vec] = result;
              return clamp(result.front(),
                std::numeric_limits<double>::min() * 1e-10,
                std::numeric_limits<double>::max() * 1e-10
              );
            };
            if(mode == pressio_search_mode_min) {
            auto should_stop = [&token, this](double value) {
              bool target_achived = (target && value < *target);
              if (target_achived) token.request_stop();
              return (inter_iteration && token.stop_requested()) || target_achived;
            };

            best_result.y = std::numeric_limits<double>::lowest();
            bool skip = false;
            for (auto& eval : evaluations) {
              //put the value into the cache
              cache[dlib_to_vector(eval.x)] = {eval.y};

              //check if we should stop early based just on evaluations
              if(should_stop(eval.y)) {
                skip = true;
              }

              //record the best result
              if(eval.y < best_result.y) {
                best_result = eval;
              }
            }

            if(!skip) {
              best_result = dlib::find_min_global(
                  pool,
                  fraz,
                  vector_to_dlib(lower_bound),
                  vector_to_dlib(upper_bound),
                  is_integral,
                  dlib::max_function_calls(max_iterations),
                  std::chrono::seconds(max_seconds),
                  local_tolerance,
                  evaluations,
                  dlib::stop_condition(should_stop)
                  );
            }
            } else {
            auto should_stop = [&token, this](double value) {
              bool target_achived = (target && value > *target);
              if (target_achived) token.request_stop();
              return (inter_iteration && token.stop_requested())|| target_achived;
            };
            best_result.y = std::numeric_limits<double>::max();
            bool skip = false;

            for (auto& eval : evaluations) {
              //put the value into the cache
              cache[dlib_to_vector(eval.x)] = {eval.y};

              //check if we should stop early based just on evaluations
              if(should_stop(eval.y)) {
                skip = true;
              }

              //record the best result
              if(eval.y > best_result.y) {
                best_result = eval;
              }
            }

            if(!skip) {
              best_result = dlib::find_max_global(
                  pool,
                  fraz,
                  vector_to_dlib(lower_bound),
                  vector_to_dlib(upper_bound),
                  is_integral,
                  dlib::max_function_calls(max_iterations),
                  std::chrono::seconds(max_seconds),
                  local_tolerance,
                  evaluations,
                  dlib::stop_condition(should_stop)
                  );
              }
            }
            break;
          }
      }

      results.inputs = dlib_to_vector(best_result.x);
      results.output = cache[results.inputs];
      results.status = 0;

      return results;
    }

    //configuration
    pressio_options get_options() const override {
      pressio_options opts;

      //need to reconfigure because input size has changed
      set(opts, "opt:lower_bound", pressio_data(std::begin(lower_bound), std::end(lower_bound)));
      set(opts, "opt:upper_bound", pressio_data(std::begin(upper_bound), std::end(upper_bound)));

      std::vector<uint8_t> is_integral(is_integral_config.begin(), is_integral_config.end());
      set(opts, "opt:is_integral", pressio_data(std::begin(is_integral), std::end(is_integral)));

      set(opts, "opt:max_iterations", max_iterations);
      set(opts, "opt:max_seconds", max_seconds);
      set(opts, "opt:global_rel_tolerance", global_rel_tolerance);
      set(opts, "opt:local_rel_tolerance", local_tolerance);
      set(opts, "opt:target", target);
      set(opts, "opt:objective_mode", mode);
      set(opts, "fraz:nthreads", nthreads);
      set(opts, "opt:evaluations", evaluations_data);
      set(opts, "opt:inter_iteration", inter_iteration);
      return opts;
    }
    int set_options(pressio_options const& options) override {
      pressio_data data;
      if(get(options, "opt:lower_bound", &data) == pressio_options_key_set) {
        lower_bound = data.to_vector<pressio_search_results::input_element_type>();
      }
      if(get(options, "opt:upper_bound", &data) == pressio_options_key_set) {
        upper_bound = data.to_vector<pressio_search_results::input_element_type>();
      }
      if(get(options, "opt:is_integral", &data) == pressio_options_key_set) {
        auto is_integral_u8 = data.to_vector<uint8_t>();
        is_integral_config = std::vector<bool>(
            is_integral_u8.begin(),
            is_integral_u8.end()
            );
      }
      get(options, "opt:max_iterations", &max_iterations);
      get(options, "opt:max_seconds", &max_seconds);
      get(options, "opt:global_rel_tolerance", &global_rel_tolerance);
      get(options, "opt:local_rel_tolerance", &local_tolerance);
      get(options, "opt:target", &target);
      get(options, "opt:thread_safe", &thread_safe);
      get(options, "fraz:nthreads", &nthreads);
      get(options, "opt:objective_mode", &mode);
      get(options, "opt:evaluations", &evaluations_data);
      get(options, "opt:inter_iteration", &inter_iteration);

      return 0;
    }
    
    //meta-data
    /** get the prefix used by this compressor for options */
    const char* prefix() const override {
      return "fraz";
    }

    /** get a version string for the compressor
     * \see pressio_compressor_version for the semantics this function should obey
     */
    const char* version() const override {
      return "0.0.3";
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
    int patch_version() const override { return 2; }

    std::shared_ptr<pressio_search_plugin> clone() override {
      return compat::make_unique<fraz_search>(*this);
    }

private:

    pressio_search_results::input_type lower_bound{};
    pressio_search_results::input_type upper_bound{};
    std::vector<bool> is_integral_config{};
    compat::optional<pressio_search_results::output_type::value_type> target{};
    pressio_data evaluations_data;
    double local_tolerance = .01;
    double global_rel_tolerance = .1;
    unsigned int max_iterations = 100;
    unsigned int max_seconds = std::numeric_limits<unsigned int>::max();
    unsigned int mode = pressio_search_mode_target;
    unsigned int nthreads = 1;
    uint32_t inter_iteration = 1;
    int thread_safe = 0;
};


static pressio_register fraz_register(search_plugins(), "fraz", [](){ return compat::make_unique<fraz_search>();});
