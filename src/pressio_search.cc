#include "pressio_search.h"
pressio_registry<std::shared_ptr<pressio_search_plugin>>& search_plugins() {
  static pressio_registry<std::shared_ptr<pressio_search_plugin>> registry;
  return registry;
}

