/*
 * disruptor.hpp
 *
 *  Created on: Aug 8, 2011
 *      Author: tingarg
 */

#ifndef DISRUPTOR_HPP_
#define DISRUPTOR_HPP_

#include <tbb/atomic.h>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace disruptor {

const static long INITIAL_CURSOR_VALUE = -1L;

/**
 *             ---- Hacked straight out of Java library. -----
 *
 * Returns the number of zero bits preceding the highest-order
 * ("leftmost") one-bit in the two's complement binary representation
 * of the specified <tt>int</tt> value.  Returns 32 if the
 * specified value has no one-bits in its two's complement representation,
 * in other words if it is equal to zero.
 *
 * <p>Note that this method is closely related to the logarithm base 2.
 * For all positive <tt>int</tt> values x:
 * <ul>
 * <li>floor(log<sub>2</sub>(x)) = <tt>31 - numberOfLeadingZeros(x)</tt>
 * <li>ceil(log<sub>2</sub>(x)) = <tt>32 - numberOfLeadingZeros(x - 1)</tt>
 * </ul>
 *
 * @return the number of zero bits preceding the highest-order
 *     ("leftmost") one-bit in the two's complement binary representation
 *     of the specified <tt>int</tt> value, or 32 if the value
 *     is equal to zero.
 * @since 1.5
 */
inline int numberOfLeadingZeros(int i) {
    // HD, Figure 5-6
    if (i == 0) return 32;
    int n = 1;
    if (i >> 16 == 0) { n += 16; i <<= 16; }
    if (i >> 24 == 0) { n +=  8; i <<=  8; }
    if (i >> 28 == 0) { n +=  4; i <<=  4; }
    if (i >> 30 == 0) { n +=  2; i <<=  2; }
    n -= i >> 31;
    return n;
}

/**
  * Calculate the next power of 2, greater than or equal to x.<p>
  * From Hacker's Delight, Chapter 3, Harry S. Warren Jr.
  *
  * @param x Value to round up
  * @return The next power of 2 from x inclusive
  */
inline int ceilingNextPowerOfTwo(const int x) {
     return 1 << (32 - numberOfLeadingZeros(x - 1));
}

/**
 * Used to alert consumers waiting at a {@link ConsumerBarrier} of status changes.
 */
class AlertException : public std::exception {
private:
  static const AlertException * _Alert;
  static boost::shared_mutex _Mutex;

public:

  static const AlertException & Alert() {
	  if (_Alert==NULL) {
		  boost::unique_lock< boost::shared_mutex > lock(_Mutex);
		  _Alert = new AlertException();
	  }
	  return *_Alert;
  }
}; // AlertException


class AbstractEntry {
private:
    long _sequence;

public:

    /**Get the sequence number assigned to this item in the series.    */
    long getSequence () const { return _sequence; }

    /**
	 * Explicitly set the sequence number for this Entry and a CommitCallback
	 * for indicating when the producer is finished with assigning data
	 * for exchange.     */
    void setSequence(const long sequence) { _sequence = sequence; }

}; // AbstractEntry


inline std::ostream& operator<<(std::ostream& os, const AbstractEntry& entry) {
	os << "AbstractEntry#" << entry.getSequence();
	return os;
}

/**
 * EntryConsumers waitFor {@link AbstractEntry}s to become available for
 * consumption from the {@link RingBuffer}
 */
class Consumer {
public:
	/**
	 * Get the sequence up to which this Consumer has consumed
	 * {@link AbstractEntry}s
	 */
    virtual long getSequence() const = 0;

    /**
     * Signal that this Consumer should stop when it has finished consuming at
     * the next clean break.  It will call {@link ConsumerBarrier#alert()} to
     * notify the thread to check status.
     */
    virtual void halt() = 0;

    // Runnable...
    virtual void run() = 0;

}; // consumer

/**
 * Get the minimum sequence from an array of {@link Consumer}s.
 *
 * @param consumers to compare.
 * @return the minimum sequence found or Long.MAX_VALUE if the array is empty.
 */
inline long getMinimumSequence(const std::vector<Consumer*> consumers) {
    long minimum = LONG_MAX;
    for (std::vector<Consumer*>::const_iterator it = consumers.begin();
    			it!=consumers.end(); ++it) {
    	const Consumer *consumer = *it;
    	minimum = std::min(minimum, consumer->getSequence());
    }
    return minimum;
}

/**
 * Used to record the batch of sequences claimed in a {@link RingBuffer}.
 */
class SequenceBatch {
private:
	const int _size;
	long _end;

public:
    /**
     * Create a holder for tracking a batch of claimed sequences
     * in a {@link RingBuffer}
     * @param size of the batch to claim.
     */
    SequenceBatch(const int size) : _size(size), _end(INITIAL_CURSOR_VALUE)
    {
    //	std::cout << "SequenceBatch: " << _size << std::endl;
    }

    /**
     * Get the end sequence of a batch.
     *
     * @return the end sequence in a batch
     */
    long getEnd() const { return _end; }

    /**
     * Set the end of the batch sequence.  To be used by
     * the {@link ProducerBarrier}.
     *
     * @param end sequence in the batch.
     */
    void setEnd(const long end) {
    	this->_end = end;
    //	std::cout << "SequenceBatch: end = " << _end << std::endl;
    }

    /**
     * Get the size of the batch.
     *
     * @return the size of the batch.
     */
    int getSize() const { return _size; }

    /**
     * Get the starting sequence for a batch.
     *
     * @return the starting sequence of a batch.
     */
    long getStart() const { return _end - (_size - 1L); }
};

/**
 * Abstraction for claiming {@link AbstractEntry}s in a {@link RingBuffer}
 * while tracking dependent {@link Consumer}s
 *
 * @param <T> {@link AbstractEntry} implementation stored in
 * the {@link RingBuffer}
 */
template <typename T>
class ProducerBarrier {
public:
	/**
     * Claim the next {@link AbstractEntry} in sequence for a producer
     * on the {@link RingBuffer}
     *
     * @return the claimed {@link AbstractEntry}
     */
	virtual T& nextEntry() = 0;

    /**
     * Claim the next batch of {@link AbstractEntry}s in sequence.
     *
     * @param sequenceBatch to be updated for the batch range.
     * @return the updated sequenceBatch.
     */
 //   virtual SequenceBatch nextEntries(SequenceBatch sequenceBatch)= 0;
	virtual SequenceBatch* nextEntries(SequenceBatch* sequenceBatch) = 0;

    /**
     * Commit an entry back to the {@link RingBuffer} to make it visible
     * to {@link Consumer}s
     * @param entry to be committed back to the {@link RingBuffer}
     */
    virtual void commit(T& entry) = 0;

    /**
     * Commit the batch of entries back to the {@link RingBuffer}.
     *
     * @param sequenceBatch to be committed.
     */
    virtual void commit(SequenceBatch* sequenceBatch) = 0;

    /**
     * Get the {@link AbstractEntry} for a given sequence from the
     * underlying {@link RingBuffer}.
     *
     * @param sequence of the {@link AbstractEntry} to get.
     * @return the {@link AbstractEntry} for the sequence.
     */
    virtual T& getEntry(long sequence) = 0;

    /**
     * Delegate a call to the {@link RingBuffer#getCursor()}
     *
     * @return value of the cursor for entries that have been published.
     */
    virtual long getCursor() = 0;
}; // ProducerBarrier

/**
 * Coordination barrier for tracking the cursor for producers and sequence of
 * dependent {@link Consumer}s for a {@link RingBuffer}
 *
 * @param <T> {@link AbstractEntry} implementation stored in
 * the {@link RingBuffer}
 */
template < typename T >
class ConsumerBarrier {
public:

    /**
     * Get the {@link AbstractEntry} for a given sequence from the
     * underlying {@link RingBuffer}.
     */
    virtual T& getEntry(const long sequence) = 0;

    /**
     * Wait for the given sequence to be available for consumption.
     *
     * @param sequence to wait for
     * @return the sequence up to which is available
     * @throws AlertException if a status change has occurred for
     *  the Disruptor
     * @throws InterruptedException if the thread needs awaking on
     * a condition variable.
     */
    virtual long waitFor(const long sequence) = 0;
    //throws AlertException, InterruptedException;

    /**
     * Wait for the given sequence to be available for consumption
     * with a time out.
     *
     * @param sequence to wait for
     * @param timeout value
     * @param units for the timeout value
     * @return the sequence up to which is available
     * @throws AlertException if a status change has occurred
     * for the Disruptor
     * @throws InterruptedException if the thread needs awaking
     *  on a condition variable.
     */
    virtual long waitFor(const long sequence,
    		const boost::posix_time::time_duration timeout) = 0;
    //throws AlertException, InterruptedException;

    /**
     * Delegate a call to the {@link RingBuffer#getCursor()}
     * @return value of the cursor for entries that have been published.
     */
    virtual long getCursor() = 0;

    /**
     * The current alert status for the barrier.
     *
     * @return true if in alert otherwise false.
     */
    virtual bool isAlerted() = 0;

    /**
     * Alert the consumers of a status change and stay in this status
     *  until cleared.
     */
    virtual void alert() = 0;

    /**
     * Clear the current alert status.
     */
    virtual void clearAlert() = 0;

}; // ConsumerBarrier

/**
 * Abstraction for claiming {@link AbstractEntry}s in a {@link RingBuffer} while tracking dependent {@link Consumer}s.
 *
 * This barrier can be used to pre-fill a {@link RingBuffer} but only when no other producers are active.
 *
 * @param <T> {@link AbstractEntry} implementation stored in the {@link RingBuffer}
 */
template <typename T>
class ForceFillProducerBarrier {
public:
    /**
     * Claim a specific sequence in the {@link RingBuffer} when only one producer is involved.
     *
     * @param sequence to be claimed.
     * @return the claimed {@link AbstractEntry}
     */
    T claimEntry(long sequence);

    /**
     * Commit an entry back to the {@link RingBuffer} to make it visible to {@link Consumer}s.
     * Only use this method when forcing a sequence and you are sure only one producer exists.
     * This will cause the {@link RingBuffer} to advance the {@link RingBuffer#getCursor()} to this sequence.
     *
     * @param entry to be committed back to the {@link RingBuffer}
     */
    void commit(T entry);

    /**
     * Delegate a call to the {@link RingBuffer#getCursor()}
     *
     * @return value of the cursor for entries that have been published.
     */
    long getCursor();
};// ForceFillProducerBarrier

/**
 * Callback interface to be implemented for processing {@link AbstractEntry}s
 * as they become available in the {@link RingBuffer}
 *
 * @see BatchConsumer#setExceptionHandler(ExceptionHandler) if you want to
 * handle exceptions propagated out of the handler.
 *
 * @param <T> AbstractEntry implementation storing the data for sharing
 * during exchange or parallel coordination of an event.
 */
template <typename T>
class BatchHandler {
public:

    /**
     * Called when a publisher has committed an {@link AbstractEntry} to
     * the {@link RingBuffer}
     *
     * @param entry committed to the {@link RingBuffer}
     * @throws Exception if the BatchHandler would like the exception handled
     * further up the chain.
     */
    virtual void onAvailable(const T& entry) = 0; //throws Exception;

    /**
     * Called after each batch of items has been have been processed before
     * the next waitFor call on a {@link ConsumerBarrier}.
     * <p>
     * This can be taken as a hint to do flush type operations before waiting
     * once again on the {@link ConsumerBarrier}.  The user should not expect
     * any pattern or frequency to the batch size.
     *
     * @throws Exception if the BatchHandler would like the exception handled
     * further up the chain.
     */
    virtual void onEndOfBatch() = 0;// throws Exception;
};

template <typename T> class SequenceTrackerCallback; // fwd

/**
 * Used by the {@link BatchConsumer} to set a callback allowing the {@link BatchHandler} to notify
 * when it has finished consuming an {@link AbstractEntry} if this happens after the {@link BatchHandler#onAvailable(AbstractEntry)} call.
 * <p>
 * Typically this would be used when the handler is performing some sort of batching operation such are writing to an IO device.
 * </p>
 * @param <T> AbstractEntry implementation storing the data for sharing during exchange or parallel coordination of an event.
 */
template <typename T>
class SequenceTrackingHandler : public BatchHandler<T>
{
    /**
     * Call by the {@link BatchConsumer} to setup the callback.
     *
     * @param sequenceTrackerCallback callback on which to notify the {@link BatchConsumer} that the sequence has progressed.
     */
    void setSequenceTrackerCallback(const SequenceTrackerCallback<T> sequenceTrackerCallback);
};




/**
 * Callback handler for uncaught exceptions in the {@link AbstractEntry}
 * processing cycle of the {@link BatchConsumer}
 */
class ExceptionHandler {
public:

	/**
     * Strategy for handling uncaught exceptions when processing
     * an {@link AbstractEntry}.
     *
     * If the strategy wishes to suspend further processing by the
     * {@link BatchConsumer}
     * then is should throw a {@link RuntimeException}.
     *
     * @param ex the exception that propagated from the {@link BatchHandler}
     * @param currentEntry being processed when the exception occurred.
     */
	virtual void handle(const void * ex, const AbstractEntry& currentEntry) = 0;

};

/**
 * Convenience implementation of an exception handler that using standard
 *  JDK logging to log
 * the exception as {@link Level}.SEVERE and re-throw it wrapped in a
 * {@link RuntimeException}
 */
class FatalExceptionHandler : public ExceptionHandler {

public:

    virtual void handle(const void * ex, const AbstractEntry& currentEntry) {
        //logger.log(Level.SEVERE, "Exception processing: " + currentEntry, ex);

    	std::cerr << "Exception processing "
    			<< currentEntry << std::endl;
//todo - exceptions
//        throw new RuntimeException(ex);
    }
};

class ClaimStrategy; // fwd
class ClaimStrategyOption; //fwd

template <class T> class RingBuffer; // fwd

/**
 * Strategy employed for making {@link Consumer}s wait on a {@link RingBuffer}.
 */
template <typename T>
class WaitStrategy {
public:
    /**
     * Wait for the given sequence to be available for consumption
     * in a {@link RingBuffer}
     *
     * @param consumers further back the chain that must advance first
     * @param ringBuffer on which to wait.
     * @param barrier the consumer is waiting on.
     * @param sequence to be waited on.
     * @return the sequence that is available which may be greater than
     * the requested sequence.
     * @throws AlertException if the status of the Disruptor has changed.
     * @throws InterruptedException if the thread is interrupted.
     */
	virtual long waitFor(std::vector<Consumer*>& consumers, RingBuffer<T>* ringBuffer,
    		ConsumerBarrier<T>* barrier,  long sequence) = 0;
//	{
//		std::cout << "WS: shouldn't be called!" << std::endl;
//	};
//        throws AlertException, InterruptedException;

    /**
     * Wait for the given sequence to be available for consumption in a
     *  {@link RingBuffer} with a timeout specified.
     *
     * @param consumers further back the chain that must advance first
     * @param ringBuffer on which to wait.
     * @param barrier the consumer is waiting on.
     * @param sequence to be waited on.
     * @param timeout value to abort after.
     * @return the sequence that is available which may be greater than the
     *  requested sequence.
     * @throws AlertException if the status of the Disruptor has changed.
     * @throws InterruptedException if the thread is interrupted.
     */
    virtual long waitFor(std::vector<Consumer*>& consumers,
    		RingBuffer<T>* ringBuffer, ConsumerBarrier<T>* barrier, long sequence,
    		boost::posix_time::time_duration timeout) = 0;
//    {
//		std::cout << "WS-to: shouldn't be called!" << std::endl;
//	};
        //throws AlertException, InterruptedException;

    /**
     * Signal those waiting that the {@link RingBuffer} cursor has advanced.
     */
    virtual void signalAll() = 0;
}; // WaitStrategy


/**
 * Yielding strategy that uses a Thread.yield() for {@link Consumer}s waiting
 * on a barrier.
 *
 * This strategy is a good compromise between performance and CPU resource.
 */
template <typename T>
class YieldingWait: public WaitStrategy<T> {
public:

	long waitFor(std::vector<Consumer*>& consumers, RingBuffer<T>* ringBuffer,
    		ConsumerBarrier<T>* barrier,  long sequence) {
		long availableSequence;

		if (0 == consumers.size()) {
//			std::cout << "YW: no consumers" << std::endl;
			while ((availableSequence = ringBuffer->getCursor()) < sequence) {
				if (barrier->isAlerted()) {
					throw AlertException::Alert();
					std::cerr << "YW ALERT ... " << std::endl;
					break;
				}
				//Thread.yield();
				boost::this_thread::yield();
			}
		} else {
//			std::cout << "YW: consumers: " << consumers.size() << std::endl;
			while ((availableSequence = getMinimumSequence(consumers))
					< sequence) {
				if (barrier->isAlerted()) {
					//throw ALERT_EXCEPTION;
					throw AlertException::Alert();
					std::cerr << "YW ALERT ... " << std::endl;
				}
				boost::this_thread::yield();
				//				Thread.yield();
			}
		}

		return availableSequence;
	}

	long waitFor(std::vector<Consumer*>& consumers, RingBuffer<T>* ringBuffer,
			ConsumerBarrier<T>* barrier, long sequence,
			boost::posix_time::time_duration timeout) {

        boost::posix_time::ptime start =
        		boost::posix_time::microsec_clock::universal_time();
		long availableSequence;

		if (0 == consumers.size()) {
			while ((availableSequence = ringBuffer->getCursor()) < sequence) {
				if (barrier->isAlerted()) {
					throw AlertException::Alert();
				}

				boost::this_thread::yield();
				//				Thread.yield();
				boost::posix_time::ptime now();
		        boost::posix_time::time_period per(start,
		        		boost::posix_time::microsec_clock::universal_time());
				if (timeout < per.length()) {
					break;
				}
			}
		} else {
			while ((availableSequence = getMinimumSequence(consumers))
					< sequence) {
				if (barrier->isAlerted()) {
					//					throw ALERT_EXCEPTION;
					std::cerr << "YW ALERT ... " << std::endl;
				}
				boost::this_thread::yield();
				//			Thread.yield();
		        boost::posix_time::time_period per(start,
		        		boost::posix_time::microsec_clock::universal_time());
				if (timeout < per.length()) {
					break;
				}
			}
		}

		return availableSequence;
	}

	void signalAll() {	}

}; // YieldingStrategy

    /**
 * Busy Spin strategy that uses a busy spin loop for {@link Consumer}s
 * waiting on a barrier.
 *
 * This strategy will use CPU resource to avoid syscalls which can introduce
 *  latency jitter.  It is best
 * used when threads can be bound to specific CPU cores.
 */
template <typename T>
class BusySpinWait: public WaitStrategy<T> {

public:

	long waitFor(std::vector<Consumer*>& consumers, RingBuffer<T>* ringBuffer,
			ConsumerBarrier<T>* barrier, long sequence) {
		long availableSequence;

		if (0 == consumers.size())
		{
			while ((availableSequence = ringBuffer.getCursor()) < sequence)
			{
				if (barrier.isAlerted())
				{
//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;

				}
			}
		}
		else
		{
			while ((availableSequence =
					getMinimumSequence(consumers)) < sequence)
			{
				if (barrier.isAlerted())
				{
//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;
				}
			}
		}

		return availableSequence;
	}

	long waitFor(std::vector<Consumer*>& consumers,
			RingBuffer<T>* ringBuffer, ConsumerBarrier<T>* barrier, long sequence,
			boost::posix_time::time_duration timeout) {
		const boost::posix_time::ptime currentTime();//System.currentTimeMillis();
		long availableSequence;

		if (0 == consumers.size())
		{
			while ((availableSequence = ringBuffer.getCursor()) < sequence)
			{
				if (barrier.isAlerted())
				{
//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;

				}

				boost::posix_time::ptime now();
				boost::posix_time::time_period per(currentTime, now);
				if (timeout < per.length()) {
					break;
				}
			}
		}
		else
		{
			while ((availableSequence = getMinimumSequence(consumers)) < sequence)
			{
				if (barrier.isAlerted())
				{
//					throw ALERT_EXCEPTION;
					std::cerr << "ALERT ... " << std::endl;
				}

				boost::posix_time::ptime now();
				boost::posix_time::time_period per(currentTime, now);
				if (timeout < per.length()) {
					break;
				}
			}
		}

		return availableSequence;
	}

	void signalAll() {	}
}; // BusySpinStrategy

/**
 * Strategies employed for claiming the sequence of {@link AbstractEntry}s in
 * the {@link RingBuffer} by producers.
 *
 * The {@link AbstractEntry} index is a the sequence value mod the
 * {@link RingBuffer} capacity.
 */
class ClaimStrategy  {
public:

	/**
     * Claim the next sequence index in the {@link RingBuffer} and increment.
     *
     * @return the {@link AbstractEntry} index to be used for the producer.
     */
    virtual long incrementAndGet() = 0;

    /**
     * Increment by a delta and get the result.
     *
     * @param delta to increment by.
     * @return the result after incrementing.
     */
    virtual long incrementAndGet(int delta) = 0;

    /**
     * Set the current sequence value for claiming {@link AbstractEntry} in
     * the {@link RingBuffer}
     *
     * @param sequence to be set as the current value.
     */
    virtual void setSequence(long sequence) = 0;

}; // ClaimStrategy

/**
 * ConsumerBarrier handed out for gating consumers of the RingBuffer and
 * dependent {@link Consumer}(s)
 */
template<typename T>
class ConsumerTrackingConsumerBarrier: public ConsumerBarrier<T> {
private:
	tbb::atomic<bool> _alerted;
	RingBuffer<T>* _ring;
  	std::vector<Consumer*>& _consumers;
public:

	ConsumerTrackingConsumerBarrier(RingBuffer<T>* ring,
			std::vector<Consumer*>& consumers)
	: _consumers(consumers), _ring(ring) {
		_alerted = false;
	}

	virtual T& getEntry(const long sequence) {
//		std::cout << "CTCB.getEntry :" << sequence << std::endl;
		return _ring->_entries[(int) sequence & _ring->_ringModMask];
	}

	virtual long waitFor(const long sequence)
	//    throws AlertException, InterruptedException
	{
		return	_ring->_waitStrategy->waitFor
				(_consumers, _ring, this, sequence);
	}

	virtual long waitFor(const long sequence, const boost::posix_time::time_duration timeout)
	//  throws AlertException, InterruptedException
	{
		return _ring->_waitStrategy->waitFor
				(_consumers, _ring, this, sequence, timeout);
	}

	virtual long getCursor() { return _ring->_cursor; }

	virtual bool isAlerted() {return _alerted;}

	virtual void alert() {
		_alerted = true; //TODO!!
		_ring->_waitStrategy->signalAll();
	}

	virtual void clearAlert() {_alerted = false;}
}; // ConsumerTrackingConsumerBarrier

/**
 * {@link ProducerBarrier} that tracks multiple {@link Consumer}s when trying
 *  to claim
 * an {@link AbstractEntry} in the {@link RingBuffer}.
 */
template <typename T>
class ConsumerTrackingProducerBarrier : public ProducerBarrier<T> {
private :
	std::vector<Consumer*>& _consumers;
	long _lastConsumerMinimum;
	RingBuffer<T>* _ring;

public:

	ConsumerTrackingProducerBarrier(RingBuffer<T>* ring,
			std::vector<Consumer*>& consumers )
	: _consumers(consumers), _lastConsumerMinimum(INITIAL_CURSOR_VALUE),
	  _ring(ring) {

		if (0 == _consumers.size())
		{
			//throw new IllegalArgumentException("There must be at least one Consumer to track for preventing ring wrap");
			std::cerr << "CTPB There must be at least one Consumer to track for "
					"preventing ring wrap" << std::endl;
		}
	}

	T& nextEntry() {
		const long sequence = _ring->_claimStrategy->incrementAndGet();
		ensureConsumersAreInRange(sequence);

		T& entry = _ring->entries()[(int)sequence & _ring->ringModMask()];
		entry.setSequence(sequence);
//		std::cout << "CTPB: nextEntry: " << sequence << std::endl;
		return entry;
	}

	virtual void commit(T& entry)	{
		commit(entry.getSequence(), 1);
	}

	SequenceBatch* nextEntries(SequenceBatch* sequenceBatch) {
		const long sequence = _ring->_claimStrategy->
				incrementAndGet(sequenceBatch->getSize());
		sequenceBatch->setEnd(sequence);
		ensureConsumersAreInRange(sequence);

		for (long i = sequenceBatch->getStart(), _end = sequenceBatch->getEnd(); i <= _end; i++)
		{
			T entry = _ring->_entries[(int)i & _ring->ringModMask()];
			entry.setSequence(i);
		}

		return sequenceBatch;
	}

	void commit(SequenceBatch* sequenceBatch)
	{
		commit(sequenceBatch->getEnd(), sequenceBatch->getSize());
	}

	T& getEntry(const long sequence)	{
		return _ring->_entries[(int) sequence & _ring->ringModMask()];
	}

	long getCursor() { return _ring->getCursor();	}

	void ensureConsumersAreInRange(const long sequence)
	{
		const long wrapPoint = sequence - _ring->entries().size();
		while (wrapPoint > _lastConsumerMinimum &&
				wrapPoint > (_lastConsumerMinimum = getMinimumSequence(_consumers)))
		{
			boost::thread::yield();
		}
	}

private:

	void commit(const long sequence, const long batchSize)
	{
		// TODO:
//		if (ClaimStrategy.Option.MULTI_THREADED == claimStrategyOption )
//		{
//			const long expectedSequence = sequence - batchSize;
//			while (expectedSequence != _ring._cursor)
//			{
//				// busy spin
//			}
//		}
//		std::cout << "CTPB: commit: " << sequence << " - " << batchSize << std::endl;

		_ring->_cursor = sequence;

//		std::cout << "CTPB: cursor : " << _ring->_cursor << std::endl;

		_ring->_waitStrategy->signalAll();
	}
};

/**
 * {@link ForceFillProducerBarrier} that tracks multiple {@link Consumer}s when trying to claim
 * a {@link AbstractEntry} in the {@link RingBuffer}.
 */
template <typename T>
class ForceFillConsumerTrackingProducerBarrier
	: public ForceFillProducerBarrier<T>
{
private:
		const std::vector<Consumer*>& _consumers;
		long _lastConsumerMinimum;
		const RingBuffer<T>& _ring;

public:
		ForceFillConsumerTrackingProducerBarrier
			(const std::vector<Consumer*>& consumers, const RingBuffer<T>& ring)
		: _consumers(consumers), _lastConsumerMinimum(INITIAL_CURSOR_VALUE),
		  _ring(ring) {
		if (0 == _consumers.size())
		{
//			throw new IllegalArgumentException("There must be at least one Consumer to track for preventing ring wrap");
			std::cerr << "FFCTPB There must be at least one Consumer to track for "
					"preventing ring wrap" << std::endl;

		}
	}

	T claimEntry(const long sequence)	{
		ensureConsumersAreInRange(sequence);

		T entry = _ring._entries[(int)sequence & _ring._ringModMask];
		entry.setSequence(sequence);

		return entry;
	}

	void commit(const T entry) {
		long sequence = entry.getSequence();
		_ring._claimStrategy.setSequence(sequence);
		_ring._cursor = sequence;
		_ring._waitStrategy.signalAll();
	}

	long getCursor() { return _ring.cursor;	}

private :

	void ensureConsumersAreInRange(const long sequence)	{
		const long wrapPoint = sequence - _ring._entries.size();
		while (wrapPoint > _lastConsumerMinimum &&
				wrapPoint > (_lastConsumerMinimum = getMinimumSequence(_consumers)))
		{
			boost::this_thread::yield();
//			Thread.yield();
		}
	}
}; // ForceFillConsumerTracingProducerBarrier


template <typename T>
class RingBuffer {
private:
	long p1, p2, p3, p4, p5, p6, p7; // cache line padding
	tbb::atomic<long>			_cursor;  // TODO: atomic_fence?
	long p8, p9, p10, p11, p12, p13, p14; // cache line padding
	const int 				_ringModMask;
	std::vector<T> 			_entries;
	ClaimStrategy* 			_claimStrategy;
	WaitStrategy<T>* 			_waitStrategy;

	friend class ConsumerTrackingConsumerBarrier<T>;
	friend class ConsumerTrackingProducerBarrier<T>;

public:

	/**
	 * Construct a RingBuffer with the full option set.
	 *
	 * @param entryFactory to create {@link AbstractEntry}s for filling the RingBuffer
	 * @param size of the RingBuffer that will be rounded up to the next power of 2
	 * @param claimStrategyOption threading strategy for producers claiming {@link AbstractEntry}s in the ring.
	 * @param waitStrategyOption waiting strategy employed by consumers waiting on {@link AbstractEntry}s becoming available.
	 */
	RingBuffer(const int size,
			ClaimStrategy* claimStrategy ,//= new MultiThreadedStrategy(),
			WaitStrategy<T>* waitStrategy = new YieldingWait<T>() )
		: //_cursor(INITIAL_CURSOR_VALUE) ,
		  _ringModMask(ceilingNextPowerOfTwo(size)-1),
		  _entries(_ringModMask+1),
		  _claimStrategy(claimStrategy),
		  _waitStrategy(waitStrategy)
	{
		_cursor = INITIAL_CURSOR_VALUE;
//		fill(entryFactory, c);
//		for (int i = 0; i < _entries.size(); i++) {
//			_entries[i] = entryFactory.create();
//		}

	}

    /**
     * Construct a RingBuffer with default strategies of:
     * {@link ClaimStrategy.Option#MULTI_THREADED} and {@link WaitStrategy.Option#BLOCKING}
     *
     * @param entryFactory to create {@link AbstractEntry}s for filling the RingBuffer
     * @param size of the RingBuffer that will be rounded up to the next power of 2
     */
//    RingBuffer(final EntryFactory<T> entryFactory, final int size) {
//        this(entryFactory, size,
//             ClaimStrategy.Option.MULTI_THREADED,
//             WaitStrategy.Option.BLOCKING);
//    }

	int ringModMask() { return _ringModMask; }

	std::vector<T>& entries() { return _entries; }


	/**
	 * Create a {@link ConsumerBarrier} that gates on the RingBuffer and a list of
	 * {@link Consumer}s
	 *
	 * @param consumersToTrack this barrier will track
	 * @return the barrier gated as required
	 */
	ConsumerBarrier<T>*
	createConsumerBarrier(std::vector<Consumer*>& consumersToTrack)  {
		return new ConsumerTrackingConsumerBarrier<T>(this, consumersToTrack);
	}

	/**
	 * Create a {@link ProducerBarrier} on this RingBuffer that tracks dependent
	 *  {@link Consumer}s.
	 *
	 * @param consumersToTrack to be tracked to prevent wrapping.
	 * @return a {@link ProducerBarrier} with the above configuration.
	 */
	 ProducerBarrier<T>* createProducerBarrier
	 	 (std::vector<Consumer*>& consumersToTrack ) const
	 {
		return new ConsumerTrackingProducerBarrier<T>(const_cast<RingBuffer<T>* >(this), consumersToTrack);
	 }
	 ProducerBarrier<T>* createProducerBarrier
	 	 (Consumer* consumer ) const
	 {
		std::vector<Consumer*>* vec = new std::vector<Consumer*> ();
		vec->push_back(consumer);
		return new ConsumerTrackingProducerBarrier<T>(const_cast<RingBuffer<T>* >(this), *vec);
	 }

	/**
	 * Create a {@link ForceFillProducerBarrier} on this RingBuffer that tracks
	 * dependent {@link Consumer}s.  This barrier is to be used for filling a
	 * RingBuffer when no other producers exist.
	 *
	 * @param consumersToTrack to be tracked to prevent wrapping.
	 * @return a {@link ForceFillProducerBarrier} with the above configuration.
	 */
	ForceFillProducerBarrier<T> createForceFillProducerBarrier(const std::vector<Consumer>& consumersToTrack)
	{
		return new ForceFillConsumerTrackingProducerBarrier<T>(consumersToTrack);
	}

	/**
	 * The capacity of the RingBuffer to hold entries.
	 *
	 * @return the size of the RingBuffer.
	 */
	int getCapacity() { return _entries.size(); }

	/**
	 * Get the current sequence that producers have committed to the RingBuffer.
	 *
	 * @return the current committed sequence.
	 */
	long getCursor() { return _cursor; }

	/**
	 * Get the {@link AbstractEntry} for a given sequence in the RingBuffer.
	 *
	 * @param sequence for the {@link AbstractEntry}
	 * @return {@link AbstractEntry} for the sequence
	 */
	T getEntry(const long sequence) {
		return _entries[(int) sequence & _ringModMask];
	}

};// RingBuffer

/**
 * Strategy to be used when there are multiple producer threads
 * claiming {@link AbstractEntry}s.
 */
class MultiThreadedStrategy : public ClaimStrategy  {
private:

    tbb::atomic<long> _sequence;

public:

    MultiThreadedStrategy() {
    	_sequence = INITIAL_CURSOR_VALUE ;
    }

    virtual long incrementAndGet() { return ++_sequence; }

    virtual long incrementAndGet(const int delta) {
        _sequence+=delta;
        return _sequence;
    }

    virtual void setSequence(const long seq) { _sequence = seq; }

}; // MultiThreadedStrategy

/**
 * Optimised strategy can be used when there is a single producer thread
 *  claiming {@link AbstractEntry}s.
 */
class SingleThreadedStrategy : public ClaimStrategy {
private:

	long _sequence;

public:
	SingleThreadedStrategy() : _sequence(INITIAL_CURSOR_VALUE) {}

	virtual long incrementAndGet() { return ++_sequence; }

	virtual long incrementAndGet(const int delta) {
        _sequence += delta;
        return _sequence;
    }

    virtual void setSequence(const long seq) { _sequence = seq; }
}; // SingleThreadedStrategy

///** Makes the {@link RingBuffer} thread safe for claiming
// * {@link AbstractEntry}s by multiple producing threads. */
//struct MultiThreaded : public ClaimStrategyOption {
//	virtual ClaimStrategy* newInstance() const {
//		return new MultiThreadedStrategy();
//     }
//}; // MultiThreaded
//
///** Optimised {@link RingBuffer} for use by single thread
// * claiming {@link AbstractEntry}s as a producer. */
//struct SingleThreaded : public ClaimStrategyOption {
//	virtual ClaimStrategy* newInstance() const {
//        return new SingleThreadedStrategy();
//     }
//}; // SingleThreaded

/**
 * No operation version of a {@link Consumer} that simply tracks a {@link RingBuffer}.
 * This is useful in tests or for pre-filling a {@link RingBuffer} from a producer.
 */
template <class T>
class NoOpConsumer : public Consumer {
private:
	const RingBuffer<T>& _ringBuffer;

public:
    /**
     * Construct a {@link Consumer} that simply tracks a {@link RingBuffer}.
     *
     * @param ringBuffer to track.
     */
	NoOpConsumer(const RingBuffer<T>& ringBuffer)
    		: _ringBuffer(ringBuffer)  {  }


	virtual long getSequence() { return _ringBuffer.getCursor();  }

    virtual void halt() {  }

	virtual void run()  {  }
}; // NoOpConsumer



/**
 * Implement this interface to be notified when a thread for the {@link BatchConsumer} starts and shuts down.
 */
class LifecycleAware {
public:
	/**
     * Called once on thread start before first entry is available.
     */
    virtual void onStart() = 0;

    /**
     * Called once just before the thread is shutdown.
     */
    virtual void onShutdown() = 0;
}; // lifecycleaware




//////////////////////// batch consumer

/**
 * Convenience class for handling the batching semantics of consuming entries from a {@link RingBuffer}
 * and delegating the available {@link AbstractEntry}s to a {@link BatchHandler}.
 *
 * If the {@link BatchHandler} also implements {@link LifecycleAware} it will be notified just after the thread
 * is started and just before the thread is shutdown.
 *
 * @param <T> Entry implementation storing the data for sharing during exchange or parallel coordination of an event.
 */
template <typename T>
class BatchConsumer : public Consumer
{
private:

	//friend class SequenceTrackerCallback<T>;

	ConsumerBarrier<T>* _consumerBarrier;
    BatchHandler<T>* 	_handler;
    ExceptionHandler* _exceptionHandler ;//= new FatalExceptionHandler();

    long p1, p2, p3, p4, p5, p6, p7;  // cache line padding
    tbb::atomic<bool> _running;
    long p8, p9, p10, p11, p12, p13, p14; // cache line padding
    tbb::atomic<long> _sequence;// = RingBuffer.INITIAL_CURSOR_VALUE;
	long p15, p16, p17, p18, p19, p20; // cache line padding

public:
    /**
     * Construct a batch consumer that will automatically track the progress by updating its sequence when
     * the {@link BatchHandler#onAvailable(AbstractEntry)} method returns.
     *
     * @param consumerBarrier on which it is waiting.
     * @param handler is the delegate to which {@link AbstractEntry}s are dispatched.
     */
    BatchConsumer(ConsumerBarrier<T>* consumerBarrier,
                     BatchHandler<T>* handler)
    : _consumerBarrier(consumerBarrier), _handler(handler),
      _exceptionHandler(new FatalExceptionHandler())
    {
    	_sequence = INITIAL_CURSOR_VALUE;
    	_running = true;
    }

    /**
     * Construct a batch consumer that will rely on the {@link SequenceTrackingHandler}
     * to callback via the {@link BatchConsumer.SequenceTrackerCallback} when it has
     * completed with a sequence within a batch.  Sequence will be updated at the end of
     * a batch regardless.
     *
     * @param consumerBarrier on which it is waiting.
     * @param entryHandler is the delegate to which {@link AbstractEntry}s are dispatched.
     */
//    public BatchConsumer(final ConsumerBarrier<T> consumerBarrier,
//                         final SequenceTrackingHandler<T> entryHandler)
//    {
//        this.consumerBarrier = consumerBarrier;
//        this.handler = entryHandler;
//
//        entryHandler.setSequenceTrackerCallback(new SequenceTrackerCallback());
//    }

	virtual long getSequence() const { return _sequence; }

	virtual void halt()
    {
		//std::cout << "Halt!" << std::endl;
        _running = false;
        _consumerBarrier->alert();
    }

    /**
     * Set a new {@link ExceptionHandler} for handling exceptions propagated out of the {@link BatchConsumer}
     *
     * @param exceptionHandler to replace the existing exceptionHandler.
     */
	void setExceptionHandler(const ExceptionHandler* exceptionHandler)
    {
        if (!exceptionHandler)
        {
        	//throw new NullPointerException();
        	std::cerr << "BatchHandler.setExceptionHandler: NULL!" << std::endl;
        }
        if (!_exceptionHandler) delete _exceptionHandler;
        _exceptionHandler = exceptionHandler;
    }

    /**
     * Get the {@link ConsumerBarrier} the {@link Consumer} is waiting on.
     *
      * @return the barrier this {@link Consumer} is using.
     */
    ConsumerBarrier<T>& getConsumerBarrier() const
    	{ return _consumerBarrier; }

    /**
     * It is ok to have another thread rerun this method after a halt().
     */
 //  virtual void operator()() {
    virtual void run()
    {
        _running = true;
   //     std::cout << "BatchConsumer: running " << std::endl;
//        if (LifecycleAware.class.isAssignableFrom(handler.getClass()))
//        {
//            ((LifecycleAware)handler).onStart();
//        }

        T entry ;
        long nextSequence = ++_sequence ;
        while (_running)
        {
//            try
//            {
                const long availableSequence = _consumerBarrier->waitFor(nextSequence);
//                std::cout << "available seq: " << availableSequence << std::endl;
//                std::cout << "next seq: " << nextSequence << std::endl;

                for (; nextSequence <= availableSequence; nextSequence++)
                {
                    entry = _consumerBarrier->getEntry(nextSequence);
                    if (nextSequence % 1000 == 0) {
						std::cout << "batchconsumer got "
								<< entry.getSequence() << " for: "
								<< nextSequence << std::endl;
                    }
                    _handler->onAvailable(entry);
                }
//std::cout << "batchconsumer: still running... " << std::endl;

                _handler->onEndOfBatch();
                _sequence = entry.getSequence();
				if (_consumerBarrier->isAlerted()) {
					//exceptionHandler.handle(ex, entry);
					_sequence = entry.getSequence();
					nextSequence = entry.getSequence() + 1;
std::cout << "batchconsumer: handling alert... " << std::endl;

				}

                //            }
//            catch (final AlertException ex)
//            {
//                // Wake up from blocking wait and check if we should continue to run
//            }
//            catch (final Exception ex)
//            {
//                exceptionHandler.handle(ex, entry);
//                sequence = entry.getSequence();
//                nextSequence = entry.getSequence() + 1;
//            }
        }
 //       std::cout << "BatchConsumer: done " << std::endl;

//        if (LifecycleAware.class.isAssignableFrom(handler.getClass()))
//        {
//            ((LifecycleAware)handler).onShutdown();
//        }
    }

}; // batchconsumer
/**
 * Used by the {@link BatchHandler} to signal when it has completed consuming a given sequence.
 */
template <typename T>
class SequenceTrackerCallback
{
private:
	BatchConsumer<T>& _batchConsumer;

public:

	SequenceTrackerCallback(BatchConsumer<T>& batchConsumer)
	: _batchConsumer(batchConsumer) {}

    /**
     * Notify that the handler has consumed up to a given sequence.
     *
     * @param sequence that has been consumed.
     */
    void onCompleted(const long sequence)  {
    	_batchConsumer._sequence = sequence;
    }
}; // SequenceTrackerCallback

//////////////////////// batch consumer

}; // namespace disruptor


#endif /* DISRUPTOR_HPP_ */
