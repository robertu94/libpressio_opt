#include "pressio_search.h"
libpressio::pressio_registry<std::shared_ptr<pressio_search_plugin>>& search_plugins() {
  static libpressio::pressio_registry<std::shared_ptr<pressio_search_plugin>> registry;
  return registry;
}

