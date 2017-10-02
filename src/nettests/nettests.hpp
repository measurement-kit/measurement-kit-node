#ifndef MK_NODE_NETTESTS_NETTESTS_H
#define MK_NODE_NETTESTS_NETTESTS_H

#include <nan.h>
#include <measurement_kit/nettests.hpp>
#include "base.hpp"

#define MK_NODE_DECLARE_TEST(_name_)                                       \
    class _name_ : public BaseTest {                                       \
      public:                                                              \
        static std::string getClassName() { return #_name_; };              \
      private:                                                             \
        mk::nettests::_name_ test;                                         \
    }

MK_NODE_DECLARE_TEST(DashTest);
MK_NODE_DECLARE_TEST(DnsInjectionTest);
MK_NODE_DECLARE_TEST(HttpHeaderFieldManipulationTest);
MK_NODE_DECLARE_TEST(HttpInvalidRequestLineTest);
MK_NODE_DECLARE_TEST(MeekFrontedRequestsTest);
MK_NODE_DECLARE_TEST(MultiNdtTest);
MK_NODE_DECLARE_TEST(NdtTest);
MK_NODE_DECLARE_TEST(TcpConnectTest);
MK_NODE_DECLARE_TEST(WebConnectivityTest);

//MK_NODE_DECLARE_TEST(TelegramTest);
//MK_NODE_DECLARE_TEST(FacebookMessengerTest);
//MK_NODE_DECLARE_TEST(CaptivePortalTest);

#endif
