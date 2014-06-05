// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/mount_fuse.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>

#include <algorithm>

#include "nacl_io/getdents_helper.h"
#include "nacl_io/kernel_handle.h"
#include "sdk_util/macros.h"


namespace nacl_io {

namespace {

struct FillDirInfo {
  FillDirInfo(GetDentsHelper* getdents, size_t num_bytes)
      : getdents(getdents), num_bytes(num_bytes), wrote_offset(false) {}

  GetDentsHelper* getdents;
  size_t num_bytes;
  bool wrote_offset;
};

}  // namespace

MountFuse::MountFuse() : fuse_ops_(NULL), fuse_user_data_(NULL) {}

Error MountFuse::Init(const MountInitArgs& args) {
  Error error = Mount::Init(args);
  if (error)
    return error;

  fuse_ops_ = args.fuse_ops;
  if (fuse_ops_ == NULL)
    return EINVAL;

  if (fuse_ops_->init) {
    struct fuse_conn_info info;
    fuse_user_data_ = fuse_ops_->init(&info);
  }

  return 0;
}

void MountFuse::Destroy() {
  if (fuse_ops_ && fuse_ops_->destroy)
    fuse_ops_->destroy(fuse_user_data_);
}

Error MountFuse::Access(const Path& path, int a_mode) {
  if (!fuse_ops_->access)
    return ENOSYS;

  int result = fuse_ops_->access(path.Join().c_str(), a_mode);
  if (result < 0)
    return -result;

  return 0;
}

Error MountFuse::Open(const Path& path,
                      int open_flags,
                      ScopedMountNode* out_node) {
  std::string path_str = path.Join();
  const char* path_cstr = path_str.c_str();
  int result = 0;

  struct fuse_file_info fi;
  memset(&fi, 0, sizeof(fi));
  fi.flags = open_flags;

  if (open_flags & (O_CREAT | O_EXCL)) {
    // According to the FUSE docs, open() is not called when O_CREAT or O_EXCL
    // is passed.
    mode_t mode = S_IRALL | S_IWALL;
    if (fuse_ops_->create) {
      result = fuse_ops_->create(path_cstr, mode, &fi);
      if (result < 0)
        return -result;
    } else if (fuse_ops_->mknod) {
      result = fuse_ops_->mknod(path_cstr, mode, dev_);
      if (result < 0)
        return -result;
    } else {
      return ENOSYS;
    }
  } else {
    // First determine if this is a regular file or a directory.
    if (fuse_ops_->getattr) {
      struct stat statbuf;
      result = fuse_ops_->getattr(path_cstr, &statbuf);
      if (result < 0)
        return -result;

      if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
        // This is a directory. Don't try to open, just create a new node with
        // this path.
        ScopedMountNode node(
            new MountNodeFuseDir(this, fuse_ops_, fi, path_cstr));
        Error error = node->Init(open_flags);
        if (error)
          return error;

        *out_node = node;
        return 0;
      }
    }

    // Existing file.
    if (open_flags & O_TRUNC) {
      // According to the FUSE docs, O_TRUNC does two calls: first truncate()
      // then open().
      if (!fuse_ops_->truncate)
        return ENOSYS;
      result = fuse_ops_->truncate(path_cstr, 0);
      if (result < 0)
        return -result;
    }

    if (!fuse_ops_->open)
      return  ENOSYS;
    result = fuse_ops_->open(path_cstr, &fi);
    if (result < 0)
      return -result;
  }

  ScopedMountNode node(new MountNodeFuseFile(this, fuse_ops_, fi, path_cstr));
  Error error = node->Init(open_flags);
  if (error)
    return error;

  *out_node = node;
  return 0;
}

Error MountFuse::Unlink(const Path& path) {
  if (!fuse_ops_->unlink)
    return ENOSYS;

  int result = fuse_ops_->unlink(path.Join().c_str());
  if (result < 0)
    return -result;

  return 0;
}

Error MountFuse::Mkdir(const Path& path, int perm) {
  if (!fuse_ops_->mkdir)
    return ENOSYS;

  int result = fuse_ops_->mkdir(path.Join().c_str(), perm);
  if (result < 0)
    return -result;

  return 0;
}

Error MountFuse::Rmdir(const Path& path) {
  if (!fuse_ops_->rmdir)
    return ENOSYS;

  int result = fuse_ops_->rmdir(path.Join().c_str());
  if (result < 0)
    return -result;

  return 0;
}

Error MountFuse::Remove(const Path& path) {
  ScopedMountNode node;
  Error error = Open(path, O_RDONLY, &node);
  if (error)
    return error;

  struct stat statbuf;
  error = node->GetStat(&statbuf);
  if (error)
    return error;

  node.reset();

  if ((statbuf.st_mode & S_IFMT) == S_IFDIR) {
    return Rmdir(path);
  } else {
    return Unlink(path);
  }
}

Error MountFuse::Rename(const Path& path, const Path& newpath) {
  if (!fuse_ops_->rename)
    return ENOSYS;

  int result = fuse_ops_->rename(path.Join().c_str(), newpath.Join().c_str());
  if (result < 0)
    return -result;

  return 0;
}

MountNodeFuse::MountNodeFuse(Mount* mount,
                             struct fuse_operations* fuse_ops,
                             struct fuse_file_info& info,
                             const std::string& path)
    : MountNode(mount),
      fuse_ops_(fuse_ops),
      info_(info),
      path_(path) {}

bool MountNodeFuse::CanOpen(int open_flags) {
  struct stat statbuf;
  Error error = GetStat(&statbuf);
  if (error)
    return false;

  // GetStat cached the mode in stat_.st_mode. Forward to MountNode::CanOpen,
  // which will check this mode against open_flags.
  return MountNode::CanOpen(open_flags);
}

Error MountNodeFuse::GetStat(struct stat* stat) {
  int result;
  if (fuse_ops_->fgetattr) {
    result = fuse_ops_->fgetattr(path_.c_str(), stat, &info_);
    if (result < 0)
      return -result;
  } else if (fuse_ops_->getattr) {
    result = fuse_ops_->getattr(path_.c_str(), stat);
    if (result < 0)
      return -result;
  } else {
    return ENOSYS;
  }

  // Also update the cached stat values.
  stat_ = *stat;
  return 0;
}

Error MountNodeFuse::VIoctl(int request, va_list args) {
  // TODO(binji): implement
  return ENOSYS;
}

Error MountNodeFuse::Tcflush(int queue_selector) {
  // TODO(binji): use ioctl for this?
  return ENOSYS;
}

Error MountNodeFuse::Tcgetattr(struct termios* termios_p) {
  // TODO(binji): use ioctl for this?
  return ENOSYS;
}

Error MountNodeFuse::Tcsetattr(int optional_actions,
                               const struct termios* termios_p) {
  // TODO(binji): use ioctl for this?
  return ENOSYS;
}

Error MountNodeFuse::GetSize(size_t* out_size) {
  struct stat statbuf;
  Error error = GetStat(&statbuf);
  if (error)
    return error;

  *out_size = stat_.st_size;
  return 0;
}

MountNodeFuseFile::MountNodeFuseFile(Mount* mount,
                                     struct fuse_operations* fuse_ops,
                                     struct fuse_file_info& info,
                                     const std::string& path)
    : MountNodeFuse(mount, fuse_ops, info, path) {}

void MountNodeFuseFile::Destroy() {
  if (!fuse_ops_->release)
    return;
  fuse_ops_->release(path_.c_str(), &info_);
}

Error MountNodeFuseFile::FSync() {
  if (!fuse_ops_->fsync)
    return ENOSYS;

  int datasync = 0;
  int result = fuse_ops_->fsync(path_.c_str(), datasync, &info_);
  if (result < 0)
    return -result;
  return 0;
}

Error MountNodeFuseFile::FTruncate(off_t length) {
  if (!fuse_ops_->ftruncate)
    return ENOSYS;

  int result = fuse_ops_->ftruncate(path_.c_str(), length, &info_);
  if (result < 0)
    return -result;
  return 0;
}

Error MountNodeFuseFile::Read(const HandleAttr& attr,
                              void* buf,
                              size_t count,
                              int* out_bytes) {
  if (!fuse_ops_->read)
    return ENOSYS;

  char* cbuf = static_cast<char*>(buf);

  int result = fuse_ops_->read(path_.c_str(), cbuf, count, attr.offs, &info_);
  if (result < 0)
    return -result;

  // Fuse docs say that a read() call will always completely fill the buffer
  // (padding with zeroes) unless the direct_io mount flag is set.
  // TODO(binji): support the direct_io flag
  if (static_cast<size_t>(result) < count)
    memset(&cbuf[result], 0, count - result);

  *out_bytes = count;
  return 0;
}

Error MountNodeFuseFile::Write(const HandleAttr& attr,
                               const void* buf,
                               size_t count,
                               int* out_bytes) {
  if (!fuse_ops_->write)
    return ENOSYS;

  int result = fuse_ops_->write(
      path_.c_str(), static_cast<const char*>(buf), count, attr.offs, &info_);
  if (result < 0)
    return -result;

  // Fuse docs say that a write() call will always write the entire buffer
  // unless the direct_io mount flag is set.
  // TODO(binji): What should we do if the user breaks this contract? Warn?
  // TODO(binji): support the direct_io flag
  *out_bytes = result;
  return 0;
}

MountNodeFuseDir::MountNodeFuseDir(Mount* mount,
                                   struct fuse_operations* fuse_ops,
                                   struct fuse_file_info& info,
                                   const std::string& path)
    : MountNodeFuse(mount, fuse_ops, info, path) {}

void MountNodeFuseDir::Destroy() {
  if (!fuse_ops_->releasedir)
    return;
  fuse_ops_->releasedir(path_.c_str(), &info_);
}

Error MountNodeFuseDir::FSync() {
  if (!fuse_ops_->fsyncdir)
    return ENOSYS;

  int datasync = 0;
  int result = fuse_ops_->fsyncdir(path_.c_str(), datasync, &info_);
  if (result < 0)
    return -result;
  return 0;
}

Error MountNodeFuseDir::GetDents(size_t offs,
                                 struct dirent* pdir,
                                 size_t count,
                                 int* out_bytes) {
  if (!fuse_ops_->readdir)
    return ENOSYS;

  bool opened_dir = false;
  int result;

  // Opendir is not necessary, only readdir. Call it anyway, if it is defined.
  if (fuse_ops_->opendir) {
    result = fuse_ops_->opendir(path_.c_str(), &info_);
    if (result < 0)
      return -result;

    opened_dir = true;
  }

  Error error = 0;
  GetDentsHelper getdents;
  FillDirInfo fill_info(&getdents, count);
  result = fuse_ops_->readdir(path_.c_str(),
                              &fill_info,
                              &MountNodeFuseDir::FillDirCallback,
                              offs,
                              &info_);
  if (result < 0)
    goto fail;

  // If the fill function ever wrote an entry with |offs| != 0, then assume it
  // was not given the full list of entries. In that case, GetDentsHelper's
  // buffers start with the entry at offset |offs|, so the call to
  // GetDentsHelpers::GetDents should use an offset of 0.
  if (fill_info.wrote_offset)
    offs = 0;

  // The entries have been filled in from the FUSE filesystem, now write them
  // out to the buffer.
  error = getdents.GetDents(offs, pdir, count, out_bytes);
  if (error)
    goto fail;

  return 0;

fail:
  if (opened_dir && fuse_ops_->releasedir) {
    // Ignore this result, we're already failing.
    fuse_ops_->releasedir(path_.c_str(), &info_);
  }

  return -result;
}

int MountNodeFuseDir::FillDirCallback(void* buf,
                                      const char* name,
                                      const struct stat* stbuf,
                                      off_t off) {
  FillDirInfo* fill_info = static_cast<FillDirInfo*>(buf);

  // It is OK for the FUSE filesystem to pass a NULL stbuf. In that case, just
  // use a bogus ino.
  ino_t ino = stbuf ? stbuf->st_ino : 1;

  // The FUSE docs say that the implementor of readdir can choose to ignore the
  // offset given, and instead return all entries. To do this, they pass
  // |off| == 0 for each call.
  if (off) {
    if (fill_info->num_bytes < sizeof(dirent))
      return 1;  // 1 => buffer is full

    fill_info->wrote_offset = true;
    fill_info->getdents->AddDirent(ino, name, strlen(name));
    fill_info->num_bytes -= sizeof(dirent);
    // return 0 => request more data. return 1 => buffer full.
    return fill_info->num_bytes > 0 ? 0 : 1;
  } else {
    fill_info->getdents->AddDirent(ino, name, strlen(name));
    fill_info->num_bytes -= sizeof(dirent);
    // According to the docs, we can never return 1 (buffer full) when the
    // offset is zero (the user is probably ignoring the result anyway).
    return 0;
  }
}


}  // namespace nacl_io
