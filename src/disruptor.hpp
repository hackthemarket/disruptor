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

#include "event_processor.hpp"
#include "wait_strats.hpp"
#include "sequence.hpp"

namespace disruptor {



int numberOfLeadingZeros(int i);

inline int ceilingNextPowerOfTwo(const int x) {
     return 1 << (32 - numberOfLeadingZeros(x - 1));
}

/**
 * Callback interface to be implemented for processing events as they become
 * available in the {@link RingBuffer}
 */
template <typename event>
class EventHandler {
public:
    /**
     * Called when a publisher has published an event to the {@link RingBuffer}
     */
    virtual void onEvent(event& ev, long sequence, bool endOfBatch) = 0;
};// EventHandler

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




/**
 * Callback handler for uncaught exceptions in the {@link AbstractEntry}
 * processing cycle of the {@link BatchConsumer}
 */
template <typename event>
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
	virtual void handle(const void * ex, long sequence, event e) = 0;

};

/**
 * Convenience implementation of an exception handler that using standard
 *  JDK logging to log
 * the exception as {@link Level}.SEVERE and re-throw it wrapped in a
 * {@link RuntimeException}
 */
template <typename event>
class FatalExceptionHandler : public ExceptionHandler<event> {

public:

    virtual void handle(const void * ex, long sequence, event e) {
        //logger.log(Level.SEVERE, "Exception processing: " + currentEntry, ex);

    	std::cerr << "Exception processing "
    			<< sequence << std::endl;
//todo - exceptions
//        throw new RuntimeException(ex);
    }
};

template <typename event>
class RingBuffer : public Sequencer<event> {
private:
	const int 					_mask;
	std::vector<event> 			_entries;

public:
	static const long DEF_BUF_SZ = 32768;

	/**
	 * Construct a RingBuffer with the full option set.
	 */
	RingBuffer(int size = DEF_BUF_SZ ,
			ClaimStrategy* claimStrategy = NULL,
			WaitStrategy<event>* waitStrategy = new WaitStrategy<event>())
		: Sequencer<event>(size, claimStrategy, waitStrategy),
		  _mask(size-1), _entries(size)
	{ }

	/**
	 * Get the event for a given sequence in the RingBuffer.
	 */
	event& get(long sequence) {
		return (event&) _entries[(int) sequence & _mask];
	}

};// RingBuffer

/**
 * No operation version of a {@link Consumer} that simply tracks a {@link RingBuffer}.
 * This is useful in tests or for pre-filling a {@link RingBuffer} from a producer.
 */
template <class T>
class NoOpConsumer : public EventProcessor {
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
 * Convenience class for handling the batching semantics of consuming entries
 * from a {@link RingBuffer} and delegating the available {@link AbstractEntry}s
 *  to a {@link BatchHandler}.
 *
 * @param <T> Entry implementation storing the data for sharing during
 * exchange or parallel coordination of an event.
 */
template <typename event>
class BatchEventProcessor : public EventProcessor {
private:

    tbb::atomic<bool> 				_running;
    ExceptionHandler<event>* 		_exceptionHandler ;
    RingBuffer<event>* 				_ring;
    SequenceBarrier<event>* 		_sequenceBarrier;
    EventHandler<event>* 			_handler;
    Sequence 						_sequence;

public:
    /**
     * Construct a {@link EventProcessor} that will automatically track the
     * progress by updating its sequence when the
     * 	{@link EventHandler#onEvent(Object, long, boolean)} method returns.
     */
    BatchEventProcessor(RingBuffer<event>* ring,
    	SequenceBarrier<event>* sequenceBarrier, EventHandler<event>* handler)
    : _ring(ring), _sequenceBarrier(sequenceBarrier), _handler(handler),
      _exceptionHandler(new FatalExceptionHandler<event>()) {

    	_running = true;
    }
    /**
     *  Construct a batch event processor that will allow a
     *  {@link SequenceReportingEventHandler} to callback and update its
     *  sequence within a batch.  The Sequence will be updated at the end of
     * 	a batch regardless.
     */
//    BatchEventProcessor(RingBuffer<event>* ring,
//    	SequenceBarrier<event>* sequenceBarrier,
//    	SequenceReportingEventHandler<event>* handler)
//    : _ring(ring), _sequenceBarrier(sequenceBarrier), _handler(handler),
//      _exceptionHandler(new FatalExceptionHandler<event>()) {
//
//        handler.setSequenceCallback(_sequence);
//    	_running = true;
//    }

    Sequence& getSequence() { return _sequence;  }

	void halt() {
        _running = false;
        _sequenceBarrier->alert();
    }

    /**
     * Set a new {@link ExceptionHandler} for handling exceptions propagated out of the {@link BatchConsumer}
     *
     * @param exceptionHandler to replace the existing exceptionHandler.
     */
	void setExceptionHandler(const ExceptionHandler<event>* exceptionHandler)
    {
        if (!exceptionHandler)
        {
        	throw std::exception("Null handler provided...");
        }
        if (!_exceptionHandler) delete _exceptionHandler;
        _exceptionHandler = exceptionHandler;
    }

    /**
     * Get the {@link ConsumerBarrier} the {@link Consumer} is waiting on.
     *
      * @return the barrier this {@link Consumer} is using.
     */
    SequenceBarrier<event>& getSequenceBarrier() const
    	{ return _sequenceBarrier; }

    /**
     * It is ok to have another thread rerun this method after a halt().
     */
 //  virtual void operator()() {
    virtual void run() {
        _sequenceBarrier->clearAlert();
        _running = true;

        event ev;
        long next = _sequence.get() + 1L;
        while (true) {
            try {
                long avail = _sequenceBarrier->waitFor(next);
                while (next <= avail) {
                    ev = _ring->get(next);
                    _handler->onEvent(ev, next, next == avail);
                    next++;
                }

                _sequence.set(next - 1L);
            } catch (AlertException ex) {
               if (!_running) { break; }
            }
            catch (const std::exception& ex)
            {
                _exceptionHandler->handle(&ex, next, ev);
                _sequence.set(next);
                next++;
            }
        }
    }

}; // batchconsumer

//////////////////////// batch consumer

}; // namespace disruptor


#endif /* DISRUPTOR_HPP_ */
