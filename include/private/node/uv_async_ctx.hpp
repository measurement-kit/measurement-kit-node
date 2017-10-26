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

// # UvAsyncCtx
//
// In the common case, MK callbacks are called in the context of an MK private
// background thread from which Node API cannot be called directly.
//
// UvAsyncCtx is the class we use to schedule MK callbacks to execute in
// the context of libuv I/O loop (i.e. Node's I/O loop).
//
// A UvAsyncCtx is created using the Make() static factory that returns an
// instance of UvAsyncCtx allocated on the heap and managed through a null-safe
// shared pointer. Because UvAsyncCtx is an async class, it contains static
// methods that receive as first argument such smart pointer, to ensure that
// the static methods operating on the object also keep it alive.
//
// The Make() factory will register an `uv_async_t` with libuv event loop that
// will prevent the loop from returning. Make() will also initialize a self
// reference that will prevent UvAsyncCtx from being destroyed as long as the
// C code of libuv is using it.
//
// After you have created a UvAsyncCtx instance using Make(), you should keep
// it alive inside MK callbacks using lambda closures. This will guarantee that
// the UvAsyncCtx lives not shorter than the MK object using it.
//
// When MK callbacks, you should schedule a second callback on UvAsyncCtx
// keeping safe all the callback arguments into a lambda closure. To this end,
// use the SuspendOn() method. In turn, libuv will Resume() the suspended
// calls, which will execute in the context of libuv's event loop.
//
// When the MK object dies, you should call UvAsyncCtx StartDelete() method,
// which will tell libuv that it is done with this async object. In turn, libuv
// will remove the `uv_async_t` from the event loop, and will later call the
// FinishDelete() callback when done. Only at this point the self reference can
// be removed because libuv is done with the object. Note that removing the
// `uv_async_t` from libuv event loop may cause the loop to terminate if that
// was the last handle keeping the loop alive.
//
// ## Methods
template <MK_MOCK(uv_async_init), MK_MOCK(uv_async_send)> class UvAsyncCtx {
  public:
    // Make() constructs a UvAsyncCtx instance.
    static Var<UvAsyncCtx> Make() {
        Var<UvAsyncCtx> self{new UvAsyncCtx};
        self->self = self; // Self reference modeling usage by libuv C code
        self->async.data = self.get();
        if (uv_async_init(uv_default_loop(), &self->async, mkuv_resume)) {
            throw std::runtime_error("uv_async_init");
        }
        return self;
    }

    // SuspendOn() suspends the execution of `f` in the context of an MK thread
    // so that later it can be resumed in the context of libuv loop. Since libuv
    // may coalesce multiple uv_async_send() calls, we use a list to keep track
    // of all the callbacks that need to be resumed. Of course, this method is
    // thread safe, since multiple threads can operate on the list. It is key
    // to move `f` so to give libuv's thread unique ownership.
    static void SuspendOn(Var<UvAsyncCtx> self, std::function<void()> &&func) {
        std::unique_lock<std::recursive_mutex> _{self->mutex};
        self->suspended.push_back(std::move(func));
        if (uv_async_send(&self->async) != 0) {
            throw std::runtime_error("uv_async_send");
        }
    }

    // Resume() is called by libuv loop to resume execution. The `async`
    // pointer used by libuv needs to be converted into an instance of the
    // UvAsyncCtx class. This method is also thread safe, since it needs
    // to extract from the list shared with MK threads. Also for thread
    // safety we use move semantic to remove from the list. As in many other
    // parts of MK, we treat exceptions as unexpected, fatal errors and we
    // do not filter them.
    static void Resume(uv_async_t *async) {
        UvAsyncCtx *context = static_cast<UvAsyncCtx *>(async->data);
        Var<UvAsyncCtx> self = context->self; // Keepalive until end of scope
        std::list<std::function<void()>> functions;
        {
            std::unique_lock<std::recursive_mutex> _{self->mutex};
            std::swap(self->suspended, functions);
        }
        for (auto &fn : functions) {
            fn();
        }
    }

    // StartDelete() initiates a delete operation of an UvAsyncCtx. We need to
    // call SuspendOn() because we have experimentally noticed that on Linux
    // it will not work if we call uv_close() from a non-libuv thread.
    static void StartDelete(Var<UvAsyncCtx> self) {
        SuspendOn(self, [self]() {
            uv_close((uv_handle_t *)&self->async, mkuv_delete);
        });
    }

    // FinishDelete() is called by libuv. We need to convert the generic
    // libuv `handle` pointer to a UvAsyncCtx instance.
    static void FinishDelete(uv_handle_t *handle) {
        uv_async_t *async = reinterpret_cast<uv_async_t *>(handle);
        UvAsyncCtx *context = static_cast<UvAsyncCtx *>(async->data);
        Var<UvAsyncCtx> self = context->self; // Keepalive until end of scope
        self->self = nullptr;
    }

  private:
    // The UvAsyncCtx() constructor is private to enforce usage of
    // this class through the Make() factory.
    UvAsyncCtx() {}

    // The `async` structure is used to wakeup libuv's event loop and running
    // callbacks in it context. Since uv_async_init() until FinishDelete(),
    // we must not delete the UvAsyncCtx class. The existence of a registered
    // `uv_async_t *` will also prevent uv_loop() from exiting.
    uv_async_t async{};

    // The mutex structure protects `suspended` and `async` (and possibly other
    // fields) from modification by concurrent threads of execution.
    std::recursive_mutex mutex;

    // The suspended field is the list of suspended callbacks.
    std::list<std::function<void()>> suspended;

    // The self field is used to keep the object alive until it can be
    // safely deleted. Libuv callbacks manipulating this class should
    // keep a copy of self on the stack, for correctness, to enforce a
    // lifecycle equal at least to the current scope.
    Var<UvAsyncCtx> self;
};

} // namespace node
} // namespace mk

// The mkuv_resume() C callback is called by libuv's I/O loop thread
// to resume execution of the suspended callbacks.
static inline void mkuv_resume(uv_async_t *async) {
    mk::node::UvAsyncCtx<>::Resume(async);
}

// The mkuv_delete() C callback is called by libuv's I/O loop thread when
// libuv has successfully closed the async handle, to tell us that now it's
// safe to dispose of the memory associated with such handle.
static inline void mkuv_delete(uv_handle_t *handle) {
    mk::node::UvAsyncCtx<>::FinishDelete(handle);
}

#endif
