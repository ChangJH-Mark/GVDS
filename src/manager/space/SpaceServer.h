#ifndef SPACESERVER_H
#define SPACESERVER_H

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <iostream>
#include <map>

#include "datastore/couchbase_helper.h"
#include "gvds_struct.h"
#include "include/aggregation_struct.h"
#include "manager/manager.h"

using namespace Pistache;

namespace gvds {
class SpaceServer : public ManagerModule {
 private:
  virtual void start() override;
  virtual void stop() override;
  virtual void router(Pistache::Rest::Router&) override;

 public:
  //--------------------------------------------
  // define your function here

  //空间定位模块：空间定位接口
  void GetSpacePosition(std::vector<Space>& result,
                        std::vector<std::string> spaceID);

  //空间信息检索模块：空间信息检索接口
  void GetSpaceInfo(std::vector<Space>& result_s,
                    std::vector<std::string> spaceID);

  //空间创建模块：空间创建接口
  std::string SpaceCreate(std::string spaceName, std::string ownerID,
                          std::vector<std::string> memberID, int64_t spaceSize,
                          std::string spacePathInfo, std::string groupname);

  //空间创建模块：添加区域空间校验接口 注：spacePathInfo 为空间元数据信息
  std::string SpaceCheck(std::string ownerID, std::vector<std::string> memberID,
                         std::string spacePathInfo);

  //空间删除模块：空间删除接口
  int SpaceDelete(std::vector<std::string> spaceID);

  // TODO：空间位置选择模块：空间位置选择接口, 初步实现
  std::tuple<std::string, std::string> GetSpaceCreatePath(
      int64_t spaceSize, std::string& hostCenterName,
      std::string& storageSrcName);

  //空间重命名模块：空间重命名接口
  void SpaceRenameRest(const Rest::Request& request,
                       Http::ResponseWriter response);
  int SpaceRename(std::string spaceID, std::string newSpaceName);

  //空间缩放模块：空间缩放申请接口
  void SpaceSizeChangeApplyRest(const Rest::Request& request,
                                Http::ResponseWriter response);
  int SpaceSizeChangeApply(std::string apply);

  void SpaceNumberRest(const Rest::Request& request,
                       Http::ResponseWriter response);
  int SpaceNumber(std::string hostCenterName);

  //空间缩放模块：空间缩放接口--管理员调用
  void SpaceSizeChangeRest(const Rest::Request& request,
                           Http::ResponseWriter response);
  int SpaceSizeChange(std::string spaceID, int64_t newSpaceSize);
  int SpaceSizeAdd(std::string StorageID, int64_t newSpaceSize);
  int SpaceSizeDeduct(std::string StorageID, int64_t newSpaceSize);

  //空间容量查询
  void SpaceUsageCheckRest(const Rest::Request& request,
                           Http::ResponseWriter response);
  std::vector<int64_t> SpaceUsageCheck(std::vector<std::string> spaceID);
  void SpaceUsageRest(const Rest::Request& request,
                      Http::ResponseWriter response);
  int64_t SpaceUsage(std::string spacepath);
  string ManagerID = *(HvsContext::get_context()->_config->get<std::string>(
      "manager.id"));  // TODO标识所述超算，当前超算的标识

  void SpaceReplicaRest(const Rest::Request& request,
                        Http::ResponseWriter response);
  int SpaceReplica(std::map<string, string>& replicaParam);
  //--------------------------------------------
 public:
  SpaceServer() : ManagerModule("space") {
    auto _config = HvsContext::get_context()->_config;
    spacebucket = _config->get<std::string>("manager.bucket").value_or("test");
    storagebucket = *(gvds::HvsContext::get_context()->_config->get<std::string>(
        "manager.bucket"));
    bucket_account_info =
        _config->get<std::string>("manager.bucket").value_or("test");
    localstoragepath = *(HvsContext::get_context()->_config->get<std::string>(
        "manager.data_path"));
    applybucket = _config->get<std::string>("manager.bucket").value_or("test");
  };
  ~SpaceServer() = default;

  static SpaceServer* instance;  // single object
 private:
  std::string storagebucket;
  std::string spacebucket;
  std::string bucket_account_info;
  std::string localstoragepath;  // 本机存储集群路径
  std::string applybucket;
  std::string space_prefix = "SPACE-";
};

// std::string md5(std::string strPlain);

}  // namespace gvds

#endif
