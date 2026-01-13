#pragma once
#include "crddagt/common/common.hpp"
#include "crddagt/common/vardata.hpp"
#include "crddagt/common/graph_core_enums.hpp"

namespace crddagt
{

class IStep;
class IField;
class IData;

/**
 * @brief A token representing authorization to access or modify data.
 * Graph assigns CrdTokens to Steps, and also reserve one for Graph itself.
 */
using CrdToken = size_t;

class IStep
{
public:
    virtual ~IStep() = 0;
    virtual std::shared_ptr<IStep> shared_from_this() = 0;
    virtual std::vector<std::shared_ptr<IField>> get_fields() = 0;

private:
    IStep(const IStep&) = delete;
    IStep(IStep&&) = delete;
    IStep& operator=(const IStep&) = delete;
    IStep& operator=(IStep&&) = delete;
};

class IField
{
public:
    virtual ~IField() = 0;
    virtual std::shared_ptr<IField> shared_from_this() = 0;
    virtual std::shared_ptr<IStep> get_step() = 0;
    virtual std::shared_ptr<IData> get_data() = 0;
    virtual std::type_index get_type() = 0;
    virtual Usage get_usage() = 0;

private:
    IField(const IField&) = delete;
    IField(IField&&) = delete;
    IField& operator=(const IField&) = delete;
    IField& operator=(IField&&) = delete;
};

class IData
{
public:
    virtual ~IData() = 0;
    virtual std::shared_ptr<IData> shared_from_this() = 0;
    virtual void set_value(CrdToken token, VarData value) = 0;
    virtual VarData get_value(CrdToken token) = 0;
    virtual VarData remove_value(CrdToken token) = 0;

private:
    IData(const IData&) = delete;
    IData(IData&&) = delete;
    IData& operator=(const IData&) = delete;
    IData& operator=(IData&&) = delete;
};

using StepPtr = std::shared_ptr<IStep>;
using FieldPtr = std::shared_ptr<IField>;
using DataPtr = std::shared_ptr<IData>;

inline IStep::~IStep() = default;
inline IField::~IField() = default;
inline IData::~IData() = default;

} // namespace crddagt
