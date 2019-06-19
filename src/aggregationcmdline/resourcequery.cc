//
// Created by weibing on 6/16/19.
// 北航系统结构所-存储组
//

#include <iostream>
#include <hvs_struct.h>
#include <future>
#include <pistache/client.h>
#include <vector>
#include <future>
#include "cmdline/CmdLineProxy.h"
#include "client/ipc_struct.h"
#include "ipc/IPCClient.h"
#include "aggregation_struct.h"
using namespace hvs;
using namespace std;

/*
 * resourcequery 命令行客户端
 */

int main(int argc, char *argv[])
{

    char *demo1[2] = {const_cast<char *>("resourcequery"), const_cast<char *>("--ri")}; //BIGBOSSSY
    char *demo2[2] = {const_cast<char *>("resourcequery"), const_cast<char *>("--help")};

    string storage_src_id = ""; // 存储资源UUID
    CmdLineProxy commandline(argc, argv);
    string cmdname = argv[0];
    // TODO：设置当前命令行解析函数
    commandline.cmd_desc_func_map[cmdname] = [](shared_ptr<po::options_description> res_cmdline_options) -> void {
        po::options_description command("资源查询模块");
        command.add_options()("ri", po::value<string>(), "存储资源UUID");
        res_cmdline_options->add(command); // 添加子模块命令行描述
    };
    // TODO： 解析命令行参数，进行赋值
    commandline.cmd_do_func_map[cmdname] = [&](shared_ptr<po::variables_map> res_variables_map) -> void {
    if (res_variables_map->count("ri"))
    {
        storage_src_id = (*res_variables_map)["ri"].as<string>();
    } };
    commandline.start(); //开始解析命令行参数

    //TODO :判断是否有参数，如果没有，则报错
    if (commandline.argc <= 1)
    {
        cerr << "请输入命令参数！" << endl;
        commandline.print_options();
        exit(-1);
    }

    try
    {
        promise<bool> prom;
        auto fu = prom.get_future();
        // TODO:  调用IPC 客户端 进行同行，并获取返回结果
        IPCClient ipcClient("127.0.0.1", 6666);
        ipcClient.set_callback_func([&](IPCMessage msg) -> void {
            string ipcresult(msg.body(), msg.body_length());
            if (ipcresult == "")
            {
                cout << "no match resource" << endl;
            }
            else
            {
                vector<string> lists;
                json_decode(ipcresult, lists);
                for (auto res : lists)
                {
                    StorageResource qres;
                    std::string value = res.substr(8, res.length() - 9);
                    qres.deserialize(value);
                    int srcidlen = qres.storage_src_id.length();
                    int prefixlen = StorageResource::prefix().length();
                    string realsrcid = qres.storage_src_id.substr(prefixlen,srcidlen - prefixlen);
                    cout<<"存储资源ID:"<<realsrcid<<endl;
                    cout<<"存储资源名称:"<<qres.storage_src_name<<endl;
                    cout<<"存储资源所在超算中心UUID:"<<qres.host_center_id<<endl;
                    cout<<"存储资源所在超算中心名称:"<<qres.host_center_name<<endl;
                    cout<<"存储资源空间容量大小:"<<qres.total_capacity<<endl;
                    cout<<"存储资源已分配空间容量大小:"<<qres.assign_capacity<<endl;
                    cout<<"存储资源MGS地址:"<<qres.mgs_address<<endl;
                    cout<<"存储资源状态:"<<(StorageResState)qres.state<<endl;
                }
            }

            prom.set_value(true);
        });
        ipcClient.run(); // 停止的时候调用stop 函数
        cout << "正在执行命令..." << endl;

        // TODO: 构造请求结构体，并发送；
        IPCreq ipcreq;
        ipcreq.cmdname = "resourcequery";
        ipcreq.storage_src_id = storage_src_id;

        // TODO: 发送
        auto msg = IPCMessage::make_message_by_charstring(ipcreq.serialize().c_str());
        ipcClient.write(*msg); // 传递一个消息；
        fu.get();              // TODO: 等待客户端返回结果
        ipcClient.stop();
    }
    catch (exception &e)
    {
        cout << e.what() << endl;
    }
    return 0;
}