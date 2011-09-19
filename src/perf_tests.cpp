/*
 * perf_tests.cpp
 *
 *  Created on: Aug 12, 2011
 *      Author: tingarg
 */

#include "perf_tests.hpp"

using namespace disruptor;
using namespace std;

int
main(int argc, char* argv[] ) {
	try {
		std::vector<Sequence*> consumers;
		UniCast1P1CPerfTest test(consumers);
		test.shouldCompareDisruptorVsQueues();

	//	UniCast1P1CBatchPerfTest test1(consumers);
	//	test1.shouldCompareDisruptorVsQueues();
	} catch( std::exception& e) {
		cout << e.what() << endl;
	} catch ( ... ) {
		cout << "boom" << endl;
	}
	return EXIT_SUCCESS;
}
