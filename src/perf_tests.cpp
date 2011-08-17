/*
 * perf_tests.cpp
 *
 *  Created on: Aug 12, 2011
 *      Author: tingarg
 */

#include "perf_tests.hpp"



void main(char** args) {

	UniCast1P1CPerfTest test();
	test.shouldCompareDisruptorVsQueues();

}
