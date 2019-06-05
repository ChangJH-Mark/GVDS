//
// Created by yaowen on 4/29/19.
// 北航系统结构所-存储组
//

#define FUSE_USE_VERSION 31
#include <dirent.h>
#include <error.h>
#include <fuse3/fuse.h>
#include <fuse3/fuse_lowlevel.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctime>
#include <iostream>
#include <vector>
#include "client/client.h"
#include "client/fuse_mod.h"
#include "client/graph_mod.h"
#include "client/rpc_mod.h"
#include "client/zone_mod.h"
#include "client/zone_struct.h"
#include "context.h"
#include "io_proxy/rpc_types.h"

extern bool zonechecker_run;

#define HVS_FUSE_DATA \
  ((struct ::hvs::ClientFuseData *)fuse_get_context()->private_data)

using namespace std;

namespace hvs {
    // TODO: 工具函数，分割字符串
    std::vector<std::string> splitWithStl(const std::string str,const std::string pattern)
    {
      std::vector<std::string> resVec;

      if ("" == str)
      {
        return resVec;
      }
      //方便截取最后一段数据
      std::string strs = str + pattern;

      size_t pos = strs.find(pattern);
      size_t size = strs.size();

      while (pos != std::string::npos)
      {
        std::string x = strs.substr(0,pos);
        resVec.push_back(x);
        strs = strs.substr(pos+1,size);
        pos = strs.find(pattern);
      }
      return resVec;
    }

void *hvsfs_init(struct fuse_conn_info *conn, struct fuse_config *cfg) {
  return HVS_FUSE_DATA;
}

// stat
int hvsfs_getattr(const char *path, struct stat *stbuf,
                  struct fuse_file_info *fi) {
  std::vector<std::string> namev = splitWithStl(path, "/");
  int nvsize = static_cast<int>(namev.size());
  // root handle
  if (strcmp(path, "/") == 0) {
    auto t = time(nullptr);
    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_dev = 0;      // ignored
    stbuf->st_ino = 0;      // ignored
    stbuf->st_blksize = 0;  // ignored
    stbuf->st_mode = S_IFDIR | 0755;
    stbuf->st_nlink = 1;
    stbuf->st_uid = 0;
    stbuf->st_gid = 0;
    stbuf->st_size = 4096;
    stbuf->st_blocks = 8;
    stbuf->st_atim.tv_sec = t;
    stbuf->st_mtim.tv_sec = t;
    stbuf->st_ctim.tv_sec = t;
    return 0;
  }

  if (nvsize==2 && !namev[1].empty()){
      auto mapping = HVS_FUSE_DATA->client->zone->zonemap.find(namev[1]);
      if(mapping != HVS_FUSE_DATA->client->zone->zonemap.end()){
          auto t = time(nullptr);
          memset(stbuf, 0, sizeof(struct stat));
          stbuf->st_dev = 0;      // ignored
          stbuf->st_ino = 0;      // ignored
          stbuf->st_blksize = 0;  // ignored
          stbuf->st_mode = S_IFDIR | 0755;
          stbuf->st_nlink = 1;
          stbuf->st_uid = 1000 /*1000*/; // TODO: 权限模块修改
          stbuf->st_gid = 1000 /*1000*/; // TODO: 权限模块修改
          stbuf->st_size = 4096;
          stbuf->st_blocks = 8;
          stbuf->st_atim.tv_sec = t;
          stbuf->st_mtim.tv_sec = t;
          stbuf->st_ctim.tv_sec = t;
          return 0;
      }
  }

  if(nvsize < 3){
      return -ENOENT;
  }

  auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);

  auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }
  // dout(-1) << "INFO: fuse_client.getattr [" << (rpath + lpath).c_str() << "]"
  //          << dendl;

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_stat",
                                              (rpath + lpath).c_str());
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }

  auto retbuf = res->as<ioproxy_rpc_statbuffer>();
  if (retbuf.error_code) {
    // stat failed on remote server
    return retbuf.error_code;
  }

  memset(stbuf, 0, sizeof(struct stat));
  stbuf->st_dev = static_cast<__dev_t>(retbuf.st_dev);
  stbuf->st_ino = static_cast<__ino_t>(retbuf.st_ino);
  stbuf->st_mode = static_cast<__mode_t>(retbuf.st_mode);
  stbuf->st_nlink = static_cast<__nlink_t>(retbuf.st_nlink);
  stbuf->st_uid = static_cast<__uid_t>(retbuf.st_uid);
  stbuf->st_gid = static_cast<__gid_t>(retbuf.st_gid);
  stbuf->st_rdev = static_cast<__dev_t>(retbuf.st_rdev);
  stbuf->st_size = retbuf.st_size;
  stbuf->st_atim.tv_nsec = retbuf.st_atim_tv_nsec;
  stbuf->st_atim.tv_sec = retbuf.st_atim_tv_sec;
  stbuf->st_mtim.tv_nsec = retbuf.st_mtim_tv_nsec;
  stbuf->st_mtim.tv_sec = retbuf.st_mtim_tv_sec;
  stbuf->st_ctim.tv_nsec = retbuf.st_ctim_tv_nsec;
  stbuf->st_ctim.tv_sec = retbuf.st_ctim_tv_sec;
  return retbuf.error_code;
}

int hvsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi,
                  enum fuse_readdir_flags flags) {
  std::vector<std::string> namev = splitWithStl(path, "/");
  int nvsize = static_cast<int>(namev.size());
  // access space level
  if (strcmp(path, "/") == 0) {
    auto zones = HVS_FUSE_DATA->client->graph->list_zone();
    for (const auto &zone : zones) {
      if (filler(buf, zone.name.c_str(), nullptr, 0,
                 static_cast<fuse_fill_dir_flags>(0)) != 0) {
        return -ENOMEM;
      }
    }
    return 0;
  }

  if(nvsize == 2 && !namev[1].empty()){
      auto spaces = HVS_FUSE_DATA->client->graph->list_space(namev[1]);
      for (const auto &space : spaces) {
        if (filler(buf, space.name.c_str(), nullptr, 0,
                   static_cast<fuse_fill_dir_flags>(0)) != 0) {
          return -ENOMEM;
        }
      }
      return 0;
  }

    if(nvsize < 3){
        return -ENOENT;
    }

    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);

  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_readdir",
                                              (rpath + lpath).c_str());
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }

  int retstat = 0;
  for (const ioproxy_rpc_dirent &ent :
       res->as<std::vector<ioproxy_rpc_dirent>>()) {
    if (filler(buf, ent.d_name.c_str(), nullptr, 0,
               static_cast<fuse_fill_dir_flags>(0)) != 0) {
      return -ENOMEM;
    }
    retstat = ent.error_code;
  }

  return retstat;
}

int hvsfs_open(const char *path, struct fuse_file_info *fi) {
  int retstat = 0;
  return retstat;
}

int hvsfs_read(const char *path, char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  ioproxy_rpc_buffer _buffer((rpath + lpath).c_str(), offset, size);

  if (HVS_FUSE_DATA->fuse_client->use_udt) {
    // UDT version
    auto res = HVS_FUSE_DATA->client->rpc->read_data(iop, _buffer);
    if (!res) {
      return -ETIMEDOUT;
    }
    if (res->error_code < 0) {
      // stat failed on remote server
      return res->error_code;
    }
    memcpy(buf, res->buf.ptr, res->buf.size);
    return res->buf.size;
  } else {
    auto res = HVS_FUSE_DATA->client->rpc->call(
        iop, "ioproxy_read", (rpath + lpath).c_str(), size, offset);
    if (!res.get()) {
      // timeout exception raised
      return -ENOENT;
    }
    auto retbuf = res->as<ioproxy_rpc_buffer>();
    if (retbuf.error_code < 0) {
      // stat failed on remote server
      return retbuf.error_code;
    }
    memcpy(buf, retbuf.buf.ptr, retbuf.buf.size);
    return retbuf.buf.size;
  }
}

int hvsfs_write(const char *path, const char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  ioproxy_rpc_buffer _buffer((rpath + lpath).c_str(), buf, offset, size);
  _buffer.is_read = false;

  if (HVS_FUSE_DATA->fuse_client->use_udt) {
    // UDT version
    auto res = HVS_FUSE_DATA->client->rpc->write_data(iop, _buffer);
    return res;
  } else {
    // tcp version
    auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_write",
                                                (rpath + lpath).c_str(),
                                                _buffer, size, offset);
    if (!res.get()) {
      // timeout exception raised
      return -ENOENT;
    }
    auto retbuf = res->as<int>();
    return retbuf;
  }
  // write may failed on remote server
}

int hvsfs_access(const char *path, int mode) {
    //TODO: 目前是通过路径判断当前是空间还是区域状态
    std::vector<std::string> namev = splitWithStl(path, "/");
    int nvsize = static_cast<int>(namev.size());
  // access space level
  if (strcmp(path, "/") == 0) {
    return 0;
  }
    if (nvsize==2 && !namev[1].empty()){
        auto mapping = HVS_FUSE_DATA->client->zone->zonemap.find(namev[1]);
        if(mapping != HVS_FUSE_DATA->client->zone->zonemap.end()){
            return 0;
        }
    }
    if(nvsize < 3){
        return -ENOENT;
    }
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }
  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_access",
                                              (rpath + lpath).c_str(), mode);
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_opendir(const char *path, struct fuse_file_info *fi) {
  int retstat = 0;

  return retstat;
}

void hvsfs_destroy(void *private_data) {
    zonechecker_run = false;
    }

int hvsfs_truncate(const char *path, off_t offset, struct fuse_file_info *fi) {
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_truncate",
                                              (rpath + lpath).c_str(), offset);
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_readlink(const char *path, char *link, size_t size) {
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_readlink",
                                              (rpath + lpath).c_str(), size);
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }

  auto retbuf = res->as<std::string>();
  memcpy(link, retbuf.c_str(), retbuf.size());
  return retbuf.size();
}

int hvsfs_mknod(const char *path, mode_t mode, dev_t dev) {
  int retstat = 0;

  return retstat;
}
int hvsfs_mkdir(const char *path, mode_t mode) {
    std::vector<std::string> namev = splitWithStl(path, "/");
    int nvsize = static_cast<int>(namev.size());
  if (namev.size() <= 3) {
    return -EPERM;
  }

  // access content level
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_mkdir",
                                              (rpath + lpath).c_str(), mode);
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_unlink(const char *path) {
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_unlink",
                                              (rpath + lpath).c_str());
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_rmdir(const char *path) {
        //跳过本地
    std::vector<std::string> namev = splitWithStl(path, "/");
    int nvsize = static_cast<int>(namev.size());
    if (namev.size() <= 3) {
        return -EPERM;
    }
    // 删除远程
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_rmdir",
                                              (rpath + lpath).c_str());
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_symlink(const char *path, const char *newpath) {
  int retstat = 0;
  std::cout << "TODO: 软连接操作有问题，待修复！" << std::endl;
  //        std::string root("/");
  //        std::string paths(path);
  //        std::string fullpath = root+paths;
  //        auto ip = new std::string(ioproxy_ip.c_str());
  //        auto port =
  //        hvs::HvsContext::get_context()->_config->get<int>("rpc.port");
  //        RpcClient client(*ip, static_cast<const unsigned int>(*port));
  //        auto res = client.call("ioproxy_symlink", fullpath.c_str(),
  //        newpath); retstat = res->as<int>();
  return retstat;
}

int hvsfs_rename(const char *path, const char *newpath, unsigned int flags) {
    //跳过本地
    std::vector<std::string> namev = splitWithStl(path, "/");
    int nvsize = static_cast<int>(namev.size());
    if (namev.size() <= 3) {
        return -EPERM;
    }
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [nzonename, nspacename, nspaceuuid, nlpath] = HVS_FUSE_DATA->client->zone->locatePosition(newpath);

  // access content level
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_rename",
                                              (rpath + lpath).c_str(),
                                              (rpath + nlpath).c_str());
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_link(const char *path, const char *newpath) {
  int retstat = 0;
  std::cout << "TODO: 硬连接操作有问题，待修复！" << std::endl;
  //        auto ip = new std::string(ioproxy_ip.c_str());
  //        auto port =
  //        hvs::HvsContext::get_context()->_config->get<int>("rpc.port");
  //        RpcClient client(*ip, static_cast<const unsigned int>(*port));
  //        auto res = client.call("ioproxy_link", path, newpath);
  //        retstat = res->as<int>();
  return retstat;
}

int hvsfs_chmod(const char *path, mode_t mode, struct fuse_file_info *fi) {
    std::vector<std::string> namev = splitWithStl(path, "/");
    int nvsize = static_cast<int>(namev.size());
    if (namev.size() <= 3) {
        return -EPERM;
    }
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);

  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_chmod",
                                              (rpath + lpath).c_str(), mode);
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_chown(const char *path, uid_t uid, gid_t gid,
                struct fuse_file_info *fi) {
    std::vector<std::string> namev = splitWithStl(path, "/");
    int nvsize = static_cast<int>(namev.size());
    if (namev.size() <= 3) {
        return -EPERM;
    }
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(
      iop, "ioproxy_chown", (rpath + lpath).c_str(), uid, gid);
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_statfs(const char *, struct statvfs *) {
  int retstat = 0;

  return retstat;
}

int hvsfs_flush(const char *, struct fuse_file_info *) {
  int retstat = 0;

  return retstat;
}

int hvsfs_release(const char *, struct fuse_file_info *) {
  int retstat = 0;

  return retstat;
}

int hvsfs_fsync(const char *, int, struct fuse_file_info *) {
  int retstat = 0;

  return retstat;
}

int hvsfs_releasedir(const char *, struct fuse_file_info *) {
  int retstat = 0;

  return retstat;
}

int hvsfs_create(const char *path, mode_t mode, struct fuse_file_info *) {
    std::vector<std::string> namev = splitWithStl(path, "/");
    int nvsize = static_cast<int>(namev.size());
    if (namev.size() <= 3) {
        return -EPERM;
    }
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_create",
                                              (rpath + lpath).c_str(), mode);
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

int hvsfs_utimens(const char *path, const struct timespec tv[2],
                  struct fuse_file_info *fi) {
    std::vector<std::string> namev = splitWithStl(path, "/");
    int nvsize = static_cast<int>(namev.size());
    if (namev.size() <= 3) {
        return -EPERM;
    }
    auto [zonename, spacename, spaceuuid, lpath] = HVS_FUSE_DATA->client->zone->locatePosition(path);
    auto [iop, rpath] = HVS_FUSE_DATA->client->graph->get_mapping(spaceuuid);
  // access content level
  long int sec0n = tv[0].tv_nsec;
  long int sec0s = tv[0].tv_sec;
  long int sec1n = tv[1].tv_nsec;
  long int sec1s = tv[1].tv_sec;
  // not exists
  if (!iop) {
    return -ENOENT;
  }

  auto res = HVS_FUSE_DATA->client->rpc->call(iop, "ioproxy_utimes",
                                              (rpath + lpath).c_str(), sec0n,
                                              sec0s, sec1n, sec1s);
  if (!res.get()) {
    // timeout exception raised
    return -ENOENT;
  }
  auto retstat = res->as<int>();
  return retstat;
}

struct fuse_operations hvsfs_oper = {
    .getattr = hvsfs_getattr,
    .readlink = hvsfs_readlink,
    .mknod = hvsfs_mknod,  // TODO: create函数重复，暂时不做
    .mkdir = hvsfs_mkdir,
    .unlink = hvsfs_unlink,
    .rmdir = hvsfs_rmdir,
    .symlink = hvsfs_symlink,  // TODO: 软连接操作暂时有问题，待修复；
    .rename = hvsfs_rename,
    .link = hvsfs_link,  // TODO: 硬连接操作暂时有问题，待修复；
    .chmod =
        hvsfs_chmod,  // TODO: 虚拟数据空间相关，涉及到权限，之后需要统一修改；
    .chown =
        hvsfs_chown,  // TODO: 虚拟数据空间相关，涉及到权限，之后需要统一修改；
    .truncate = hvsfs_truncate,
    .open = hvsfs_open,
    .read = hvsfs_read,
    .write = hvsfs_write,
    .statfs = hvsfs_statfs,    // TODO: vfs 相关，暂时不做
    .flush = hvsfs_flush,      // TODO: cache 相关，暂时不做
    .release = hvsfs_release,  // TODO: 未保存文件fd, 暂时不做
    .fsync = hvsfs_fsync,      // TODO: caced 相关暂时不做
    .setxattr = NULL,
    .getxattr = NULL,
    .listxattr = NULL,
    .removexattr = NULL,
    .opendir = hvsfs_opendir,
    .readdir = hvsfs_readdir,
    .releasedir = hvsfs_releasedir,  // TODO: 未保存DIR, 暂时不做
    .fsyncdir = NULL,
    .init = hvsfs_init,
    .destroy = hvsfs_destroy,
    .access = hvsfs_access,
    .create = hvsfs_create,
    .lock = NULL,
    .utimens = hvsfs_utimens,
    .bmap = NULL,
    .ioctl = NULL,
    .poll = NULL,
    .write_buf = NULL,
    .read_buf = NULL,
    .flock = NULL,
    .fallocate = NULL,
};
}  // namespace hvs