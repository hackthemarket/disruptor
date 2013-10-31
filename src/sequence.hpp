/*
 * sequence.hpp
 *
 *  Created on: Sep 16, 2011
 *      Author: tingarg
 */

#ifndef SEQUENCE_HPP_
#define SEQUENCE_HPP_

namespace disruptor {

template <typename event> class BatchEventProcessor;
template <typename event> class BatchHandler;
template <typename S >class WaitStrategy;


typedef tbb::atomic<long> PaddedAtomicLong;// TODO: pad!
typedef long PaddedLong;

/**
 * Cache line padded sequence counter.
 *
 * Can be used across threads without worrying about false sharing if a
 * located adjacent to another counter in memory.
 */
class Sequence {
private:

	PaddedAtomicLong _value ;

public :

	/** Set to -1 as sequence starting point */
    static const long INITIAL_CURSOR_VALUE = -1L;

    /**
     * Default Constructor that uses an initial value of
     * {@link Sequencer#INITIAL_CURSOR_VALUE}.
     */
    Sequence() { _value = INITIAL_CURSOR_VALUE ;  }

    /**
     * Construct a sequence counter that can be tracked across threads.
     *
     * @param initialValue for the counter.
     */
    Sequence(long initialValue)  {
        set(initialValue);
    }

    long get() const { return _value; }

    void set(long value) { _value = value;  }
}; // Sequence


long getMinimumSequence(std::vector<Sequence*>& consumers) ;

/**
 * Used to record the batch of sequences claimed via a {@link Sequencer}.
 */
class SequenceBatch  {
private :
    const int 	_size;
    long 		_end;

public :

    /** Create a holder for tracking a batch of claimed sequences in a
     * {@link Sequencer} */
    SequenceBatch(int size) : _size(size), _end(Sequence::INITIAL_CURSOR_VALUE)
    { }

    /** Get the end sequence of a batch. */
    long getEnd() { return _end; }

    /**
     * Set the end of the batch sequence.  To be used by the {@link Sequencer}.
     */
    void setEnd(long end) { _end = end;  }

    /** Get the size of the batch. */
    int getSize() { return _size; }

    /** Get the starting sequence for a batch.*/
    long getStart() { return _end - (_size - 1L); }

};  // SequenceBatch


/**
 * Coordination barrier for tracking the cursor for producers and sequence of
 * dependent {@link Consumer}s for a {@link RingBuffer}
 *
 * @param <T> {@link AbstractEntry} implementation stored in
 * the {@link RingBuffer}
 */
//template < typename T >
//class SequenceBarrier {
//public:
//
//    /** Wait for the given sequence to be available for consumption. */
//    virtual long waitFor(long sequence) = 0;
//
//    /**
//     * Wait for the given sequence to be available for consumption
//     * with a time out.
//     */
//    virtual long waitFor
//    	(long sequence, boost::posix_time::time_duration timeout) = 0;
//
//    /** Delegate a call to the {@link RingBuffer#getCursor()}  */
//    virtual long getCursor() = 0;
//
//    /**
//     * The current alert status for the barrier.
//     *
//     * @return true if in alert otherwise false.
//     */
//    virtual bool isAlerted() = 0;
//
//    /**
//     * Alert the consumers of a status change and stay in this status
//     *  until cleared.
//     */
//    virtual void alert() = 0;
//
//    /** Clear the current alert status. */
//    virtual void clearAlert() = 0;
//
//}; // SequenceBarrier

/**
 * {@link SequenceBarrier} handed out for gating {@link EventProcessor}s on a
 * cursor sequence and optional dependent {@link EventProcessor}(s)
 */
template<typename event>
class SequenceBarrier {
private:
	WaitStrategy<event>* 		_waitStrategy;
	Sequence*					_cursor;
	std::vector<Sequence*>&		_dependents;
	tbb::atomic<bool> 			_alerted;

public:

	SequenceBarrier(WaitStrategy<event>*  waitStrategy,
			Sequence* cursor, std::vector<Sequence*>& dependents)
	: _waitStrategy(waitStrategy), _cursor(cursor), _dependents(dependents)
	{
		_alerted = false;
	}

	virtual long waitFor(long sequence)	{
		return	_waitStrategy->waitFor(_dependents, *_cursor, *this, sequence);
	}

	virtual long waitFor(long sequence,
			const boost::posix_time::time_duration timeout)	{
		return _waitStrategy->waitFor
				(_dependents, *_cursor, *this, sequence, timeout);
	}

	virtual long getCursor() { return _cursor->get(); }

	virtual bool isAlerted() {return _alerted;}

	virtual void alert() {
		_alerted = true;
		_waitStrategy->signalAll();
	}

	virtual void clearAlert() {_alerted = false;}
}; // ProcessingSequenceBarrier


/**
 * Strategies employed for claiming the sequence of {@link AbstractEntry}s in
 * the {@link RingBuffer} by producers.
 *
 * The {@link AbstractEntry} index is a the sequence value mod the
 * {@link RingBuffer} capacity.
 */
class ClaimStrategy  {
public:

    /** Is there available capacity in the buffer for the requested sequence. */
    virtual bool hasAvailableCapacity(std::vector<Sequence*>& dep_seqs) = 0;

	/**
     * Claim the next sequence index in the {@link RingBuffer} and increment.
     */
    virtual long incrementAndGet(std::vector<Sequence*>& dep_seqs) = 0;

    /** Increment by a delta and get the result. */
    virtual long incrementAndGet(int delta, std::vector<Sequence*>& dep_seqs) = 0;

    /**
     * Set the current sequence value for claiming {@link AbstractEntry} in
     * the {@link RingBuffer}
     */
    virtual void setSequence(long sequence, std::vector<Sequence*>& dep_seqs) = 0;

    /** Serialise publishing in sequence. */
    virtual void serialisePublishing
    	(long sequence, Sequence& cursor, long batchSize) = 0;

}; // ClaimStrategy


/**
 * Strategy to be used when there are multiple producer threads
 * claiming {@link AbstractEntry}s.
 */
//class MultiThreadedStrategy : public ClaimStrategy  {
//private:
//
//    tbb::atomic<long> _sequence;
//
//public:
//
//    MultiThreadedStrategy() {
//    	_sequence = Sequence::INITIAL_CURSOR_VALUE ;
//    }
//
//    virtual long incrementAndGet() { return ++_sequence; }
//
//    virtual long incrementAndGet(const int delta) {
//        _sequence+=delta;
//        return _sequence;
//    }
//
//    virtual void setSequence(const long seq) { _sequence = seq; }
//
//}; // MultiThreadedStrategy

/**
 * Optimised strategy can be used when there is a single producer thread
 *  claiming {@link AbstractEntry}s.
 */
class SingleThreadedStrategy : public ClaimStrategy {
private:
	static const int RETRIES = 100;
	int _bufsz;
	PaddedLong _sequence;
	PaddedLong _minGate;

public:
	// TODO:  defaulting correctly
	SingleThreadedStrategy(int bufsz = 32768)
	: _bufsz(bufsz), _sequence(Sequence::INITIAL_CURSOR_VALUE),
	  _minGate(Sequence::INITIAL_CURSOR_VALUE) {}

    /** Is there available capacity in the buffer for the requested sequence. */
    virtual bool hasAvailableCapacity(std::vector<Sequence*>& dep_seqs) {
    	long wrap = (_sequence+1L) - _bufsz;
        if (wrap > _minGate) {
            long minSequence = getMinimumSequence(dep_seqs);
            _minGate = minSequence;

            if (wrap > minSequence) { return false; }
        }
        return true;
    }

	/**
     * Claim the next sequence index in the {@link RingBuffer} and increment.
     */
    virtual long incrementAndGet(std::vector<Sequence*>& dep_seqs) {
         ++_sequence;
         ensureCapacity(_sequence, dep_seqs);
         return _sequence;
    }

    /**
     * Increment by a delta and get the result.
     */
    virtual long incrementAndGet(int delta, std::vector<Sequence*>& dep_seqs) {
        _sequence += delta;
        ensureCapacity(_sequence, dep_seqs);
        return _sequence;
    }

    /**
     * Set the current sequence value for claiming {@link AbstractEntry} in
     * the {@link RingBuffer}
     */
    virtual void setSequence(long sequence, std::vector<Sequence*>& dep_seqs) {
        _sequence = sequence;
        ensureCapacity(sequence, dep_seqs);
    }
    /**
     * Serialise publishing in sequence.
     */
    virtual void serialisePublishing
    	(long sequence, Sequence& cursor, long batchSize) {}

private:

    void ensureCapacity(long sequence, std::vector<Sequence*>& dep_seqs) {
		long wrap = sequence - _bufsz;
		if (wrap > _minGate) {
			long minSequence;
			int counter = RETRIES;
			while (wrap > (minSequence = getMinimumSequence(dep_seqs))) {
				if (--counter == 0) {
					counter = RETRIES;
					boost::this_thread::yield();
				}
			}
			_minGate = minSequence;
		}
	}

}; // SingleThreadedStrategy

/**
 * Coordinator for claiming sequences for access to a data structure while
 * tracking dependent {@link Sequence}s
 */
template <typename event>
class Sequencer {
protected:
    const int 				_buffer_sz;

    Sequence 				_cursor;
    std::vector<Sequence*> 	_gating_seqs;

    ClaimStrategy* 			_claim_strat;
    WaitStrategy<event>*	_wait_strat;

public:
    /** Construct a Sequencer with the selected strategies. */
    Sequencer
    	(int buffer_sz, ClaimStrategy* claim_strat,
  			WaitStrategy<event>* wait_strat )
    	: _buffer_sz(buffer_sz), _claim_strat(claim_strat),
    	  _wait_strat(wait_strat)
    { }

    /**
     * Set the sequences that will gate publishers to prevent the buffer
     * wrapping.
     */
    void setGatingSequences(std::vector<Sequence>& sequences)
    	{ _gating_seqs = sequences; }
    void setGatingSequence(Sequence& sequence)
    	{ _gating_seqs.push_back(&sequence); }

    /**
     * Create a {@link SequenceBarrier} that gates on the the cursor and a
     * list of {@link Sequence}s
     */
	SequenceBarrier<event>* newBarrier(std::vector<Sequence*>&  sequences) {
        return new SequenceBarrier<event>(_wait_strat, &_cursor, sequences);
    }

    /** The capacity of the data structure to hold entries.*/
    int getBufferSize() { return _buffer_sz; }

    /** Get the value of the cursor indicating the published sequence. */
    long getCursor() { return _cursor.get(); }

    /**
     * Has the buffer got capacity to allocate another sequence.  This is a
     * concurrent method so the response should only be taken as an
     * indication of available capacity.
     */
    bool hasAvailableCapacity()
    	{ return _claim_strat->hasAvailableCapacity(_gating_seqs); }

    /**
     * Claim the next event in sequence for publishing to the
     * {@link RingBuffer}
     */
    long next() {
        if (_gating_seqs.empty())
        {
        	std::string msg("gatingSequences must be set before claiming sequences");
        	std::cout << msg << std::endl;
            throw "gatingSequences must be set before claiming sequences";
        }

        return _claim_strat->incrementAndGet(_gating_seqs);
    }

    /** Claim the next batch of sequence numbers for publishing. */
    SequenceBatch& next(SequenceBatch& sequenceBatch) {
        if (_gating_seqs.empty() ) {
        	throw std::exception
        		("gatingSequences must be set before claiming sequences");
        }

        int batchSize = sequenceBatch.getSize();
        if (batchSize > _buffer_sz)
        {
            throw std::exception( "Batch size is greater than buffer size" );
        }

        long sequence = _claim_strat->incrementAndGet(batchSize, _gating_seqs);
        sequenceBatch.setEnd(sequence);
        return sequenceBatch;
    }

    /** Claim a specific sequence when only one publisher is involved. */
	long claim(long sequence)
    {
        if (_gating_seqs.empty()) {
        	throw std::exception("gatingSequences must be set before "
        			"claiming sequences");
        }

        _claim_strat->setSequence(sequence, _gating_seqs);

        return sequence;
    }

    /** Publish an event and make it visible to {@link EventProcessor}s */
    void publish(long sequence) { publish(sequence, 1); }

    /** Publish the batch of events in sequence.*/
    void publish(SequenceBatch& sequenceBatch)
    	{ publish(sequenceBatch.getEnd(), sequenceBatch.getSize()); }

    /** Force the publication of a cursor sequence.
     *
     * Only use this method when forcing a sequence and you are sure only
     * one publisher exists.
     * This will cause the cursor to advance to this sequence.
     */
    void forcePublish(long sequence)  {
        _cursor.set(sequence);
        _wait_strat->signalAll();
    }

    void publish(long sequence, long batchSize)
    {
        _claim_strat->serialisePublishing(sequence, _cursor, batchSize);
        _cursor.set(sequence);
        _wait_strat->signalAll();
    }
};// Sequencer

}; // namespace disruptor


#endif /* SEQUENCE_HPP_ */
