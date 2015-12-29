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

#include <process/dispatch.hpp>

#include <stout/path.hpp>

#include <stout/os/exists.hpp>

#include "slave/containerizer/mesos/provisioner/cvmfs/store.hpp"

using namespace process;

using std::string;

namespace mesos {
namespace internal {
namespace slave {
namespace cvmfs {

class StoreProcess : public Process<StoreProcess>
{
public:
  StoreProcess(const string& _root) : root(_root) {}

  Future<Nothing> recover();

  Future<ImageInfo> get(const mesos::Image& image);

private:
  const string root;
};


Try<Owned<slave::Store>> Store::create(const Flags& flags)
{
  if (!os::exists(flags.cvmfs_root)) {
    return Error("CVMFS root '" + flags.cvmfs_root + "' does not exist");
  }

  Owned<StoreProcess> process(new StoreProcess(flags.cvmfs_root));

  return Owned<slave::Store>(new Store(process));
}


Store::Store(const Owned<StoreProcess>& _process) : process(_process)
{
  spawn(CHECK_NOTNULL(process.get()));
}


Store::~Store()
{
  terminate(process.get());
  wait(process.get());
}


Future<Nothing> Store::recover()
{
  return dispatch(process.get(), &StoreProcess::recover);
}


Future<ImageInfo> Store::get(const mesos::Image& image)
{
  return dispatch(process.get(), &StoreProcess::get, image);
}


Future<Nothing> StoreProcess::recover()
{
  return Nothing();
}


Future<ImageInfo> StoreProcess::get(const mesos::Image& image)
{
  if (image.type() != mesos::Image::CVMFS) {
    return Failure("CVMFS privisioner store only supports CVMFS images");
  }

  const Image::Cvmfs& cvmfs = image.cvmfs();

  const string rootfs = path::join(root, cvmfs.repository(), cvmfs.path());
  if (!os::exists(rootfs)) {
    return Failure("Rootfs cannot be found at '" + rootfs + "'");
  }

  return ImageInfo{{rootfs}, None()};
}

} // namespace cvmfs {
} // namespace slave {
} // namespace internal {
} // namespace mesos {
