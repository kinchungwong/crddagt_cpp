/**
 * @file vardata.hpp
 * @brief Definition of VarData class for type-erased variable storage.
 * @see vardata.inline.hpp for implementations of type-parameterized methods.
 */

#pragma once
#include "crddagt/common/common.hpp"

namespace crddagt
{

/**
 * @brief A type-erased container for storing a variable of any type.
 * 
 * @todo Implement all methods. Some methods are type parameterized and should be
 *      defined in vardata.inline.hpp. Examples: emplace<T, ...Args>, as<T>.
 */
class VarData
{
public:


private:
    std::shared_ptr<void> m_pvoid{};
    std::type_index m_ti{typeid(void)};
};

} // namespace crddagt
