// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef __MESOS_CONTAINERIZER_IO_SWITCHBOARD_HPP__
#define __MESOS_CONTAINERIZER_IO_SWITCHBOARD_HPP__

#include <string>

#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/socket.hpp>

#include <stout/try.hpp>

#include <mesos/slave/container_logger.hpp>

#include "slave/flags.hpp"

#include "slave/containerizer/mesos/isolator.hpp"

namespace mesos {
namespace internal {
namespace slave {

// The `IOSwitchboard` is a component in the agent whose job it is to
// instantiate an `IOSwitchboardServer` that can be used to feed the
// stdin to a container from an external source, as well as redirect
// the stdin/stdout of a container to multiple targets.
//
// The primary motivation of this component is to enable support in
// mesos similar to `docker attach` and `docker exec` whereby an
// external client can attach to the stdin/stdout/stderr of a running
// container as well as launch arbitrary subcommands inside a
// container and attach to its stdin/stdout/stderr.
//
// The I/O switchboard is integrated with `MesosContainerizer` through
// the `Isolator` interface.
class IOSwitchboard : public MesosIsolatorProcess
{
public:
  static Try<IOSwitchboard*> create(
      const Flags& flags,
      bool local);

  virtual ~IOSwitchboard();

  virtual bool supportsNesting();

  virtual process::Future<Option<mesos::slave::ContainerLaunchInfo>> prepare(
      const ContainerID& containerId,
      const mesos::slave::ContainerConfig& containerConfig);

  virtual process::Future<Nothing> cleanup(
      const ContainerID& containerId);

private:
  struct Info
  {
    Info(pid_t _pid, const process::Future<Option<int>>& _status)
      : pid(_pid),
        status(_status) {}

    pid_t pid;
    process::Future<Option<int>> status;
  };

  IOSwitchboard(
      const Flags& flags,
      bool local,
      process::Owned<mesos::slave::ContainerLogger> logger);

  process::Future<Option<mesos::slave::ContainerLaunchInfo>> _prepare(
      const ContainerID& containerId,
      const mesos::slave::ContainerLogger::SubprocessInfo& loggerInfo);

  Flags flags;
  bool local;
  process::Owned<mesos::slave::ContainerLogger> logger;
  hashmap<ContainerID, process::Owned<Info>> infos;
};


#ifndef __WINDOWS__
// The `IOSwitchboardServer` encapsulates the server side logic for
// redirecting the `stdin/stdout/stderr` of a container to/from
// multiple sources/targets. It runs an HTTP server over a unix domain
// socket in order to process incoming `ATTACH_CONTAINER_INPUT` and
// `ATTACH_CONTAINER_OUTPUT` calls and redirect a containers
// `stdin/stdout/stderr` through them. In 'local' mode, it is run
// inside the agent itself. In 'non-local' mode, it is run as an
// external process to survive agent restarts.
class IOSwitchboardServerProcess;


class IOSwitchboardServer
{
public:
  static constexpr char NAME[] = "mesos-io-switchboard";

  static Try<process::Owned<IOSwitchboardServer>> create(
      int stdinToFd,
      int stdoutFromFd,
      int stdoutToFd,
      int stderrFromFd,
      int stderrToFd,
      const std::string& socketPath);

  ~IOSwitchboardServer();

  process::Future<Nothing> run();

private:
  IOSwitchboardServer(
      int stdinToFd,
      int stdoutFromFd,
      int stdoutToFd,
      int stderrFromFd,
      int stderrToFd,
      const process::network::unix::Socket& socket);

  process::Owned<IOSwitchboardServerProcess> process;
};


// The set of flags to pass to the io switchboard server when launched
// in an external binary.
struct IOSwitchboardServerFlags : public virtual flags::FlagsBase
{
  IOSwitchboardServerFlags()
  {
    setUsageMessage(
      "Usage: " + stringify(IOSwitchboardServer::NAME) + " [options]\n"
      "The io switchboard server is designed to feed stdin to a container\n"
      "from an external source, as well as redirect the stdin/stdout of a\n"
      "container to multiple targets.\n"
      "\n"
      "It runs an HTTP server over a unix domain socket in order to process\n"
      "incoming `ATTACH_CONTAINER_INPUT` and `ATTACH_CONTAINER_OUTPUT` calls\n"
      "and redirect a containers `stdin/stdout/stderr` through them.\n"
      "\n"
      "The primary motivation of this component is to enable support in mesos\n"
      "similar to `docker attach` and `docker exec` whereby an external\n"
      "client can attach to the stdin/stdout/stderr of a running container as\n"
      "well as launch arbitrary subcommands inside a container and attach to\n"
      "its stdin/stdout/stderr.\n");

    add(&IOSwitchboardServerFlags::stdin_to_fd,
        "stdin_to_fd",
        "The file descriptor where incoming stdin data should be written.");

    add(&IOSwitchboardServerFlags::stdout_from_fd,
        "stdout_from_fd",
        "The file descriptor that should be read to consume stdout data.");

    add(&IOSwitchboardServerFlags::stdout_to_fd,
        "stdout_to_fd",
        "A file descriptor where data read from\n"
        "'stdout_from_fd' should be redirected to.");

    add(&IOSwitchboardServerFlags::stderr_from_fd,
        "stderr_from_fd",
        "The file descriptor that should be read to consume stderr data.");

    add(&IOSwitchboardServerFlags::stderr_to_fd,
        "stderr_to_fd",
        "A file descriptor where data read from\n"
        "'stderr_from_fd' should be redirected to.");

    add(&IOSwitchboardServerFlags::socket_path,
        "socket_address",
        "The path of the unix domain socket this\n"
        "io switchboard should attach itself to.");
  }

  int stdin_to_fd;
  int stdout_from_fd;
  int stdout_to_fd;
  int stderr_from_fd;
  int stderr_to_fd;
  std::string socket_path;
};
#endif // __WINDOWS__

} // namespace slave {
} // namespace internal {
} // namespace mesos {

#endif // __MESOS_CONTAINERIZER_IO_SWITCHBOARD_HPP__
