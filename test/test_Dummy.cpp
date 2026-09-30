#include <boost/test/unit_test.hpp>
#include <svp_aml_datalogger/Dummy.hpp>

using namespace svp_aml_datalogger;

BOOST_AUTO_TEST_CASE(it_should_not_crash_when_welcome_is_called)
{
    svp_aml_datalogger::DummyClass dummy;
    dummy.welcome();
}
