# AsyncJsonRPC library

### Introduction
The AsyncJsonRPC library is a simple, header-only, high-performance, thread-safe library that acts as the backend of a remote json-rpc system. It can be both synchronous and asynchronous. 

The library doesn't contain any communication protocol. The way it works, from a high-level perspective, is that you post a string containing the jsonrpc call [according to the jsonrpc standard](https://www.jsonrpc.org/specification), and you will get a callback function called (of your choosing, defined with a closure) with the response.

### Context-per-call support... OR... why another jsonrpc library?

The reason for developing this library is that I needed something that I couldn't find in any other jsonrpc library, which is passing a context with every call.

Imagine you're running an http server where you receive restful requests. A user might pass a request that is json-rpc. Given that authentication (whether it's username/password or API key) shouldn't be in the rpc-call itself, for security reasons, how would a handler know how to create a stateful response based on the user id or authentication token?

There are ways to solve this problem, by wrapping everything in stateful classes. But this doesn't only create a performance hit, but can easily create thread-safety problems and can complicate the design.

AsyncJsonRPC for the rescue! So now, you can simply pass as many context variables to every RPC call in a thread-safe manner. Here's an example:

```c++
    // you can add as many context types as you want! It's variadic.
    AsyncJsonRPC<ContextType1, ContextType2> rpc; 

    // here we add a handler for the method "mymethod", which takes two json parameters, an integer and a string
    rpc.addHandler(
        [](const Json::Value& request, Json::Value& response, ContextType1 ctx1, ContextType2 ctx2) {
            // handle the call with the context variables
        },
        "mymethod", {{"p1", Json::ValueType::intValue}, {"p2", Json::ValueType::stringValue}});

   // here we set the callback for any json call
    rpc.setResponseCallback([](std::string&& res) {
        // res has the json response of the server
    });

    // post is synchronous, and asyncPost is asynchronous
    rpc.post(
        R"({"jsonrpc": "2.0", "method": "testmethod1", "params": {"p1": 5, "p2": "HiThere!!!"},
                "id": 4})",
        "TheString");
```

# Dependencies
AsyncJsonRPC depends on two libraries:
1. libjsoncpp
2. boost containers

**libjsoncpp** is required for json parsing. That cannot be changed.

**Boost** is necessary just for a [`flat_map`](https://www.boost.org/doc/libs/1_65_1/doc/html/boost/container/flat_map.html), which I used instead of `std::map`. This is done purely for performance. A `flat_map` uses a vector underneath, which guarantees cache locality and makes look-ups very fast. A hash-map isn't very suitable if performance is to be sought, because hashing strings isn't that fast. With a `flat_map`, for this specific problem where methods will be added once and will never be changed later. 

**But I don't want boost!** in that case, just change the type from `flat_map` to `std::map` if performance isn't a big deal for you.

**Conan as a dependency manager**: Conan retrieves boost for you and compiles it automatically for you. It's not necessary if you want to use your system version of boost. Feel free to change the `CMakeLists.txt` file and remove conan.

### Thread-safety
Thread-safety is meant to be satisfied during operation. The only thread-safe methods are `AsyncJsonRPC::post()` and `AsyncJsonRPC::asyncPost()`. Anything else is **not thread-safe**, by design! Either that, all I'll have to stack mutexes all over the place. I'm sure you don't want that :-)

So, it's expected that you define your methods, handlers, callback in the main-thread, then start with the heavy-load stuff.

### Thread, memory, undefined behavior and other safety checks

For quality assurance, you can build the project with clang-sanitizers enable. Please enable one only at a time. The following are the CMake options to enable:

- `SANITIZE_THREAD`
- `SANITIZE_UNDEFINED`
- `SANITIZE_ADDRESS`
- `SANITIZE_LEAK`

For example, run cmake with `-DSANITIZE_LEAK=ON` to enable leak sanitizer.

### Building
You **do not** need to build this library to use it. Just include `#include "asyncjsonrpc/AsyncJsonRPC.h"` and it'll work. It's header only.

You may, however, build the tests! Feel free to build and run them. The `CMakeLists.txt` file in the root directory does that for you.
