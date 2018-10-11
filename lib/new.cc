#include "./wrapped_re2.h"
#include "./util.h"
#include "./iterator.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>


using std::map;
using std::move;
using std::pair;
using std::string;
using std::unique_ptr;
using std::unordered_set;
using std::vector;

using v8::Local;
using v8::RegExp;
using v8::String;
using v8::Value;


static char hex[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

inline bool isUpperCaseAlpha(char ch) {
	return 'A' <= ch && ch <= 'Z';
}

inline bool isHexadecimal(char ch) {
	return ('0' <= ch && ch <= '9') || ('A' <= ch && ch <= 'Z') || ('a' <= ch && ch <= 'z');
}

static bool translateRegExp(const char* data, size_t size, vector<char>& buffer) {
	string result;
	bool changed = false;

	if (!size) {
		result = "(?:)";
		changed = true;
	}

	for (size_t i = 0; i < size;) {
		char ch = data[i];
		if (ch == '\\') {
			if (i + 1 < size) {
				ch = data[i + 1];
				switch (ch) {
					case '\\':
						result += "\\\\";
						i += 2;
						continue;
					case 'c':
						if (i + 2 < size) {
							ch = data[i + 2];
							if (isUpperCaseAlpha(ch)) {
								result += "\\x";
								result += hex[((ch - '@') / 16) & 15];
								result += hex[(ch - '@') & 15];
								i += 3;
								changed = true;
								continue;
							}
						}
						result += "\\c";
						i += 2;
						continue;
					case 'u':
						if (i + 2 < size) {
							ch = data[i + 2];
							if (isHexadecimal(ch)) {
								result += "\\x{";
								result += ch;
								i += 3;
								for (size_t j = 0; j < 3 && i < size; ++i, ++j) {
									ch = data[i];
									if (!isHexadecimal(ch)) {
										break;
									}
									result += ch;
								}
								result += '}';
								changed = true;
								continue;
							} else if (ch == '{') {
								result += "\\x";
								i += 2;
								changed = true;
								continue;
							}
						}
						result += "\\u";
						i += 2;
						continue;
					default:
						result += "\\";
						size_t sym_size = getUtf8CharSize(ch);
						result.append(data + i + 1, sym_size);
						i += sym_size + 1;
						continue;
				}
			}
		} else if (ch == '/') {
			result += "\\/";
			i += 1;
			changed = true;
			continue;
		} else if (ch == '(' && i + 2 < size && data[i + 1] == '?' && data[i + 2] == '<') {
			if (i + 3 >= size || (data[i + 3] != '=' && data[i + 3] != '!')) {
				result += "(?P<";
				i += 3;
				changed = true;
				continue;
			}
		}
		size_t sym_size = getUtf8CharSize(ch);
		result.append(data + i, sym_size);
		i += sym_size;
	}

	if (!changed) {
		return false;
	}

	buffer.resize(0);
	buffer.insert(buffer.end(), result.begin(), result.end());
	buffer.push_back('\0');

	return true;
}

static bool translateRegExp(const string& str, vector<char>& buffer) {
	return translateRegExp(str.data(), str.size(), buffer);
}

static string escapeRegExp(const char* data, size_t size) {
	string result;

	if (!size) {
		result = "(?:)";
	}

	size_t prevBackSlashes = 0;
	for (size_t i = 0; i < size;) {
		char ch = data[i];
		if (ch == '\\') {
			++prevBackSlashes;
		} else if (ch == '/' && !(prevBackSlashes & 1)) {
			result += "\\/";
			i += 1;
			prevBackSlashes = 0;
			continue;
		} else {
			prevBackSlashes = 0;
		}
		size_t sym_size = getUtf8CharSize(ch);
		result.append(data + i, sym_size);
		i += sym_size;
	}

	return result;
}

static bool isSafeToUseInSet(const string& source) {
	const char* p = source.data();
	const char* end = p + source.size();

	while (p < end) {
		switch (*p) {
			case '\\':
				p += 2;
				break;
			case '[':
				++p;
				if (p < end && *p == '^') { // Avoid false-positives on "[^"
					++p;
				}
				break;
			case '^':
			case '$':
				return false;
			default:
				++p;
				break;
		}
	}

	return true;
}


bool WrappedRE2::alreadyWarnedAboutUnicode = false;

static const char* depricationMessage = "BMP patterns aren't supported by node-re2. An implicit \"u\" flag is assumed by the RE2 constructor. In a future major version, calling the RE2 constructor without the \"u\" flag may become forbidden, or cause a different behavior. Please see https://github.com/uhop/node-re2/issues/21 for more information.";


inline bool ensureUniqueNamedGroups(const map<int, string>& groups) {
	unordered_set<string> names;

	for (pair<int, string> group: groups) {
		if (!names.insert(group.second).second) {
			return false;
		}
	}

	return true;
}


NAN_METHOD(WrappedRE2::New) {

	if (!info.IsConstructCall()) {
		// call a constructor and return the result

		vector< Local<Value> > parameters(info.Length());
		for (size_t i = 0, n = info.Length(); i < n; ++i) {
			parameters[i] = info[i];
		}
		auto newObject = Nan::NewInstance(Nan::New<Function>(constructor), parameters.size(), &parameters[0]);
		if (!newObject.IsEmpty()) {
			info.GetReturnValue().Set(newObject.ToLocalChecked());
		}
		return;
	}

	// process arguments

	vector<char> buffer;

	char*  data = NULL;
	size_t size = 0;

	bool useSet = false;
	string source;
	vector<string> sources;
	vector<string> internalSources;
	bool   global = false;
	bool   ignoreCase = false;
	bool   multiline = false;
	bool   unicode = false;
	bool   sticky = false;

	if (info.Length() > 1) {
		if (info[1]->IsString()) {
			Local<String> t(info[1]->ToString());
			buffer.resize(t->Utf8Length() + 1);
			t->WriteUtf8(&buffer[0]);
			size = buffer.size() - 1;
			data = &buffer[0];
		} else if (node::Buffer::HasInstance(info[1])) {
			size = node::Buffer::Length(info[1]);
			data = node::Buffer::Data(info[1]);
		}
		for (size_t i = 0; i < size; ++i) {
			switch (data[i]) {
				case 'g':
					global = true;
					break;
				case 'i':
					ignoreCase = true;
					break;
				case 'm':
					multiline = true;
					break;
				case 'u':
					unicode = true;
					break;
				case 'y':
					sticky = true;
					break;
			}
		}
		size = 0;
	}

	bool needConversion = true;

	if (node::Buffer::HasInstance(info[0])) {
		size = node::Buffer::Length(info[0]);
		data = node::Buffer::Data(info[0]);
		source = escapeRegExp(data, size);
	} else if (info[0]->IsRegExp()) {
		const RegExp* re = RegExp::Cast(*info[0]);

		Local<String> t(re->GetSource());
		buffer.resize(t->Utf8Length() + 1);
		t->WriteUtf8(&buffer[0]);
		size = buffer.size() - 1;
		data = &buffer[0];
		source = escapeRegExp(data, size);

		RegExp::Flags flags = re->GetFlags();
		global     = bool(flags & RegExp::kGlobal);
		ignoreCase = bool(flags & RegExp::kIgnoreCase);
		multiline  = bool(flags & RegExp::kMultiline);
		unicode    = bool(flags & RegExp::kUnicode);
		sticky     = bool(flags & RegExp::kSticky);
	} else if (info[0]->IsObject() && !info[0]->IsString()) {
		v8::Local<v8::Object> object = info[0].As<Object>();
		if (WrappedRE2::HasInstance(object)) {
			WrappedRE2* re2 = Nan::ObjectWrap::Unwrap<WrappedRE2>(object);
			if (re2->useSet) {
				useSet = true;
				sources = re2->sources;
				internalSources.resize(0);
				for (const unique_ptr<RE2>& regex: re2->regexps) {
					internalSources.push_back(regex->pattern());
				}
				if (sources.size() != internalSources.size()) {
					return Nan::ThrowError("Inconsistency in RE2 constructor : sources and internalSources have different sizes (with RE2 argument).");
				}
			} else {
				const string& pattern = re2->regexp.pattern();
				size = pattern.size();
				buffer.resize(size);
				data = &buffer[0];
				memcpy(data, pattern.data(), size);
				needConversion = false;

				source     = re2->source;
			}

			global     = re2->global;
			ignoreCase = re2->ignoreCase;
			multiline  = re2->multiline;
			unicode    = true;
			sticky     = re2->sticky;
		} else if (isIterable(object)) {
			useSet = true;
			for (Local<Value> element: object) {
				if (node::Buffer::HasInstance(element)) {
					size = node::Buffer::Length(element);
					data = node::Buffer::Data(element);
					sources.push_back(escapeRegExp(data, size));
					internalSources.push_back(translateRegExp(sources.back(), buffer) ? string(buffer.begin(), buffer.end() - 1) : sources.back());
				} else if (element->IsRegExp()) {
					const RegExp* re = RegExp::Cast(*element);

					Local<String> t(re->GetSource());
					buffer.resize(t->Utf8Length() + 1);
					t->WriteUtf8(&buffer[0]);
					size = buffer.size() - 1;
					data = &buffer[0];
					sources.push_back(escapeRegExp(data, size));
					internalSources.push_back(translateRegExp(sources.back(), buffer) ? string(buffer.begin(), buffer.end() - 1) : sources.back());
				} else if (element->IsObject() && !element->IsString()) {
					auto objectElement = element.As<Object>();
					if (WrappedRE2::HasInstance(objectElement)) {
						WrappedRE2* re2 = Nan::ObjectWrap::Unwrap<WrappedRE2>(object);
						sources.push_back(re2->source);
						internalSources.push_back(re2->regexp.pattern());
					} else {
						return Nan::ThrowTypeError("Expected string, Buffer, RegExp, RE2, or non-empty iterable thereof as the 1st argument.");
					}
				} else if (element->IsString()) {
					Local<String> t(element->ToString());
					buffer.resize(t->Utf8Length() + 1);
					t->WriteUtf8(&buffer[0]);
					size = buffer.size() - 1;
					data = &buffer[0];
					sources.push_back(escapeRegExp(data, size));
					internalSources.push_back(translateRegExp(sources.back(), buffer) ? string(buffer.begin(), buffer.end() - 1) : sources.back());
				} else {
					return Nan::ThrowTypeError("Expected string, Buffer, RegExp, RE2, or non-empty iterable thereof as the 1st argument.");
				}
			}
			if (ESIterator::failed()) {
				return;
			}
			if (sources.size() != internalSources.size()) {
				return Nan::ThrowError("Inconsistency in RE2 constructor : sources and internalSources have different sizes (with iterable argument).");
			}
		}
	} else if (info[0]->IsString()) {
		Local<String> t(info[0]->ToString());
		buffer.resize(t->Utf8Length() + 1);
		t->WriteUtf8(&buffer[0]);
		size = buffer.size() - 1;
		data = &buffer[0];
		source = escapeRegExp(data, size);
	}

	if (useSet ? sources.empty() : !data) {
		return Nan::ThrowTypeError("Expected string, Buffer, RegExp, RE2, or non-empty iterable thereof as the 1st argument.");
	}

	if (!unicode) {
		switch(unicodeWarningLevel) {
			case THROW:
				return Nan::ThrowSyntaxError(depricationMessage);
			case WARN:
				printDeprecationWarning(depricationMessage);
				break;
			case WARN_ONCE:
				if (!alreadyWarnedAboutUnicode) {
					printDeprecationWarning(depricationMessage);
					alreadyWarnedAboutUnicode = true;
				}
				break;
			default:
				break;
		}
	}

	if (useSet) {
		source.resize(0);
		for (string pattern: sources) {
			source += pattern;
			source.push_back('|');
		}
		source.pop_back();
		buffer.resize(0);
		for (string pattern: internalSources) {
			buffer.insert(buffer.end(), pattern.begin(), pattern.end());
			buffer.push_back('|');
		}
		size = buffer.size() - 1;
		data = &buffer[0];
	} else if (needConversion && translateRegExp(data, size, buffer)) {
		size = buffer.size() - 1;
		data = &buffer[0];
	}

	// create and return an object

	RE2::Options options;
	options.set_case_sensitive(!ignoreCase);
	options.set_one_line(!multiline);
	options.set_log_errors(false); // inappropriate when embedding

	unique_ptr<WrappedRE2> re2(new WrappedRE2(useSet, StringPiece(data, size), options, source, global, ignoreCase, multiline, sticky));
	if (useSet) {
		re2->sources = move(sources);
		for (string source: internalSources) {
			if (!isSafeToUseInSet(source)) {
				string prefixedError;
				prefixedError.resize(50 + sizeof(re2->regexps.size()) * 3);
				prefixedError.resize(snprintf(
					&prefixedError[0], prefixedError.size(),
					"[%lu] operators ^ and $ are not safe to use in a set",
					re2->regexps.size()));

				return Nan::ThrowSyntaxError(Nan::New(prefixedError).ToLocalChecked());
			}
			unique_ptr<RE2> regex(new RE2(StringPiece(source.data(), source.size()), options));
			if (!regex->ok()) {
				string error = regex->error();
				string prefixedError;
				prefixedError.resize(4 + sizeof(re2->regexps.size()) * 3 + error.size());
				prefixedError.resize(snprintf(
					&prefixedError[0], prefixedError.size(),
					"[%lu] %s",
					re2->regexps.size(), error.data()));

				return Nan::ThrowSyntaxError(Nan::New(prefixedError).ToLocalChecked());
			}
			if (!ensureUniqueNamedGroups(regex->CapturingGroupNames())) {
				string prefixedError;
				prefixedError.resize(32 + sizeof(re2->regexps.size()) * 3);
				prefixedError.resize(snprintf(
					&prefixedError[0], prefixedError.size(),
					"[%lu] duplicate capture group name",
					re2->regexps.size()));

				return Nan::ThrowSyntaxError(Nan::New(prefixedError).ToLocalChecked());
			}
			re2->regexps.push_back(move(regex));
			if (re2->set.Add(StringPiece(source.data(), source.size()), nullptr) == -1) {
				return Nan::ThrowError("Inconsistency in RE2 constructor : regex looks valid but is rejected by set.");
			}
		}
		if (!re2->regexp.ok()) {
			return Nan::ThrowError("Inconsistency in RE2 constructor : individual regexes look valid but piped regex doesn't.");
		}
		if (!re2->set.Compile()) {
			return Nan::ThrowError("RE2::Set ran out of memory.");
		}
	} else {
		if (!re2->regexp.ok()) {
			return Nan::ThrowSyntaxError(re2->regexp.error().data());
		}
		if (!ensureUniqueNamedGroups(re2->regexp.CapturingGroupNames())) {
			return Nan::ThrowSyntaxError("duplicate capture group name");
		}
	}
	re2->Wrap(info.This());
	re2.release();

	info.GetReturnValue().Set(info.This());
}
