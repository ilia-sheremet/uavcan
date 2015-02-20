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
 
static void runForever(const uavcan_linux::NodePtr& node)
{
 
    /*
     * Subscribing to the logging message just for fun
     */
    auto log_sub = node->makeSubscriber<uavcan::protocol::debug::LogMessage>(
        [](const uavcan::ReceivedDataStructure<uavcan::protocol::debug::LogMessage>& msg)
        {
            std::cout << msg << std::endl;
        });
 
    /*
     * Key Value publisher
     */
    auto keyvalue_pub = node->makePublisher<uavcan::protocol::debug::KeyValue>();
 

    /*
     * Timer that uses the above publisher once a 10 sec
     */
    auto send_10s_massage = [&node](const uavcan::TimerEvent&)
    {
        std::cout << "This massage is send by " << int(node->getNodeID().get()) << std::endl;
    };
    auto timer = node->makeTimer(uavcan::MonotonicDuration::fromMSec(10000), send_10s_massage);
 

    /*
     * A useless server that just prints the request and responds with a default-initialized response data structure
     */
    auto server = node->makeServiceServer<uavcan::protocol::param::SaveErase>(
        [](const uavcan::protocol::param::SaveErase::Request& req, uavcan::protocol::param::SaveErase::Response&)
        {
            std::cout << req << std::endl;
        });
 
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
    auto node = initNode(iface_names, self_node_id, "org.uavcan.pan_galactic_gargle_blaster");
    std::cout << "Initialized" << std::endl;



    const TransferListenerBase* p = node_.getDispatcher().getListOfMessageListeners().get();

    runForever(node);
    return 0;
} 
