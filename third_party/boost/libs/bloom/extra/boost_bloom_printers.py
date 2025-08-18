# Copyright 2025 Braden Ganetsky
# Distributed under the Boost Software License, Version 1.0.
# https://www.boost.org/LICENSE_1_0.txt

import gdb.printing
import gdb.xmethod

class BoostBloomFilterPrinter:
    def __init__(self, val):
        self.void_pointer = gdb.lookup_type("void").pointer()
        nullptr = gdb.Value(0).cast(self.void_pointer)

        has_array = val["ar"]["data"] != nullptr

        if has_array:
            stride = int(val["stride"])
            used_value_size = int(val["used_value_size"])
            self.array_size = int(val["hs"]["rng"]) * stride + (used_value_size - stride)
        else:
            self.array_size = 0
        self.capacity = self.array_size * 8
        if has_array:
            self.data = val["ar"]["array"]
        else:
            self.data = nullptr

    def to_string(self):
        return f"boost::bloom::filter with {{capacity = {self.capacity}, data = {self.data.cast(self.void_pointer)}, size = {self.array_size}}}"

    def display_hint(self):
        return "map"
    def children(self):
        def generator():
            data = self.data
            for i in range(self.array_size):
                yield "", f"{i}"
                yield "", data.dereference()
                data = data + 1
        return generator()

def boost_bloom_build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("boost_bloom")
    add_template_printer = lambda name, printer: pp.add_printer(name, f"^{name}<.*>$", printer)

    add_template_printer("boost::bloom::filter", BoostBloomFilterPrinter)

    return pp

gdb.printing.register_pretty_printer(gdb.current_objfile(), boost_bloom_build_pretty_printer())

# https://sourceware.org/gdb/current/onlinedocs/gdb.html/Writing-an-Xmethod.html
class BoostBloomFilterSubscriptMethod(gdb.xmethod.XMethod):
    def __init__(self):
        gdb.xmethod.XMethod.__init__(self, 'subscript')

    def get_worker(self, method_name):
        if method_name == 'operator[]':
            return BoostBloomFilterSubscriptWorker()

class BoostBloomFilterSubscriptWorker(gdb.xmethod.XMethodWorker):
    def get_arg_types(self):
        return [gdb.lookup_type('std::size_t')]

    def get_result_type(self, obj):
        return gdb.lookup_type('unsigned char')

    def __call__(self, obj, index):
        fp = BoostBloomFilterPrinter(obj)
        if fp.array_size == 0:
            print('Error: Filter is null')
            return
        elif index < 0 or index >= fp.array_size:
            print('Error: Out of bounds')
            return
        else:
            data = fp.data
            return (data + index).dereference()

class BoostBloomFilterMatcher(gdb.xmethod.XMethodMatcher):
    def __init__(self):
        gdb.xmethod.XMethodMatcher.__init__(self, 'BoostBloomFilterMatcher')
        self.methods = [BoostBloomFilterSubscriptMethod()]

    def match(self, class_type, method_name):
        if not class_type.tag.startswith('boost::bloom::filter<'):
            return None

        workers = []
        for method in self.methods:
            if method.enabled:
                worker = method.get_worker(method_name)
                if worker:
                    workers.append(worker)
        return workers

gdb.xmethod.register_xmethod_matcher(None, BoostBloomFilterMatcher())
