/**
 * This file is part of the CernVM File System.
 */

#include "fuse_smartforget.h"

#include <pthread.h>
#include <dirent.h>

#include <cassert>

#include "platform.h"
#include "util.h"
#include "logging.h"

using namespace std;  // NOLINT

namespace smartforget {

string *mountpoint_;
bool spawned;
pthread_t thread_trigger_;
int pipe_trigger_[2];


void Init(const string &mountpoint) {
  mountpoint_ = new string(mountpoint);
  spawned = false;
  MakePipe(pipe_trigger_);
}


static void *MainTrigger(void *data __attribute__((unused))) {
  LogCvmfs(kLogCvmfs, kLogDebug, "starting forget trigger thread");
  char ctrl;
  while (read(pipe_trigger_[0], &ctrl, 1) == 1) {
    if (ctrl == 0)
      break;
    if (ctrl == 'T') {
      LogCvmfs(kLogCvmfs, kLogDebug, "triggering top-level lookup/forget");
      platform_dirent64 *dirent;
      DIR *dirp = opendir(mountpoint_->c_str());
      while ((dirent = platform_readdir(dirp)) != NULL) {
        platform_stat64 info;
        // TODO: do this with root credentials
        const string path = (*mountpoint_) + "/" + string(dirent->d_name);
        int retval = platform_lstat(path.c_str(), &info);
        LogCvmfs(kLogCvmfs, kLogDebug, "triggered %s, result %d",
                 path.c_str(), retval);
      }
      closedir(dirp);
    }
  }
  LogCvmfs(kLogCvmfs, kLogDebug, "terminating forget trigger thread");
  return NULL;
}


void Spawn() {
  int retval = pthread_create(&thread_trigger_, NULL, MainTrigger, NULL);
  assert(retval == 0);
  spawned = true;
}


void Trigger() {
  char trigger = 'T';
  WritePipe(pipe_trigger_[1], &trigger, 1);
}


void Fini() {
  if (spawned) {
    char fin = 0;
    WritePipe(pipe_trigger_[1], &fin, 1);
    close(pipe_trigger_[1]);
    pthread_join(thread_trigger_, NULL);
  } else {
    ClosePipe(pipe_trigger_);
  }

  delete mountpoint_;
  mountpoint_ = NULL;
}

}  // namespace smartforget
