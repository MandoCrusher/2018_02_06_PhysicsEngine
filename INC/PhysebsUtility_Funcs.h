#pragma once

namespace Physebs {
	/**
	*	@brief Return the smaller of two values, or the first value if both are equal.
	*	@param a_val1 is the first value.
	*	@param a_val2 is the second value.
	*	@return const reference to smallest value.
	*/
	template<class T>
	const T& Min(const T& a_val1, const T& a_val2) {
		return !(a_val2 < a_val1) ? a_val1 : a_val2;		// Return smaller value or val1 if val1 and val2 are equal
	}

	/**
	*	@brief Return the smaller of two values, or the first value if both are equal.
	*	@param a_val1 is the first value.
	*	@param a_val2 is the second value.
	*	@return const reference to smallest value.
	*/
	template<class T>
	const T& Max(const T& a_val1, const T& a_val2) {
		return !(a_val2 > a_val1) ? a_val1 : a_val2;
	}

	/**
	*	@brief Constrain given value between a range.
	*	@param a_val is the value to constrain.
	*	@param a_upper is the upper range.
	*	@param a_lower is the lower range.
	*	@return const reference to constrained value.
	*/
	template<class T>
	const T& Clamp(const T& a_val, const T& a_upper, const T& a_lower) {
		return Min<T>(Max<T>(a_val, a_lower), a_upper);
	}

}