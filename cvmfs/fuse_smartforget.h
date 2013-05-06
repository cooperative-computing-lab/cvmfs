/**
 * This file is part of the CernVM File System.
 */

#ifndef CVMFS_FUSE_SMARTFORGET_
#define CVMFS_FUSE_SMARTFORGET_

#include <string>

namespace smartforget {

void Init(const std::string &mountpoint);
void Spawn();
void Trigger();
void Fini();

}  // namespace smartforget

#endif  // CVMFS_FUSE_SMARTFORGET_
