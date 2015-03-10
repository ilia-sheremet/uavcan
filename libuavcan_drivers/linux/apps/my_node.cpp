#include <cstdlib>
#include <unistd.h>
#include <cstdio>

#include <iostream>
#include <vector>
#include <string>
#include <uavcan_linux/uavcan_linux.hpp>
#include <uavcan/protocol/debug/KeyValue.hpp>
#include <uavcan/protocol/param/SaveErase.hpp>
#include "debug.hpp"

#include <uavcan/driver/can.hpp>
#include <uavcan/transport/transfer_listener.hpp>
#include <uavcan/transport/dispatcher.hpp>
//#include <uavcan/transport/can_acceptance_filter_configurator.hpp>

#include </home/postal/github/uavcan/libuavcan_drivers/lpc11c24/test_olimex_lpc_p11c24/das_test/MyDataType.hpp>

#include <bitset>

using namespace uavcan;

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
    node->logInfo ("Ilias_node", "Hello, Ilia is my creator!");
 
    return node;
}
 

static void runForever(const uavcan_linux::NodePtr& node, const uavcan_linux::NodePtr& node_2)
{
       /*
        * Publisher
        */
      uavcan::Publisher<uavcan::protocol::debug::KeyValue> kv_pub(*node);

      uavcan::protocol::debug::KeyValue kv_msg;
      kv_msg.type = kv_msg.TYPE_FLOAT;
   // kv_msg.numeric_value.push_back(std::rand() / float(RAND_MAX));kv_msg.type = kv_msg.TYPE_FLOAT;
      kv_msg.numeric_value.push_back(std::rand() / float(RAND_MAX));
      kv_msg.key = "random";  // "random"*/

      const int pub_res = kv_pub.broadcast(kv_msg);
      if (pub_res < 0)
              {
                  std::printf("KV publication failure: %d\n", pub_res);
              }

      std::cout << "Massege is broadcasted from Node_1" << std::endl;



      /*
       * Subscriber
       */
      uavcan::Subscriber<uavcan::protocol::debug::KeyValue> kv_sub(*node_2);

      const int kv_sub_start_res =
              kv_sub.start([&](const uavcan::protocol::debug::KeyValue& msg) { std::cout << "Message received on Node_2 = " <<msg << std::endl; });
      if (kv_sub_start_res < 0)
        {
            std::exit(1);                   // TODO proper error handling
        }

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

    auto node_2 = initNode(iface_names, self_node_id + 1, "My_node_2");
    std::cout << "Node 2 Initialized" << std::endl<< std::endl;

  //auto keyvalue_pub = node->makePublisher<uavcan::protocol::debug::KeyValue>();


/*
    CanAcceptanceFilterConfigurator can_filter_cfg(*node);
    CanFilterConfig data_pair;

    const  TransferListenerBase* p = node->getDispatcher().getListOfMessageListeners().get();

    data_pair.id = static_cast<uint32_t>(p->getDataTypeDescriptor().getID().get()) << 19;
    data_pair.id |= static_cast<uint32_t>(p->getDataTypeDescriptor().getKind()) << 17;

    std::cout << "data_id = " << std::bitset<29>(data_pair.id) << std::endl;
*/

    runForever(node, node_2);
    return 0;
} 

