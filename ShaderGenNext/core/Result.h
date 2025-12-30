#pragma once

#include <string>
#include <variant>
#include <optional>
#include <functional>

namespace hgl::shader_next {

/**
 * Error 类 - 表示操作失败的错误信息
 */
class Error {
    std::string message_;
    int code_;
    
public:
    Error(std::string msg, int code = -1) 
        : message_(std::move(msg)), code_(code) {}
    
    const std::string& message() const { return message_; }
    int code() const { return code_; }
};

/**
 * Result<T, E> - 现代化的错误处理模式
 * 
 * 类似 Rust 的 Result<T, E>，避免异常，强制错误处理
 * 
 * 使用示例：
 * ```cpp
 * Result<CompiledShader, Error> result = compiler.compile(source);
 * 
 * if (result.isOk()) {
 *     auto shader = result.unwrap();
 *     // 使用 shader
 * } else {
 *     LogError("Compilation failed: {}", result.error().message());
 * }
 * 
 * // 或使用链式操作
 * result
 *     .map([](auto shader) { return optimize(shader); })
 *     .andThen([](auto optimized) { return cache.store(optimized); })
 *     .unwrapOr(fallbackShader);
 * ```
 */
template<typename T, typename E = Error>
class Result {
private:
    std::variant<T, E> data_;
    
public:
    // 构造函数
    static Result Ok(T value) {
        Result r;
        r.data_ = std::move(value);
        return r;
    }
    
    static Result Err(E error) {
        Result r;
        r.data_ = std::move(error);
        return r;
    }
    
    // 查询状态
    bool isOk() const { return std::holds_alternative<T>(data_); }
    bool isError() const { return std::holds_alternative<E>(data_); }
    
    // 获取值（会抛出异常如果是错误）
    T& unwrap() {
        if (isError()) {
            throw std::runtime_error("Called unwrap() on Error value: " + error().message());
        }
        return std::get<T>(data_);
    }
    
    const T& unwrap() const {
        if (isError()) {
            throw std::runtime_error("Called unwrap() on Error value: " + error().message());
        }
        return std::get<T>(data_);
    }
    
    // 获取值或返回默认值
    T unwrapOr(T default_value) const {
        if (isOk()) {
            return std::get<T>(data_);
        }
        return default_value;
    }
    
    // 获取值或通过函数生成默认值
    template<typename F>
    T unwrapOrElse(F&& func) const {
        if (isOk()) {
            return std::get<T>(data_);
        }
        return func(error());
    }
    
    // 获取错误
    const E& error() const {
        return std::get<E>(data_);
    }
    
    // 转换值（如果是 Ok）
    template<typename F>
    auto map(F&& func) -> Result<decltype(func(std::declval<T>())), E> {
        using U = decltype(func(std::declval<T>()));
        
        if (isOk()) {
            return Result<U, E>::Ok(func(unwrap()));
        }
        return Result<U, E>::Err(error());
    }
    
    // 链式操作（如果是 Ok，调用函数；否则传递错误）
    template<typename F>
    auto andThen(F&& func) -> decltype(func(std::declval<T>())) {
        using ResultType = decltype(func(std::declval<T>()));
        
        if (isOk()) {
            return func(unwrap());
        }
        return ResultType::Err(error());
    }
    
    // 转换错误（如果是 Error）
    template<typename F>
    auto mapError(F&& func) -> Result<T, decltype(func(std::declval<E>()))> {
        using NewE = decltype(func(std::declval<E>()));
        
        if (isError()) {
            return Result<T, NewE>::Err(func(error()));
        }
        return Result<T, NewE>::Ok(unwrap());
    }
    
private:
    Result() = default;
};

/**
 * Option<T> - 表示可选值
 * 
 * 类似 std::optional，但提供更丰富的函数式操作
 */
template<typename T>
class Option {
private:
    std::optional<T> value_;
    
public:
    Option() = default;
    Option(T value) : value_(std::move(value)) {}
    
    static Option Some(T value) {
        return Option(std::move(value));
    }
    
    static Option None() {
        return Option();
    }
    
    bool isSome() const { return value_.has_value(); }
    bool isNone() const { return !value_.has_value(); }
    
    T& unwrap() { return value_.value(); }
    const T& unwrap() const { return value_.value(); }
    
    T unwrapOr(T default_value) const {
        return value_.value_or(std::move(default_value));
    }
    
    template<typename F>
    T unwrapOrElse(F&& func) const {
        if (isSome()) {
            return value_.value();
        }
        return func();
    }
    
    template<typename F>
    auto map(F&& func) -> Option<decltype(func(std::declval<T>()))> {
        using U = decltype(func(std::declval<T>()));
        
        if (isSome()) {
            return Option<U>::Some(func(unwrap()));
        }
        return Option<U>::None();
    }
    
    template<typename F>
    auto andThen(F&& func) -> decltype(func(std::declval<T>())) {
        if (isSome()) {
            return func(unwrap());
        }
        return decltype(func(std::declval<T>()))::None();
    }
};

} // namespace hgl::shader_next
