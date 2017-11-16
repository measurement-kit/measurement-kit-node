// Part of measurement-kit <https://measurement-kit.github.io/>.
// Measurement-kit is free software under the BSD license. See AUTHORS
// and LICENSE for more information on the copying conditions.
#ifndef PRIVATE_NODE_UV_ASYNC_CTX_HPP
#define PRIVATE_NODE_UV_ASYNC_CTX_HPP

#include "private/common/compat.hpp"
#include <list>
#include <mutex>
#include <uv.h>

extern "C" {
static inline void mkuv_resume(uv_async_t *async);
static inline void mkuv_delete(uv_handle_t *handle);
}

namespace mk {
namespace node {

/// # UvAsyncCtx
///
/// In the common case, MK callbacks are called in the context of an MK private
/// background thread from which Node API cannot be called directly.
///
/// UvAsyncCtx is the template class we use to schedule MK callbacks to execute
/// in the context of libuv I/O loop (i.e. Node's I/O loop). The template
/// arguments are libuv APIs, for testability. UvAsyncCtx<> is the template
/// instance (hence the class) with default libuv APIs.
///
/// You should call make() to get an instance of UvAsyncCtx<> allocated on the
/// heap and managed through SharedPtr, a null-safe shared pointer.
///
/// ```C++
///   SharedPtr<UvAsyncCtx<>> async = UvAsyncCtx<>::make().
/// ```
///
/// When you register a callback for an MK test, copy the shared pointer into
/// the lambda closure. When the callback is invoked by MK, you should call
/// suspend() passing it a lambda in which closure you should store the arguments
/// received by MK's callback. Make sure that tempory arguments like pointers
/// are made persistent to the lambda by creating copies. For example:
///
/// ```C++
///   test.on_log([async](uint32_t level, const char *s) {
///       UvAsyncCtx<>::suspend(async, [level, msg = std::string(s)] {
///           // Here you can use level and msg with Node's C++ API.
///       });
///   });
/// ```
///
/// Internally, suspend() will wakeup the libuv loop. This means that eventually
/// the suspended lambda will be called in the context of libuv loop. Inside
/// such lambda you can safely call Node APIs.
///
/// When you create an UvAsyncCtx<>, this will register a pointer to its
/// `uv_async_t` field as a libuv handle with libuv's default loop. This implies
/// that libuv's default loop will not exit as long as this handle is active. So
/// to avoid libuv's loop to run forever, we need to understand also how to
/// unregister such handle when we are done.
///
/// To understand that, we need to understand all the copies of the shared
/// pointer that are keeping it alive. We have one copy for each callback
/// we registered on the test (as shown above). Plus, we have one extra copy
/// meant to represent the fact that libuv is using the UvAsyncCtx<>.
///
/// In measurement-kit >= v0.8.0, the way in which we internally run the test
/// should be such that, using the above pattern, the test should correctly
/// die sometime after the final callback has been called. This means we need
/// to concern ourself only with the self referencing smart pointer.
///
/// To remove such last copy, register a `on_destroy` handler for the test. This
/// will be triggered at the end of the test, as explained above. Pass to this
/// method a lambda referencing the shared pointer. This lambda must call the
/// start_delete() method. This method will internally make sure that libuv knows
/// we don't need the async handle anymore and clean it up.
///
/// ```C++
///     test.on_destroy([async]() {
///         UvAsyncWorker<>::StartDelete(async);
///     });
/// ```
///
/// If everything is fine, a Node program consisting of only the test should
/// correctly leave after the test is over. Otherwise, if you find Node stuck,
/// the first thing you should check is whether the internal test object is
/// actually destroyed (i.e. whether on_destroy is called).
///
/// ## Methods
template <MK_MOCK(uv_async_init), MK_MOCK(uv_async_send)> class UvAsyncCtx {
  public:
    /// make() constructs an UvAsyncCtx<> instance.
    static SharedPtr<UvAsyncCtx> make() {
        SharedPtr<UvAsyncCtx> self{new UvAsyncCtx};
        self->self = self; // Self reference modeling usage by libuv C code
        self->async.data = self.get();
        if (uv_async_init(uv_default_loop(), &self->async, mkuv_resume)) {
            throw std::runtime_error("uv_async_init");
        }
        return self;
    }

    /// suspend() suspends the execution of `f` in the context of an MK thread
    /// so that later it can be resumed in the context of libuv loop. As libuv
    /// may coalesce multiple uv_async_send() calls, we use a list to keep track
    /// of all the callbacks that need to be resumed. Of course, this method is
    /// thread safe, since multiple threads can operate on the list. It is key
    /// to move `f` so to give libuv's thread unique ownership.
    static void suspend(
            SharedPtr<UvAsyncCtx> self, std::function<void()> &&func) {
        std::unique_lock<std::recursive_mutex> _{self->mutex};
        self->suspended.push_back(std::move(func));
        if (uv_async_send(&self->async) != 0) {
            throw std::runtime_error("uv_async_send");
        }
    }

    /// resume() is called by libuv loop to resume execution. The `async`
    /// pointer used by libuv needs to be converted into an instance of the
    /// UvAsyncCtx<> class. This method is also thread safe, since it needs
    /// to extract from the list shared with MK threads. Also for thread
    /// safety we use move semantic to remove from the list. (As in many other
    /// parts of MK, we treat exceptions as unexpected, fatal errors and we
    /// do not filter them.)
    static void resume(uv_async_t *async) {
        UvAsyncCtx *context = static_cast<UvAsyncCtx *>(async->data);
        SharedPtr<UvAsyncCtx> self = context->self; // Until end of scope
        std::list<std::function<void()>> functions;
        {
            std::unique_lock<std::recursive_mutex> _{self->mutex};
            std::swap(self->suspended, functions);
        }
        for (auto &fn : functions) {
            fn();
        }
    }

    /// start_delete() initiates a delete operation of an UvAsyncCtx. We need to
    /// suspend() because we have experimentally noticed that on Linux it will
    /// not work if we call uv_close() from a non-libuv thread.
    static void start_delete(SharedPtr<UvAsyncCtx> self) {
        suspend(self, [self]() {
            uv_close((uv_handle_t *)&self->async, mkuv_delete);
        });
    }

    /// finish_delete() is called by libuv. We need to convert the generic
    /// libuv `handle` pointer to a UvAsyncCtx instance.
    static void finish_delete(uv_handle_t *handle) {
        uv_async_t *async = reinterpret_cast<uv_async_t *>(handle);
        UvAsyncCtx *context = static_cast<UvAsyncCtx *>(async->data);
        SharedPtr<UvAsyncCtx> self = context->self; // Until end of scope
        self->self = nullptr;
    }

  private:
    /// The UvAsyncCtx() constructor is private to enforce usage of
    /// this class through the make() factory.
    UvAsyncCtx() {}

    /// The `async` structure is used to wakeup libuv's event loop and running
    /// callbacks in its context. Since uv_async_init() until finish_delete()
    /// we must not delete the UvAsyncCtx class. The existence of a registered
    /// `uv_async_t *` will also prevent uv_loop() from exiting. We must init
    /// this field explicitly because it's a piece of POD.
    uv_async_t async{};

    /// The mutex structure protects `suspended` and `async` (and possibly other
    /// fields) from modification by concurrent threads of execution.
    std::recursive_mutex mutex;

    /// The suspended field is the list of suspended callbacks.
    std::list<std::function<void()>> suspended;

    /// The self field is used to keep the object alive until it can be
    /// safely deleted. Libuv callbacks manipulating this class should
    /// keep a copy of self on the stack, for correctness, to enforce a
    /// lifecycle equal at least to the current scope.
    SharedPtr<UvAsyncCtx> self;
};

} // namespace node
} // namespace mk

/// The mkuv_resume() C callback is called by libuv's I/O loop thread
/// to resume execution of the suspended callbacks.
static inline void mkuv_resume(uv_async_t *async) {
    mk::node::UvAsyncCtx<>::resume(async);
}

/// The mkuv_delete() C callback is called by libuv's I/O loop thread when
/// libuv has successfully closed the async handle, to tell us that now it's
/// safe to dispose of the memory associated with such handle.
static inline void mkuv_delete(uv_handle_t *handle) {
    mk::node::UvAsyncCtx<>::finish_delete(handle);
}

#endif
