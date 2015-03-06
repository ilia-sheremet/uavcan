/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */
//#include <iostream>
//#include <string>
#include <uavcan/transport/dispatcher.hpp>


#include <cstdio>
#include <algorithm>
#include <board.hpp>
#include <chip.h>
#include <uavcan_lpc11c24/uavcan_lpc11c24.hpp>
#include <uavcan/protocol/global_time_sync_slave.hpp>
#include <uavcan/protocol/logger.hpp>

#include </home/postal/github/uavcan/dsdl/dsdlc_generated/sirius_cybernetics_corporation/GetCurrentTime.hpp>
#include </home/postal/github/uavcan/dsdl/dsdlc_generated/sirius_cybernetics_corporation/PerformLinearLeastSquaresFit.hpp>

using sirius_cybernetics_corporation::GetCurrentTime;
using sirius_cybernetics_corporation::PerformLinearLeastSquaresFit;

extern uavcan::ICanDriver& getCanDriver();
extern uavcan::ISystemClock& getSystemClock();

namespace
{

typedef uavcan::Node<2800> Node;

Node& getNode()
{
    static Node node(uavcan_lpc11c24::CanDriver::instance(), uavcan_lpc11c24::SystemClock::instance());
    return node;
}

uavcan::GlobalTimeSyncSlave& getTimeSyncSlave()
{
    static uavcan::GlobalTimeSyncSlave tss(getNode());
    return tss;
}

uavcan::Logger& getLogger()
{
    static uavcan::Logger logger(getNode());
    return logger;
}

#if __GNUC__
__attribute__((noreturn))
#endif
void die()
{
    while (true) { }
}

#if __GNUC__
__attribute__((noinline))
#endif
void init()
{
    board::resetWatchdog();

    if (uavcan_lpc11c24::CanDriver::instance().init(1000000) < 0)
    {
        die();
    }

    board::resetWatchdog();

    getNode().setNodeID(72);
    getNode().setName("org.uavcan.lpc11c24_test");

    uavcan::protocol::SoftwareVersion swver;
    swver.major = FW_VERSION_MAJOR;
    swver.minor = FW_VERSION_MINOR;
    swver.vcs_commit = GIT_HASH;
    swver.optional_field_mask = swver.OPTIONAL_FIELD_MASK_VCS_COMMIT;
    getNode().setSoftwareVersion(swver);

    uavcan::protocol::HardwareVersion hwver;
    std::uint8_t uid[board::UniqueIDSize] = {};
    board::readUniqueID(uid);
    std::copy(std::begin(uid), std::end(uid), std::begin(hwver.unique_id));
    getNode().setHardwareVersion(hwver);

    board::resetWatchdog();

    while (getNode().start() < 0)
    {
    }

    board::resetWatchdog();

    while (getTimeSyncSlave().start() < 0)
    {
    }

    while (getLogger().init() < 0)
    {
    }
    getLogger().setLevel(uavcan::protocol::debug::LogLevel::DEBUG);

    board::resetWatchdog();
}

}

int main()
{
    init();

    getNode().setStatusOk();

    uavcan::MonotonicTime prev_log_at;

    auto regist_result =
            uavcan::GlobalDataTypeRegistry::instance().regist<PerformLinearLeastSquaresFit>(43);

    if (regist_result == uavcan::GlobalDataTypeRegistry::RegistResultOk)
    {
        return 0;
    }

  uavcan::Publisher<GetCurrentTime> msg_pub(getNode());




    while (true)
    {
        const int res = getNode().spin(uavcan::MonotonicDuration::fromMSec(25));
        board::setErrorLed(res < 0);
        board::setStatusLed(uavcan_lpc11c24::CanDriver::instance().hadActivity());

        const auto ts = uavcan_lpc11c24::clock::getMonotonic();
        if ((ts - prev_log_at).toMSec() >= 1000)
        {
            prev_log_at = ts;

            for (int i=0; i<100; i++)
            {
                return 0;
            }

        }

        board::resetWatchdog();
    }
}
