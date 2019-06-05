#ifndef ZONESERVER_H
#define ZONESERVER_H

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <iostream>
#include <map>


#include "datastore/couchbase_helper.h"
#include "manager/zone/Zone.h"
#include "manager/space/SpaceServer.h"
#include "manager/zone/Zone.h"
#include "manager/manager.h"





namespace hvs{
class ZoneServer : public ManagerModule {
private:
  virtual void start() override;
  virtual void stop() override;
  virtual void router(Pistache::Rest::Router&) override;
  
public:
 //--------------------------------------------
    //define your function here
    
    //区域重命名模块：区域重命名接口 复查
    void ZoneRenameRest(const Rest::Request& request, Http::ResponseWriter response);
    int ZoneRename(std::string zoneID, std::string ownerID, std::string newZoneName);

    //区域定位模块：区域定位接口 并获取到空间元数据信息 复查
    void GetZoneLocateInfoRest(const Rest::Request& request, Http::ResponseWriter response);
    bool GetZoneLocateInfo(std::vector<std::string> &result, std::string clientID, std::string zoneID, std::vector<std::string> spaceID);

    //区域信息检索模块：区域信息检索接口 复查
    void GetZoneInfoRest(const Rest::Request& request, Http::ResponseWriter response);
    bool GetZoneInfo(std::vector<std::string> &result_z, std::string clientID);

    //区域共享模块：区域共享接口 复查
    void ZoneShareRest(const Rest::Request& request, Http::ResponseWriter response);
    int ZoneShare(std::string zoneID, std::string ownerID, std::vector<std::string> memberID);

    //区域共享模块：区域共享取消接口 复查
    void ZoneShareCancelRest(const Rest::Request& request, Http::ResponseWriter response);
    int ZoneShareCancel(std::string zoneID, std::string ownerID, std::vector<std::string> memberID);

    //区域注册模块：区域注册接口 复查
    void ZoneRegisterRest(const Rest::Request& request, Http::ResponseWriter response);
    int ZoneRegister(std::string zoneName, std::string ownerID, std::vector<std::string> memberID,
                     std::string spaceName, int64_t spaceSize, std::string spacePathInfo);

    //区域注册模块：管理员区域添加接口 复查
    void ZoneAddRest(const Rest::Request& request, Http::ResponseWriter response);
    int ZoneAdd(std::string zoneName, std::string ownerID, std::vector<std::string> memberID,
                     std::string spacePathInfo);

    //区域注销模块：区域注销接口 复查
    void ZoneCancelRest(const Rest::Request& request, Http::ResponseWriter response);
    int ZoneCancel(std::string zoneID, std::string ownerID);

    //映射编辑模块：区域映射增加接口 复查
    void MapAddRest(const Rest::Request& request, Http::ResponseWriter response);
    int MapAdd(std::string zoneID, std::string ownerID, std::string spaceName, int64_t spaceSize, std::string spacePathInfo);

    //映射编辑模块：区域映射删除接口 复查
    void MapDeductRest(const Rest::Request& request, Http::ResponseWriter response);
    int MapDeduct(std::string zoneID, std::string ownerID, std::vector<std::string> spaceID);

 //--------------------------------------------
public:
    ZoneServer() : ManagerModule("zone") {
        zonebucket = "zone_info";
    };
    ~ZoneServer() = default;

private:
    std::string  zonebucket;
};

}// namespace hvs

//判断vector1是否包含vector2
bool isSubset(std::vector<std::string> v1, std::vector<std::string> v2);

#endif






