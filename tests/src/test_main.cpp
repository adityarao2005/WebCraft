#include "test_suite.hpp"

namespace
{
    class diagnostics_listener : public testing::EmptyTestEventListener
    {
        void OnTestStart(const testing::TestInfo &info) override
        {
            TEST_TRACE(std::string("begin ") + info.test_suite_name() + "." + info.name());
        }

        void OnTestEnd(const testing::TestInfo &info) override
        {
            TEST_TRACE(std::string("end ") + info.test_suite_name() + "." + info.name()
                       + " elapsed_ms=" + std::to_string(info.result()->elapsed_time())
                       + (info.result()->Passed() ? " passed" : " failed"));
        }
    };
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    testing::UnitTest::GetInstance()->listeners().Append(new diagnostics_listener);
    return RUN_ALL_TESTS();
}
