#include "gtest/gtest.h"

#include "include/asyncjsonrpc/AsyncJsonRPC.h"
#include <atomic>
#include <future>
#include <string>

#include <boost/asio/io_context.hpp>

std::string GenerateRandomString__test(const int len)
{
    static const char alphanum[] = "0123456789"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz";

    std::string s;
    s.resize(len);

    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    return s;
}

TEST(AsyncJsonRPC, basic)
{
    boost::asio::io_context                              executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type> rpc(executionContext.get_executor());

    EXPECT_FALSE(rpc.handlerExists("testmethod1"));
    EXPECT_EQ(rpc.handlerCount(), 0u);

    EXPECT_NO_THROW(
        rpc.addHandler([](const Json::Value& /*request*/, Json::Value& /*response*/) {}, "testmethod1",
                       {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}}));

    EXPECT_ANY_THROW(
        rpc.addHandler([](const Json::Value& /*request*/, Json::Value& /*response*/) {}, "testmethod1",
                       {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}}));

    EXPECT_EQ(rpc.handlerCount(), 1u);
    EXPECT_FALSE(rpc.handlerExists("test"));
    EXPECT_TRUE(rpc.handlerExists("testmethod1"));

    EXPECT_NO_THROW(rpc.removeHandler("testmethod1"));
    EXPECT_FALSE(rpc.handlerExists("testmethod1"));
    EXPECT_EQ(rpc.handlerCount(), 0u);
}

TEST(AsyncJsonRPC, single_rpc_calls_successful)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_EQ(val["result"].asInt(), 15);
    });

    rpc.post(
        R"({"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"},
                "id": 4})",
        "TheString");
}

TEST(AsyncJsonRPC, async_single_rpc_calls_successful)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    std::promise<void> promise;
    std::future<void>  future = promise.get_future();

    rpc.setResponseCallback([&promise](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_EQ(val["result"].asInt(), 15);
        promise.set_value();
    });

    rpc.asyncPost(
        R"({"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"},
                "id": 4})",
        "TheString");

    executionContext.run();

    future.get();
}

TEST(AsyncJsonRPC, async_many_single_rpc_calls_successful)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    std::promise<void> promise;
    std::future<void>  future = promise.get_future();

    const unsigned        postCount = 250;
    std::atomic<unsigned> currCount{0};
    currCount.store(0); // ensure atomicity as constructor is not atomic

    rpc.setResponseCallback([&promise, &currCount](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_EQ(val["result"].asInt(), 15);

        currCount++;

        if (currCount >= postCount) {
            promise.set_value();
        }
    });

    for (unsigned i = 0; i < postCount; i++) {
        std::thread t([&rpc]() {
            rpc.asyncPost(
                R"({"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"},
                "id": 4})",
                "TheString");
        });
        t.detach();
    }

    executionContext.run();

    future.get();
}

TEST(AsyncJsonRPC, single_rpc_calls_wrong_parameter_type)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    // wrong parameter type
    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_TRUE(val.isMember("error"));
        EXPECT_EQ(val["error"]["code"].asInt(), -32602);
    });

    rpc.post(R"({"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2": 1.2}, "id": 4})",
             "TheString");
}

TEST(AsyncJsonRPC, single_rpc_calls_one_less_parameter)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    // wrong parameter count (less)
    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_TRUE(val.isMember("error"));
        EXPECT_EQ(val["error"]["code"].asInt(), -32602);
    });

    rpc.post(R"({"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5}, "id": 4})",
             "TheString");
}

TEST(AsyncJsonRPC, single_rpc_calls_one_more_parameter)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    // wrong parameter count (more)
    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_TRUE(val.isMember("error"));
        EXPECT_EQ(val["error"]["code"].asInt(), -32602);
    });

    rpc.post(
        R"({"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2": "abc", "p3":
                "hi"}, "id": 4})",
        "TheString");
}

TEST(AsyncJsonRPC, single_rpc_calls_missing_id)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    // missing id
    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_TRUE(val.isMember("error"));
        EXPECT_EQ(val["error"]["code"].asInt(), -32600);
    });

    rpc.post(R"({"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2":
            "abc"}})",
             "TheString");
}

TEST(AsyncJsonRPC, single_rpc_calls_missing_jsonrpc_key)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    // missing jsonrpc key
    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_TRUE(val.isMember("error"));
        EXPECT_EQ(val["error"]["code"].asInt(), -32600);
    });

    rpc.post(R"({"method": "testmethod1", "params": {"p1": 5, "p2": "abc"}, "id": 4})", "TheString");
}

TEST(AsyncJsonRPC, single_rpc_calls_wrong_jsonrpc_version)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    // wrong jsonrpc version
    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_TRUE(val.isMember("error"));
        EXPECT_EQ(val["error"]["code"].asInt(), -32600);
    });

    rpc.post(
        R"({"jsonrpc": "1.0", "method": "testmethod1", "params": {"p1": 5, "p2": "abc"}, "id":
                4})",
        "TheString");
}

TEST(AsyncJsonRPC, single_rpc_calls_invalid_json)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, std::string str) {
            EXPECT_EQ(request["p1"].asInt(), 5);
            EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
            EXPECT_EQ(str, "TheString");
            response = Json::Value(15);
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    // invalid json
    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        EXPECT_TRUE(val.isMember("error"));
        EXPECT_EQ(val["error"]["code"].asInt(), -32700);
    });

    rpc.post(
        R"({"jsonrpc": "2.0, "method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"},
                "id": 4})",
        "TheString");
}

TEST(AsyncJsonRPC, batch_rpc_calls_successful)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    // we use this count to process handling different array elements differently
    int count = 0;
    rpc.addHandler(
        [&count](const Json::Value& request, Json::Value& response, std::string str) {
            Json::FastWriter writer;
            EXPECT_EQ(str, "TheString");
            if (count == 0) {
                EXPECT_EQ(request["p1"].asInt(), 5);
                EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
                response = Json::Value(15);
            } else if (count == 1) {
                EXPECT_EQ(request["p1"].asInt(), 7);
                EXPECT_EQ(request["p2"].asString(), "RpcIsCool!!!");
                response = Json::Value(18);
            } else if (count == 2) {
                EXPECT_EQ(request["p1"].asInt(), 10);
                EXPECT_EQ(request["p2"].asString(), "XYZ!");
                response = Json::Value(42);
            }
            count++;
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        ASSERT_EQ(val.type(), Json::ValueType::arrayValue);
        ASSERT_EQ(val.size(), 3);
        EXPECT_EQ(val[0]["result"].asInt(), 15);
        EXPECT_EQ(val[1]["result"].asInt(), 18);
        EXPECT_EQ(val[2]["result"].asInt(), 42);
    });

    rpc.post(
        R"([{"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2":
                "HiThere!!!"}, "id": 4},
    {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 7, "p2": "RpcIsCool!!!"}, "id": 5},
    {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 10, "p2": "XYZ!"}, "id": 6}])",
        "TheString");

    EXPECT_EQ(count, 3); // count must reach this because it processed all array elements
}

TEST(AsyncJsonRPC, batch_rpc_calls_one_invalid_parameter_type)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    // we use this count to process handling different array elements differently
    int count = 0;
    rpc.addHandler(
        [&count](const Json::Value& request, Json::Value& response, std::string str) {
            Json::FastWriter writer;
            EXPECT_EQ(str, "TheString");
            if (count == 0) {
                EXPECT_EQ(request["p1"].asInt(), 5);
                EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
                response = Json::Value(15);
            } else if (count == 1) {
                EXPECT_EQ(request["p1"].asInt(), 7);
                EXPECT_EQ(request["p2"].asString(), "RpcIsCool!!!");
                response = Json::Value(18);
            } else if (count == 2) {
                throw std::runtime_error("This should never be reached");
            }
            count++;
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        ASSERT_EQ(val.type(), Json::ValueType::arrayValue);
        ASSERT_EQ(val.size(), 3);
        EXPECT_EQ(val[0]["result"].asInt(), 15);
        EXPECT_EQ(val[1]["result"].asInt(), 18);
        EXPECT_EQ(val[2]["error"]["code"].asInt(), -32602);
    });

    rpc.post(
        R"([{"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2":
                        "HiThere!!!"}, "id": 4},
            {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 7, "p2": "RpcIsCool!!!"},
            "id": 5},
            {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 10, "p2": 2.1}, "id": 6}])",
        "TheString");

    EXPECT_EQ(count, 2); // count must reach this because it processed all array elements
}

TEST(AsyncJsonRPC, batch_rpc_calls_one_extra_param)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    // we use this count to process handling different array elements differently
    int count = 0;
    rpc.addHandler(
        [&count](const Json::Value& request, Json::Value& response, std::string str) {
            Json::FastWriter writer;
            EXPECT_EQ(str, "TheString");
            if (count == 0) {
                EXPECT_EQ(request["p1"].asInt(), 5);
                EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
                response = Json::Value(15);
            } else if (count == 1) {
                EXPECT_EQ(request["p1"].asInt(), 7);
                EXPECT_EQ(request["p2"].asString(), "RpcIsCool!!!");
                response = Json::Value(18);
            } else if (count == 2) {
                throw std::runtime_error("This should never be reached");
            }
            count++;
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        ASSERT_EQ(val.type(), Json::ValueType::arrayValue);
        ASSERT_EQ(val.size(), 3);
        EXPECT_EQ(val[0]["result"].asInt(), 15);
        EXPECT_EQ(val[1]["error"]["code"].asInt(), -32602);
        EXPECT_EQ(val[2]["result"].asInt(), 18);
    });

    rpc.post(
        R"([{"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2":
                "HiThere!!!"}, "id": 4},
        {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 10, "p2": "coolyCool!", "p3":
        232}, "id": 5},
        {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 7, "p2": "RpcIsCool!!!"}, "id":
        6}])",
        "TheString");

    EXPECT_EQ(count, 2); // count must reach this because it processed all array elements
}

TEST(AsyncJsonRPC, batch_rpc_calls_one_less)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    // we use this count to process handling different array elements differently
    int count = 0;
    rpc.addHandler(
        [&count](const Json::Value& request, Json::Value& response, std::string str) {
            Json::FastWriter writer;
            EXPECT_EQ(str, "TheString");
            if (count == 0) {
                EXPECT_EQ(request["p1"].asInt(), 5);
                EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
                response = Json::Value(15);
            } else if (count == 1) {
                EXPECT_EQ(request["p1"].asInt(), 7);
                EXPECT_EQ(request["p2"].asString(), "RpcIsCool!!!");
                response = Json::Value(18);
            } else if (count == 2) {
                throw std::runtime_error("This should never be reached");
            }
            count++;
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        ASSERT_EQ(val.type(), Json::ValueType::arrayValue);
        ASSERT_EQ(val.size(), 3);
        EXPECT_EQ(val[0]["error"]["code"].asInt(), -32602);
        EXPECT_EQ(val[1]["result"].asInt(), 15);
        EXPECT_EQ(val[2]["result"].asInt(), 18);
    });

    rpc.post(
        R"([{"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 10}, "id": 5},
        {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"}, "id": 4},
        {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 7, "p2": "RpcIsCool!!!"}, "id":
        6}])",
        "TheString");

    EXPECT_EQ(count, 2); // count must reach this because it processed all array elements
}

TEST(AsyncJsonRPC, batch_rpc_calls_no_id)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    // we use this count to process handling different array elements differently
    int count = 0;
    rpc.addHandler(
        [&count](const Json::Value& request, Json::Value& response, std::string str) {
            Json::FastWriter writer;
            EXPECT_EQ(str, "TheString");
            if (count == 0) {
                EXPECT_EQ(request["p1"].asInt(), 5);
                EXPECT_EQ(request["p2"].asString(), "HiThere!!!");
                response = Json::Value(15);
            } else if (count == 1) {
                EXPECT_EQ(request["p1"].asInt(), 7);
                EXPECT_EQ(request["p2"].asString(), "RpcIsCool!!!");
                response = Json::Value(18);
            } else if (count == 2) {
                throw std::runtime_error("This should never be reached");
            }
            count++;
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        ASSERT_EQ(val.type(), Json::ValueType::arrayValue);
        ASSERT_EQ(val.size(), 3);
        EXPECT_EQ(val[0]["error"]["code"].asInt(), -32600);
        EXPECT_EQ(val[1]["result"].asInt(), 15);
        EXPECT_EQ(val[2]["result"].asInt(), 18);
    });

    rpc.post(
        R"([{"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 10}},
    {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"}, "id": 4},
    {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 7, "p2": "RpcIsCool!!!"}, "id": 6}])",
        "TheString");

    EXPECT_EQ(count, 2); // count must reach this number because it processed all array elements
}

TEST(AsyncJsonRPC, batch_rpc_calls_no_method_and_no_jsonrpc_key)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    // we use this count to process handling different array elements differently
    int count = 0;
    rpc.addHandler(
        [&count](const Json::Value& request, Json::Value& response, std::string str) {
            Json::FastWriter writer;
            EXPECT_EQ(str, "TheString");
            if (count == 0) {
                EXPECT_EQ(request["p1"].asInt(), 7);
                EXPECT_EQ(request["p2"].asString(), "RpcIsCool!!!");
                response = Json::Value(15);
            } else if (count == 1) {
                throw std::runtime_error("This should never be reached");
            } else if (count == 2) {
                throw std::runtime_error("This should never be reached");
            }
            count++;
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        ASSERT_EQ(val.type(), Json::ValueType::arrayValue);
        ASSERT_EQ(val.size(), 3);
        EXPECT_EQ(val[0]["error"]["code"].asInt(), -32600);
        EXPECT_EQ(val[1]["error"]["code"].asInt(), -32600);
        EXPECT_EQ(val[2]["result"].asInt(), 15);
    });

    rpc.post(
        R"([{"jsonrpc": "2.0", "params": {"p1": 10, "p2": "Hi!"}},
    {"method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"}, "id": 4},
    {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 7, "p2": "RpcIsCool!!!"}, "id": 6}])",
        "TheString");

    EXPECT_EQ(count, 1); // count must reach this number because it processed all array elements
}

TEST(AsyncJsonRPC, batch_rpc_calls_no_method_and_invalid_jsonrpc_key_and_invalid_param)
{
    boost::asio::io_context                                           executionContext;
    AsyncJsonRPC<boost::asio::io_context::executor_type, std::string> rpc(
        executionContext.get_executor());

    // error: no method and invalid jsonrpc key in each and invalid parameter (so 3/3 errors)

    // we use this count to process handling different array elements differently
    int count = 0;
    rpc.addHandler(
        [&count](const Json::Value& request, Json::Value& response, std::string str) {
            Json::FastWriter writer;
            EXPECT_EQ(str, "TheString");
            if (count == 0) {
                EXPECT_EQ(request["p1"].asInt(), 7);
                EXPECT_EQ(request["p2"].asString(), "RpcIsCool!!!");
                response = Json::Value(15);
            } else if (count == 1) {
                throw std::runtime_error("This should never be reached");
            } else if (count == 2) {
                throw std::runtime_error("This should never be reached");
            }
            count++;
        },
        "testmethod1", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

    rpc.setResponseCallback([](std::string&& res) {
        Json::Reader reader;
        Json::Value  val;
        reader.parse(res, val);
        ASSERT_EQ(val.type(), Json::ValueType::arrayValue);
        ASSERT_EQ(val.size(), 3);
        EXPECT_EQ(val[0]["error"]["code"].asInt(), -32600);
        EXPECT_EQ(val[1]["error"]["code"].asInt(), -32600);
        EXPECT_EQ(val[2]["error"]["code"].asInt(), -32602);
    });

    rpc.post(
        R"([{"jsonrpc": "2.0", "params": {"p1": 10, "p2": "Hi!"}},
    {"jsonrpc": "1.0", "method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"}, "id": 4},
    {"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 7, "p2": 15}, "id": 6}])",
        "TheString");

    EXPECT_EQ(count, 0); // count must reach this number because it processed all array elements
}

TEST(AsyncJsonRPC, parsing_json_by_name)
{
    std::string data =
        R"({"jsonrpc": "2.0", "method": "subtract", "params": {"minuend": 42, "subtrahend": 23},
            "id": 4})";
    Json::Reader reader;
    Json::Value  root;
    ASSERT_TRUE(reader.parse(data, root, false));
    EXPECT_TRUE(root.isMember("jsonrpc"));
    EXPECT_TRUE(root.isMember("method"));
    EXPECT_TRUE(root.isMember("params"));
    EXPECT_TRUE(root.isMember("id"));
    EXPECT_EQ(root["params"].type(), Json::ValueType::objectValue);
    EXPECT_EQ(root["params"].size(), 2);
    EXPECT_EQ(root["params"]["minuend"], 42);
    EXPECT_EQ(root["params"]["subtrahend"], 23);
}

TEST(AsyncJsonRPC, parsing_json_by_pos)
{
    std::string data =
        R"({"jsonrpc": "2.0", "method": "subtract", "params": [42, 23],
            "id": 4})";
    Json::Reader reader;
    Json::Value  root;
    ASSERT_TRUE(reader.parse(data, root, false));
    EXPECT_TRUE(root.isMember("jsonrpc"));
    EXPECT_TRUE(root.isMember("method"));
    EXPECT_TRUE(root.isMember("params"));
    EXPECT_TRUE(root.isMember("id"));
    EXPECT_EQ(root["params"].type(), Json::ValueType::arrayValue);
    EXPECT_EQ(root["params"].size(), 2);
    EXPECT_EQ(root["params"][0], 42);
    EXPECT_EQ(root["params"][1], 23);
}
