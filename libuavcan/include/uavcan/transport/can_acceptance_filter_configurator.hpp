/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>,
 *                    Ilia  Sheremet <illia.sheremet@gmail.com>
 */

#ifndef CAN_ACCEPTANCE_FILTER_CONFIGURATOR_HPP_INCLUDED
#define CAN_ACCEPTANCE_FILTER_CONFIGURATOR_HPP_INCLUDED

#include <cassert>
#include <uavcan/data_type.hpp>
#include <uavcan/error.hpp>
#include <uavcan/transport/dispatcher.hpp>
#include <uavcan/node/abstract_node.hpp>
#include <uavcan/build_config.hpp>
#include <uavcan/util/map.hpp>

namespace uavcan
{

class CanAcceptanceFilterConfigurator
{
    /**
     * Below constants based on UAVCAN transport layer specification. Masks and Id's depends on message Priority,
     * TypeID, TransferID (RequestNotResponse - for service types, BroadcastNotUnicast - for message types).
     * For more details refer to uavcan.org/CAN_bus_transport_layer_specification.
     * For clarity let's represent "i" as Data Type ID
     * DefaultFilterMsgMask = 00111111111110000000000000000
     * DefaultFilterMsgId   = 00iiiiiiiiiii0000000000000000, no need to explicitly define, since MsgId initialized as 0.
     * DefaultFilterServiceRequestMask = 11111111111100000000000000000
     * DefaultFilterServiceRequestId   = 101iiiiiiiii00000000000000000
     * ServiceRespFrameMask = 11100000000000000000000000000
     * ServiceRespFrameId   = 10000000000000000000000000000, all Service Response Frames are accepted by HW filters.
     */
    static const unsigned DefaultFilterMsgMask = 0x7FF0000;
    static const unsigned DefaultFilterServiceRequestId = 0x14000000;
    static const unsigned DefaultFilterServiceRequestMask = 0x1FFE0000;
    static const unsigned ServiceRespFrameId = 0x10000000;
    static const unsigned ServiceRespFrameMask = 0x1C000000;

    typedef Map<uint16_t, CanFilterConfig> MapConfigArray;

    CanFilterConfig mergeFilters(CanFilterConfig &a_, CanFilterConfig &b_);
    uint8_t countBits(uint32_t n_);
    uint16_t getNumFilters() const;

    /**
     * Fills the map_configs_ to proceed it with computeConfiguration()
     */
    int16_t fillArray();

    /**
     * This method merges several listeners's filter configurations by predermined algortihm
     * if number of available hardware acceptance filters less than number of listeners
     */
    int16_t computeConfiguration();

    /**
     * This method loads the configuration comfputed with computeConfiguration() to the CAN driver.
     */
    int16_t applyConfiguration();

    INode& node_;               //< Node reference is needed for access to ICanDriver and Dispatcher
    MapConfigArray map_configs_;

public:
    explicit CanAcceptanceFilterConfigurator(INode& node)
        : node_(node)
        , map_configs_(node.getAllocator())
    { }

    /**
     * This method invokes fillArray(), computeConfiguration() and applyConfiguration() consequently, so that
     * optimal acceptance filter configuration will be computed and loaded through CanDriver::configureFilters()
     * @return 0 = success, negative for error.
     */
    int configureFilters();

    /**
     * Returns the configuration computed with computeConfiguration().
     * If computeConfiguration() has not been called yet, an empty configuration will be returned.
     */
    const MapConfigArray& getConfiguration()
    {
        return map_configs_;
    }
};

}

#endif // UAVCAN_BUILD_CONFIG_HPP_INCLUDED
