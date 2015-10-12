#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <uavcan/uavcan.hpp>

#include <uavcan/protocol/file/BeginFirmwareUpdate.hpp>
#include <uavcan/equipment/air_data/Sideslip.hpp>
#include <uavcan/equipment/air_data/TrueAirspeed.hpp>

extern uavcan::ICanDriver& getCanDriver();
extern uavcan::ISystemClock& getSystemClock();

constexpr unsigned NodeMemoryPoolSize = 16384;
typedef uavcan::Node<NodeMemoryPoolSize> Node;


static Node& getNode()
{
    static Node node(getCanDriver(), getSystemClock());
    return node;
}

int main(int argc, const char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <node-id>" << std::endl;
        return 1;
    }

    const int self_node_id = std::stoi(argv[1]);

    ///////////////////////
    auto& node = getNode();
    node.setNodeID(self_node_id);
    node.setName("org.uavcan.tutorial.publisher");
    const int node_start_res = node.start();
    if (node_start_res < 0)
    {
        throw std::runtime_error("Failed to start the node; error: " + std::to_string(node_start_res));
    }

    ////////////////////////
    uavcan::Publisher<uavcan::equipment::air_data::Sideslip> sideslip_pub(node);
    const int sideslip_pub_init_res = sideslip_pub.init();
    if (sideslip_pub_init_res < 0)
    {
    	throw std::runtime_error("Failed to start the publisher; error: " + std::to_string(sideslip_pub_init_res));
    }

    ////////////////////////
    uavcan::Publisher<uavcan::equipment::air_data::TrueAirspeed> airspeed_pub(node);
    const int airspeed_pub_init_res = airspeed_pub.init();
    if (airspeed_pub_init_res < 0)
    {
    	throw std::runtime_error("Failed to start the publisher; error: " + std::to_string(airspeed_pub_init_res));
    }

    ////////////////////////
    using uavcan::protocol::file::BeginFirmwareUpdate;
    uavcan::ServiceServer<BeginFirmwareUpdate> srv(node);
    const int srv_start_res = srv.start(
       [&](const uavcan::ReceivedDataStructure<BeginFirmwareUpdate::Request>& req, BeginFirmwareUpdate::Response& rsp)
       {
           std::cout << req << std::endl;
           rsp.error = rsp.ERROR_UNKNOWN;
           rsp.optional_error_message = "I am filtered";
       });
    if (srv_start_res < 0)
    {
    	throw std::runtime_error("Failed to start the server; error: " + std::to_string(srv_start_res));
    }

    node.setModeOperational(); //TODO ask what are the differences? Just status that doesn't do anything?

    while (true)
    {

        const int spin_res = node.spin(uavcan::MonotonicDuration::fromMSec(1000));
        if (spin_res < 0)
        {
            std::cerr << "Transient failure: " << spin_res << std::endl;
        }

        uavcan::equipment::air_data::Sideslip sideslip_msg;
        sideslip_msg.sideslip_angle = std::rand() / float(RAND_MAX);
        sideslip_msg.sideslip_angle_variance = std::rand() / float(RAND_MAX);
        const int sideslip_msg_pub_res = sideslip_pub.broadcast(sideslip_msg);
        if (sideslip_msg_pub_res < 0)
        {
            std::cerr << "KV publication failure: " << sideslip_msg_pub_res << std::endl;
        }

        uavcan::equipment::air_data::TrueAirspeed airspd_msg;
        airspd_msg.true_airspeed = 10;
        airspd_msg.true_airspeed_variance = 1;
        const int airspd_msg_pub_res = airspeed_pub.broadcast(airspd_msg);
        if (airspd_msg_pub_res < 0)
        {
            std::cerr << "KV publication failure: " << airspd_msg_pub_res << std::endl;
        }

    }
}
