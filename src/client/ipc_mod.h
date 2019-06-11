//
// Created by yaowen on 6/11/19.
// 北航系统结构所-存储组
//


#pragma once //HVSONE_IPC_MOD_H
#include "client.h"
#include "ipc/IPCServer.hpp"
#include "manager/zone/Zone.h"
#include "manager/space/Space.h"
#include "ipc_struct.h"
#include <future>
#include <pistache/client.h>


namespace hvs{
    class ClientIPC :public ClientModule{
    private:
        virtual void start() override;
        virtual void stop() override;

    public:
        ClientIPC (const char* name, Client* cli) : ClientModule(name, cli) {
            isThread = true;
            init();
        }

        void init();

    private:
        friend class Client;
        std::shared_ptr<IPCServer> sp_ipcserver;
        std::unordered_map<std::string, std::string> zonemap; // 区域重命名使用

    private:
        // TODO: 客户端具体处理函数
        std::string dospacerename(IPCreq &ipcreq); // 处理客户端函数
        bool GetZoneInfo(std::string ip, int port, std::string clientID);
    };
}

//HVSONE_IPC_MOD_H