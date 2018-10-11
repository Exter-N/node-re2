"use strict";


var unit = require("heya-unit");
var RE2  = require("../re2");


// tests

unit.add(module, [

	// These tests are copied from MDN:
	// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/RegExp/exec

	function test_execBasic(t) {
		"use strict";

		var re = new RE2("quick\\s(brown).+?(jumps)", "ig");

		eval(t.TEST("re.source === 'quick\\\\s(brown).+?(jumps)'"));
		eval(t.TEST("re.ignoreCase"));
		eval(t.TEST("re.global"));
		eval(t.TEST("!re.multiline"));

		var result = re.exec("The Quick Brown Fox Jumps Over The Lazy Dog");

		eval(t.TEST("t.unify(result, ['Quick Brown Fox Jumps', 'Brown', 'Jumps'])"));
		eval(t.TEST("result.index === 4"));
		eval(t.TEST("result.input === 'The Quick Brown Fox Jumps Over The Lazy Dog'"));
		eval(t.TEST("re.lastIndex === 25"));
	},
	function test_execSucc(t) {
		"use strict";

		var str = "abbcdefabh";

		var re = new RE2("ab*", "g");
		var result = re.exec(str);

		eval(t.TEST("!!result"));
		eval(t.TEST("result[0] === 'abb'"));
		eval(t.TEST("result.index === 0"));
		eval(t.TEST("re.lastIndex === 3"));

		result = re.exec(str);

		eval(t.TEST("!!result"));
		eval(t.TEST("result[0] === 'ab'"));
		eval(t.TEST("result.index === 7"));
		eval(t.TEST("re.lastIndex === 9"));

		result = re.exec(str);

		eval(t.TEST("!result"));
	},
	function test_execSimple(t) {
		"use strict";

		var re = new RE2("(hello \\S+)");
		var result = re.exec("This is a hello world!");

		eval(t.TEST("result[1] === 'hello world!'"));
	},
	function test_execFail(t) {
		"use strict";

		var re = new RE2("(a+)?(b+)?");
		var result = re.exec("aaabb");

		eval(t.TEST("result[1] === 'aaa'"));
		eval(t.TEST("result[2] === 'bb'"));

		result = re.exec("aaacbb");

		eval(t.TEST("result[1] === 'aaa'"));
		eval(t.TEST("result[2] === undefined"));
	},
	function test_execAnchoredToBeginning(t) {
		"use strict";

		var re = RE2('^hello', 'g');

		var result = re.exec("hellohello");

		eval(t.TEST("t.unify(result, ['hello'])"));
		eval(t.TEST("result.index === 0"));
		eval(t.TEST("re.lastIndex === 5"));

		eval(t.TEST("re.exec('hellohello') === null"));
	},
	function test_execInvalid(t) {
		"use strict";

		var re = RE2('');

		try {
			re.exec({ toString() { throw "corner"; } });
			t.test(false); // shouldn't be here
		} catch(e) {
			eval(t.TEST("e === 'corner'"));
		}
	},
	function test_execAnchor1(t) {
		"use strict";

		var re = new RE2("b|^a", "g");

		var result = re.exec("aabc");
		eval(t.TEST("!!result"));
		eval(t.TEST("result.index === 0"));
		eval(t.TEST("re.lastIndex === 1"));

		result = re.exec("aabc");
		eval(t.TEST("!!result"));
		eval(t.TEST("result.index === 2"));
		eval(t.TEST("re.lastIndex === 3"));

		result = re.exec("aabc");
		eval(t.TEST("!result"));
	},
	function test_execAnchor2(t) {
		"use strict";

		var re = new RE2("(?:^a)", "g");

		var result = re.exec("aabc");
		eval(t.TEST("!!result"));
		eval(t.TEST("result.index === 0"));
		eval(t.TEST("re.lastIndex === 1"));

		result = re.exec("aabc");
		eval(t.TEST("!result"));
	},

	// Unicode tests

	function test_execUnicode(t) {
		"use strict";

		var re = new RE2("охотник\\s(желает).+?(где)", "ig");

		eval(t.TEST("re.source === 'охотник\\\\s(желает).+?(где)'"));
		eval(t.TEST("re.ignoreCase"));
		eval(t.TEST("re.global"));
		eval(t.TEST("!re.multiline"));

		var result = re.exec("Каждый Охотник Желает Знать Где Сидит Фазан");

		eval(t.TEST("t.unify(result, ['Охотник Желает Знать Где', 'Желает', 'Где'])"));
		eval(t.TEST("result.index === 7"));
		eval(t.TEST("result.input === 'Каждый Охотник Желает Знать Где Сидит Фазан'"));
		eval(t.TEST("re.lastIndex === 31"));

		eval(t.TEST("result.input.substr(result.index) === 'Охотник Желает Знать Где Сидит Фазан'"));
		eval(t.TEST("result.input.substr(re.lastIndex) === ' Сидит Фазан'"));
	},
	function test_execUnicodeSubsequent(t) {
		"use strict";

		var str = "аббвгдеабё";

		var re = new RE2("аб*", "g");
		var result = re.exec(str);

		eval(t.TEST("!!result"));
		eval(t.TEST("result[0] === 'абб'"));
		eval(t.TEST("result.index === 0"));
		eval(t.TEST("re.lastIndex === 3"));

		result = re.exec(str);

		eval(t.TEST("!!result"));
		eval(t.TEST("result[0] === 'аб'"));
		eval(t.TEST("result.index === 7"));
		eval(t.TEST("re.lastIndex === 9"));

		result = re.exec(str);

		eval(t.TEST("!result"));
	},
	function test_execUnicodeSupplementary(t) {
		"use strict";

		var re = new RE2("\\u{1F603}", "g");

		eval(t.TEST("re.source === '\\\\u{1F603}'"));
		eval(t.TEST("re.internalSource === '\\\\x{1F603}'"));
		eval(t.TEST("!re.ignoreCase"));
		eval(t.TEST("re.global"));
		eval(t.TEST("!re.multiline"));

		var result = re.exec("\u{1F603}"); // 1F603 is the SMILING FACE WITH OPEN MOUTH emoji

		eval(t.TEST("t.unify(result, ['\\u{1F603}'])"));
		eval(t.TEST("result.index === 0"));
		eval(t.TEST("result.input === '\\u{1F603}'"));
		eval(t.TEST("re.lastIndex === 2"));

		var re2 = new RE2(".", "g");

		eval(t.TEST("re2.source === '.'"));
		eval(t.TEST("!re2.ignoreCase"));
		eval(t.TEST("re2.global"));
		eval(t.TEST("!re2.multiline"));

		var result2 = re2.exec("\u{1F603}");

		eval(t.TEST("t.unify(result2, ['\\u{1F603}'])"));
		eval(t.TEST("result2.index === 0"));
		eval(t.TEST("result2.input === '\\u{1F603}'"));
		eval(t.TEST("re2.lastIndex === 2"));

		var re3 = new RE2("[\u{1F603}-\u{1F605}]", "g");

		eval(t.TEST("re3.source === '[\u{1F603}-\u{1F605}]'"));
		eval(t.TEST("!re3.ignoreCase"));
		eval(t.TEST("re3.global"));
		eval(t.TEST("!re3.multiline"));

		var result3 = re3.exec("\u{1F604}");

		eval(t.TEST("t.unify(result3, ['\\u{1F604}'])"));
		eval(t.TEST("result3.index === 0"));
		eval(t.TEST("result3.input === '\\u{1F604}'"));
		eval(t.TEST("re3.lastIndex === 2"));
	},

	// Buffer tests

	function test_execBuffer(t) {
		"use strict";

		var re  = new RE2("охотник\\s(желает).+?(где)", "ig");
		var buf = new Buffer("Каждый Охотник Желает Знать Где Сидит Фазан");

		var result = re.exec(buf);

		eval(t.TEST("result.length === 3"));
		eval(t.TEST("result[0] instanceof Buffer"));
		eval(t.TEST("result[1] instanceof Buffer"));
		eval(t.TEST("result[2] instanceof Buffer"));

		eval(t.TEST("result[0].toString() === 'Охотник Желает Знать Где'"));
		eval(t.TEST("result[1].toString() === 'Желает'"));
		eval(t.TEST("result[2].toString() === 'Где'"));

		eval(t.TEST("result.index === 13"));
		eval(t.TEST("result.input instanceof Buffer"));
		eval(t.TEST("result.input.toString() === 'Каждый Охотник Желает Знать Где Сидит Фазан'"));
		eval(t.TEST("re.lastIndex === 58"));

		eval(t.TEST("result.input.toString('utf8', result.index) === 'Охотник Желает Знать Где Сидит Фазан'"));
		eval(t.TEST("result.input.toString('utf8', re.lastIndex) === ' Сидит Фазан'"));
	},

	// Sticky tests

	function test_execSticky(t) {
		"use strict";

		var re = new RE2("\\s+", "y");

		eval(t.TEST("re.exec('Hello world, how are you?') === null"));

		re.lastIndex = 5;

		var result = re.exec("Hello world, how are you?");

		eval(t.TEST("t.unify(result, [' '])"));
		eval(t.TEST("result.index === 5"));
		eval(t.TEST("re.lastIndex === 6"));

		var re2 = new RE2("\\s+", "gy");

		eval(t.TEST("re2.exec('Hello world, how are you?') === null"));

		re2.lastIndex = 5;

		var result2 = re2.exec("Hello world, how are you?");

		eval(t.TEST("t.unify(result2, [' '])"));
		eval(t.TEST("result2.index === 5"));
		eval(t.TEST("re2.lastIndex === 6"));
	},

	// RE2-Set tests

	function test_execSetRouterUseCase(t) {
		"use strict";

		var re = new RE2([
			`/about-us/?`,
			`/blog/(?<slug>[0-9a-z-]+)/?`,
			`/blog/?`,
			`/`,
		], 'y');

		var matches = [ ];
		var numericGroups = [ ];
		var namedGroups = [ ];
		var indices = [ ];
		var patternIndices = [ ];
		var lastIndices = [ ];

		for (let uri of [
			'/',
			'/about-us',
			'/blog',
			'/blog/hello-world/',
			'/nonexistent-page'
		]) {
			re.lastIndex = 0;
			var match = re.exec(uri);
			matches.push(match[0]);
			numericGroups.push(match.slice(1));
			namedGroups.push(match.groups);
			indices.push(match.index);
			patternIndices.push(match.patternIndex);
			lastIndices.push(re.lastIndex);
		}
		
		re.lastIndex = 0;
		eval(t.TEST(`re.exec('malformed-uri') === null`));
		
		var U; // abbreviation of undefined
		eval(t.TEST(`t.unify(matches, ['/', '/about-us', '/blog', '/blog/hello-world/', '/'])`));
		eval(t.TEST(`t.unify(numericGroups, [[], [], [], ['hello-world'], []])`));
		eval(t.TEST(`t.unify(namedGroups, [U, U, U, {slug: 'hello-world'}, U])`));
		eval(t.TEST(`t.unify(indices, [0, 0, 0, 0, 0])`));
		eval(t.TEST(`t.unify(patternIndices, [3, 0, 2, 1, 3])`));
		eval(t.TEST(`t.unify(lastIndices, [1, 9, 5, 18, 1])`));
	},
	function test_execSetWrongRouterUseCase(t) {
		"use strict";

		var re = new RE2([
			`/`,
			`/about-us/?`,
			`/blog/?`,
			`/blog/(?<slug>[0-9a-z-]+)/?`,
		], 'y');

		var matches = [ ];
		var numericGroups = [ ];
		var namedGroups = [ ];
		var indices = [ ];
		var patternIndices = [ ];
		var lastIndices = [ ];

		for (let uri of [
			'/',
			'/about-us',
			'/blog',
			'/blog/hello-world/',
			'/nonexistent-page'
		]) {
			re.lastIndex = 0;
			var match = re.exec(uri);
			matches.push(match[0]);
			numericGroups.push(match.slice(1));
			namedGroups.push(match.groups);
			indices.push(match.index);
			patternIndices.push(match.patternIndex);
			lastIndices.push(re.lastIndex);
		}

		re.lastIndex = 0;
		eval(t.TEST(`re.exec('malformed-uri') === null`));

		var U; // abbreviation of undefined
		eval(t.TEST(`t.unify(matches, ['/', '/', '/', '/', '/'])`));
		eval(t.TEST(`t.unify(numericGroups, [[], [], [], [], []])`));
		eval(t.TEST(`t.unify(namedGroups, [U, U, U, U, U])`));
		eval(t.TEST(`t.unify(indices, [0, 0, 0, 0, 0])`));
		eval(t.TEST(`t.unify(patternIndices, [0, 0, 0, 0, 0])`));
		eval(t.TEST(`t.unify(lastIndices, [1, 1, 1, 1, 1])`));
	},
	function test_execSetLexerUseCase(t) {
		"use strict";

		var re = new RE2([
			`\\s+`,
			`\\{`,
			`\\}`,
			`\\[`,
			`\\]`,
			`,`,
			`:`,
			`"(?<value>(?:[^"\\\\]+|\\.)*)"`,
			`(?<sign>-)?(?<integer>\\d+)(?:\\.(?<fractional>\\d+))?(?:[Ee](?<exponent>[+-]?\\d+))?`,
			`null`,
			`true`,
			`false`,
		], 'gy');

		var input = `{ "name": "John Doe", "profession": "captain", "age": 42 }`;

		var matches = [ ];
		var numericGroups = [ ];
		var namedGroups = [ ];
		var indices = [ ];
		var patternIndices = [ ];
		var lastIndices = [ ];

		var match;
		while ((match = re.exec(input)) !== null) {
			matches.push(match[0]);
			numericGroups.push(match.slice(1));
			namedGroups.push(match.groups);
			indices.push(match.index);
			patternIndices.push(match.patternIndex);
			lastIndices.push(re.lastIndex);
		}

		var U; // abbreviation of undefined
		eval(t.TEST(`t.unify(matches, ['{', ' ', '"name"', ':', ' ', '"John Doe"', ',', ' ', '"profession"', \
			':', ' ', '"captain"', ',', ' ', '"age"', ':', ' ', '42', ' ', '}'])`));
		eval(t.TEST(`t.unify(numericGroups, [[], [], ['name'], [], [], ['John Doe'], [], [], ['profession'], \
			[], [], ['captain'], [], [], ['age'], [], [], [, '42'], [], []])`));
		eval(t.TEST(`t.unify(namedGroups, [U, U, {value: 'name'}, U, U, {value: 'John Doe'}, U, U, {value: 'profession'}, \
			U, U, {value: 'captain'}, U, U, {value: 'age'}, U, U, {sign: U, integer: '42', fractional: U, exponent: U}, U, U])`));
		eval(t.TEST(`t.unify(indices, [0, 1, 2, 8, 9, 10, 20, 21, 22, \
			34, 35, 36, 45, 46, 47, 52, 53, 54, 56, 57])`));
		eval(t.TEST(`t.unify(patternIndices, [1, 0, 7, 6, 0, 7, 5, 0, 7, \
			6, 0, 7, 5, 0, 7, 6, 0, 8, 0, 2])`));
		eval(t.TEST(`t.unify(lastIndices, [1, 2, 8, 9, 10, 20, 21, 22, 34, \
			35, 36, 45, 46, 47, 52, 53, 54, 56, 57, 58])`));
	}
]);
