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

namespace disruptor {

void AbstractPerfTestQueueVsDisruptor::testImplementations() {
	const int RUNS = 3;
	long disruptorOps = 0L;
	long queueOps = 0L;

	for (int i = 0; i < RUNS; i++)  {
		disruptorOps = runDisruptorPass(i);
		queueOps = runQueuePass(i);
		printResults(disruptorOps, queueOps, i);
	}
} //testImplementations

void AbstractPerfTestQueueVsDisruptor::printResults
	(const long disruptorOps, const long queueOps, const int i) {

	std::cout << testName() << " OpsPerSecond run #" << i
			<< " : BlockingQueue=" << queueOps << ", Disruptor="
			<< disruptorOps << std::endl;
};

long UniCast1P1CPerfTest::runQueuePass(int passNumber) {
    _qConsumer->reset();
    boost::posix_time::ptime start =
    		boost::posix_time::microsec_clock::universal_time();

    boost::thread task(boost::ref(*_qConsumer));

    for (long i = 0; i < ITERATIONS; i++)
    	{ _blockingQ.push(i); }

    const long expectedSequence = ITERATIONS - 1L;

    _qConsumer->halt();
    task.join();

    boost::posix_time::time_period per(start,
    		boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::time_duration dur = per.length();
    long opsPerSecond = (ITERATIONS * 1000L) / dur.total_milliseconds();

    assert(CalcExpectedResult() == _qConsumer->getValue());

    return opsPerSecond;
} // runQueuePass

long UniCast1P1CPerfTest::runDisruptorPass(int passNumber) {
    _handler->reset();
    boost::thread task(boost::bind(&BatchEventProcessor<ValueEvent>::run,
    		boost::ref(*_batchConsumer)));
    boost::posix_time::ptime start =
    		boost::posix_time::microsec_clock::universal_time();
    // publish all 'events'
    //
    for (long i = 0; i < ITERATIONS; i++) {
    	long sequence = _ring.next();
        ValueEvent& entry = _ring.get(sequence);
        entry.set(i);
        _ring.publish(sequence);
    }

    const long expectedSequence = _ring.getCursor();
    // wait 'til they're 'processed'
    //
    while (_batchConsumer->getSequence().get() < expectedSequence) {
        boost::thread::yield();
    }
    _batchConsumer->halt();
    task.interrupt();
    task.join();
    boost::posix_time::time_period per(start,
    		boost::posix_time::microsec_clock::universal_time());
    boost::posix_time::time_duration dur = per.length();
    long opsPerSecond = (ITERATIONS * 1000L) / dur.total_milliseconds();

    assert(CalcExpectedResult() == _handler->getValue());

    return opsPerSecond;
}

}; // namespace disruptor
