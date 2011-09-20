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

	long get() const { return _value; }

    void set(const long value) { _value = value; }
};

class AbstractPerfTestQueueVsDisruptor {

public :

    void testImplementations()  ;

    void printResults
    	(const long disruptorOps, const long queueOps, const int i) ;

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
        _value += ev.get();
    }

	long getValue()  { return _value; }

    void reset()  { _value = 0L; }

    void onAvailable(const ValueEvent& entry){  _value += entry.get(); }

    void onEndOfBatch() {}

};  // ValueAdditionHandler

class ValueAdditionQueueConsumer : public boost::noncopyable
{
private :
	tbb::atomic<bool> 						_running;
    tbb::atomic<long> 						_sequence;
    long 									_value;
    tbb::concurrent_bounded_queue<long>& 	_blockingQ;

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

    virtual long runQueuePass(int passNumber) ;

    virtual long runDisruptorPass(int passNumber) ;

    virtual std::string testName() { return "UniCast1P1CPerfTest"; }

}; // UniCast1P1CPerfTest

};  // namespace disruptor
#endif /* PERF_TESTS_HPP_ */
