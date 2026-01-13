#include <gtest/gtest.h>
#include "crddagt/common/graph_builder.hpp"
#include "crddagt/common/vardata.inline.hpp"
#include "crddagt/execution/executable_graph.hpp"
#include "crddagt/execution/single_threaded_executor.hpp"
#include "crddagt/execution/task_wrapper.hpp"
#include <atomic>

using namespace crddagt;

// =============================================================================
// Test Implementations
// =============================================================================

/**
 * @brief Simple IData implementation for testing.
 */
class TestData : public IData, public std::enable_shared_from_this<TestData>
{
public:
    std::shared_ptr<IData> shared_from_this() override
    {
        return std::enable_shared_from_this<TestData>::shared_from_this();
    }

    void set_value(CrdToken token, VarData value) override
    {
        m_value = std::move(value);
        m_created = true;
    }

    VarData get_value(CrdToken token) override
    {
        if (!m_created)
        {
            throw std::runtime_error("Value not set");
        }
        return m_value;
    }

    VarData remove_value(CrdToken token) override
    {
        if (!m_created)
        {
            throw std::runtime_error("Value not set");
        }
        m_created = false;
        return std::move(m_value);
    }

    bool is_created() const { return m_created; }

private:
    VarData m_value;
    bool m_created{false};
};

/**
 * @brief Simple IField implementation for testing.
 */
class TestField : public IField, public std::enable_shared_from_this<TestField>
{
public:
    TestField(std::shared_ptr<class TestStep> step,
              std::shared_ptr<TestData> data,
              std::type_index ti,
              Usage usage)
        : m_step{step}
        , m_data{data}
        , m_ti{ti}
        , m_usage{usage}
    {}

    std::shared_ptr<IField> shared_from_this() override
    {
        return std::enable_shared_from_this<TestField>::shared_from_this();
    }

    std::shared_ptr<IStep> get_step() override;

    std::shared_ptr<IData> get_data() override
    {
        return m_data;
    }

    std::type_index get_type() override { return m_ti; }
    Usage get_usage() override { return m_usage; }

private:
    std::shared_ptr<class TestStep> m_step;
    std::shared_ptr<TestData> m_data;
    std::type_index m_ti;
    Usage m_usage;
};

/**
 * @brief Simple IStep implementation for testing.
 */
class TestStep : public IStep, public std::enable_shared_from_this<TestStep>
{
public:
    explicit TestStep(const std::string& name)
        : m_name{name}
    {}

    std::shared_ptr<IStep> shared_from_this() override
    {
        return std::enable_shared_from_this<TestStep>::shared_from_this();
    }

    std::vector<std::shared_ptr<IField>> get_fields() override
    {
        return m_fields;
    }

    void execute() override
    {
        m_execute_count++;
        if (m_should_throw)
        {
            throw std::runtime_error("TestStep error: " + m_name);
        }
    }

    const std::string& class_name() const override
    {
        static std::string name = "TestStep";
        return name;
    }

    std::string friendly_name() const override { return m_name; }
    std::string unique_name() const override { return "TestStep_" + m_name; }

    void add_field(std::shared_ptr<TestField> field)
    {
        m_fields.push_back(field);
    }

    void set_should_throw(bool value) { m_should_throw = value; }
    int execute_count() const { return m_execute_count; }

private:
    std::string m_name;
    std::vector<std::shared_ptr<IField>> m_fields;
    bool m_should_throw{false};
    std::atomic<int> m_execute_count{0};
};

std::shared_ptr<IStep> TestField::get_step()
{
    return m_step;
}

// =============================================================================
// Test Fixture
// =============================================================================

class ExecutionTests : public ::testing::Test
{
protected:
    std::shared_ptr<TestStep> make_step(const std::string& name)
    {
        return std::make_shared<TestStep>(name);
    }

    std::shared_ptr<TestData> make_data()
    {
        return std::make_shared<TestData>();
    }

    std::shared_ptr<TestField> make_field(
        std::shared_ptr<TestStep> step,
        std::shared_ptr<TestData> data,
        Usage usage)
    {
        auto field = std::make_shared<TestField>(
            step, data, std::type_index{typeid(int)}, usage);
        step->add_field(field);
        return field;
    }
};

// =============================================================================
// ExecutableGraph Tests
// =============================================================================

TEST_F(ExecutionTests, ExecutableGraph_GetInitialReadySteps_NoSteps)
{
    ExecutableGraph graph;
    auto ready = graph.get_initial_ready_steps();
    EXPECT_TRUE(ready.empty());
}

TEST_F(ExecutionTests, ExecutableGraph_GetInitialReadySteps_AllReady)
{
    ExecutableGraph graph;
    graph.predecessor_counts = {0, 0, 0};
    auto ready = graph.get_initial_ready_steps();
    EXPECT_EQ(ready.size(), 3u);
}

TEST_F(ExecutionTests, ExecutableGraph_GetInitialReadySteps_SomeReady)
{
    ExecutableGraph graph;
    graph.predecessor_counts = {0, 1, 0, 2};
    auto ready = graph.get_initial_ready_steps();
    EXPECT_EQ(ready.size(), 2u);
    EXPECT_EQ(ready[0], 0u);
    EXPECT_EQ(ready[1], 2u);
}

// =============================================================================
// GraphBuilder.build() Tests
// =============================================================================

TEST_F(ExecutionTests, Build_EmptyGraph_Succeeds)
{
    GraphBuilder builder(true);
    auto graph = builder.build();
    EXPECT_EQ(graph->step_count(), 0u);
}

TEST_F(ExecutionTests, Build_SingleStep_Succeeds)
{
    auto step = make_step("A");
    auto data = make_data();
    auto field = make_field(step, data, Usage::Create);

    GraphBuilder builder(true);
    builder.add_step(step);

    auto graph = builder.build();
    EXPECT_EQ(graph->step_count(), 1u);
    EXPECT_EQ(graph->steps[0], step);
}

TEST_F(ExecutionTests, Build_LinearChain_CorrectPredecessorCounts)
{
    // A(Create) -> B(Read) -> C(Destroy)
    auto data = make_data();
    auto stepA = make_step("A");
    auto stepB = make_step("B");
    auto stepC = make_step("C");

    auto fieldA = make_field(stepA, data, Usage::Create);
    auto fieldB = make_field(stepB, data, Usage::Read);
    auto fieldC = make_field(stepC, data, Usage::Destroy);

    GraphBuilder builder(true);
    builder.add_step(stepA);
    builder.add_step(stepB);
    builder.add_step(stepC);
    builder.link_fields(fieldA, fieldB, TrustLevel::High);
    builder.link_fields(fieldB, fieldC, TrustLevel::High);

    auto graph = builder.build();
    EXPECT_EQ(graph->predecessor_counts[0], 0u);  // A has no preds
    EXPECT_EQ(graph->predecessor_counts[1], 1u);  // B depends on A (Create < Read)
    // C depends on A (Create < Destroy) and B (Read < Destroy), so 2 predecessors
    EXPECT_EQ(graph->predecessor_counts[2], 2u);
}

TEST_F(ExecutionTests, Build_InvalidGraph_Throws)
{
    // Create a cycle: A -> B -> A
    auto stepA = make_step("A");
    auto stepB = make_step("B");

    GraphBuilder builder(false);  // Deferred validation
    builder.add_step(stepA);
    builder.add_step(stepB);
    builder.link_steps(stepA, stepB, TrustLevel::High);
    builder.link_steps(stepB, stepA, TrustLevel::High);

    EXPECT_THROW(builder.build(), GraphValidationError);
}

// =============================================================================
// SingleThreadedExecutor Tests
// =============================================================================

TEST_F(ExecutionTests, Execute_EmptyGraph_Succeeds)
{
    GraphBuilder builder(true);
    auto graph = builder.build();

    auto executor = make_single_threaded_executor();
    auto result = executor->execute(graph);

    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.completed_steps.empty());
    EXPECT_TRUE(result.failed_steps.empty());
}

TEST_F(ExecutionTests, Execute_SingleStep_Succeeds)
{
    auto step = make_step("A");
    auto data = make_data();
    auto field = make_field(step, data, Usage::Create);

    GraphBuilder builder(true);
    builder.add_step(step);
    auto graph = builder.build();

    auto executor = make_single_threaded_executor();
    auto result = executor->execute(graph);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.completed_steps.size(), 1u);
    EXPECT_EQ(step->execute_count(), 1);
}

TEST_F(ExecutionTests, Execute_LinearChain_ExecutesInOrder)
{
    auto data = make_data();
    auto stepA = make_step("A");
    auto stepB = make_step("B");
    auto stepC = make_step("C");

    auto fieldA = make_field(stepA, data, Usage::Create);
    auto fieldB = make_field(stepB, data, Usage::Read);
    auto fieldC = make_field(stepC, data, Usage::Destroy);

    GraphBuilder builder(true);
    builder.add_step(stepA);
    builder.add_step(stepB);
    builder.add_step(stepC);
    builder.link_fields(fieldA, fieldB, TrustLevel::High);
    builder.link_fields(fieldB, fieldC, TrustLevel::High);
    auto graph = builder.build();

    auto executor = make_single_threaded_executor();
    auto result = executor->execute(graph);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.completed_steps.size(), 3u);
    EXPECT_EQ(stepA->execute_count(), 1);
    EXPECT_EQ(stepB->execute_count(), 1);
    EXPECT_EQ(stepC->execute_count(), 1);
}

TEST_F(ExecutionTests, Execute_StepFailure_AbortsExecution)
{
    auto data = make_data();
    auto stepA = make_step("A");
    auto stepB = make_step("B");

    auto fieldA = make_field(stepA, data, Usage::Create);
    auto fieldB = make_field(stepB, data, Usage::Read);

    stepA->set_should_throw(true);  // A will fail

    GraphBuilder builder(true);
    builder.add_step(stepA);
    builder.add_step(stepB);
    builder.link_fields(fieldA, fieldB, TrustLevel::High);
    auto graph = builder.build();

    ExecutorConfig config;
    config.abort_on_failure = true;
    auto executor = std::make_shared<SingleThreadedExecutor>(config);
    auto result = executor->execute(graph);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.failed_steps.size(), 1u);
    EXPECT_EQ(result.failed_steps[0], 0u);  // Step A
    EXPECT_EQ(stepB->execute_count(), 0);  // B should not execute
}

TEST_F(ExecutionTests, Execute_ParallelSteps_AllExecute)
{
    // A -> B and A -> C (B and C can run in parallel, but in single-threaded they run sequentially)
    auto data1 = make_data();
    auto data2 = make_data();
    auto stepA = make_step("A");
    auto stepB = make_step("B");
    auto stepC = make_step("C");

    auto fieldA1 = make_field(stepA, data1, Usage::Create);
    auto fieldA2 = make_field(stepA, data2, Usage::Create);
    auto fieldB = make_field(stepB, data1, Usage::Read);
    auto fieldC = make_field(stepC, data2, Usage::Read);

    GraphBuilder builder(true);
    builder.add_step(stepA);
    builder.add_step(stepB);
    builder.add_step(stepC);
    builder.link_fields(fieldA1, fieldB, TrustLevel::High);
    builder.link_fields(fieldA2, fieldC, TrustLevel::High);
    auto graph = builder.build();

    auto executor = make_single_threaded_executor();
    auto result = executor->execute(graph);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.completed_steps.size(), 3u);
    EXPECT_EQ(stepA->execute_count(), 1);
    EXPECT_EQ(stepB->execute_count(), 1);
    EXPECT_EQ(stepC->execute_count(), 1);
}

TEST_F(ExecutionTests, Execute_DiamondPattern_AllExecute)
{
    // A -> B, A -> C, B -> D, C -> D (diamond pattern)
    auto data1 = make_data();
    auto data2 = make_data();
    auto stepA = make_step("A");
    auto stepB = make_step("B");
    auto stepC = make_step("C");
    auto stepD = make_step("D");

    auto fieldA1 = make_field(stepA, data1, Usage::Create);
    auto fieldA2 = make_field(stepA, data2, Usage::Create);
    auto fieldB1 = make_field(stepB, data1, Usage::Read);
    auto fieldC1 = make_field(stepC, data2, Usage::Read);
    auto fieldD1 = make_field(stepD, data1, Usage::Destroy);
    auto fieldD2 = make_field(stepD, data2, Usage::Destroy);

    GraphBuilder builder(true);
    builder.add_step(stepA);
    builder.add_step(stepB);
    builder.add_step(stepC);
    builder.add_step(stepD);
    builder.link_fields(fieldA1, fieldB1, TrustLevel::High);
    builder.link_fields(fieldA2, fieldC1, TrustLevel::High);
    builder.link_fields(fieldB1, fieldD1, TrustLevel::High);
    builder.link_fields(fieldC1, fieldD2, TrustLevel::High);
    auto graph = builder.build();

    auto executor = make_single_threaded_executor();
    auto result = executor->execute(graph);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.completed_steps.size(), 4u);
    EXPECT_EQ(stepA->execute_count(), 1);
    EXPECT_EQ(stepB->execute_count(), 1);
    EXPECT_EQ(stepC->execute_count(), 1);
    EXPECT_EQ(stepD->execute_count(), 1);
}

TEST_F(ExecutionTests, Execute_WithTiming_CollectsDurations)
{
    auto step = make_step("A");
    auto data = make_data();
    auto field = make_field(step, data, Usage::Create);

    GraphBuilder builder(true);
    builder.add_step(step);
    auto graph = builder.build();

    ExecutorConfig config;
    config.collect_timing = true;
    auto executor = std::make_shared<SingleThreadedExecutor>(config);
    auto result = executor->execute(graph);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.step_durations.size(), 1u);
    EXPECT_GE(result.step_durations[0].count(), 0);
}

TEST_F(ExecutionTests, RequestStop_CancelsPendingSteps)
{
    auto data = make_data();
    auto stepA = make_step("A");
    auto stepB = make_step("B");

    auto fieldA = make_field(stepA, data, Usage::Create);
    auto fieldB = make_field(stepB, data, Usage::Read);

    GraphBuilder builder(true);
    builder.add_step(stepA);
    builder.add_step(stepB);
    builder.link_fields(fieldA, fieldB, TrustLevel::High);
    auto graph = builder.build();

    auto executor = make_single_threaded_executor();

    // Pre-request stop before execution
    executor->request_stop();

    auto result = executor->execute(graph);

    // At least some steps should be cancelled
    // Note: The first step might execute before stop is checked
    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.stopped);
}

// =============================================================================
// TaskWrapper State Tests
// =============================================================================

TEST_F(ExecutionTests, TaskWrapper_InitialState_NotReadyOrReady)
{
    auto step = make_step("A");
    auto executor = make_single_threaded_executor();

    // Task with predecessors starts NotReady
    TaskWrapper wrapper1(step, 0, 1, 2, executor);
    EXPECT_EQ(wrapper1.state(), StepState::NotReady);
    EXPECT_FALSE(wrapper1.is_ready());

    // Task without predecessors starts Ready
    TaskWrapper wrapper2(step, 0, 1, 0, executor);
    EXPECT_EQ(wrapper2.state(), StepState::Ready);
    EXPECT_TRUE(wrapper2.is_ready());
}

TEST_F(ExecutionTests, TaskWrapper_DecrementPredecessorCount_BecomesReady)
{
    auto step = make_step("A");
    auto executor = make_single_threaded_executor();

    auto wrapper = std::make_shared<TaskWrapper>(step, 0, 1, 2, executor);

    EXPECT_FALSE(wrapper->decrement_predecessor_count());  // 2 -> 1
    EXPECT_FALSE(wrapper->is_ready());

    EXPECT_TRUE(wrapper->decrement_predecessor_count());   // 1 -> 0
    EXPECT_TRUE(wrapper->is_ready());
    EXPECT_EQ(wrapper->state(), StepState::Ready);
}

TEST_F(ExecutionTests, TaskWrapper_Cancel_SetsState)
{
    auto step = make_step("A");
    auto executor = make_single_threaded_executor();

    TaskWrapper wrapper(step, 0, 1, 1, executor);
    EXPECT_EQ(wrapper.state(), StepState::NotReady);

    wrapper.cancel();
    EXPECT_EQ(wrapper.state(), StepState::Cancelled);
}
