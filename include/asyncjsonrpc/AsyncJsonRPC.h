#ifndef ASYNCJSONRPC_H
#define ASYNCJSONRPC_H

#include "detail/ThreadPool.h"

#include "AsyncJsonRPCMethod.h"
#include "JsonErrorCode.h"
#include <functional>
#include <jsoncpp/json/json.h>
#include <stdexcept>
#include <type_traits>

template <typename... HandlerContext>
class AsyncJsonRPC
{
    boost::container::flat_map<std::string, AsyncJsonRPCMethod<HandlerContext...>> methods;

    ThreadPool                         threadPool;
    std::function<void(std::string&&)> responseCallback;

    void basicRpcCallValidation(const Json::Value& root);

    Json::Value getResultForSingleRpcCall(const Json::Value& root, HandlerContext... handlerContext);

    Json::Value getResponseForSingleRpcCall(const Json::Value& root, const Json::Value& requestId,
                                            HandlerContext... handlerContext);

public:
    AsyncJsonRPC(unsigned threadCount = std::thread::hardware_concurrency())
    {
        threadCount = (threadCount >= 1 ? threadCount : 1);
        threadPool.start(threadCount);
    }

    template <typename Handler>
    void addHandler(Handler handler, const std::string& methodName,
                    const std::map<std::string, Json::ValueType>& MethodParamsTypes)
    {
        static_assert(std::is_convertible<Handler, std::function<void(const Json::Value&, Json::Value&,
                                                                      HandlerContext...)>>::value,
                      "Invalid function type");
        if (methods.find(methodName) != methods.end()) {
            throw std::runtime_error("Method " + methodName + " already registered");
        }
        AsyncJsonRPCMethod<HandlerContext...> method(handler, methodName, MethodParamsTypes);
        methods.emplace(methodName, method);
    }

    template <typename Handler>
    void addHandler(Handler handler, const std::string& methodName,
                    const std::vector<Json::ValueType>& MethodParamsTypes = {})
    {
        static_assert(std::is_convertible<Handler, std::function<void(const Json::Value&, Json::Value&,
                                                                      HandlerContext...)>>::value,
                      "Invalid function type");
        if (methods.find(methodName) != methods.end()) {
            throw std::runtime_error("Method " + methodName + " already registered");
        }
        AsyncJsonRPCMethod<HandlerContext...> method(handler, methodName, MethodParamsTypes);
        methods.emplace(methodName, method);
    }

    void removeHandler(const std::string& methodName);

    bool handlerExists(const std::string& methodName) const;

    std::size_t handlerCount() const;

    static inline Json::Value PutResultInResponseContext(Json::Value&&      result,
                                                         const Json::Value& requestId);

    void setResponseCallback(std::function<void(std::string&&)> callback);

    void post(const std::string& jsonCall, HandlerContext... handlerContext);
    void asyncPost(const std::string& jsonCall, HandlerContext... handlerContext);
};

template <typename... HandlerContext>
Json::Value AsyncJsonRPC<HandlerContext...>::PutResultInResponseContext(Json::Value&&      result,
                                                                        const Json::Value& requestId)
{
    Json::Value response;
    response["jsonrpc"] = "2.0";
    response["id"]      = requestId;
    response["result"]  = std::move(result);

    return response;
}

template <typename... HandlerContext>
void AsyncJsonRPC<HandlerContext...>::setResponseCallback(std::function<void(std::string&&)> callback)
{
    responseCallback = callback;
}

template <typename... HandlerContext>
void AsyncJsonRPC<HandlerContext...>::basicRpcCallValidation(const Json::Value& root)
{
    if (!root.isMember("method")) {
        throw JsonErrorCode::make_InvalidRequest();
    }

    if (!root.isMember("id")) {
        throw JsonErrorCode::make_InvalidRequest();
    }

    if (root["id"].type() != Json::ValueType::intValue &&
        root["id"].type() != Json::ValueType::uintValue) {
        throw JsonErrorCode::make_InvalidRequest();
    }

    if (!root.isMember("jsonrpc")) {
        throw JsonErrorCode::make_InvalidRequest(root["id"]);
    }

    if (root["jsonrpc"] != "2.0") {
        throw JsonErrorCode::make_InvalidRequest(root["id"]);
    }
}

template <typename... HandlerContext>
Json::Value AsyncJsonRPC<HandlerContext...>::getResultForSingleRpcCall(const Json::Value& root,
                                                                       HandlerContext... handlerContext)
{
    const std::string& methodName = root["method"].asString();

    auto methodIt = methods.find(methodName);
    if (methodIt == methods.cend()) {
        throw JsonErrorCode::make_MethodNotFound(root["id"]);
    }

    const AsyncJsonRPCMethod<HandlerContext...>& methodObj = methodIt->second;

    // if number of params is > 0, make sure there is a params object
    if (!root.isMember("params") && methodObj.parameterCount() > 0) {
        throw JsonErrorCode::make_InvalidParams(root["id"]);
    }

    Json::Value result;
    if (methodObj.parameterCount() > 0) {
        const Json::Value& paramsObj = root["params"];
        methodObj.verifyParameterTypes(paramsObj, root["id"]);
        methodObj.invoke(paramsObj, result, handlerContext...);
    } else {
        methodObj.invoke(Json::Value(), result, handlerContext...);
    }

    return result;
}

template <typename... HandlerContext>
Json::Value AsyncJsonRPC<HandlerContext...>::getResponseForSingleRpcCall(
    const Json::Value& root, const Json::Value& requestId, HandlerContext... handlerContext)
{
    try {
        // validate basic properties (for example, that "id" exists)
        basicRpcCallValidation(root);

        // single request
        Json::Value result = getResultForSingleRpcCall(root, handlerContext...);

        // create the response string
        return PutResultInResponseContext(std::move(result), requestId);
    } catch (JsonErrorCode& ex) {
        // create response from json error
        ex.setRequestId(root["id"]);
        return ex.toJsonRpcResponse();
    } catch (std::exception& ex) {
        // create response from error
        return JsonErrorCode::make_InternalError(root["id"]).toJsonRpcResponse();
    }
}

template <typename... HandlerContext>
void AsyncJsonRPC<HandlerContext...>::removeHandler(const std::string& methodName)
{
    if (methods.find(methodName) == methods.end()) {
        throw std::runtime_error("Method " + methodName + " does not exist");
    }
    methods.erase(methodName);
}

template <typename... HandlerContext>
bool AsyncJsonRPC<HandlerContext...>::handlerExists(const std::string& methodName) const
{
    return methods.find(methodName) != methods.end();
}

template <typename... HandlerContext>
std::size_t AsyncJsonRPC<HandlerContext...>::handlerCount() const
{
    return methods.size();
}

template <typename... HandlerContext>
void AsyncJsonRPC<HandlerContext...>::post(const std::string& jsonCall, HandlerContext... handlerContext)
{
    try {

        Json::Reader reader;
        Json::Value  root_;
        bool         success = reader.parse(jsonCall, root_, false);
        if (!success) {
            responseCallback(JsonErrorCode::make_ParseError().toJsonRpcResponseStr());
            return;
        }
        const Json::Value& root = root_;

        // requests can either be batch requests (array type) or single requests, in an object
        if (root.type() == Json::ValueType::arrayValue) {
            // batch request
            for (unsigned i = 0; i < root.size(); i++) {
                // inside the array there should be objects, this protects from infinite recursion
                if (root[i].type() != Json::ValueType::objectValue) {
                    responseCallback(JsonErrorCode::make_ParseError().toJsonRpcResponseStr());
                    return;
                }
            }
            // prepare the responses and put them in an array
            Json::Value arrayResponse = Json::Value(Json::arrayValue);
            for (unsigned i = 0; i < root.size(); i++) {

                const Json::Value& idVal = (root[i].isMember("id") ? root[i]["id"] : Json::Value());

                // process every single request, and add it to the response array
                Json::Value response = getResponseForSingleRpcCall(root[i], idVal, handlerContext...);
                arrayResponse.append(response);
            }
            std::string arrayResponseStr = JsonErrorCode::JsonValueToString(arrayResponse);
            responseCallback(std::move(arrayResponseStr));

        } else if (root.type() == Json::ValueType::objectValue) {
            // validate basic properties (for example, that "id" exists)
            basicRpcCallValidation(root);

            // single request
            Json::Value response = getResponseForSingleRpcCall(root, root["id"], handlerContext...);

            // the result as string
            responseCallback(JsonErrorCode::JsonValueToString(response));

        } else {
            responseCallback(JsonErrorCode::make_ParseError().toJsonRpcResponseStr());
            return;
        }

    } catch (JsonErrorCode& ex) {
        responseCallback(ex.toJsonRpcResponseStr());
        return;
    } catch (std::exception& ex) {
        responseCallback(JsonErrorCode::make_InternalError().toJsonRpcResponseStr());
        return;
    }
}

template <typename... HandlerContext>
void AsyncJsonRPC<HandlerContext...>::asyncPost(const std::string& jsonCall,
                                                HandlerContext... handlerContext)
{
    threadPool.push([this, jsonCall, handlerContext...]() { this->post(jsonCall, handlerContext...); });
}

#endif // ASYNCJSONRPC_H
