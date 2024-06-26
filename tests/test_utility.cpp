#include <gtest/gtest.h>

#include "hnzutility.h"

TEST(HNZUtility, Logs)
{
    std::string text("This message is at level %s");
    ASSERT_NO_THROW(HnzUtility::log_debug(text.c_str(), "debug"));
    ASSERT_NO_THROW(HnzUtility::log_info(text.c_str(), "info"));
    ASSERT_NO_THROW(HnzUtility::log_warn(text.c_str(), "warning"));
    ASSERT_NO_THROW(HnzUtility::log_error(text.c_str(), "error"));
    ASSERT_NO_THROW(HnzUtility::log_fatal(text.c_str(), "fatal"));
}

TEST(HNZUtility, Audit)
{
    std::string text("This audit is of type ");
    ASSERT_NO_THROW(HnzUtility::audit_success("SRVFL", text + "success"));
    ASSERT_NO_THROW(HnzUtility::audit_info("SRVFL", text + "info"));
    ASSERT_NO_THROW(HnzUtility::audit_warn("SRVFL", text + "warn"));
    ASSERT_NO_THROW(HnzUtility::audit_fail("SRVFL", text + "fail"));
}