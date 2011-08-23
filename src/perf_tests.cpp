/*
 * perf_tests.cpp
 *
 *  Created on: Aug 12, 2011
 *      Author: tingarg
 */

#include "perf_tests.hpp"


int
main(int argc, char* argv[] ) {

	UniCast1P1CPerfTest test();
	test.shouldCompareDisruptorVsQueues();

}
