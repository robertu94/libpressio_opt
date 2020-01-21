#include "pressio_search_metrics.h"

pressio_registry<std::shared_ptr<pressio_search_metrics_plugin>>& search_metrics_plugins() {
  static pressio_registry<std::shared_ptr<pressio_search_metrics_plugin>> registry;
  return registry;
}
