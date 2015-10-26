/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>,
 *                    Ilia  Sheremet <illia.sheremet@gmail.com>
 */

#include <uavcan/transport/can_acceptance_filter_configurator.hpp>
#include <cassert>

namespace uavcan
{
const unsigned CanAcceptanceFilterConfigurator::DefaultFilterMsgMask;
const unsigned CanAcceptanceFilterConfigurator::DefaultFilterServiceID;
const unsigned CanAcceptanceFilterConfigurator::DefaultFilterServiceMask;
const unsigned CanAcceptanceFilterConfigurator::DefaultAnonMsgMask;
const unsigned CanAcceptanceFilterConfigurator::DefaultAnonMsgID;

int16_t CanAcceptanceFilterConfigurator::loadInputConfiguration(AnonymousMessages load_mode)
{
    if (load_mode == AcceptAnonymousMessages)
    {
        CanFilterConfig anon_frame_cfg;
        anon_frame_cfg.id = DefaultAnonMsgID | CanFrame::FlagEFF;
        anon_frame_cfg.mask = DefaultAnonMsgMask | CanFrame::FlagEFF | CanFrame::FlagRTR | CanFrame::FlagERR;
        if (multiset_configs_.emplace(anon_frame_cfg) == NULL)
        {
            return -ErrMemory;
        }
    }

    CanFilterConfig service_cfg;
    service_cfg.id = DefaultFilterServiceID;
    service_cfg.id |= (static_cast<uint32_t>(node_.getNodeID().get()) << 8) | CanFrame::FlagEFF;
    service_cfg.mask = DefaultFilterServiceMask | CanFrame::FlagEFF | CanFrame::FlagRTR | CanFrame::FlagERR;
    if (multiset_configs_.emplace(service_cfg) == NULL)
    {
        return -ErrMemory;
    }

    const TransferListener* p = node_.getDispatcher().getListOfMessageListeners().get();
    while (p != NULL)
    {
        CanFilterConfig cfg;
        cfg.id = (static_cast<uint32_t>(p->getDataTypeDescriptor().getID().get()) << 8) | CanFrame::FlagEFF;
        cfg.mask = DefaultFilterMsgMask | CanFrame::FlagEFF | CanFrame::FlagRTR | CanFrame::FlagERR;
        if (multiset_configs_.emplace(cfg) == NULL)
        {
            return -ErrMemory;
        }
        p = p->getNextListNode();
    }

    if (multiset_configs_.getSize() == 0)
    {
        return -ErrLogic;
    }

#if UAVCAN_DEBUG
    for (uint16_t i = 0; i < multiset_configs_.getSize(); i++)
    {
        UAVCAN_TRACE("CanAcceptanceFilterConfigurator::loadInputConfiguration()", "cfg.ID [%u] = %u", i,
                     multiset_configs_.getByIndex(i)->id & CanFrame::MaskExtID);
        UAVCAN_TRACE("CanAcceptanceFilterConfigurator::loadInputConfiguration()", "cfg.MK [%u] = %u", i,
                     multiset_configs_.getByIndex(i)->mask & CanFrame::MaskExtID);
    }
#endif

    return 0;
}

int16_t CanAcceptanceFilterConfigurator::mergeConfigurations()
{
    const uint16_t acceptance_filters_number = getNumFilters();
    if (acceptance_filters_number == 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter", "No HW filters available");
        return -ErrDriver;
    }
    UAVCAN_ASSERT(multiset_configs_.getSize() != 0);

    while (acceptance_filters_number < multiset_configs_.getSize())
    {
        uint16_t i_rank = 0, j_rank = 0;
        uint8_t best_rank = 0;

        const uint16_t multiset_array_size = static_cast<uint16_t>(multiset_configs_.getSize());

        for (uint16_t i_ind = 0; i_ind < multiset_array_size - 1; i_ind++)
        {
            for (uint16_t j_ind = static_cast<uint8_t>(i_ind + 1); j_ind < multiset_array_size; j_ind++)
            {
                CanFilterConfig temp_config = mergeFilters(*multiset_configs_.getByIndex(i_ind),
                                                           *multiset_configs_.getByIndex(j_ind));
                uint8_t rank = countBits(temp_config.mask);
                if (rank > best_rank)
                {
                    best_rank = rank;
                    i_rank = i_ind;
                    j_rank = j_ind;
                }
            }
        }

        *multiset_configs_.getByIndex(j_rank) = mergeFilters(*multiset_configs_.getByIndex(i_rank),
                                                             *multiset_configs_.getByIndex(j_rank));
        multiset_configs_.removeFirst(*multiset_configs_.getByIndex(i_rank));
    }

    UAVCAN_ASSERT(acceptance_filters_number >= multiset_configs_.getSize());

    return 0;
}

int16_t CanAcceptanceFilterConfigurator::applyConfiguration(void)
{
    CanFilterConfig filter_conf_array[MaxCanAcceptanceFilters];
    unsigned int filter_array_size = multiset_configs_.getSize();

    const uint16_t acceptance_filters_number = getNumFilters();
    if (acceptance_filters_number == 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter", "No HW filters available");
        return -ErrDriver;
    }

    if (filter_array_size > acceptance_filters_number)
    {
        UAVCAN_TRACE("CanAcceptanceFilter", "Too many filter configurations. Executing computeConfiguration()");
        computeConfiguration(IgnoreAnonymousMessages);
        filter_array_size = multiset_configs_.getSize();
    }

    if (filter_array_size > MaxCanAcceptanceFilters)
    {
        UAVCAN_ASSERT(0);
        return -ErrLogic;
        UAVCAN_TRACE("CanAcceptanceFilter", "Failed to apply HW filter configuration");
    }

    for (uint16_t i = 0; i < filter_array_size; i++)
    {
        CanFilterConfig temp_filter_config = *multiset_configs_.getByIndex(i);

        filter_conf_array[i] = temp_filter_config;
    }

    ICanDriver& can_driver = node_.getDispatcher().getCanIOManager().getCanDriver();
    for (uint8_t i = 0; i < node_.getDispatcher().getCanIOManager().getNumIfaces(); i++)
    {
        ICanIface* iface = can_driver.getIface(i);
        if (iface == NULL)
        {
            return -ErrDriver;
            UAVCAN_TRACE("CanAcceptanceFilter", "Failed to apply HW filter configuration");
        }
        int16_t num = iface->configureFilters(reinterpret_cast<CanFilterConfig*>(&filter_conf_array),
                                              static_cast<uint16_t>(filter_array_size));
        if (num < 0)
        {
            return -ErrDriver;
            UAVCAN_TRACE("CanAcceptanceFilter", "Failed to apply HW filter configuration");
        }
    }

    return 0;
}

int CanAcceptanceFilterConfigurator::computeConfiguration(AnonymousMessages mode)
{
    if (getNumFilters() == 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter", "No HW filters available");
        return -ErrDriver;
    }

    int16_t fill_array_error = loadInputConfiguration(mode);
    if (fill_array_error != 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter::loadInputConfiguration", "Failed to execute loadInputConfiguration()");
        return fill_array_error;
    }

    int16_t merge_configurations_error = mergeConfigurations();
    if (merge_configurations_error != 0)
    {
        UAVCAN_TRACE("CanAcceptanceFilter", "Failed to compute optimal acceptance fliter's configuration");
        return merge_configurations_error;
    }

    return 0;
}

uint16_t CanAcceptanceFilterConfigurator::getNumFilters() const
{
	if (filters_number_ == 0)
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
        if (out > MaxCanAcceptanceFilters)
        {
            out = MaxCanAcceptanceFilters;
        }
    }

    return (out == InvalidOut) ? 0 : out;
	}
	else
	{
		return filters_number_;
	}
}

int16_t CanAcceptanceFilterConfigurator::addFilterConfig(const CanFilterConfig& config)
{
    if (multiset_configs_.emplace(config) == NULL)
    {
        return -ErrMemory;
    }

	return 0;
}

CanFilterConfig CanAcceptanceFilterConfigurator::mergeFilters(CanFilterConfig& a_, CanFilterConfig& b_)
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
