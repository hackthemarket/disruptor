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

class ValueEntry : public AbstractEntry {
private:
	long _value;

public:
	ValueEntry() : _value(0) {}

	long getValue() const { return _value; }

    void setValue(const long value) { _value = value; }

//    class Factory : public EntryFactory<ValueEntry> {
//    	ValueEntry* create() { return new ValueEntry(); }
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
//            System.gc();

            //disruptorOps = runDisruptorPass(i);
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

class ValueAdditionHandler : public BatchHandler<ValueEntry>
{
private:
	long _value;

public:
	long getValue()  { return _value; }

    void reset()  { _value = 0L; }

    void onAvailable(const ValueEntry entry) //throws Exception
    {
        _value += entry.getValue();
    }

    void onEndOfBatch() //throws Exception
    {
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
	tbb::atomic<bool> writing;

    void operator()() {
        _running = true;
         long val;
         size_t recvd;
         unsigned int pri;
         while (_running==true )  {
        	_blockingQ.pop(val);
        	// _blockingQ.receive(&val,sizeof(val), recvd, pri);
  //      	 if (recvd != sizeof(val) || val < 0) break;//_running = false;
         	_value += val;
            _sequence++;
//            if (!writing)
//           	std::cout << " consumer: popped: " << val << " seq = "
//           			<< getSequence() << " val = " << getValue()
//           			<< " writing = " << writing << std::endl;
         }
         std::cout << "consumer: done" << std::endl;
   }

    ValueAdditionQueueConsumer
    	(tbb::concurrent_bounded_queue<long>& blockingQueue)
    	: _value(0), _blockingQ(blockingQueue)
    { }

    long getValue() { return _value; }

	void reset()  {
		_value = 0L;
        _sequence = -1L;
        writing = true;
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
private:
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
    static const long ITERATIONS = 1000L * 1000L * 500L;
	//static const long ITERATIONS = 1000L * 1000L * 10L ;//* 500L;
  //  const ExecutorService EXECUTOR = Executors.newSingleThreadExecutor();
    const long _expectedResult;

//    tbb::concurrent_bounded_queue<long> _blockingQ;
    tbb::concurrent_bounded_queue<long>  _blockingQ;
    ValueAdditionQueueConsumer* _qConsumer;// = new ValueAdditionQueueConsumer(blockingQueue);

    ///////////////////////////////////////////////////////////////////////////////////////////////

    RingBuffer<ValueEntry> _ring;
//        new RingBuffer<ValueEntry>(ValueEntry::Factory, SIZE,
//                                   SingleThreadedStrategy,
//                                   YieldingWait);
    ConsumerBarrier<ValueEntry>* _consumerBarrier;// = ringBuffer.createConsumerBarrier();
	ValueAdditionHandler* _handler;// = new ValueAdditionHandler();
    BatchConsumer<ValueEntry>* _batchConsumer;// = new BatchConsumer<ValueEntry>(consumerBarrier, handler);
    ProducerBarrier<ValueEntry>* _producerBarrier;// = ringBuffer.createProducerBarrier(batchConsumer);

    ///////////////////////////////////////////////////////////////////////////////////////////////

public :
    UniCast1P1CPerfTest(std::vector<Consumer*> consumers )
    : _expectedResult(CalcExpectedResult()) ,
      _blockingQ( tbb::concurrent_bounded_queue<long>()),
      _qConsumer(new ValueAdditionQueueConsumer(_blockingQ)),
      _ring( SIZE,
    		  new SingleThreadedStrategy(), new YieldingWait()),
      _consumerBarrier(_ring.createConsumerBarrier(consumers)),
      _handler(new ValueAdditionHandler()),
      _batchConsumer( new BatchConsumer<ValueEntry>(_consumerBarrier, _handler))
      //_producerBarrier(_ring.createProducerBarrier(std::vector _batchConsumer))

    {
    	std::vector<Consumer*> cons;
    	cons.push_back(_batchConsumer);
    	_producerBarrier = _ring.createProducerBarrier(cons);
    	_blockingQ.set_capacity(SIZE);
    	std::cout << "set q size to " << SIZE << std::endl;
    }

    ~UniCast1P1CPerfTest() {  boost::interprocess::message_queue::remove("UniCast1P1CPerfTest_Q"); }


    void shouldCompareDisruptorVsQueues()
       // throws Exception
    {
        testImplementations();
    }

    virtual long runQueuePass(const int passNumber) //throws InterruptedException
    {
        _qConsumer->reset();
        //Future future = EXECUTOR.submit(_qConsumer);

        //long start = System.currentTimeMillis();
        boost::posix_time::ptime start =
        		boost::posix_time::microsec_clock::universal_time();

        boost::thread task(boost::ref(*_qConsumer));

        for (long i = 0; i < ITERATIONS; i++)
        {
        	_blockingQ.push(i);
//            std::cout << "producer: pushed " << i << std::endl;
        }
        std::cout << "producer: done writing "  << std::endl;
        _qConsumer->writing = false;
        const long expectedSequence = ITERATIONS - 1L;
        while ( _blockingQ.size() > 0 )
        {
  //            // busy spin
        }
        std::cout << "producer: done spin "  << std::endl;

        boost::posix_time::time_period per(start,
        		boost::posix_time::microsec_clock::universal_time());
        boost::posix_time::time_duration dur = per.length();
        long opsPerSecond = (ITERATIONS * 1000L) / dur.total_milliseconds();
        std::cout << "ops per sec: " << opsPerSecond << std::endl;
        _qConsumer->halt();
       // future.cancel(true);  TODO!!
      // task.join();
        std::cout << "value = " << _qConsumer->getValue() << std::endl;
        std::cout << "expected = " << CalcExpectedResult() << std::endl;

        assert(CalcExpectedResult() == _qConsumer->getValue());

        return opsPerSecond;
    }

    virtual long runDisruptorPass(int passNumber)// throws InterruptedException
    {
        _handler->reset();

//       EXECUTOR.submit(_batchConsumer);  TODO!!


        boost::posix_time::ptime start =
        		boost::posix_time::microsec_clock::universal_time();

        for (long i = 0; i < ITERATIONS; i++)
        {
            ValueEntry entry = _producerBarrier->nextEntry();
            entry.setValue(i);
            _producerBarrier->commit(entry);
        }

        const long expectedSequence = _ring.getCursor();
        while (_batchConsumer->getSequence() < expectedSequence)
        {
            // busy spin
        }

        boost::posix_time::time_period per(start,
        		boost::posix_time::microsec_clock::universal_time());
        boost::posix_time::time_duration dur = per.length();
        long opsPerSecond = (ITERATIONS * 1000L) / dur.total_milliseconds();

        _batchConsumer->halt();

        assert(CalcExpectedResult() == _handler->getValue());

        return opsPerSecond;
    }

    virtual std::string testName() {
    	return "UniCast1P1CPerfTest";
    }

}; // UniCast1P1CPerfTest


};  // namespace disruptor
#endif /* PERF_TESTS_HPP_ */
