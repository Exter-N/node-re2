#include "./functional.h"
#include "./iterator.h"


using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Symbol;
using v8::Value;


MaybeLocal<Function> getIteratorMethod(Local<Object> iterable) {
	return bind<Function>(
		getWellKnownSymbol(Nan::New("iterator").ToLocalChecked()),
		[iterable] (Local<Symbol> iterator) { return Nan::Get(iterable, iterator); },
		as<Function>);
}


bool ESIterator::failed_ = false;


ESIterator::ESIterator(const Local<Object>& iterable) : iterator_(), value_(), done_(false) {
	MaybeLocal<Object> iterator = bind<Object>(
		getIteratorMethod(iterable),
		[iterable] (Local<Function> method) { return Nan::Call(method, iterable, 0, nullptr); },
		as<Object>);

	if (iterator.IsEmpty()) {
		done_ = true;
		failed_ = true;

		return;
	}

	iterator_ = iterator;

	++*this;
}


ESIterator::ESIterator(ESIterator&& other) : iterator_(other.iterator_), value_(other.value_), done_(other.done_) {
	other.iterator_ = MaybeLocal<Object>();
	other.value_ = Local<Value>();
	other.done_ = true;
}


ESIterator& ESIterator::operator =(ESIterator&& other) {
	iterator_ = other.iterator_;
	value_ = other.value_;
	done_ = other.done_;

	other.iterator_ = MaybeLocal<Object>();
	other.value_ = Local<Value>();
	other.done_ = true;

	return *this;
}


ESIterator& ESIterator::operator ++() {
	if (iterator_.IsEmpty()) {
		value_ = Local<Value>();
		done_ = true;

		return *this;
	}

	Local<Object> iterator = iterator_.ToLocalChecked();
	MaybeLocal<Object> maybeRecord = bind<Object>(
		Nan::Get(iterator, Nan::New("next").ToLocalChecked()),
		as<Function>,
		[iterator] (Local<Function> next) { return Nan::Call(next, iterator, 0, nullptr); },
		as<Object>);

	if (maybeRecord.IsEmpty()) {
		value_ = Local<Value>();
		done_ = true;
		failed_ = true;

		return *this;
	}

	Local<Object> record = maybeRecord.ToLocalChecked();

	MaybeLocal<Value> value = Nan::Get(record, Nan::New("value").ToLocalChecked());
	if (value.IsEmpty()) {
		value_ = Local<Value>();
		done_ = true;
		failed_ = true;

		return *this;
	} else {
		value_ = value.ToLocalChecked();
	}

	MaybeLocal<Boolean> done = bind<Boolean>(
		Nan::Get(record, Nan::New("done").ToLocalChecked()),
		as<Boolean>);
	if (done.IsEmpty()) {
		done_ = true;
		failed_ = true;
	} else {
		done_ = done.ToLocalChecked()->Value();
	}

	return *this;
}


MaybeLocal<Symbol> getWellKnownSymbol(Local<String> name) {
	Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();

	return bind<Symbol>(
		Nan::Get(context->Global(), Nan::New("Symbol").ToLocalChecked()),
		as<Object>,
		[name] (Local<Object> symbolCtor) { return Nan::Get(symbolCtor, name); },
		as<Symbol>);
}


bool isIterable(Local<Object> maybeIterable) {
	return !getIteratorMethod(maybeIterable).IsEmpty();
}
