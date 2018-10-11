#ifndef FUNCTIONAL_H_
#define FUNCTIONAL_H_


#include <node.h>
#include <nan.h>


template<typename R, typename P, typename L>
inline v8::MaybeLocal<R> bind(v8::MaybeLocal<P> param, L lambda) {
	if (param.IsEmpty()) return v8::MaybeLocal<R>();

	return lambda(param.ToLocalChecked());
}

template<typename R, typename P, typename L1, typename... Ls>
inline v8::MaybeLocal<R> bind(v8::MaybeLocal<P> param, L1 lambda, Ls... lambdas) {
	if (param.IsEmpty()) return v8::MaybeLocal<R>();

	auto next = lambda(param.ToLocalChecked());

	return bind<R>(next, lambdas...);
}


template<typename T>
inline bool is(v8::Local<v8::Value> value);

#define SIMPLE_IS_IMPL(T) \
	template<> \
	inline bool is<v8::T>(v8::Local<v8::Value> value) { \
		return value->Is##T(); \
	}

SIMPLE_IS_IMPL(Boolean);
SIMPLE_IS_IMPL(Function);
SIMPLE_IS_IMPL(Object);
SIMPLE_IS_IMPL(Symbol);

#undef SIMPLE_IS_IMPL


template<typename T>
inline v8::MaybeLocal<T> as(v8::Local<v8::Value> value) {
	if (is<T>(value)) {
		return value.As<T>();
	}

	Nan::ThrowTypeError("Encountered a value of a wrong type"); // TODO make this error message more helpful
	return v8::MaybeLocal<T>();
}


#endif
