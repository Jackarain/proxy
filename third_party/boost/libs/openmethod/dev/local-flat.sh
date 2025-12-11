mkdir -p flat/boost/openmethod
python3 dev/flatten.py \
  flat/boost/openmethod.hpp \
  include/boost/openmethod.hpp \
  include/boost/openmethod/interop/std_unique_ptr.hpp \
  include/boost/openmethod/interop/std_shared_ptr.hpp \
  include/boost/openmethod/initialize.hpp
python3 dev/flatten.py \
  flat/boost/openmethod/registry.hpp \
  include/boost/openmethod/registry.hpp \
  include/boost/openmethod/policies/*.hpp \
  include/boost/openmethod/default_registry.hpp
