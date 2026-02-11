#include "pressio_search.h"
#include "pressio_search_results.h"
#include "pressio_search_defines.h"
#include <std_compat/memory.h>
#include <random>
#include <atomic>
#include <dlib/matrix/matrix_la.h>
#include <dlib/matrix/matrix_qr.h>
#include <dlib/matrix/matrix.h>
#include <stdexcept>
#include <vector>
#include <iterator>
#include <ostream>
#include <optional>
#include <algorithm>

namespace libpressio_opt { namespace search { namespace hopt {
class poly {
    public:
    poly(poly const& p): coeffs(p.coeffs) {
        if(coeffs.size() == 0) throw std::domain_error("invalid polynomial");
    }
    poly(std::initializer_list<double> args ): coeffs(args) {
        if(coeffs.size() == 0) throw std::domain_error("invalid polynomial");
    }
    template <class... Args>
    poly(Args&&... args): coeffs(std::forward<Args>(args)...) {
        if(coeffs.size() == 0) throw std::domain_error("invalid polynomial");
    }

    size_t size() const { return coeffs.size(); }
    std::vector<double>::iterator begin() { return coeffs.begin(); }
    std::vector<double>::iterator end() { return coeffs.end(); }
    std::vector<double>::const_iterator begin() const { return coeffs.begin(); }
    std::vector<double>::const_iterator end() const { return coeffs.end(); }
    std::vector<double>::const_iterator cbegin() const { return coeffs.cbegin(); }
    std::vector<double>::const_iterator cend() const { return coeffs.cend(); }
    double const& operator[](size_t i) const { return coeffs[i]; }
    double& operator[](size_t i) { return coeffs[i]; }

    static poly fit(std::vector<double> const& x, std::vector<double> const& y, unsigned int degree) {
        if(x.size() != y.size()) throw std::logic_error("size is not equal");
        if(degree < 1) throw std::logic_error("degree 1+ is required");
        const size_t obs = x.size();
        dlib::matrix<double> vand_A;
        dlib::matrix<double,0,1> vand_b;
        vand_A.set_size(obs, degree+1);
        vand_b.set_size(obs);
        for(size_t o = 0; o < obs; ++o) {
            double val = 1.0;
            vand_A(o,1) = val;
            vand_b(o) = y[o];
            for(size_t d = 0; d < degree + 1; ++d) {
                vand_A(o,d) = val;
                val *= x[o];
            }
        }
        auto d_result = dlib::qr_decomposition<decltype(vand_A)>(vand_A).solve(vand_b);

        std::vector<double> d(d_result.begin(), d_result.end());

        return poly{d};
    }
    poly derivative() const {
        size_t const N = coeffs.size();

        if(N == 1) return poly{0};

        std::vector<double> derivatives(coeffs.size() - 1);
        for(size_t i = 1; i < N; ++i) {
            derivatives[i - 1] = coeffs[i]*i;
        }
        return poly{derivatives};
    }

    double eval(double x) const {
        double result = 0.0;
        for (auto it = coeffs.rbegin(); it != coeffs.rend(); ++it) {
            result = result * x + *it;
        }
        return result;
    }

    std::vector<double> real_roots() const {
        //now build the characteristic matrix
        if(coeffs.size() == 1) return {};
        size_t const N = coeffs.size() - 1;
        dlib::matrix<double> characteristic(N, N);
        characteristic = 0;

        for(size_t i = 0; i < (N - 1); ++i) {
            characteristic(i,i+1) = 1;
        }
        for(size_t c = 0; c < N; ++c) {
            characteristic(N - 1, c) = -coeffs[c] / coeffs[N];
        }

        auto eigvals = real_eigenvalues(characteristic);
        std::vector<double> r;
        r.reserve(eigvals.size());
        for(auto const e: eigvals) {
            auto f = eval(e);
            if( f < 1e-3 && f > -1e-3) {
                r.push_back(e);
            }
        }


        return r;

    }




    private:
    enum class improvement { better, worse, same };
    std::vector<double> solve(double lower, double upper, improvement(*is_improvement)(double,double) ) const {
        if(lower > upper) throw std::domain_error("invalid range: lower > upper");

        auto candidates = derivative().real_roots();
        candidates.reserve(candidates.size() + 2);
        candidates.emplace_back(lower);
        candidates.emplace_back(upper);

        std::vector<double> best;
        for(auto c: candidates) {
            if(best.empty()) {
                best.emplace_back(c); 
            } else {
                switch(is_improvement(best.front(), c)) {
                    case improvement::better:
                        best.clear();
                        best.emplace_back(c);
                        break;
                    case improvement::same:
                        best.emplace_back(c);
                        break;
                    case improvement::worse:
                        break;
                }
            }
        }

        return best;
    }
    std::vector<double> coeffs;

    public:
    std::vector<double> max(double lower, double upper) const {
        return solve(lower, upper, [](double prev_best, double candidate) {
                if(candidate > prev_best) return improvement::better; 
                else if(candidate == prev_best) return improvement::same; 
                else return improvement::worse;
            });
    }
    std::vector<double> min(double lower, double upper) const {
        return solve(lower, upper, [](double prev_best, double candidate) {
                if(candidate < prev_best) return improvement::better; 
                else if(candidate == prev_best) return improvement::same; 
                else return improvement::worse;
            });
    }
    std::vector<double> solve(double target) const {
        poly p = *this;
        p.coeffs.front() -= target;
        return p.real_roots();
    }
    std::vector<double> solve(double target, double lower, double upper) const {
        auto candidates = solve(target);
        candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [lower, upper](double x) { return !(lower < x && x < upper);}), candidates.end());
            return candidates;
    }
};
template<class CharT, class Traits>
std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& out, poly const& p) {
    out << '[';
    for(auto const& c: p) {
        out << c << ", ";
    }
    return out << ']';
}

struct hopt_search: public pressio_search_plugin {
  public:

    pressio_search_results search(compat::span<const pressio_data *const> const &input_datas,
                                  std::function<pressio_search_results::output_type(
                                          pressio_search_results::input_type const &)> compress_fn,
                                  distributed::queue::StopToken & token) override {
      const pressio_search_results::output_type worst_result(1, std::numeric_limits<double>::max());
      pressio_search_results results{};
      pressio_search_results::input_type candidate(lower_bound.size());
      std::vector<pressio_search_results::input_type> stage_1_input(max_iter_stage1);
      std::vector<pressio_search_results::output_type> stage_1_result(max_iter_stage1, worst_result);
      std::vector<pressio_search_results::input_type> stage_2_input(max_iter_stage2);
      std::vector<pressio_search_results::output_type> stage_2_result(max_iter_stage2, worst_result);
      std::atomic<int> early_terminate = 0;

      std::seed_seq seed{seed_value};
      std::mt19937 gen(seed);
      auto start_time = std::chrono::system_clock::now();
      std::atomic<int> time_limit_exceeded = 0;
      auto good_enough = [this,&token,&time_limit_exceeded, start_time](pressio_search_results::output_type const& result) {
          if(token.stop_requested()) return true;
          if(!target) return false;
          auto current_time = std::chrono::system_clock::now();
          if(std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count() > max_seconds) {
              time_limit_exceeded.fetch_or(1);
              return true;
          }
          switch(mode) {
              case pressio_search_mode::pressio_search_mode_max:
                  if(result.at(0) > *target) {
                      token.request_stop();
                      return true;
                  }
                  break;
              case pressio_search_mode::pressio_search_mode_min:
                  if(result.at(0) < *target) {
                      token.request_stop();
                      return true;
                  }
                  break;
              case pressio_search_mode::pressio_search_mode_target:
                  if(*target*(1-global_rel_tolerance) < result.at(0) && result.at(0) < *target*(1+global_rel_tolerance)) {
                      token.request_stop();
                      return true;
                  }
                  break;
              default:
                  break;
          }
          return false;
      };

      auto sample = [this,&gen] {
          pressio_search_results::input_type v(lower_bound.size());
          for(size_t i = 0; i < lower_bound.size(); ++i) {
              std::uniform_real_distribution<double> dist(lower_bound[i], upper_bound[i]);
              v[i] = dist(gen);
          }
          return v;
      };
      stage_1_input[0] = lower_bound;
      stage_1_input[1] = upper_bound;
      for(size_t i = 2; i < max_iter_stage1; ++i) {
          stage_1_input[i] = sample();
      }

      results.status = 0;
      results.msg = "";

#pragma omp parallel num_threads(nthreads)
      {
          //first sample max_iter_stage1 points
#pragma omp for schedule(dynamic)
          for(size_t i = 0; i < max_iter_stage1; ++i) {
            if(!early_terminate.load()) {
                stage_1_result[i] = compress_fn(stage_1_input[i]);
                if (good_enough(stage_1_result[i])) {
                    early_terminate |= 1;
                }
            }
          }

#pragma omp master
      {
          if(!early_terminate.load()) {
              std::vector<double> x,y;
              std::transform(stage_1_input.begin(), stage_1_input.end(), std::back_inserter(x), [](auto v){ return v.front();} );
              std::transform(stage_1_result.begin(), stage_1_result.end(), std::back_inserter(y), [](auto v){ return v.front();} );
              
              auto p = poly::fit(x, y, degree.value_or(max_iter_stage1 - 1));

              double candidate;
              switch(mode) {
                  case pressio_search_mode_min:
                      candidate = p.min(lower_bound.front(), upper_bound.front()).front();
                      break;
                  case pressio_search_mode_max:
                      candidate = p.max(lower_bound.front(), upper_bound.front()).front();
                      break;
                  case pressio_search_mode_target:
                      {
                          auto solution = p.solve(*target, lower_bound.front(), upper_bound.front());
                          if(solution.empty()) {
                            early_terminate.fetch_or(1);
                            results.status = 1;
                            results.msg = "failed to find solution";
                          } else {
                              candidate = *std::max_element(solution.begin(), solution.end(), [&p, this](double prev_best, double candidate){
                                          return std::abs(p.eval(prev_best) - *target) < std::abs(p.eval(candidate) - *target);
                                      });
                          }
                      }
                      break;
                  default:
                      break;
              }

              if(!early_terminate.load()) {
              auto sample_near_candiate = [this,&gen,&candidate] {
                  pressio_search_results::input_type v(lower_bound.size());
                  std::uniform_real_distribution<double> dist((1.0-epsilon)*candidate, (1.0+epsilon)*candidate);
                  v[0] =  dist(gen);
                  return v;
              };

              for(size_t i = 0; i < max_iter_stage2; ++i) {
                  stage_2_input[i] = sample_near_candiate();
              }
              }
          }
      }

#pragma omp barrier


#pragma omp for schedule(dynamic)
          for(size_t i = 0; i < max_iter_stage2; ++i) {
            if(!early_terminate.load()) {
                stage_2_result[i] = compress_fn(stage_2_input[i]);
                if (good_enough(stage_2_result[i])) {
                    early_terminate |= 1;
                }
            }
          }
      }


      auto is_best = [&](pressio_search_results::output_type const& c) {
          auto loss = [this](double value) {
                auto clamp = [](double value, double low, double high) {
                  assert(low < high);
                  if(value < low) return low;
                  if(high < value) return high;
                  return value;
                };
                return clamp(pow((*target-value),2),
                      std::numeric_limits<double>::min() * 1e-10,
                      std::numeric_limits<double>::max() * 1e-10
                      );
          };
          switch(mode) {
              case pressio_search_mode_min:
                  return c.at(0) < results.output.at(0);
              case pressio_search_mode_max:
                  return c.at(0) > results.output.at(0);
              case pressio_search_mode_target:
                  return loss(c.at(0)) < loss(results.output.at(0));
              default:
                  return false;
          }
      };
      results.inputs = stage_1_input.front();
      results.output = stage_1_result.front();
      for(size_t i = 0; i < max_iter_stage1; ++i) {
          if(is_best(stage_1_result[i])) {
                  results.inputs = stage_1_input[i];
                  results.output = stage_1_result[i];
          }
      }
      for(size_t i = 0; i < max_iter_stage2; ++i) {
          if(is_best(stage_2_result[i])) {
                  results.inputs = stage_2_input[i];
                  results.output = stage_2_result[i];
          }
      }

      return results;
    }

    //configuration
    pressio_options get_options() const override {
      pressio_options opts;
      set(opts, "opt:lower_bound", pressio_data(lower_bound.begin(), lower_bound.end()));
      set(opts, "opt:upper_bound", pressio_data(upper_bound.begin(), upper_bound.end()));
      set(opts, "opt:max_seconds", max_seconds);
      set(opts, "opt:objective_mode", mode);
      set(opts, "opt:target", target);
      set(opts, "opt:global_rel_tolerance", global_rel_tolerance);
      set(opts, "hopt:max_iter_stage1", max_iter_stage1);
      set(opts, "hopt:degree", degree);
      set(opts, "hopt:max_iter_stage2", max_iter_stage2);
      set(opts, "hopt:seed", seed_value);
      set(opts, "hopt:nthreads", nthreads);
      set(opts, "hopt:epsilon", epsilon);
      return opts;
    }
    pressio_options get_documentation() const override {
      pressio_options opts;
      return opts;
    }
    pressio_options get_configuration_impl() const override {
      pressio_options opts;
      return opts;
    }

    int set_options(pressio_options const& options) override {
        {
            std::vector<std::string> tmp;
            if(get(options, "opt:inputs", &tmp) == pressio_options_key_set) {
                if(tmp.size() > 1) {
                    return set_error(1, "hopt does not support multiple inputs for now");
                }
            }
        }
      {
          pressio_data tmp;
          if(get(options, "opt:lower_bound", &tmp) == pressio_options_key_set) {
              lower_bound = tmp.to_vector<double>();
          }
          if(get(options, "opt:upper_bound", &tmp) == pressio_options_key_set) {
              upper_bound = tmp.to_vector<double>();
          }
      }
      get(options, "opt:max_seconds", &max_seconds);
      get(options, "opt:objective_mode", &mode);
      get(options, "opt:target", &target);
      get(options, "opt:global_rel_tolerance", &global_rel_tolerance);

      get(options, "hopt:max_iter_stage1", &max_iter_stage1);
      get(options, "hopt:max_iter_stage2", &max_iter_stage2);
      get(options, "hopt:degree", &degree);
      get(options, "hopt:seed", &seed_value);
      get(options, "hopt:nthreads", &nthreads);
      get(options, "hopt:epsilon", &epsilon);
      get(options, "opt:thread_safe", &thread_safe);
      return 0;
    }
    
    const char* prefix() const override {
      return "hopt";
    }
    const char* version() const override {
      return "0.0.1";
    }
    int major_version() const override { return 0; }
    int minor_version() const override { return 0; }
    int patch_version() const override { return 1; }

    std::shared_ptr<pressio_search_plugin> clone() override {
      return compat::make_unique<hopt_search>(*this);
    }
private:

    pressio_search_results::input_type lower_bound{};
    pressio_search_results::input_type upper_bound{};
    compat::optional<pressio_search_results::output_type::value_type> target{};
    uint32_t max_iter_stage1 = 5;
    compat::optional<unsigned int> degree;
    uint32_t max_iter_stage2 = 5;
    uint64_t max_seconds = std::numeric_limits<uint64_t>::max();
    unsigned int mode = pressio_search_mode_max;
    uint64_t seed_value = 0;
    uint32_t nthreads = 1;
    double global_rel_tolerance = .1;
    double epsilon = .2;
    int thread_safe = 0;
};


pressio_register libpressio_io_hopt_register(search_plugins(), "hopt", [](){ return compat::make_unique<hopt_search>();});
}}

}

