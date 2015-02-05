#pragma once

#include <cassert>
#include <uavcan/marshal/array.hpp>
#include <uavcan/stdint.hpp>
#include <uavcan/data_type.hpp>
#include <uavcan/error.hpp>
#include <uavcan/transport/dispatcher.hpp>
#include <uavcan/node/abstract_node.hpp>

#ifdef DEFAULT_FILTER_MASK
static const unsigned DefaultFilterMask = DEFAULT_FILTER_MASK;
#else
static const unsigned DefaultFilterMask = 0x1ffe0000;
#endif

namespace uavcan
{

class CanAcceptanceFilterConfigurator
{
        struct FilterConfig : public CanFilterConfig
                        {
                            enum { MinBitLen = sizeof(CanFilterConfig) * 8 };
                            enum { MaxBitLen = sizeof(CanFilterConfig) * 8 };
                        };

	typedef Array<FilterConfig, ArrayModeDynamic, CanAcceptanceFilterConfiguratorBufferSize> ConfigArray;

	typedef typename ConfigArray::SizeType IndexType;  //configs_[IndexType(i_ind)]

	FilterConfig mergeFilters(FilterConfig &a_, FilterConfig &b_);
	uint32_t countBits(uint32_t n_);
	void fillArray();
	void cleanZeroItems();
	uint16_t getNumFilters() const;

	INode& node_;               ///< Node reference is needed for access to ICanDriver and Dispatcher
	ConfigArray configs_;

public:

	 explicit CanAcceptanceFilterConfigurator(INode& node);

	 void computeConfiguration();

	 /**
	 * This method loads the configuration computed with computeConfiguration() to the CAN driver.
	 * Returns negative error code, see libuavcan/include/uavcan/error.hpp
	 */
	 int16_t applyConfiguration();

	 /**
	 * Returns the configuration computed with computeConfiguration().
	 * If computeConfiguration() has not been called yet, an empty configuration will be returned.
	 */
	 const ConfigArray& getConfiguration() { return configs_; }


};

}
