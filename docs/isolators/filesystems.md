---
title: Apache Mesos - Filesystem Isolators in Mesos Containerizer
layout: documentation
---

# Filesystem Isolators in Mesos Containerizer

[Mesos Containerizer](../mesos-containerizer.md) has several
'filesystem' isolators that are used to provide isolation for
container's filesystems. Usually, each platform has a corresponding
filesystem isolator associated with it, because the level of isolation
depends on the capabilities of that platform.

Currently, Mesos Containerizer supports
[`filesystem/posix`](#filesystemposix-isolator) and
[`filesystem/linux`](#filesystemlinux-isolator) isolators.
[`filesystem/shared`](filesystem-shared.md) isolator has a subset of
the features provided by the
[`filesystem/linux`](#filesystemlinux-isolator) isolator, thus is not
recommended and will be deprecated.

If you are using Mesos Containerizer, at least one of the filesystem
isolator needs to be specified through the `--isolation` flag. If a
user does not specify any filesystem isolator, Mesos Containerizer
will by default use the
[`filesystem/posix`](#filesystemposix-isolator) isolator.

Filesystem isolator is a pre-requisite for all the [container volume
isolators](../container-volume.md) isolators because it provides some
basic functionalities that the volume isolators depends on. For
example, the [`filesystem/linux`](#filesystemlinux-isolator) isolator
will create a new mount namespace for the container so that any volume
mounts made by the volume isolators will be hidden from the host mount
namespace.

Filesystem isolator is also responsible for preparing [persistent
volumes](../persistent-volume.md) for containers.

## `filesystem/posix` isolator

[`filesystem/posix`](#filesystemposix-isolator) isolator works on all
POSIX systems. It isolates container sandboxes and persistent volumes
using UNIX file permissions.

All containers share the same host filesystem. As a result, if you
want to specify a [container image](../container-image.md) for the
container, you cannot use this isolator. Use
[`filesystem/linux`](#filesystemlinux-isolator) isolator instead.

It handles [persistent volumes](../persistent-volume.md) by creating
symlinks in the container's sandbox that point to the actual
persistent volumes on the host filesystem.

## `filesystem/linux` isolator

[`filesystem/linux`](#filesystemlinux-isolator) isolator works only on
Linux. It isolates the filesystems of containers using the following
primitives:

* Each container gets its own mount namespace. The default [mount
  propagation](https://www.kernel.org/doc/Documentation/filesystems/sharedsubtree.txt)
  in each container is set to 'slave'.
* Use UNIX file permissions to protect container sandboxes and
  persistent volumes.

Container is allowed to define its own [image](../container-image.md).
If a container image is specified, by default, the container won't be
able to see files and directories on the host filesystem.

It handles [persistent volumes](../persistent-volume.md) by bind
mounting persistent volumes into the container's sandbox.
