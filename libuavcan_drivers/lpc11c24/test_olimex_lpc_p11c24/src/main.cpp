/*
 * Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
 */

#include <cstdlib>
#include <unistd.h>

#include <cstdio>
#include <algorithm>
#include <board.hpp>
#include <chip.h>
#include <uavcan_lpc11c24/uavcan_lpc11c24.hpp>
#include <uavcan/protocol/global_time_sync_slave.hpp>
#include <uavcan/protocol/logger.hpp>

#include </home/postal/github/uavcan/libuavcan_drivers/lpc11c24/test_olimex_lpc_p11c24/das_test/MyDataType.hpp>


using das_test::MyDataType;



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

    getNode().setNodeID(112);
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

    uavcan::GlobalDataTypeRegistry::instance().regist<MyDataType>(100);

//    auto descriptor = uavcan::GlobalDataTypeRegistry::instance().find(uavcan::DataTypeKindService,
//                                                                         "das_test.MyDataType");
//    assert(descriptor->getID() == uavcan::DataTypeID(45));
//
//    if (descriptor == 0)
//        return 0;

    uavcan::Publisher<MyDataType> msg_pub(getNode());

    das_test::MyDataType trnsmt_msg;
    trnsmt_msg.my_number = 0;

    for (int i=0; i<100; i++)
              {
                  trnsmt_msg.my_number ++;
                  msg_pub.broadcast(trnsmt_msg);
              }


    while (true)
    {
        const int res = getNode().spin(uavcan::MonotonicDuration::fromMSec(25));
        board::setErrorLed(res < 0);
        board::setStatusLed(uavcan_lpc11c24::CanDriver::instance().hadActivity());


          const auto ts = uavcan_lpc11c24::clock::getMonotonic();
        if ((ts - prev_log_at).toMSec() >= 1000)
        {
            prev_log_at = ts;

        }

        board::resetWatchdog();
    }
}
