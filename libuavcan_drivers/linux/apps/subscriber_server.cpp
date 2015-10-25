#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <uavcan/uavcan.hpp>
#include <uavcan/transport/can_acceptance_filter_configurator.hpp>

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

    /*
     * Initializing a new node. Refer to the "Node initialization and startup" tutorial for more details.
     */
    auto& node = getNode();
    node.setNodeID(self_node_id);
    node.setName("org.uavcan.tutorial.subscriber");
    const int node_start_res = node.start();
    if (node_start_res < 0)
    {
        throw std::runtime_error("Failed to start the node; error: " + std::to_string(node_start_res));
    }

    /*
     *The first step to configure CAN hardware acceptance filters is to create the uavcan::CanAcceptanceFilterConfigurator object with
     *the desired node as an input argument. You may already execute publisher_client which will constantly send messages to this node.
     *
     *PLEASE, NOTICE! Only for this tutorial we are going to put the second argument, which override the actual number of available
     *hardware filters to 6. You may never use this feature and it's only made to make this tutorial more illustrative.
     *There is also a parameter MaxCanAcceptanceFilters in libuavcan/include/uavcan/build_config.hpp that limits the number of maximum
     * CAN acceptance filters available on the platform. You may increase this number manually if your platform has higher number.
     *
     *This object doesn't configure anything yet, in order to make the configuration proceed to the
     *next step.
     */
    uavcan::CanAcceptanceFilterConfigurator anon_test_configuration(node, 6); // There is no second argument in any case, but this tutorial.

    /*
     * The method computeConfiguration() gather information from all subscribers and service messages on the configurator's node. Moreover, it
     * creates a container with automatically calculated configurations of Masks and ID's.
     *
     * It may or may not take an argument:
     * - IgnoreAnonymousMessages
     * - AcceptAnonymousMessages (default, if no input arguments)
     *
     * By default filter configurator accepts all anonymous messages. But if you don't need it you may specify the IgnoreAnonymousMessages
     * input argument. Let's make configurator that accepts anonymous messages.
     */
    anon_test_configuration.computeConfiguration();

    /*
     * At this point you have your configuration calculated and stored in the container within the CanAcceptanceFilterConfigurator's object.
     * But this configuration is still haven't been applied to the processor's hardware filters! Let's take a look what is inside of the
     * container using method getConfiguration(). The output should consist of two configurations: the one that accepts all service responses
     * (default) and another for anonymous messages.
     */
    auto& configure_array = anon_test_configuration.getConfiguration();
    uint16_t configure_array_size = configure_array.getSize();

    std::cout << std::endl << "Configuration with AcceptAnonymousMessages input:" << std::endl;
    for (uint16_t i = 0; i<configure_array_size; i++)
    {
        std::cout << "config.ID [" << i << "]= " << configure_array.getByIndex(i)->id << std::endl;
        std::cout << "config.MK [" << i << "]= " << configure_array.getByIndex(i)->mask << std::endl;
    }
    std::cout << std::endl;

    /*
     * Initializing a subscriber uavcan::equipment::air_data::Sideslip. Please refer to the "Publishers and subscribers" tutorial for more details.
     * Will receive messages for 5 seconds, until we apply the configuration by means of applyConfiguration() method. As soon filters will be configured
     * the node will stop to receive the air_data::Sideslip messages, since we invoke computeConfiguration() before declaring the subscriber.
     */
    uavcan::Subscriber<uavcan::equipment::air_data::Sideslip> sideslip_sub(node);
    int sideslip_sub_start_res = sideslip_sub.start([&](const uavcan::equipment::air_data::Sideslip& msg) { std::cout << msg << std::endl; });
    if (sideslip_sub_start_res < 0)
    {
        throw std::runtime_error("Failed to start the key/value subscriber; error: " + std::to_string(sideslip_sub_start_res));
    }

    node.spin(uavcan::MonotonicDuration::fromMSec(5000)); // Wait 5 seconds

    anon_test_configuration.applyConfiguration();  // Applying configuration
    std::cout << std::endl << "Configuration has been applied. Only anonymous and service messages are being accepted ..." << std::endl;

    node.spin(uavcan::MonotonicDuration::fromMSec(5000)); // Wait 5 seconds

    /*
     * Initializing another subscriber uavcan::equipment::air_data::TrueAirspeed.
     */
    uavcan::Subscriber<uavcan::equipment::air_data::TrueAirspeed> airspd_sub(node);
    int airspd_sub_sub_start_res = airspd_sub.start([&](const uavcan::equipment::air_data::TrueAirspeed& msg) { std::cout << msg << std::endl; });
    if (airspd_sub_sub_start_res < 0)
    {
        throw std::runtime_error("Failed to start the key/value subscriber; error: " + std::to_string(airspd_sub_sub_start_res));
    }

    /*
     * Initializing service server. Please refer to the "Services" tutorial for more details.
     */
    using uavcan::protocol::file::BeginFirmwareUpdate;
    uavcan::ServiceServer<BeginFirmwareUpdate> srv(node);
    int srv_start_res = srv.start(
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

    /*
     * At this point we have 2 subscribers and one service server declared: air_data::TrueAirspeed, air_data::Sideslip and BeginFirmwareUpdate service.
     * But non of them is included to hardware filter's configuration, since the computeConfiguration() function was called before subscriber's declaration.
     * Each time you call the computeConfiguration() it adds (without cleaning the container) all subscriber's and service's filter configurations
     * until you have the hardware filters available (in our tutorial we have 6). When the container fills up the excessive configurations merge in
     * the most efficient way. Next time we call computeConfiguration() the output will be 6 configurations: air_data::Sideslip and air_data::TrueAirspeed
     * subscribers, two exactly the same service message configurations and two configuration for anonymous messages (container doesn't clean itself, only
     * merges).
     */
    anon_test_configuration.computeConfiguration();
    configure_array_size = configure_array.getSize();

    std::cout<< std::endl << "Running computeConfiguration() again with 2 subscribers and service messages:" << std::endl;
    for (uint16_t i = 0; i<configure_array_size; i++)
    {
        std::cout << "config.ID [" << i << "]= " << configure_array.getByIndex(i)->id << std::endl;
        std::cout << "config.MK [" << i << "]= " << configure_array.getByIndex(i)->mask << std::endl;
    }
    std::cout<< "Getting all the messages being published ..." << std::endl << std::endl;

    anon_test_configuration.applyConfiguration();

    node.spin(uavcan::MonotonicDuration::fromMSec(5000)); // Wait 5 seconds

    airspd_sub.stop();
    sideslip_sub.stop();
    srv.stop();
    std::cout<< std::endl << "The message and service subscribers are stopped ..." << std::endl;

    /*
     * If there is a need of adding new custom configuration to hardware filter you may utilize addFilterConfig(CanFilterConfig& config) method.
     * Let's add 5 additional configurations ...
     */
    uavcan::CanFilterConfig new_filter;

    for (uint16_t i = 1; i < 6; i++)
    {
    	new_filter.mask = 255;
    	new_filter.id = i * 2;
        anon_test_configuration.addFilterConfig(new_filter);
    }

    std::cout<< std::endl << "Container after adding new custom configurations:" << std::endl;
    configure_array_size = configure_array.getSize();
    for (uint16_t i = 0; i<configure_array_size; i++)
    {
        std::cout << "config.ID [" << i << "]= " << configure_array.getByIndex(i)->id << std::endl;
        std::cout << "config.MK [" << i << "]= " << configure_array.getByIndex(i)->mask << std::endl;
    }

    /*
     * The container has 10 configurations at the moment. Let's invoke applyConfiguration() method at once, if the number of configurations within
     * the container is higher than number of available filters, the computeConfiguration() function will be automatically called.
     */
    anon_test_configuration.applyConfiguration();

    std::cout<< std::endl << "Container after adding new custom configurations and applyConfiguration():" << std::endl;
    configure_array_size = configure_array.getSize();
    for (uint16_t i = 0; i<configure_array_size; i++)
    {
        std::cout << "config.ID [" << i << "]= " << configure_array.getByIndex(i)->id << std::endl;
        std::cout << "config.MK [" << i << "]= " << configure_array.getByIndex(i)->mask << std::endl;
    }

    node.spin(uavcan::MonotonicDuration::fromMSec(5000)); // Wait 5 seconds


    std::cout<< std::endl << "Starting subscribers and server again to see the final result:" << std::endl << std::endl;

    airspd_sub_sub_start_res = airspd_sub.start([&](const uavcan::equipment::air_data::TrueAirspeed& msg) { std::cout << msg << std::endl; });
    if (airspd_sub_sub_start_res < 0)
    {
        throw std::runtime_error("Failed to start the key/value subscriber; error: " + std::to_string(airspd_sub_sub_start_res));
    }

    sideslip_sub_start_res = sideslip_sub.start([&](const uavcan::equipment::air_data::Sideslip& msg) { std::cout << msg << std::endl; });
    if (sideslip_sub_start_res < 0)
    {
        throw std::runtime_error("Failed to start the key/value subscriber; error: " + std::to_string(sideslip_sub_start_res));
    }

    srv_start_res = srv.start(
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
