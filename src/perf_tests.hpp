/*
 * perf_tests.hpp
 *
 *  Created on: Aug 16, 2011
 *      Author: tingarg
 */

#include "disruptor.hpp"
#include <tbb/concurrent_queue.h>
#include <boost/interprocess/ipc/message_queue.hpp>
#include <tbb/task.h>

#ifndef PERF_TESTS_HPP_
#define PERF_TESTS_HPP_

namespace disruptor {

class ValueEvent {
private:
	long _value;

public:
	ValueEvent() : _value(0) {}

	long getValue() const { return _value; }

    void setValue(const long value) { _value = value; }
};

class AbstractPerfTestQueueVsDisruptor {

public :

    void testImplementations()  {
        const int RUNS = 3;
        long disruptorOps = 0L;
        long queueOps = 0L;

        for (int i = 0; i < RUNS; i++)  {
            disruptorOps = runDisruptorPass(i);
            queueOps = runQueuePass(i);
            printResults(disruptorOps, queueOps, i);
        }
    }

    void printResults
    	(const long disruptorOps, const long queueOps, const int i) {

    	std::cout << testName() << " OpsPerSecond run #" << i
    			<< " : BlockingQueue=" << queueOps << ", Disruptor="
    			<< disruptorOps << std::endl;
    }

    virtual long runQueuePass(int passNumber) = 0;

    virtual long runDisruptorPass(int passNumber) = 0;

    virtual void shouldCompareDisruptorVsQueues() = 0;

    virtual std::string testName() = 0;
}; // AbstractPerfTestQueueVsDisruptor

class ValueAdditionHandler : public EventHandler<ValueEvent>
{
private:
	PaddedLong _value;

public:
    virtual void onEvent(ValueEvent& ev, long sequence, bool endOfBatch)  {
        _value += ev.getValue();
    }

	long getValue()  { return _value; }

    void reset()  { _value = 0L; }

    void onAvailable(const ValueEvent& entry)
    	{ _value += entry.getValue(); }

    void onEndOfBatch() {}

};  // ValueAdditionHandler

class ValueAdditionQueueConsumer : public boost::noncopyable  //implements Runnable
{
private :
	tbb::atomic<bool> _running;
    tbb::atomic<long> _sequence;
    long 		_value;
    tbb::concurrent_bounded_queue<long>& _blockingQ;

public:

    ValueAdditionQueueConsumer
    	(tbb::concurrent_bounded_queue<long>& blockingQueue)
    	: _value(0), _blockingQ(blockingQueue)
    { }

    void operator()() {
    	_running = true;
    	long val;
    	while (_running || _blockingQ.size() > 0 )  {
    		_blockingQ.pop(val);
    		_value += val;
    		_sequence++;
    	}
    }

    long getValue() { return _value; }

	void reset()  {
		_value = 0L;
        _sequence = -1L;
    }

    long getSequence() { return _sequence; }

    void halt() { _running = false; }

};// ValueAdditionQueueConsumer


/**
 * <pre>
 * UniCast a series of items between 1 producer and 1 consumer.
 *
 * +----+    +----+
 * | P0 |--->| C0 |
 * +----+    +----+
 *
 *
 * Queue Based:
 * ============
 *
 *        put      take
 * +----+    +====+    +----+
 * | P0 |--->| Q0 |<---| C0 |
 * +----+    +====+    +----+
 *
 * P0 - Producer 0
 * Q0 - Queue 0
 * C0 - Consumer 0
 *
 *
 * Disruptor:
 * ==========
 *                   track to prevent wrap
 *             +-----------------------------+
 *             |                             |
 *             |                             v
 * +----+    +====+    +====+    +====+    +----+
 * | P0 |--->| PB |--->| RB |<---| CB |    | C0 |
 * +----+    +====+    +====+    +====+    +----+
 *                claim      get    ^        |
 *                                  |        |
 *                                  +--------+
 *                                    waitFor
 *
 * P0 - Producer 0
 * PB - ProducerBarrier
 * RB - RingBuffer
 * CB - ConsumerBarrier
 * C0 - Consumer 0
 *
 * </pre>
 */
class UniCast1P1CPerfTest : public AbstractPerfTestQueueVsDisruptor {
protected:
    static const long CalcExpectedResult()   {
        long temp = 0L;
        for (long i = 0L; i < ITERATIONS; i++)
        {
            temp += i;
        }

        return temp;
    }

	static const long SIZE = 1024 * 32;
	static const long ITERATIONS = 1000L * 1000L * 300L ;

	const long 									_expectedResult;
    tbb::concurrent_bounded_queue<long>  		_blockingQ;
    ValueAdditionQueueConsumer* 				_qConsumer;
    RingBuffer<ValueEvent> 						_ring;
    SequenceBarrier<ValueEvent>* 				_consumerBarrier;
	ValueAdditionHandler* 						_handler;
    BatchEventProcessor<ValueEvent>* 			_batchConsumer;

public :

    UniCast1P1CPerfTest(std::vector<Sequence*>& consumers )
    : _expectedResult(CalcExpectedResult()) ,
      _blockingQ( tbb::concurrent_bounded_queue<long>()),
      _qConsumer(new ValueAdditionQueueConsumer(_blockingQ)),
      _ring( SIZE ),
      _consumerBarrier(_ring.newBarrier(consumers)),
      _handler(new ValueAdditionHandler()),
      _batchConsumer( new BatchEventProcessor<ValueEvent>
    						(&_ring, _consumerBarrier, _handler))

    {
    	_ring.setGatingSequence(_batchConsumer->getSequence());
    	_blockingQ.set_capacity(SIZE);
    	std::cout << "set q size to " << SIZE << std::endl;
    }

    ~UniCast1P1CPerfTest() {    }

    void shouldCompareDisruptorVsQueues()  { testImplementations(); }

    virtual long runQueuePass(const int passNumber) {
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
    }

    virtual long runDisruptorPass(int passNumber)// throws InterruptedException
    {
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
            entry.setValue(i);
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

    virtual std::string testName() { return "UniCast1P1CPerfTest"; }

}; // UniCast1P1CPerfTest


};  // namespace disruptor
#endif /* PERF_TESTS_HPP_ */
