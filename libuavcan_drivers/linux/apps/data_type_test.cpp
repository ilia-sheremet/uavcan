#include <cstdlib>
#include <unistd.h>
#include <cstdio>

#include <iostream>
#include <vector>
#include <string>
#include <uavcan_linux/uavcan_linux.hpp>

#include <uavcan/protocol/param/SaveErase.hpp>
#include "debug.hpp"

#include <uavcan/driver/can.hpp>
#include <uavcan/transport/transfer_listener.hpp>
#include <uavcan/transport/dispatcher.hpp>
//#include <uavcan/transport/can_acceptance_filter_configurator.hpp>

#include </home/postal/github/uavcan/libuavcan_drivers/lpc11c24/test_olimex_lpc_p11c24/das_test/MyDataType.hpp>

#include <bitset>

using namespace uavcan;
using das_test::MyDataType;

extern uavcan::ICanDriver& getCanDriver();
extern uavcan::ISystemClock& getSystemClock();

static uavcan_linux::NodePtr initNode(const std::vector<std::string>& ifaces, uavcan::NodeID nid,
                                      const std::string& name)
{
    auto node = uavcan_linux::makeNode(ifaces);
 
    node->setNodeID(nid);
    node->setName(name.c_str());
 
    /*
     * Starting the node
     */
    if (0 != node->start())
    {
        throw std::runtime_error("Bad luck");
    }
 
    /*
     * Checking whether our node conflicts with other nodes. This may take a few seconds.
     */
    uavcan::NetworkCompatibilityCheckResult init_result;
    if (0 != node->checkNetworkCompatibility(init_result))
    {
        throw std::runtime_error("Bad luck");
    }
    if (!init_result.isOk())
    {
        throw std::runtime_error("Network conflict with node " + std::to_string(init_result.conflicting_node.get()));
    }

    /*
    * Set the status to OK to inform other nodes that we're ready to work now
    */
    node->setStatusOk();
    node->logInfo ("Ilias_node", "Hello, Ilia.");

    return node;
}
 

static void runForever(const uavcan_linux::NodePtr& node)
{

    /*
     * Timer that uses the above publisher once a 10 sec
     */
    auto send_10s_massage = [&node](const uavcan::TimerEvent&)
    {
        std::cout << "This massage is send by " << int(node->getNodeID().get()) << std::endl;
    };
    auto timer = node->makeTimer(uavcan::MonotonicDuration::fromMSec(10000), send_10s_massage);
 
 
    /*
     * Spinning forever
     */
    while (true)
    {
        const int res = node->spin(uavcan::MonotonicDuration::getInfinite());
        if (res < 0)
        {
            node->logError("spin", "Error %*", res);
        }
    }
}



int main(int argc, const char** argv)
{
    if (argc < 3)
    {
        std::cout << "Usage:\n\t" << argv[0] << " <node-id> <can-iface-name-1> [can-iface-name-N...]" << std::endl;
        return 1;
    }
    const int self_node_id = std::stoi(argv[1]);
    std::vector<std::string> iface_names(argv + 2, argv + argc);

    auto node = initNode(iface_names, self_node_id, "My_node_1");
    std::cout << "Node 1 Initialized" << std::endl;



    auto regist_result = uavcan::GlobalDataTypeRegistry::instance().regist<MyDataType>(852);
       if (regist_result != uavcan::GlobalDataTypeRegistry::RegistResultOk)
          {
              /*
               * Possible reasons for a failure:
               * - Data type name or ID is not unique
               * - Data Type Registry has been frozen and can't be modified anymore
               */
              std::printf("Failed to register the data type: %d\n", int(regist_result));
          }
       else
           std::printf("Success.\n");
    /*
     * Publisher
     */

    uavcan::Publisher<MyDataType> msg_pub(*node);

    das_test::MyDataType trnsmt_msg;
    trnsmt_msg.my_number = 0;

    for (int i=0; i<100; i++)
                 {
                  trnsmt_msg.my_number ++;
                  msg_pub.broadcast(trnsmt_msg);
                 }


    runForever(node);

    return 0;
} 

