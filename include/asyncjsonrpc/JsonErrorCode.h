#ifndef JSONERRORCODE_H
#define JSONERRORCODE_H

#include <cassert>
#include <jsoncpp/json/json.h>
#include <stdexcept>

class JsonErrorCode : public std::exception
{
    int         code;
    std::string message;
    Json::Value requestId;

public:
    inline std::string getMessage() const;
    inline int         getCode() const;
    inline Json::Value getRequestId() const;
    inline void        setRequestId(Json::Value val);
    inline JsonErrorCode(int Code = 0, std::string Message = "", Json::Value requestId = Json::Value());
    inline Json::Value toJsonValue() const;
    inline Json::Value toJsonRpcResponse() const;
    inline std::string toJsonRpcResponseStr() const;

    static JsonErrorCode make_ParseError(Json::Value requestId = Json::Value())
    {
        return JsonErrorCode(-32700, "Parse error", requestId);
    }
    static JsonErrorCode make_InvalidRequest(Json::Value requestId = Json::Value())
    {
        return JsonErrorCode(-32600, "Invalid Request", requestId);
    }
    static JsonErrorCode make_MethodNotFound(Json::Value requestId = Json::Value())
    {
        return JsonErrorCode(-32601, "Method not found", requestId);
    }
    static JsonErrorCode make_InvalidParams(Json::Value requestId = Json::Value())
    {
        return JsonErrorCode(-32602, "Invalid params", requestId);
    }
    static JsonErrorCode make_InternalError(Json::Value requestId = Json::Value())
    {
        return JsonErrorCode(-32603, "Internal error", requestId);
    }
    static JsonErrorCode make_ServerError(int code, Json::Value requestId = Json::Value())
    {
        assert(code >= -32099 && code <= -32000);
        return JsonErrorCode(code, "Server error", requestId);
    }

    static std::string JsonValueToString(const Json::Value& val)
    {
        Json::FastWriter fastWriter;
        return fastWriter.write(val);
    }

    inline const char* what() const noexcept override;
};

std::string JsonErrorCode::getMessage() const { return message; }

int JsonErrorCode::getCode() const { return code; }

Json::Value JsonErrorCode::getRequestId() const { return requestId; }

void JsonErrorCode::setRequestId(Json::Value val) { requestId = std::move(val); }

JsonErrorCode::JsonErrorCode(int Code, std::string Message, Json::Value RequestId)
    : code(Code), message(std::move(Message)), requestId(std::move(RequestId))
{
}

Json::Value JsonErrorCode::toJsonValue() const
{
    Json::Value err;
    err["code"]    = code;
    err["message"] = message;
    return err;
}

Json::Value JsonErrorCode::toJsonRpcResponse() const
{
    Json::Value err;
    err["code"]    = code;
    err["message"] = message;

    Json::Value response;
    response["id"]      = requestId;
    response["jsonrpc"] = "2.0";
    response["error"]   = err;
    return response;
}

std::string JsonErrorCode::toJsonRpcResponseStr() const
{
    Json::FastWriter writer;
    return writer.write(toJsonRpcResponse());
}

const char* JsonErrorCode::what() const noexcept { return message.c_str(); }

#endif // JSONERRORCODE_H
