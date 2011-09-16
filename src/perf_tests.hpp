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

//    class Factory : public EntryFactory<ValueEvent> {
//    	ValueEvent* create() { return new ValueEvent(); }
//    };
};

class AbstractPerfTestQueueVsDisruptor {

public :

    void testImplementations()
    //throws Exception
    {
        const int RUNS = 3;
        long disruptorOps = 0L;
        long queueOps = 0L;

        for (int i = 0; i < RUNS; i++)
        {
            disruptorOps = runDisruptorPass(i);
            queueOps = runQueuePass(i);
            printResults(disruptorOps, queueOps, i);
        }

 //       Assert.assertTrue("Performance degraded", disruptorOps > queueOps);
    }


    void printResults(const long disruptorOps, const long queueOps, const int i)
    {
    	std::cout << testName() << " OpsPerSecond run #" << i
    			<< " : BlockingQueue=" << queueOps << ", Disruptor="
    			<< disruptorOps << std::endl;
    }

    virtual long runQueuePass(int passNumber) = 0; //throws Exception;

    virtual long runDisruptorPass(int passNumber) = 0; //throws Exception;

    virtual void shouldCompareDisruptorVsQueues() = 0; // throws Exception;

    virtual std::string testName() = 0;
}; // AbstractPerfTestQueueVsDisruptor

class ValueAdditionHandler : public BatchHandler<ValueEvent>
{
private:
	long _value;

public:
	long getValue()  { return _value; }

    void reset()  { _value = 0L; }

    void onAvailable(const ValueEvent& entry) //throws Exception
    {
        _value += entry.getValue();
    //	std::cout << "#" << entry.getValue() << " --> " << _value << std::endl;
    }

    void onEndOfBatch() //throws Exception
    {
    //	std::cout << "VAH: onEndOfBatch()" << std::endl;
    }
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
    static const long CalcExpectedResult()
    {
        long temp = 0L;
        for (long i = 0L; i < ITERATIONS; i++)
        {
            temp += i;
        }

        return temp;
    }

	static const long SIZE = 1024 * 32;
 //   static const long ITERATIONS = 1000L * 1000L * 500L;
	static const long ITERATIONS = 1000L * 1000L * 20L ;//* 500L;

	const long 									_expectedResult;
    tbb::concurrent_bounded_queue<long>  		_blockingQ;
    ValueAdditionQueueConsumer* 				_qConsumer;
    RingBuffer<ValueEvent> 						_ring;
    SequenceBarrier<ValueEvent>* 				_consumerBarrier;
	ValueAdditionHandler* 						_handler;
    BatchEventProcessor<ValueEvent>* 					_batchConsumer;
    ProducerBarrier<ValueEvent>* 				_producerBarrier;

public :

    UniCast1P1CPerfTest(std::vector<EventProcessor*>& consumers )
    : _expectedResult(CalcExpectedResult()) ,
      _blockingQ( tbb::concurrent_bounded_queue<long>()),
      _qConsumer(new ValueAdditionQueueConsumer(_blockingQ)),
      _ring( SIZE,
    		  new SingleThreadedStrategy(), new YieldingWait<ValueEvent>()),
      _consumerBarrier(_ring.createConsumerBarrier(consumers)),
      _handler(new ValueAdditionHandler()),
      _batchConsumer( new BatchEventProcessor<ValueEvent>(_consumerBarrier, _handler)),
      _producerBarrier(_ring.createProducerBarrier(_batchConsumer))

    {
    	//consumers.push_back(_batchConsumer);
    	_blockingQ.set_capacity(SIZE);
    	std::cout << "set q size to " << SIZE << std::endl;
    }

    ~UniCast1P1CPerfTest() {

    }

    void shouldCompareDisruptorVsQueues()
       // throws Exception
    {
        testImplementations();
    }

    virtual long runQueuePass(const int passNumber) //throws InterruptedException
    {
        _qConsumer->reset();
        boost::posix_time::ptime start =
        		boost::posix_time::microsec_clock::universal_time();

        boost::thread task(boost::ref(*_qConsumer));

        for (long i = 0; i < ITERATIONS; i++)
        {
        	_blockingQ.push(i);
        }
       // std::cout << "producer: done writing "  << std::endl;

        const long expectedSequence = ITERATIONS - 1L;

        _qConsumer->halt();
        task.join();

        boost::posix_time::time_period per(start,
        		boost::posix_time::microsec_clock::universal_time());
        boost::posix_time::time_duration dur = per.length();
        long opsPerSecond = (ITERATIONS * 1000L) / dur.total_milliseconds();

//		std::cout << "op/s: " << opsPerSecond << std::endl;
//		std::cout << "expected: " << CalcExpectedResult() << " value: "
//				<< _handler->getValue() << std::endl;
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

//        std::cout << "launched disruptor's consumer... " << std::endl;

        for (long i = 0; i < ITERATIONS; i++)
        {
            ValueEvent& entry = _producerBarrier->nextEntry();
            entry.setValue(i);
 //           std::cout << "producer put " << i << std::endl;
            _producerBarrier->commit(entry);
        }

        const long expectedSequence = _ring.getCursor();
        while (_batchConsumer->getSequence().get() < expectedSequence)
        {
            boost::thread::yield();
        }
        _batchConsumer->halt();
        task.interrupt();
        task.join();
 //       std::cout << "joined BatchConsumer... done " << std::endl;
        boost::posix_time::time_period per(start,
        		boost::posix_time::microsec_clock::universal_time());
        boost::posix_time::time_duration dur = per.length();
        long opsPerSecond = (ITERATIONS * 1000L) / dur.total_milliseconds();

//        std::cout << "op/s: " << opsPerSecond << std::endl;
//        std::cout << "expected: " << CalcExpectedResult() << "value: "
//        		<< _handler->getValue() << std::endl;
        assert(CalcExpectedResult() == _handler->getValue());

        return opsPerSecond;
    }

    virtual std::string testName() {
    	return "UniCast1P1CPerfTest";
    }

}; // UniCast1P1CPerfTest

/**
 * <pre>
 * UniCast a series of items between 1 producer and 1 consumer.
 * This test illustrates the benefits of writing batches of 10 entries
 * for exchange at a time.
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
class UniCast1P1CBatchPerfTest : public UniCast1P1CPerfTest //AbstractPerfTestQueueVsDisruptor
{
public:

    UniCast1P1CBatchPerfTest(std::vector<EventProcessor*>& consumers )
    : UniCast1P1CPerfTest(consumers)
    {
    }

    virtual long runDisruptorPass(int passNumber) //throws InterruptedException
    {
        _handler->reset();
        boost::thread task(boost::bind(&BatchEventProcessor<ValueEvent>::run,
        		boost::ref(*_batchConsumer)));

        const int batchSize = 10;
        SequenceBatch sequenceBatch(batchSize);

        boost::posix_time::ptime start =
        		boost::posix_time::microsec_clock::universal_time();

        long offset = 0;
        for (long i = 0; i < ITERATIONS; i += batchSize)
        {
            _producerBarrier->nextEntries(&sequenceBatch);
            for (long c = sequenceBatch.getStart(), end = sequenceBatch.getEnd(); c <= end; c++)
            {
                ValueEvent& entry = _producerBarrier->getEntry(c);
                entry.setValue(offset++);
std::cout << "producer: set " << entry.getValue() << " on " << c << std::endl;
            }
            _producerBarrier->commit(&sequenceBatch);
        }

        const long expectedSequence = _ring.getCursor();
        while (_batchConsumer->getSequence().get() < expectedSequence)
        {
            // busy spin
            //boost::thread::yield();
        }

        _batchConsumer->halt();
        task.interrupt();
        task.join();
 //       std::cout << "joined BatchConsumer... done " << std::endl;
        boost::posix_time::time_period per(start,
        		boost::posix_time::microsec_clock::universal_time());
        boost::posix_time::time_duration dur = per.length();
        long opsPerSecond = (ITERATIONS * 1000L) / dur.total_milliseconds();

//        std::cout << "op/s: " << opsPerSecond << std::endl;
//        std::cout << "expected: " << CalcExpectedResult() << "value: "
//        		<< _handler->getValue() << std::endl;
        assert(CalcExpectedResult() == _handler->getValue());


        return opsPerSecond;
    }
}; // UniCast1P1CBatchPerfTest




};  // namespace disruptor
#endif /* PERF_TESTS_HPP_ */
