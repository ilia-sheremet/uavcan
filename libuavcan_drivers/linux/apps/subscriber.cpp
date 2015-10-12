#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <uavcan/uavcan.hpp>
#include <uavcan/transport/can_acceptance_filter_configurator.hpp>

#include <uavcan/protocol/debug/LogMessage.hpp>
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

    auto& node = getNode();
    node.setNodeID(self_node_id);
    node.setName("org.uavcan.tutorial.subscriber");

    const int node_start_res = node.start();
    if (node_start_res < 0)
    {
        throw std::runtime_error("Failed to start the node; error: " + std::to_string(node_start_res));
    }

    uavcan::Subscriber<uavcan::protocol::debug::LogMessage> log_sub(node);
    const int log_sub_start_res = log_sub.start(
        [&](const uavcan::ReceivedDataStructure<uavcan::protocol::debug::LogMessage>& msg)
        {
            std::cout << msg << std::endl;
        });
    if (log_sub_start_res < 0)
    {
        throw std::runtime_error("Failed to start the log subscriber; error: " + std::to_string(log_sub_start_res));
    }


    uavcan::Subscriber<uavcan::equipment::air_data::Sideslip> sideslip_sub(node);
    const int sideslip_sub_start_res =
    		sideslip_sub.start([&](const uavcan::equipment::air_data::Sideslip& msg) { std::cout << msg << std::endl; });
    if (sideslip_sub_start_res < 0)
    {
        throw std::runtime_error("Failed to start the key/value subscriber; error: " + std::to_string(sideslip_sub_start_res));
    }



    uavcan::CanAcceptanceFilterConfigurator anon_test_configuration(node);
    int configure_filters_assert = anon_test_configuration.configureFilters();
    if (configure_filters_assert == 0)
    {
        std::cout << "Filters are configured with anonymous configuration..." << std::endl;
    }

    uavcan::Subscriber<uavcan::equipment::air_data::TrueAirspeed> airspd_sub(node);
    const int airspd_sub_sub_start_res =
    airspd_sub.start([&](const uavcan::equipment::air_data::TrueAirspeed& msg) { std::cout << msg << std::endl; });
    if (airspd_sub_sub_start_res < 0)
    {
        throw std::runtime_error("Failed to start the key/value subscriber; error: " + std::to_string(airspd_sub_sub_start_res));
    }



    node.setModeOperational();

    while (true)
    {
        /*
         * The method spin() may return earlier if an error occurs (e.g. driver failure).
         * All error codes are listed in the header uavcan/error.hpp.
         */
        const int res = node.spin(uavcan::MonotonicDuration::getInfinite());
        if (res < 0)
        {
            std::cerr << "Transient failure: " << res << std::endl;
        }
    }
}
