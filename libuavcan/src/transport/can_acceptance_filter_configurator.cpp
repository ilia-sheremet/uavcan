#include <uavcan/transport/can_acceptance_filter_configurator.hpp>
//#include <uavcan/debug.hpp>
#include <cassert>


namespace uavcan
{

void CanAcceptanceFilterConfigurator::computeConfiguration()
{
    fillArray();

    uint16_t i_ind, j_ind, rank, i_rank, j_rank, best_rank;
    FilterConfig temp_array;

    while (getNumFilters()  >= configs_.size())      /// "=" because we need +1 iteration for ServiceResponse
    {
        best_rank = 0;
        for (i_ind = 0; i_ind < configs_.size() - 1; i_ind++)
        {
            for (j_ind = 1; j_ind < configs_.size(); j_ind++)
            {
                if (((configs_[i_ind].id & configs_[i_ind].mask) | (configs_[j_ind].id & configs_[j_ind].mask)) != 0)
                {
                    temp_array = mergeFilters(configs_[i_ind], configs_[j_ind]);
                    rank = countBits(temp_array.mask);

                    if (rank > best_rank)
                    {
                        best_rank = rank;
                        i_rank = i_ind;
                        j_rank = j_ind;
                    }
                }
            }
        }

        configs_[j_rank] = mergeFilters(configs_[i_rank], configs_[j_rank]);
        configs_[i_rank].id = configs_[i_rank].mask = 0;
    }

    cleanZeroItems();

    FilterConfig ServiseRespFrame;
    ServiseRespFrame.id = 0;
    ServiseRespFrame.mask = 0x00060000;
    configs_.push_back(ServiseRespFrame);
}

void CanAcceptanceFilterConfigurator::fillArray()
{
    const TransferListenerBase* p = dispatcher.getListOfMessageListeners().get();
    configs_.reset();

    while (p)
    {
        FilterConfig cfg;

        cfg.id = static_cast<uint32_t>(p->getDataTypeDescriptor().getID()) << 19;
        cfg.id |= static_cast<uint32_t>(p->getDataTypeDescriptor().getKind()) << 17;
        cfg.mask = DefaultFilterMask;

        configs_.push_back(cfg);
        p = p->getNextListNode();
    }
}

void CanAcceptanceFilterConfigurator::cleanZeroItems()
{
    FilterConfig switch_array;

    for (int i = 0; i < configs_.size(); i++)
    {
        while ((configs_[i].mask & configs_[i].id) == 0)
        {
            switch_array = configs_[configs_.size()];
            configs_[i] = configs_[configs_.size()];
            configs_[configs_.size()] = switch_array;
            if ((i == configs_.size()) & ((configs_[i].mask & configs_[i].id) == 0)) // this function precludes two
            {                                                                        // last elements zeros
                configs_.pop_back();
                break;
            }
            configs_.pop_back();
        }
    }
}

CanAcceptanceFilterConfigurator::FilterConfig CanAcceptanceFilterConfigurator::mergeFilters(FilterConfig &a_, FilterConfig &b_)
{
    FilterConfig temp_arr;
    temp_arr.mask = a_.mask & b_.mask & ~(a_.id ^ b_.id);
    temp_arr.id = a_.id & temp_arr.mask;

    return (temp_arr);
}

uint32_t CanAcceptanceFilterConfigurator::countBits(uint32_t n_)
{
    uint32_t c_; // c accumulates the total bits set in v
    for (c_ = 0; n_; c_++)
    {
        n_ &= n_ - 1; // clear the least significant bit set
    }
    return c_;
}

uint16_t CanAcceptanceFilterConfigurator::getNumFilters() const
{
    static const uint16_t InvalidOut = 0xFFFF;
    uint16_t out = InvalidOut;
    const ICanDriver& can_driver = node_.getDispatcher().getCanIOManager().getCanDriver();

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
    }

    return (out == InvalidOut) ? 0 : out;
}

}


