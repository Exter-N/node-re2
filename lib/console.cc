#include "./functional.h"
#include "./util.h"

#include <string>

#include <sys/types.h>
#include <unistd.h>


using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;
using std::string;


void consoleCall(const char* methodName, Local<Value> text) {
	Local<v8::Context> context = v8::Isolate::GetCurrent()->GetCurrentContext();

	MaybeLocal<Object> maybeConsole = bind<Object>(
		Nan::Get(context->Global(), Nan::New("console").ToLocalChecked()),
		as<Object>);
	if (maybeConsole.IsEmpty()) return;

	Local<Object> console = maybeConsole.ToLocalChecked();

	MaybeLocal<Function> maybeMethod = bind<Function>(
		Nan::Get(console, Nan::New(methodName).ToLocalChecked()),
		as<Function>);
	if (maybeMethod.IsEmpty()) return;

	Local<Object> method = maybeMethod.ToLocalChecked();

	Nan::Call(method.As<Function>(), console, 1, &text);
}


void printDeprecationWarning(const char* warning) {
	string prefixedWarning;
	prefixedWarning.resize(29 + sizeof(pid_t) * 3 + strlen(warning));
	prefixedWarning.resize(snprintf(
		&prefixedWarning[0], prefixedWarning.size(),
		"(node:%d) DeprecationWarning: %s",
		getpid(), warning));

	consoleCall("error", Nan::New(prefixedWarning).ToLocalChecked());
}
