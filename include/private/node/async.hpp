// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_NODE_SYNC_HPP
#define PRIVATE_NODE_SYNC_HPP

#include "private/common/compat.hpp"
#include <list>
#include <mutex>
#include <uv.h>

/// # async
///
/// In the common case, MK callbacks are called in the context of an MK private
/// background thread from which Node API cannot be called directly.
///
/// `async` is the namespace that we use to schedule MK callbacks to execute
/// in the context of libuv I/O loop (i.e. Node's I/O loop). It contains
/// template functions whose arguments are libuv APIs, for testability. When
/// we execute code in the common case we use as template arguments real
/// libuv functions. In tests, we shall use mocked functions.
///
/// In this file we have only inline functions and template functions. We
/// inline all the code because it's pretty small and having all inside of
/// this private header in our opinion increases code readability.
///
/// You should call make<>() to get an instance of Context allocated on the
/// heap and managed through SharedPtr, a null-safe shared pointer.
///
/// ```C++
///   auto async_ctx = mk::node::async::make<>().
/// ```
///
/// When you register a callback for an MK test, copy the shared pointer into
/// the lambda closure. When the callback is invoked by MK, you should call
/// suspend<>() passing it a lambda in which closure you should store arguments
/// received by MK's callback. Make sure that tempory arguments like pointers
/// are made persistent to the lambda by creating copies. For example:
///
/// ```C++
///   test.on_log([async_ctx](uint32_t level, const char *s) {
///       mk::node::async::suspend<>(async_ctx, [level, msg = std::string(s)] {
///           // Here you can use level and msg with Node's C++ API.
///       });
///   });
/// ```
///
/// Internally, suspend<>() will wakeup the libuv loop. This means that
/// eventually the suspended lambda will be called in the context of libuv
/// loop. Inside such lambda you can safely call Node APIs.
///
/// When you create a Context, this will register a pointer to its
/// `uv_async_t` field as a libuv handle with libuv's default loop. This implies
/// that libuv's loop will not exit as long as this handle is active. So
/// to avoid libuv's loop to run forever, we need to understand also how to
/// unregister such handle when we are done.
///
/// To understand that, we need to understand all the copies of the shared
/// pointer that are keeping it alive. We have one copy for each callback
/// we registered on the test (as shown above). Plus, we have one extra copy
/// meant to represent the fact that libuv is using the Context.
///
/// In measurement-kit >= v0.8.0, the way in which we internally run the test
/// should be such that, using the above pattern, the test should correctly
/// die sometime after the final callback has been called. This means we need
/// to concern ourself only with the self referencing smart pointer.
///
/// To remove such last copy, register a `on_destroy` handler for the test. This
/// will be triggered at the end of the test, as explained above. Pass to this
/// method a lambda referencing the shared pointer. This lambda must call the
/// start_delete() method. This method will internally make sure that libuv
/// knows we don't need the async handle anymore and clean it up.
///
/// ```C++
///     test.on_destroy([async_ctx]() {
///         mk::node::async:start_delete(async_ctx);
///     });
/// ```
///
/// If everything is fine, a Node program consisting of only the test should
/// correctly leave after the test is over. Otherwise, if you find Node stuck,
/// the first thing you should check is whether the internal test object is
/// actually destroyed (i.e. whether on_destroy is called).

extern "C" {
// Functions declared as extern C because the C++ FAQ recommends that the
// code called from C must be as such to maximize portability.
static inline void mkuv_resume(uv_async_t *handle);
static inline void mkuv_delete(uv_handle_t *handle);
}

namespace mk {
namespace node {
namespace async {

/// ## Context
///
/// Context is the class containing all the variables we need. It is a class
/// because we only use `struct` in MK when it's a real piece of POD.
///
/// ### Fields
class Context {
  public:
    /// The `async` structure is used to wakeup libuv's event loop and running
    /// callbacks in its context. Since uv_async_init() until finish_delete()
    /// we must not delete the Context class. The existence of a registered
    /// `uv_async_t *` will also prevent uv_loop() from exiting. We must init
    /// this field explicitly because it's a piece of POD.
    uv_async_t async{};

    /// The mutex structure is used to make this class thread safe.
    std::recursive_mutex mutex;

    /// The suspended field is the list of suspended callbacks.
    std::list<std::function<void()>> suspended;

    /// The self field is used to keep the object alive until it can be
    /// safely deleted. Libuv callbacks manipulating this class should
    /// keep a copy of self on the stack, for correctness, to enforce a
    /// lifecycle equal at least to the current scope.
    ///
    /// We could have used `std::enable_shared_from_this` but rather we
    /// decided to have an always active shared pointer to really
    /// gurantee that the object cannot be destroyed as long as the
    /// underlying libevent loop is using it.
    SharedPtr<Context> self;
};

/// make<>() constructs an Context instance. This function shall throw if an
/// unrecoverable error occurs, as we do in other places in MK.
template <MK_MOCK(uv_async_init)> static SharedPtr<Context> make() {
    SharedPtr<Context> ctx{new Context};
    ctx->self = ctx; // Self reference modeling usage by libuv C code
    ctx->async.data = ctx.get();
    if (uv_async_init(uv_default_loop(), &ctx->async, mkuv_resume)) {
        throw std::runtime_error("uv_async_init");
    }
    return ctx;
}

/// suspend<>() suspends the execution of `f` in the context of an MK thread
/// so that later it can be resumed in the context of libuv loop. As libuv
/// may coalesce multiple uv_async_send() calls, we use a list to keep track
/// of all the callbacks that need to be resumed. Of course, this method is
/// thread safe, since multiple threads can operate on the list. It is key
/// to move `f` so to give libuv's thread unique ownership.
template <MK_MOCK(uv_async_send)>
static void suspend(SharedPtr<Context> ctx, std::function<void()> &&func) {
    std::unique_lock<std::recursive_mutex> _{ctx->mutex};
    ctx->suspended.push_back(std::move(func));
    if (uv_async_send(&ctx->async) != 0) {
        throw std::runtime_error("uv_async_send");
    }
}

/// start_delete() initiates a delete operation of an Context. We need to
/// suspend() because we have experimentally noticed that on Linux it will
/// not work if we call uv_close() from a non-libuv thread.
///
/// We cannot delete the context right away because we must need to wait
/// for libuv to finish using it, which happens when mkuv_delete is called.
static void start_delete(SharedPtr<Context> ctx) {
    suspend(ctx,
            [ctx]() { uv_close((uv_handle_t *)&ctx->async, mkuv_delete); });
}

} // namespace async
} // namespace node
} // namespace mk

/// The mkuv_resume() C callback is called by libuv's I/O loop thread to resume
/// execution of the suspended callbacks.
///
/// The `handle` pointer used by libuv needs to be converted into an instance of
/// the Context class. This function is also thread safe, since it needs to
/// extract from the list shared with MK threads. Also for thread safety we use
/// move semantic to remove from the list. (As in many other parts of MK, we
/// treat exceptions as fatal errors and we do not filter them.)
static inline void mkuv_resume(uv_async_t *handle) {
    using namespace mk::node::async;
    using namespace mk;
    Context *pctx = static_cast<Context *>(handle->data);
    SharedPtr<Context> ctx = pctx->self; // Until end of scope, keep it safe
    std::list<std::function<void()>> functions;
    {
        std::unique_lock<std::recursive_mutex> _{ctx->mutex};
        std::swap(ctx->suspended, functions);
    }
    for (auto &fn : functions) {
        fn(); // As said above, exception are fatal, so don't catch them
    }
}

/// The mkuv_delete() C callback is called by libuv's I/O loop thread when
/// libuv has successfully closed the async handle, to tell us that now it's
/// safe to dispose of the memory associated with such handle.
static inline void mkuv_delete(uv_handle_t *handle) {
    using namespace mk::node::async;
    using namespace mk;
    uv_async_t *async_handle = reinterpret_cast<uv_async_t *>(handle);
    Context *pctx = static_cast<Context *>(async_handle->data);
    SharedPtr<Context> ctx;
    std::swap(ctx, pctx->self); // Remove ref and keep alive until end of scope
}

#endif
