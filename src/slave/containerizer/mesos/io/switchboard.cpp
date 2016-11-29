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

#include <string>

#include <process/defer.hpp>
#include <process/future.hpp>
#include <process/owned.hpp>

#include "slave/containerizer/mesos/io/switchboard.hpp"

using std::string;

using process::Future;
using process::Owned;
using process::PID;

using mesos::slave::ContainerConfig;
using mesos::slave::ContainerLaunchInfo;
using mesos::slave::ContainerLogger;
using mesos::slave::Isolator;

namespace mesos {
namespace internal {
namespace slave {

Try<Isolator*> IOSwitchboardIsolatorProcess::create(
    const Flags& flags,
    bool local)
{
  Try<ContainerLogger*> logger =
    ContainerLogger::create(flags.container_logger);

  if (logger.isError()) {
    return Error("Cannot create container logger: " + logger.error());
  }

  return new MesosIsolator(Owned<MesosIsolatorProcess>(
      new IOSwitchboardIsolatorProcess(
          flags,
          local,
          Owned<ContainerLogger>(logger.get()))));
}


IOSwitchboardIsolatorProcess::IOSwitchboardIsolatorProcess(
    const Flags& _flags,
    bool _local,
    Owned<ContainerLogger> _logger)
  : flags(_flags),
    local(_local),
    logger(_logger) {}


IOSwitchboardIsolatorProcess::~IOSwitchboardIsolatorProcess() {}


bool IOSwitchboardIsolatorProcess::supportsNesting()
{
  return true;
}


Future<Option<ContainerLaunchInfo>> IOSwitchboardIsolatorProcess::prepare(
    const ContainerID& containerId,
    const ContainerConfig& containerConfig)
{
  if (local) {
    return None();
  }

  return logger->prepare(
      containerConfig.executor_info(),
      containerConfig.directory(),
      containerConfig.has_user()
        ? Option<string>(containerConfig.user())
        : None())
    .then(defer(
        PID<IOSwitchboardIsolatorProcess>(this),
        &IOSwitchboardIsolatorProcess::_prepare,
        lambda::_1));
}


Future<Option<ContainerLaunchInfo>> IOSwitchboardIsolatorProcess::_prepare(
    const ContainerLogger::SubprocessInfo& loggerInfo)
{
  ContainerLaunchInfo launchInfo;
  ContainerLaunchInfo::IOInfo* io = launchInfo.mutable_io();

  switch (loggerInfo.out.type()) {
    case ContainerLogger::SubprocessInfo::IO::Type::FD:
      io->mutable_out()->set_type(ContainerLaunchInfo::IOInfo::IO::FD);
      io->mutable_out()->set_fd(loggerInfo.out.fd().get());
      break;
    case ContainerLogger::SubprocessInfo::IO::Type::PATH:
      io->mutable_out()->set_type(ContainerLaunchInfo::IOInfo::IO::PATH);
      io->mutable_out()->set_path(loggerInfo.out.path().get());
      break;
    default:
      UNREACHABLE();
  }

  switch (loggerInfo.err.type()) {
    case ContainerLogger::SubprocessInfo::IO::Type::FD:
      io->mutable_err()->set_type(ContainerLaunchInfo::IOInfo::IO::FD);
      io->mutable_err()->set_fd(loggerInfo.err.fd().get());
      break;
    case ContainerLogger::SubprocessInfo::IO::Type::PATH:
      io->mutable_err()->set_type(ContainerLaunchInfo::IOInfo::IO::PATH);
      io->mutable_err()->set_path(loggerInfo.err.path().get());
      break;
    default:
      UNREACHABLE();
  }

  return launchInfo;
}

} // namespace slave {
} // namespace internal {
} // namespace mesos {
