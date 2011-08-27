/*
 * perf_tests.cpp
 *
 *  Created on: Aug 12, 2011
 *      Author: tingarg
 */

#include "perf_tests.hpp"

using namespace disruptor;

int
main(int argc, char* argv[] ) {
	boost::interprocess::message_queue::remove("UniCast1P1CPerfTest_Q");

	std::vector<Consumer*> consumers;
	UniCast1P1CPerfTest test(consumers);
	test.shouldCompareDisruptorVsQueues();

	return EXIT_SUCCESS;
}
