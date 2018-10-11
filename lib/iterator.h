#ifndef ITERATOR_H_
#define ITERATOR_H_


#include <node.h>
#include <nan.h>

#if __cpp_range_based_for < 201603
#error This file requires support of the C++17 generalized range-based for loop
#endif


const struct { } ESEndIterator;


class ESIterator {
	v8::MaybeLocal<v8::Object> iterator_;
	v8::Local<v8::Value> value_;
	bool done_;
	static bool failed_;

public:
	explicit ESIterator(const v8::Local<v8::Object>& iterable);
	ESIterator(const ESIterator&) = delete;
	ESIterator(ESIterator&& other);

	ESIterator& operator =(const ESIterator&) = delete;
	ESIterator& operator =(ESIterator&& other);

	ESIterator& operator ++();
	inline v8::Local<v8::Value> operator *() {
		return value_;
	}

	inline bool operator ==(decltype(ESEndIterator)) const {
		return done_;
	}
	inline bool operator !=(decltype(ESEndIterator)) const {
		return !done_;
	}

	inline static bool failed() {
		bool f = failed_;
		failed_ = false;

		return f;
	}
};


v8::MaybeLocal<v8::Symbol> getWellKnownSymbol(v8::Local<v8::String> name);


bool isIterable(v8::Local<v8::Object> maybeIterable);


namespace v8 { // For the range-based for loop
	inline ESIterator begin(v8::Local<v8::Object> iterable) {
		return ESIterator(iterable);
	}
	inline decltype(ESEndIterator) end(v8::Local<v8::Object>) {
		return ESEndIterator;
	}
}


#endif
