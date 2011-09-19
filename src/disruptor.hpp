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

    virtual void handle(const void * ex, long sequence, event e) {  // TODO
        //logger.log(Level.SEVERE, "Exception processing: " + currentEntry, ex);
    	std::cerr << "Exception processing "
    			<< sequence << std::endl;
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
			ClaimStrategy* claimStrategy = new SingleThreadedStrategy(),
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

   Sequence& getSequence() { return _sequence;  }

	void halt() {
        _running = false;
        _sequenceBarrier->alert();
    }

    /**
     * Set a new {@link ExceptionHandler} for handling exceptions propagated
     * out of the {@link BatchConsumer}
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

}; // namespace disruptor

#endif /* DISRUPTOR_HPP_ */
