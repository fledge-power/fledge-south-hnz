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