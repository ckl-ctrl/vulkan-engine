/**
 * @file concepts.hpp
 * @brief 项目通用 C++20 concept 定义
 *
 * 集中管理所有自定义 concept，供各模块复用。
 */

#pragma once

#include <type_traits>

/**
 * @brief 约束派生类型 D 必须是基类 B 的子类（含自身）
 *
 * @tparam D 派生类型
 * @tparam B 基类型
 *
 * @note 与 std::derived_from 不同，std::is_base_of_v<B, D> 在 B==D 时也返回 true
 */
template<typename D, typename B>
concept is_base_of = std::is_base_of_v<B, D>;
