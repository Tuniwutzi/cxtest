#include <jtest/jtest.hpp>

#include <algorithm>

int main()
{
    auto sink = jtest::PrintingRunOutputSink{};
    jtest::run_registered_tests(sink);
    return sink.failed ? 1 : 0;
}
