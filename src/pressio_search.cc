#include "pressio_search.h"
namespace libpressio_opt { namespace search {
pressio_registry<std::shared_ptr<pressio_search_plugin>>& search_plugins() {
  static pressio_registry<std::shared_ptr<pressio_search_plugin>> registry;
  return registry;
}
}}

