#pragma once

#include <memory>
#include <meta>
#include <optional>
#include <span>

namespace cxtest
{

struct RTContext;
struct CTContext;

namespace detail::execution
{

namespace concepts
{

template<std::meta::info candidate>
concept RTTestFunction = is_function<candidate> && std::is_invocable_r<void, [:candidate:], RTContext&>;
template<std::meta::info candidate>
concept CTTestFunction = is_function<candidate> && std::is_invocable_r<void, [:candidate:], CTContext&>;
template<std::meta::info candidate>
concept TestFunction = RTTestFunction<candidate> || CTTestFunction<candidate>;

} // namespace concepts

struct Executor
{
    consteval virtual std::optional<std::span<const char* const>> execute_compiletime() noexcept = 0;

    // TODO: can we just make this one non-consteval? I don't know if we can mix runtime and compiletime virtual methods
    // in the same interface.
    consteval virtual void (*)(RTContext&) get_runtime_executor() noexcept = 0;
};

template<std::meta::info function>
    requires(concepts::TestFunction<function>)
struct FunctionExecutor : Executor
{
    consteval std::optional<std::span<const char* const>> execute_compiletime() noexcept final
    {
        if constexpr (concepts::CTTestFunction<function>)
        {
            throw "Executing CT test function not yet implemented";
        }
        else
        {
            return std::nullopt;
        }
    }
    consteval void (*)(RTContext&) get_runtime_executor() noexcept final
    {
        if constexpr (concepts::RTTestFunction<function>)
        {
            throw "Creating RT test executor function not yet implemented";
        }
        else
        {
            return std::nullopt;
        }
    }
};

template<std::derived_from<Executor> ExecutorImpl>
consteval std::unique_ptr<Executor> make_executor_ptr()
{
    return std::make_unique<ExecutorImpl>();
}

consteval std::unique_ptr<Executor> make_executor(std::meta::info candidate)
{
    auto refl_candidate = reflect_constant(candidate);
    if (can_substitute(^^FunctionExecutor, {refl_candidate}))
    {
        auto executor_type = substitute(^^FunctionExecutor, {refl_candidate});
        auto maker = substitute(^^make_executor_ptr, {executor_type});
        auto maker_function = extract<std::unique_ptr<Executor> (*)()>(maker);
        return maker_function();
    }

    return nullptr;
}

} // namespace detail::execution

} // namespace cxtest