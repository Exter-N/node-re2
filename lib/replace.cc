#include "./wrapped_re2.h"
#include "./util.h"
#include "./iterator.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <iostream>

#include <node_buffer.h>


using std::map;
using std::min;
using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;

using v8::Array;
using v8::Integer;
using v8::Local;
using v8::MaybeLocal;
using v8::String;
using v8::Value;


inline int getMaxSubmatch(const char* data, size_t size, const map<string, int>& namedGroups) {
	int maxSubmatch = 0, index, index2;
	const char* nameBegin;
	const char* nameEnd;
	for (size_t i = 0; i < size;) {
		char ch = data[i];
		if (ch == '$') {
			if (i + 1 < size) {
				ch = data[i + 1];
				switch (ch) {
					case '$':
					case '&':
					case '`':
					case '\'':
						i += 2;
						continue;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						index = ch - '0';
						if (i + 2 < size) {
							ch = data[i + 2];
							if ('0' <= ch && ch <= '9') {
								index2 = index * 10 + (ch - '0');
								if (maxSubmatch < index2) maxSubmatch = index2;
								i += 3;
								continue;
							}
						}
						if (maxSubmatch < index) maxSubmatch = index;
						i += 2;
						continue;
					case '<':
						nameBegin = data + i + 2;
						nameEnd = (const char*)memchr(nameBegin, '>', size - i - 2);
						if (nameEnd) {
							string name(nameBegin, nameEnd - nameBegin);
							map<string, int>::const_iterator group = namedGroups.find(name);
							if (group != namedGroups.end()) {
								index = group->second;
								if (maxSubmatch < index) maxSubmatch = index;
							}
							i = nameEnd + 1 - data;
						} else {
							i += 2;
						}
						continue;
				}
			}
			++i;
			continue;
		}
		i += getUtf8CharSize(ch);
	}
	return maxSubmatch;
}


inline string replace(const char* data, size_t size, const vector<StringPiece>& groups, const StringPiece& str, const map<string, int>& namedGroups) {
	string result;
	size_t index, index2;
	const char* nameBegin;
	const char* nameEnd;
	for (size_t i = 0; i < size;) {
		char ch = data[i];
		if (ch == '$') {
			if (i + 1 < size) {
				ch = data[i + 1];
				switch (ch) {
					case '$':
						result += ch;
						i += 2;
						continue;
					case '&':
						result += groups[0].as_string();
						i += 2;
						continue;
					case '`':
						result += string(str.data(), groups[0].data() - str.data());
						i += 2;
						continue;
					case '\'':
						result += string(groups[0].data() + groups[0].size(),
							str.data() + str.size() - groups[0].data() - groups[0].size());
						i += 2;
						continue;
					case '0':
					case '1':
					case '2':
					case '3':
					case '4':
					case '5':
					case '6':
					case '7':
					case '8':
					case '9':
						index = ch - '0';
						if (i + 2 < size) {
							ch = data[i + 2];
							if ('0' <= ch && ch <= '9') {
								i += 3;
								index2 = index * 10 + (ch - '0');
								if (index2 && index2 < groups.size()) {
									result += groups[index2].as_string();
									continue;
								}
								result += '$';
								result += '0' + index;
								result += ch;
								continue;
							}
							ch = '0' + index;
						}
						i += 2;
						if (index && index < groups.size()) {
							result += groups[index].as_string();
							continue;
						}
						result += '$';
						result += ch;
						continue;
					case '<':
						if (!namedGroups.empty()) {
							nameBegin = data + i + 2;
							nameEnd = (const char*)memchr(nameBegin, '>', size - i - 2);
							if (nameEnd) {
								string name(nameBegin, nameEnd - nameBegin);
								map<string, int>::const_iterator group = namedGroups.find(name);
								if (group != namedGroups.end()) {
									index = group->second;
									result += groups[index].as_string();
								}
								i = nameEnd + 1 - data;
							} else {
								result += "$<";
								i += 2;
							}
						} else {
							result += "$<";
							i += 2;
						}
						continue;
				}
			}
			result += '$';
			++i;
			continue;
		}
		size_t sym_size = getUtf8CharSize(ch);
		result.append(data + i, sym_size);
		i += sym_size;
	}
	return result;
}


static Nan::Maybe<string> replace(WrappedRE2* re2, const StrVal& replacee, const char* replacer, size_t replacer_size) {
	const StringPiece str(replacee);
	const char* data = str.data();
	size_t      size = str.size();

	const map<string, int>& namedGroups = re2->regexp.NamedCapturingGroups();

	vector<StringPiece> groups(min(re2->regexp.NumberOfCapturingGroups(), getMaxSubmatch(replacer, replacer_size, namedGroups)) + 1);
	const StringPiece& match = groups[0];

	size_t lastIndex = 0;
	string result;
	RE2::Anchor anchor = RE2::UNANCHORED;

	if (re2->sticky) {
		if (!re2->global) {
			if (replacee.isBuffer) {
				lastIndex = re2->lastIndex;
			} else {
				for (size_t n = re2->lastIndex; n; --n) {
					lastIndex += getUtf8CharSize(data[lastIndex]);
				}
			}
		}
		anchor = RE2::ANCHOR_START;
	}

	if (lastIndex) {
		result = string(data, lastIndex);
	}

	bool noMatch = true;
	while (lastIndex <= size && re2->regexp.Match(str, lastIndex, size, anchor, &groups[0], groups.size())) {
		noMatch = false;
		if (!re2->global && re2->sticky) {
			re2->lastIndex += replacee.isBuffer ? match.data() - data + match.size() - lastIndex :
				getUtf16Length(data + lastIndex, match.data() + match.size());
		}
		if (match.size()) {
			if (match.data() == data || match.data() - data > lastIndex) {
				result += string(data + lastIndex, match.data() - data - lastIndex);
			}
			result += replace(replacer, replacer_size, groups, str, namedGroups);
			lastIndex = match.data() - data + match.size();
		} else {
			result += replace(replacer, replacer_size, groups, str, namedGroups);
			size_t sym_size = getUtf8CharSize(data[lastIndex]);
			if (lastIndex < size) {
				result.append(data + lastIndex, sym_size);
			}
			lastIndex += sym_size;
		}
		if (!re2->global) {
			break;
		}
	}
	if (lastIndex < size) {
		result += string(data + lastIndex, size - lastIndex);
	}

	if (re2->global) {
		re2->lastIndex = 0;
	} else if (re2->sticky) {
		if (noMatch) {
			re2->lastIndex = 0;
		}
	}

	return Nan::Just(result);
}


inline Nan::Maybe<string> replace(const Nan::Callback* replacer, const vector<StringPiece>& groups, const StringPiece& str, const Local<Value>& input, bool useBuffers, const map<string, int>& namedGroups) {
	vector< Local<Value> >	argv;

	if (useBuffers) {
		for (size_t i = 0, n = groups.size(); i < n; ++i) {
			const StringPiece& item = groups[i];
			argv.push_back(Nan::CopyBuffer(item.data(), item.size()).ToLocalChecked());
		}
		argv.push_back(Nan::New(static_cast<int>(groups[0].data() - str.data())));
	} else {
		for (size_t i = 0, n = groups.size(); i < n; ++i) {
			const StringPiece& item = groups[i];
			argv.push_back(Nan::New(item.data(), item.size()).ToLocalChecked());
		}
		argv.push_back(Nan::New(static_cast<int>(getUtf16Length(str.data(), groups[0].data()))));
	}
	argv.push_back(input);

	if (!namedGroups.empty()) {
		Local<Object> groups = Nan::New<Object>();
		auto ignore(groups->SetPrototype(v8::Isolate::GetCurrent()->GetCurrentContext(), Nan::Null()));

		for (pair<string, int> group: namedGroups) {
			Nan::Set(groups, Nan::New(group.first).ToLocalChecked(), argv[group.second]);
		}

		argv.push_back(groups);
	}

	MaybeLocal<Value> maybeResult(Nan::Call(replacer->GetFunction(), v8::Isolate::GetCurrent()->GetCurrentContext()->Global(), static_cast<int>(argv.size()), &argv[0]));

	if (maybeResult.IsEmpty()) {
		return Nan::Nothing<string>();
	}

	Local<Value> result = maybeResult.ToLocalChecked();

	if (node::Buffer::HasInstance(result)) {
		return Nan::Just(string(node::Buffer::Data(result), node::Buffer::Length(result)));
	}

	Nan::Utf8String val(result->ToString());
	return Nan::Just(string(*val, val.length()));
}


static Nan::Maybe<string> replace(WrappedRE2* re2, const StrVal& replacee, const Nan::Callback* replacer, const Local<Value>& input, bool useBuffers) {
	const StringPiece str(replacee);
	const char* data = str.data();
	size_t      size = str.size();

	vector<StringPiece> groups(re2->regexp.NumberOfCapturingGroups() + 1);
	const StringPiece& match = groups[0];

	size_t lastIndex = 0;
	string result;
	RE2::Anchor anchor = RE2::UNANCHORED;

	if (re2->sticky) {
		if (!re2->global) {
			if (replacee.isBuffer) {
				lastIndex = re2->lastIndex;
			} else {
				for (size_t n = re2->lastIndex; n; --n) {
					lastIndex += getUtf8CharSize(data[lastIndex]);
				}
			}
		}
		anchor = RE2::ANCHOR_START;
	}

	if (lastIndex) {
		result = string(data, lastIndex);
	}

	const map<string, int>& namedGroups = re2->regexp.NamedCapturingGroups();

	bool noMatch = true;
	while (lastIndex <= size && re2->regexp.Match(str, lastIndex, size, anchor, &groups[0], groups.size())) {
		noMatch = false;
		if (!re2->global && re2->sticky) {
			re2->lastIndex += replacee.isBuffer ? match.data() - data + match.size() - lastIndex :
				getUtf16Length(data + lastIndex, match.data() + match.size());
		}
		if (match.size()) {
			if (match.data() == data || match.data() - data > lastIndex) {
				result += string(data + lastIndex, match.data() - data - lastIndex);
			}
			const Nan::Maybe<string> part(replace(replacer, groups, str, input, useBuffers, namedGroups));
			if (part.IsNothing()) {
				return part;
			}
			result += part.FromJust();
			lastIndex = match.data() - data + match.size();
		} else {
			const Nan::Maybe<string> part(replace(replacer, groups, str, input, useBuffers, namedGroups));
			if (part.IsNothing()) {
				return part;
			}
			result += part.FromJust();
			size_t sym_size = getUtf8CharSize(data[lastIndex]);
			if (lastIndex < size) {
				result.append(data + lastIndex, sym_size);
			}
			lastIndex += sym_size;
		}
		if (!re2->global) {
			break;
		}
	}
	if (lastIndex < size) {
		result += string(data + lastIndex, size - lastIndex);
	}

	if (re2->global) {
		re2->lastIndex = 0;
	} else if (re2->sticky) {
		if (noMatch) {
			re2->lastIndex = 0;
		}
	}

	return Nan::Just(result);
}


static bool requiresBuffers(const Local<Function>& f) {
	Local<Value> flag(Nan::Get(f, Nan::New("useBuffers").ToLocalChecked()).ToLocalChecked());
	if (flag->IsUndefined() || flag->IsNull() || flag->IsFalse()) {
		return false;
	}
	if (flag->IsNumber()){
		return flag->NumberValue() != 0;
	}
	if (flag->IsString()){
		return flag->ToString()->Length() > 0;
	}
	return true;
}


struct replacer {
	StrVal str;
	unique_ptr<const Nan::Callback> cb;
	int maxSubmatch;
	bool useBuffers;
	map<string, int> namedGroups;

	replacer(const Local<Value>& value, const RE2& regexp, bool& good): str(), cb(nullptr), maxSubmatch(0), useBuffers(false)
	{
		namedGroups = regexp.NamedCapturingGroups();
		if (value->IsNull() || value->IsUndefined()) {
			str.data = const_cast<char*>("$&");
			str.size = 2;
			str.length = 2;
		} else if (value->IsFunction()) {
			Local<Function> fun(value.As<Function>());
			cb = unique_ptr<const Nan::Callback>(new Nan::Callback(fun));
			maxSubmatch = regexp.NumberOfCapturingGroups();
			useBuffers = requiresBuffers(fun);
		} else {
			str = value;
			if (!str.data) {
				good = false;
				return;
			}
			maxSubmatch = min(regexp.NumberOfCapturingGroups(), getMaxSubmatch(str.data, str.length, namedGroups));
		}
	}

	Nan::Maybe<string> replace(const vector<StringPiece>& groups, const StringPiece& replacee, const Local<Value>& input) const {
		if (str.data) {
			return Nan::Just(::replace(str.data, str.size, groups, replacee, namedGroups));
		} else {
			return ::replace(cb.get(), groups, replacee, input, useBuffers, namedGroups);
		}
	}
};


static Nan::Maybe<string> replace(WrappedRE2* re2, const StrVal& replacee, const vector<replacer>& replacers, const Local<Value>& input) {
	const StringPiece str(replacee);
	const char* data = str.data();
	size_t      size = str.size();

	vector<StringPiece> groups(1);
	const StringPiece& match = groups[0];

	size_t lastIndex = 0;
	string result;
	RE2::Anchor anchor = RE2::UNANCHORED;

	if (re2->sticky) {
		if (!re2->global) {
			if (replacee.isBuffer) {
				lastIndex = re2->lastIndex;
			} else {
				for (size_t n = re2->lastIndex; n; --n) {
					lastIndex += getUtf8CharSize(data[lastIndex]);
				}
			}
		}
		anchor = RE2::ANCHOR_START;
	}

	if (lastIndex) {
		result = string(data, lastIndex);
	}

	bool noMatch = true;
	while (lastIndex <= size && re2->regexp.Match(str, lastIndex, size, anchor, &groups[0], groups.size())) {
		noMatch = false;
		vector<int> patterns;
		if (!re2->set.Match(match, &patterns)) {
			Nan::ThrowError("Inconsistency in RE2.replace : piped regex finds a match but set doesn't agree.");
			return Nan::Nothing<string>();
		}
		if (patterns.empty()) {
			Nan::ThrowError("Inconsistency in RE2.replace : match found but the set didn't tell for which pattern.");
			return Nan::Nothing<string>();
		}
		int patternIndex = patterns[0];
		vector<StringPiece> actualGroups(replacers[patternIndex].maxSubmatch + 1);
		const StringPiece& actualMatch = actualGroups[0];
		if (!re2->regexps[patternIndex]->Match(str, match.data() - str.data(), (match.data() - str.data()) + match.size(), RE2::ANCHOR_BOTH, &actualGroups[0], actualGroups.size())) {
			Nan::ThrowError("Inconsistency in RE2.replace : set finds a match but designated regex doesn't agree.");
			return Nan::Nothing<string>();
		}
		if (!re2->global && re2->sticky) {
			re2->lastIndex += replacee.isBuffer ? actualMatch.data() - data + actualMatch.size() - lastIndex :
				getUtf16Length(data + lastIndex, actualMatch.data() + actualMatch.size());
		}
		if (actualMatch.size()) {
			if (actualMatch.data() == data || actualMatch.data() - data > lastIndex) {
				result += string(data + lastIndex, actualMatch.data() - data - lastIndex);
			}
			const Nan::Maybe<string> part(replacers[patternIndex].replace(actualGroups, str, input));
			if (part.IsNothing()) {
				return part;
			}
			result += part.FromJust();
			lastIndex = actualMatch.data() - data + actualMatch.size();
		} else {
			const Nan::Maybe<string> part(replacers[patternIndex].replace(actualGroups, str, input));
			if (part.IsNothing()) {
				return part;
			}
			result += part.FromJust();
			size_t sym_size = getUtf8CharSize(data[lastIndex]);
			if (lastIndex < size) {
				result.append(data + lastIndex, sym_size);
			}
			lastIndex += sym_size;
		}
		if (!re2->global) {
			break;
		}
	}
	if (lastIndex < size) {
		result += string(data + lastIndex, size - lastIndex);
	}

	if (re2->global) {
		re2->lastIndex = 0;
	} else if (re2->sticky) {
		if (noMatch) {
			re2->lastIndex = 0;
		}
	}

	return Nan::Just(result);
}


NAN_METHOD(WrappedRE2::Replace) {

	WrappedRE2* re2 = Nan::ObjectWrap::Unwrap<WrappedRE2>(info.This());
	if (!re2) {
		info.GetReturnValue().Set(info[0]);
		return;
	}

	StrVal replacee(info[0]);
	if (!replacee.data) {
		return;
	}

	string result;

	if (re2->useSet && info[1]->IsObject() && isIterable(info[1].As<Object>())) {
		vector<replacer> replacers;
		bool good = true;
		size_t i = 0;
		for (Local<Value> replacer: info[1].As<Object>()) {
			replacers.emplace_back(replacer, *re2->regexps[i++], good);
			if (!good) {
				return;
			}
			if (i >= re2->regexps.size()) {
				break;
			}
		}
		if (ESIterator::failed()) {
			return;
		}
		while (i < re2->regexps.size()) {
			replacers.emplace_back(Nan::Null(), *re2->regexps[i++], good);
		}
		const Nan::Maybe<string> replaced(replace(re2, replacee, replacers, info[0]));
		if (replaced.IsNothing()) {
			return;
		}
		result = replaced.FromJust();
	} else if (info[1]->IsFunction()) {
		Local<Function> fun(info[1].As<Function>());
		const unique_ptr<const Nan::Callback> cb(new Nan::Callback(fun));
		const Nan::Maybe<string> replaced(replace(re2, replacee, cb.get(), info[0], requiresBuffers(fun)));
		if (replaced.IsNothing()) {
			return;
		}
		result = replaced.FromJust();
	} else {
		StrVal replacer(info[1]);
		if (!replacer.data) {
			return;
		}
		const Nan::Maybe<string> replaced(replace(re2, replacee, replacer.data, replacer.size));
		if (replaced.IsNothing()) {
			return;
		}
		result = replaced.FromJust();
	}

	if (replacee.isBuffer) {
		info.GetReturnValue().Set(Nan::CopyBuffer(result.data(), result.size()).ToLocalChecked());
		return;
	}
	info.GetReturnValue().Set(Nan::New(result).ToLocalChecked());
}
