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

#include <unistd.h>

#include <glog/logging.h>

#include <gmock/gmock.h>

#include <process/gtest.hpp>

#include <stout/gtest.hpp>

#include <stout/os/shell.hpp>

#include "tests/mesos.hpp"

using namespace process;

using std::string;
using std::vector;

using mesos::internal::master::Master;

using mesos::internal::slave::Slave;

namespace mesos {
namespace internal {
namespace tests {

#ifdef __linux__
class ProvisionerCvmfsTest : public MesosTest
{
protected:
  virtual void TearDown()
  {
    // Try to remove any mounts under sandbox.
    if (::geteuid() == 0) {
      Try<string> umount = os::shell(
          "grep '%s' /proc/mounts | "
          "cut -d' ' -f2 | "
          "xargs --no-run-if-empty umount -l",
          sandbox.get().c_str());

      if (umount.isError()) {
        LOG(ERROR) << "Failed to umount for sandbox '" << sandbox.get()
                   << "': " << umount.error();
      }
    }

    MesosTest::TearDown();
  }
};


TEST_F(ProvisionerCvmfsTest, ROOT_CommandTask)
{
  Try<PID<Master>> master = StartMaster();
  ASSERT_SOME(master);

  slave::Flags flags = CreateSlaveFlags();
  flags.isolation = "filesystem/linux";
  flags.image_providers = "cvmfs";
  flags.image_provisioner_backend = "bind";

  Try<PID<Slave>> slave = StartSlave(flags);
  ASSERT_SOME(slave);

  MockScheduler sched;

  MesosSchedulerDriver driver(
      &sched, DEFAULT_FRAMEWORK_INFO, master.get(), DEFAULT_CREDENTIAL);

  EXPECT_CALL(sched, registered(_, _, _));

  Future<vector<Offer>> offers;
  EXPECT_CALL(sched, resourceOffers(_, _))
    .WillOnce(FutureArg<1>(&offers))
    .WillRepeatedly(Return());      // Ignore subsequent offers.

  driver.start();

  AWAIT_READY(offers);
  EXPECT_FALSE(offers.get().empty());

  Offer offer = offers.get()[0];

  TaskInfo task = createTask(
      offer.slave_id(),
      Resources::parse("cpus:1;mem:128").get(),
      "ls -al /");

  ContainerInfo container;
  container.set_type(ContainerInfo::MESOS);

  Image* image = container.mutable_mesos()->mutable_image();
  image->set_type(Image::CVMFS);

  Image::Cvmfs* cvmfs = image->mutable_cvmfs();
  cvmfs->set_repository("mesosphere.com");
  cvmfs->set_path("precise");

  task.mutable_container()->CopyFrom(container);

  Future<TaskStatus> status1;
  Future<TaskStatus> status2;
  EXPECT_CALL(sched, statusUpdate(&driver, _))
    .WillOnce(FutureArg<1>(&status1))
    .WillOnce(FutureArg<1>(&status2))
    .WillRepeatedly(Return());       // Ignore subsequent updates.

  driver.launchTasks(offers.get()[0].id(), {task});

  AWAIT_READY(status1);
  EXPECT_EQ(task.task_id(), status1.get().task_id());
  EXPECT_EQ(TASK_RUNNING, status1.get().state());

  AWAIT_READY(status2);
  EXPECT_EQ(task.task_id(), status2.get().task_id());
  EXPECT_EQ(TASK_FINISHED, status2.get().state());

  driver.stop();
  driver.join();

  Shutdown();
}
#endif // __linux__

} // namespace tests {
} // namespace internal {
} // namespace mesos {
