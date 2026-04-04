#include <cxtest/cxtest.hpp>

#include <algorithm>

int main()
{
    auto sink = cxtest::PrintingRunOutputSink{};
    cxtest::run_registered_tests(sink);
    return sink.failed ? 1 : 0;
}
