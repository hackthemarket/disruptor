/*
 * disruptor.cpp
 *
 *  Created on: Sep 13, 2011
 *      Author: tingarg
 */

#include "disruptor.hpp"

namespace disruptor {

const static AlertException::AlertException Alert;

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
int numberOfLeadingZeros(int i) {
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
 * Get the minimum sequence from an array of {@link Consumer}s.
 *
 * @param consumers to compare.
 * @return the minimum sequence found or Long.MAX_VALUE if the array is empty.
 */
long getMinimumSequence(std::vector<Sequence*>& consumers) {
    long minimum = LONG_MAX;
    for (std::vector<Sequence*>::const_iterator it = consumers.begin();
    			it!=consumers.end(); ++it) {
    	Sequence* seq = *it;
    	minimum = std::min(minimum, seq->get());
    }
    return minimum;
}


}; // namespace dsiruptor
