#ifndef ASYNCJSONRPCMETHOD_H
#define ASYNCJSONRPCMETHOD_H

#include "asyncjsonrpc/JsonErrorCode.h"
#include <boost/container/flat_map.hpp>
#include <jsoncpp/json/json.h>
#include <string>

#include <boost/filesystem.hpp>

template <typename... ContextParams>
class AsyncJsonRPCMethod
{
public:
    enum ParamsDeclaration : bool
    {
        ParamsByName,
        ParamsByPosition
    };

private:
    using ParametersByNameMapType = boost::container::flat_map<std::string, Json::ValueType>;
    using ParametersByPosVecType  = std::vector<Json::ValueType>;
    std::string             methodName;
    ParametersByNameMapType methodParameters_byName; // name versus type
    ParametersByPosVecType  methodParameters_byPos;
    ParamsDeclaration       paramsDecl;

    std::function<void(const Json::Value&, Json::Value&, ContextParams...)> handler;

public:
    AsyncJsonRPCMethod(
        std::function<void(const Json::Value&, Json::Value&, ContextParams...)> HandlerClosure,
        std::string MethodName, const std::map<std::string, Json::ValueType>& MethodParams = {});
    AsyncJsonRPCMethod(
        std::function<void(const Json::Value&, Json::Value&, ContextParams...)> HandlerClosure,
        std::string MethodName, const std::vector<Json::ValueType>& MethodParams = {});
    std::size_t parameterCount() const;
    void        verifyParameterTypes(const Json::Value& parameters, const Json::Value& requestId) const;

    void invoke(const Json::Value& request, Json::Value& response, ContextParams... contextParams) const;
};

template <typename... ContextParams>
AsyncJsonRPCMethod<ContextParams...>::AsyncJsonRPCMethod(
    std::function<void(const Json::Value&, Json::Value&, ContextParams...)> HandlerClosure,
    std::string MethodName, const std::map<std::string, Json::ValueType>& MethodParams)
{
    paramsDecl = ParamsDeclaration::ParamsByName;
    methodName = std::move(MethodName);
    methodParameters_byName.insert(MethodParams.cbegin(), MethodParams.cend());
    handler = HandlerClosure;
}

template <typename... ContextParams>
AsyncJsonRPCMethod<ContextParams...>::AsyncJsonRPCMethod(
    std::function<void(const Json::Value&, Json::Value&, ContextParams...)> HandlerClosure,
    std::string MethodName, const std::vector<Json::ValueType>& MethodParams)
{
    paramsDecl = ParamsDeclaration::ParamsByPos;
    methodName = std::move(MethodName);
    methodParameters_byPos.insert(MethodParams.cbegin(), MethodParams.cend());
    handler = HandlerClosure;
}

template <typename... ContextParams>
std::size_t AsyncJsonRPCMethod<ContextParams...>::parameterCount() const
{
    assert(paramsDecl == ParamsDeclaration::ParamsByName ||
           paramsDecl == ParamsDeclaration::ParamsByPosition);
    switch (paramsDecl) {
    case ParamsDeclaration::ParamsByName:
        return methodParameters_byName.size();
    case ParamsDeclaration::ParamsByPosition:
        return methodParameters_byPos.size();
    default:
        throw JsonErrorCode::make_InternalError();
    }
}

template <typename... ContextParams>
void AsyncJsonRPCMethod<ContextParams...>::verifyParameterTypes(const Json::Value& parameters,
                                                                const Json::Value& requestId) const
{
    std::size_t expectedSize = parameterCount();
    if (parameters.size() != expectedSize) {
        throw JsonErrorCode::make_InvalidParams(requestId);
    }

    if (paramsDecl == ParamsDeclaration::ParamsByName) {
        if (parameters.type() != Json::ValueType::objectValue) {
            throw JsonErrorCode::make_InvalidParams(requestId);
        }
        for (Json::Value::const_iterator itr = parameters.begin(); itr != parameters.end(); itr++) {
            auto it = methodParameters_byName.find(itr.key().asString());
            if (it == methodParameters_byName.cend()) {
                throw JsonErrorCode::make_InvalidParams(requestId);
            }
            if (itr->type() != it->second) {
                throw JsonErrorCode::make_InvalidParams(requestId);
            }
        }
    } else if (paramsDecl == ParamsDeclaration::ParamsByPosition) {
        if (parameters.type() != Json::ValueType::arrayValue) {
            throw JsonErrorCode::make_InvalidParams(requestId);
        }
        for (int i = 0; i < static_cast<int>(parameters.size()); i++) {
            if (parameters[i] != methodParameters_byPos[i]) {
                throw JsonErrorCode::make_InvalidParams(requestId);
            }
        }
    } else {
        throw JsonErrorCode::make_InternalError();
    }
}

template <typename... ContextParams>
void AsyncJsonRPCMethod<ContextParams...>::invoke(const Json::Value& request, Json::Value& response,
                                                  ContextParams... contextParams) const
{
    handler(request, response, contextParams...);
}

#endif // ASYNCJSONRPCMETHOD_H
