// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License

#ifndef __PROCESS_LOOP_HPP__
#define __PROCESS_LOOP_HPP__

#include <process/defer.hpp>
#include <process/dispatch.hpp>
#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/pid.hpp>
#include <process/process.hpp>

namespace process {

// Provides an asynchronous "loop" abstraction. This abstraction is
// helpful for code that would have synchronously been written as a
// loop but asynchronously ends up being a recursive set of functions
// which depending on the compiler may result in a stack overflow
// (i.e., a compiler that can't do sufficient tail call optimization
// may add stack frames for each recursive call).
//
// The loop abstraction takes a PID `pid` and uses it as the execution
// context to run the loop. The implementation does a `defer` on this
// `pid` to "pop" the stack when it needs to asynchronously
// recurse. This also lets callers synchronize execution with other
// code dispatching and deferring using `pid`.
//
// The two functions passed to the loop represent the loop "iterate"
// step and the loop "body" step respectively. Each invocation of
// "iterate" returns the next value and the "body" returns whether or
// not to continue looping (as well as any other processing necessary
// of course). You can think of this synchronously as:
//
//     bool condition = true;
//     do {
//       condition = body(iterate());
//     } while (condition);
//
// Asynchronously using recursion this might look like:
//
//     Future<Nothing> loop()
//     {
//       return iterate()
//         .then([](T t) {
//           if (body(t)) {
//             return loop();
//           }
//           return Nothing();
//         });
//     }
//
// And asynchronously using `pid` as the execution context:
//
//     Future<Nothing> loop()
//     {
//       return dispatch(pid, []() {
//         return iterate()
//           .then(defer(pid, [](T t) {
//             if (body(t)) {
//               return loop();
//             }
//             return Nothing();
//           }));
//       });
//     }
//
// And now what this looks like using `loop`:
//
//     loop(self(),
//          []() { return iterate(); },
//          [](T t) {
//            return body(t);
//          });
//
// TODO(benh): Provide an implementation that doesn't require a `pid`
// for situations like `io::read` and `io::write` where for
// performance reasons it could make more sense to NOT defer but
// rather just let the I/O thread handle the execution.
template <typename Iterate, typename Body>
Future<Nothing> loop(const UPID& pid, Iterate&& iterate, Body&& body);


// A helper for `loop` which creates a Process for us to provide an
// execution context for running the loop.
template <typename Iterate, typename Body>
Future<Nothing> loop(Iterate&& iterate, Body&& body)
{
  ProcessBase* process = new ProcessBase();
  spawn(process);
  return loop(
      process->self(),
      std::forward<Iterate>(iterate),
      std::forward<Body>(body))
    .onAny(defer([=](const Future<Nothing>&) {
      // NOTE: must `defer` here so we don't deadlock waiting on
      // `process` in the likely event that the loop is completed
      // while executing within `process`.
      terminate(process);
      wait(process);
      delete process;
    }));
}


namespace internal {

template <typename Iterate, typename Body, typename T>
struct Loop
{
  Loop(const UPID& pid, Iterate&& iterate, Body&& body)
    : pid(pid),
      iterate(std::forward<Iterate>(iterate)),
      body(std::forward<Body>(body)) {}

  UPID pid;
  Iterate iterate;
  Body body;
  Promise<Nothing> promise;
  Future<T> future;
  Future<bool> condition;
};


template <typename Iterate, typename Body, typename T>
void run(Owned<Loop<Iterate, Body, T>> loop)
{
  while (loop->future.isReady()) {
    loop->condition = loop->body(loop->future.get());
    if (loop->promise.future().hasDiscard()) {
      loop->condition.discard();
    }
    if (loop->condition.isReady()) {
      if (loop->condition.get()) {
        loop->future = loop->iterate();
        if (loop->promise.future().hasDiscard()) {
          loop->future.discard();
        }
        continue;
      } else {
        loop->promise.set(Nothing());
        return;
      }
    } else {
      loop->condition
        .onAny(defer(loop->pid, [=](const Future<bool>&) {
          if (loop->condition.isReady()) {
            if (loop->condition.get()) {
              loop->future = loop->iterate();
              if (loop->promise.future().hasDiscard()) {
                loop->future.discard();
              }
              run(loop);
            } else {
              loop->promise.set(Nothing());
            }
          } else if (loop->condition.isFailed()) {
            loop->promise.fail(loop->condition.failure());
          } else if (loop->condition.isDiscarded()) {
            loop->promise.discard();
          }
        }));
      return;
    }
  }

  loop->future
    .onAny(defer(loop->pid, [=](const Future<T>&) {
      if (loop->future.isReady()) {
        run(loop);
      } else if (loop->future.isFailed()) {
        loop->promise.fail(loop->future.failure());
      } else if (loop->future.isDiscarded()) {
        loop->promise.discard();
      }
    }));
}

} // namespace internal {


template <typename Iterate, typename Body>
Future<Nothing> loop(const UPID& pid, Iterate&& iterate, Body&& body)
{
  using T =
    typename internal::unwrap<typename result_of<Iterate()>::type>::type;

  Owned<internal::Loop<Iterate, Body, T>> loop(
      new internal::Loop<Iterate, Body, T>(
          pid,
          std::forward<Iterate>(iterate),
          std::forward<Body>(body)));

  // Start the loop using `pid` as the execution context.
  dispatch(pid, [=]() {
    loop->future = loop->iterate();
    if (loop->promise.future().hasDiscard()) {
      loop->future.discard();
    }
    internal::run(loop);
  });

  // Make sure we propagate discarding. Note that to avoid an infinite
  // memory bloat we explicitly don't add a new `onDiscard` callback
  // for every new future that gets created from invoking `iterate()`
  // or `body()` but instead discard those futures explicitly with our
  // single callback here.
  loop->promise.future()
    .onDiscard(defer(pid, [=]() {
      // NOTE: There's no race here between setting `loop->future` or
      // `loop->condition` and calling `discard()` on those futures
      // because we're serializing execution via `defer` on `pid`. An
      // alternative would require something like `atomic_shared_ptr`
      // or a mutex.
      loop->future.discard();
      loop->condition.discard();
    }));

  return loop->promise.future();
}

} // namespace process {

#endif // __PROCESS_LOOP_HPP__
