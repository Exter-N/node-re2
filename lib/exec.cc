#include "./wrapped_re2.h"
#include "./util.h"

#include <vector>

#include <node_buffer.h>


using std::map;
using std::pair;
using std::string;
using std::vector;

using v8::Array;
using v8::Integer;
using v8::Local;
using v8::String;
using v8::Value;


NAN_METHOD(WrappedRE2::Exec) {

	// unpack arguments

	WrappedRE2* re2 = Nan::ObjectWrap::Unwrap<WrappedRE2>(info.This());
	if (!re2) {
		info.GetReturnValue().SetNull();
		return;
	}

	StrVal str(info[0]);
	if (!str.data) {
		return;
	}

	size_t lastIndex = 0;

	if (str.isBuffer) {
		if ((re2->global || re2->sticky) && re2->lastIndex) {
			if (re2->lastIndex > str.size) {
				re2->lastIndex = 0;
				info.GetReturnValue().SetNull();
				return;
			}
			lastIndex = re2->lastIndex;
		}
	} else {
		if ((re2->global || re2-> sticky) && re2->lastIndex) {
			if (re2->lastIndex > str.length) {
				re2->lastIndex = 0;
				info.GetReturnValue().SetNull();
				return;
			}
			for (size_t n = re2->lastIndex; n; --n) {
				lastIndex += getUtf8CharSize(str.data[lastIndex]);
			}
		}
	}

	// actual work

	int patternIndex = 0;
	vector<StringPiece> groups(re2->useSet ? 1 : (re2->regexp.NumberOfCapturingGroups() + 1));

	if (!re2->regexp.Match(str, lastIndex, str.size, re2->sticky ? RE2::ANCHOR_START : RE2::UNANCHORED, &groups[0], groups.size())) {
		if (re2->global || re2->sticky) {
			re2->lastIndex = 0;
		}
		info.GetReturnValue().SetNull();
		return;
	}

	if (re2->useSet) {
		vector<int> patterns;
		if (!re2->set.Match(groups[0], &patterns)) {
			return Nan::ThrowError("Inconsistency in RE2.exec : piped regex finds a match but set doesn't agree.");
		}
		if (patterns.empty()) {
			return Nan::ThrowError("Inconsistency in RE2.exec : match found but the set didn't tell for which pattern.");
		}
		patternIndex = patterns[0];
		groups.resize(re2->regexps[patternIndex]->NumberOfCapturingGroups() + 1);
		if (!re2->regexps[patternIndex]->Match(str, groups[0].data() - str.data, (groups[0].data() - str.data) + groups[0].size(), RE2::ANCHOR_BOTH, &groups[0], groups.size())) {
			return Nan::ThrowError("Inconsistency in RE2.exec : set finds a match but designated regex doesn't agree.");
		}
	}

	// form a result

	Local<Array> result = Nan::New<Array>();

	int indexOffset = re2->global || re2->sticky ? re2->lastIndex : 0;

	if (str.isBuffer) {
		for (size_t i = 0, n = groups.size(); i < n; ++i) {
			const StringPiece& item = groups[i];
			if (item.data() != NULL) {
				Nan::Set(result, i, Nan::CopyBuffer(item.data(), item.size()).ToLocalChecked());
			}
		}
		Nan::Set(result, Nan::New("index").ToLocalChecked(), Nan::New<Integer>(
			indexOffset + static_cast<int>(groups[0].data() - str.data)));
	} else {
		for (size_t i = 0, n = groups.size(); i < n; ++i) {
			const StringPiece& item = groups[i];
			if (item.data() != NULL) {
				Nan::Set(result, i, Nan::New(item.data(), item.size()).ToLocalChecked());
			}
		}
		Nan::Set(result, Nan::New("index").ToLocalChecked(), Nan::New<Integer>(
			indexOffset + static_cast<int>(getUtf16Length(str.data + lastIndex, groups[0].data()))));
	}

	Nan::Set(result, Nan::New("input").ToLocalChecked(), info[0]);

	const map<int, string>& groupNames = re2->useSet ? re2->regexps[patternIndex]->CapturingGroupNames() : re2->regexp.CapturingGroupNames();
	if (!groupNames.empty()) {
		Local<Object> groups = Nan::New<Object>();
		auto ignore(groups->SetPrototype(v8::Isolate::GetCurrent()->GetCurrentContext(), Nan::Null()));

		for (pair<int, string> group: groupNames) {
			Nan::MaybeLocal<Value> value = Nan::Get(result, group.first);
			if (!value.IsEmpty()) {
				Nan::Set(groups, Nan::New(group.second).ToLocalChecked(), value.ToLocalChecked());
			}
		}

		Nan::Set(result, Nan::New("groups").ToLocalChecked(), groups);
	} else {
		Nan::Set(result, Nan::New("groups").ToLocalChecked(), Nan::Undefined());
	}

	if (re2->useSet) {
		Nan::Set(result, Nan::New("patternIndex").ToLocalChecked(), Nan::New(patternIndex));
	}

	if (re2->global || re2->sticky) {
		re2->lastIndex += str.isBuffer ? groups[0].data() - str.data + groups[0].size() - lastIndex :
			getUtf16Length(str.data + lastIndex, groups[0].data() + groups[0].size());
	}

	info.GetReturnValue().Set(result);
}
