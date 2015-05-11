/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>,
 *                    Ilia  Sheremet <illia.sheremet@gmail.com>
 */

#include <uavcan/transport/can_acceptance_filter_configurator.hpp>
#include <cassert>

namespace uavcan
{
const unsigned CanAcceptanceFilterConfigurator::DefaultFilterMsgMask;
const unsigned CanAcceptanceFilterConfigurator::DefaultFilterServiceRequestID;
const unsigned CanAcceptanceFilterConfigurator::DefaultFilterServiceRequestMask;
const unsigned CanAcceptanceFilterConfigurator::ServiceRespFrameID;
const unsigned CanAcceptanceFilterConfigurator::ServiceRespFrameMask;

int16_t CanAcceptanceFilterConfigurator::fillArray()
{
    map_configs_.removeAll();
    uint16_t i = 1;

    CanFilterConfig ServiseRespFrame;
    ServiseRespFrame.id = ServiceRespFrameID;
    ServiseRespFrame.mask = ServiceRespFrameMask;
    map_configs_.insert(i, ServiseRespFrame);
    if (map_configs_.access(i) == NULL)
    {
        UAVCAN_ASSERT(0);
    }
    i++;

    const TransferListenerBase* p = node_.getDispatcher().getListOfMessageListeners().get();
    while (p)
    {
        CanFilterConfig cfg;
        cfg.id = static_cast<uint32_t>(p->getDataTypeDescriptor().getID().get()) << 16;
        cfg.id |= static_cast<uint32_t>(p->getDataTypeDescriptor().getKind()) << 8;
        cfg.mask = DefaultFilterMsgMask;

        map_configs_.insert(i, cfg);
        if (map_configs_.access(i) == NULL)
        {
            UAVCAN_ASSERT(0);
        }

        i++;
        p = p->getNextListNode();
    }

    const TransferListenerBase* p1 = node_.getDispatcher().getListOfServiceRequestListeners().get();
    while (p1)
    {
        CanFilterConfig cfg;
        cfg.id = DefaultFilterServiceRequestID;
        cfg.id |= static_cast<uint32_t>(p1->getDataTypeDescriptor().getID().get()) << 17;
        cfg.mask = DefaultFilterServiceRequestMask;

        map_configs_.insert(i, cfg);
        if (map_configs_.access(i) == NULL)
        {
            UAVCAN_ASSERT(0);
        }

        i++;
        p1 = p1->getNextListNode();
    }

    if (map_configs_.getSize() == 0)
    {
        return -ErrDriver;
    }

    for (uint16_t i=0; i < map_configs_.getSize(); i++)
    {
        UAVCAN_TRACE("CanAcceptanceFilterConfigurator::fillArray()", "map.ID [%u] = %d", i,
                     map_configs_.getByIndex(i)->value.id);
        UAVCAN_TRACE("CanAcceptanceFilterConfigurator::fillArray()", "map.MK [%u] = %d", i,
                     map_configs_.getByIndex(i)->value.mask);
    }

    return 0;
}

int16_t CanAcceptanceFilterConfigurator::computeConfiguration()
{
    uint16_t i_rank = 0, j_rank = 0;
    const uint16_t acceptance_filters_number = getNumFilters();

    while (acceptance_filters_number < map_configs_.getSize())
    {
        UAVCAN_ASSERT(map_configs_.getSize() != 0);

        uint8_t best_rank = 0;
        const uint16_t map_array_size = static_cast<uint16_t>(map_configs_.getSize());

        for (uint16_t i_ind = 0; i_ind < map_array_size - 1; i_ind++)
        {
            for (uint16_t j_ind = static_cast<uint16_t>(i_ind + 1); j_ind < map_array_size; j_ind++)
            {
                CanFilterConfig temp_array = mergeFilters(map_configs_.getByIndex(i_ind)->value,
                                                          map_configs_.getByIndex(j_ind)->value);
                uint8_t rank = countBits(temp_array.mask);
                if (rank > best_rank)
                {
                    best_rank = rank;
                    i_rank = i_ind;
                    j_rank = j_ind;
                }
            }
        }

        map_configs_.getByIndex(j_rank)->value = mergeFilters(map_configs_.getByIndex(i_rank)->value,
                                                              map_configs_.getByIndex(j_rank)->value);
        uint16_t temp_key = map_configs_.getByIndex(i_rank)->key;
        map_configs_.remove(temp_key); //remove() the element by the map key

        UAVCAN_ASSERT(map_configs_.access(temp_key) == NULL);
    }

    UAVCAN_ASSERT(acceptance_filters_number >= map_configs_.getSize());

    return 0;
}

int16_t CanAcceptanceFilterConfigurator::applyConfiguration(void)
{
    CanFilterConfig filter_conf_array[MaxCanAcceptanceFilters];
    const unsigned int filter_array_size = map_configs_.getSize();

    if (filter_array_size > MaxCanAcceptanceFilters)
    {
        UAVCAN_ASSERT(0);
        return -ErrLogic;
    }

    for (uint16_t i = 0; i < filter_array_size; i++)
    {
        CanFilterConfig temp_filter_config = map_configs_.getByIndex(i)->value;
        if (map_configs_.getByIndex(i) == NULL)
        {
            UAVCAN_ASSERT(0);
        }

        if ((temp_filter_config.id != 0) || (temp_filter_config.mask != 0))
        {
            filter_conf_array[i] = temp_filter_config;
        }
    }

    ICanDriver& can_driver = node_.getDispatcher().getCanIOManager().getCanDriver();
    for (uint8_t i = 0; i < node_.getDispatcher().getCanIOManager().getNumIfaces(); i++)
    {
        ICanIface* iface = can_driver.getIface(i);
        if (iface == NULL)
        {
            UAVCAN_ASSERT(0);
            break;
        }
        int16_t num = iface->configureFilters(reinterpret_cast<CanFilterConfig*>(&filter_conf_array),
                                              static_cast<uint16_t>(filter_array_size));
        if (num < 0)
        {
            return -ErrDriver;
        }
    }

    return 0;
}

int CanAcceptanceFilterConfigurator::configureFilters()
{
    if (getNumFilters() == 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter", "No HW filters available");
        return -ErrDriver;
    }

    int16_t fill_array_error = fillArray();
    if (fill_array_error != 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter::fillArray", "Failed to execute fillArray()");
        return fill_array_error;
    }

    int16_t compute_configuration_error = computeConfiguration();
    if (compute_configuration_error != 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter", "Failed to compute optimal acceptance fliter's configuration");
        return compute_configuration_error;
    }

    if (applyConfiguration() != 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter", "Failed to apply HW filter configuration");
        return -ErrDriver;
    }

    return 0;
}

uint16_t CanAcceptanceFilterConfigurator::getNumFilters() const
{

    static const uint16_t InvalidOut = 0xFFFF;
    uint16_t out = InvalidOut;
    ICanDriver& can_driver = node_.getDispatcher().getCanIOManager().getCanDriver();

    for (uint8_t i = 0; i < node_.getDispatcher().getCanIOManager().getNumIfaces(); i++)
    {
        const ICanIface* iface = can_driver.getIface(i);
        if (iface == NULL)
        {
            UAVCAN_ASSERT(0);
            out = 0;
            break;
        }
        const uint16_t num = iface->getNumFilters();
        out = min(out, num);
        if (num > MaxCanAcceptanceFilters)
        {
            return MaxCanAcceptanceFilters;
        }
    }

    return (out == InvalidOut) ? 0 : out;
}

CanFilterConfig CanAcceptanceFilterConfigurator::mergeFilters(CanFilterConfig &a_, CanFilterConfig &b_)
{
    CanFilterConfig temp_arr;
    temp_arr.mask = a_.mask & b_.mask & ~(a_.id ^ b_.id);
    temp_arr.id = a_.id & temp_arr.mask;

    return temp_arr;
}

uint8_t CanAcceptanceFilterConfigurator::countBits(uint32_t n_)
{
    uint8_t c_; // c accumulates the total bits set in v
    for (c_ = 0; n_; c_++)
    {
        n_ &= n_ - 1; // clear the least significant bit set
    }

    return c_;
}

}
