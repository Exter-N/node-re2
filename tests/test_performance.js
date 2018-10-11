"use strict";

const unit = require("heya-unit");
const RE2 = require("../re2");

// tests

unit.add(module, [
	function test_setPerf(t) {
        "use strict";

		var input = `{ "name": "John Doe", "profession": "captain", "age": 42 }`;

        var tokenTypeByFirstGroup = { 1: 0, 2: 1, 3: 2, 4: 3, 5: 4, 6: 5, 7: 6, 8: 7, 10: 8, 15: 9, 16: 10, 17: 11 };

        var rePiped = new RE2(`(\\s+)|(\\{)|(\\})|(\\[)|(\\])|(,)|(:)|("(?<value>(?:[^"\\\\]+|\\.)*)")|((?<sign>-)?(?<integer>\\d+)(?:\\.(?<fractional>\\d+))?(?:[Ee](?<exponent>[+-]?\\d+))?)|(null)|(true)|(false)`, 'gy');
        function nextTokenPiped() {
            var match = rePiped.exec(input);
            if (!match) {
                return null;
            }
            var firstGroup = 1;
            while (firstGroup < match.length && match[firstGroup] === undefined) {
                ++firstGroup;
            }
            return [ tokenTypeByFirstGroup[firstGroup] ].concat(match.slice(firstGroup + 1));
        }
        
        var reSet = new RE2([
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
        
        function nextTokenSet() {
            var match = reSet.exec(input);
            if (!match) {
                return null;
            }
            return [ match.patternIndex ].concat(match.slice(1));
        }

        var resultPiped = Array(21).fill(null).map(() => nextTokenPiped());
        var resultSet = Array(21).fill(null).map(() => nextTokenSet());
        eval(t.TEST(`t.unify(resultPiped, resultSet)`));

        console.info('Timing 100k iterations of piped RE2 vs RE2-Set and String vs Buffer...');
        var timePiped = time(nextTokenPiped, 100000);
        console.info('Time using piped RE2 and String: %d sec', timePiped);
        var timeSet = time(nextTokenSet, 100000);
        console.info('Time using RE2-Set and String  : %d sec', timeSet);
        eval(t.TEST(`timeSet < timePiped`));

        input = Buffer.from(input, 'utf8');

        var timePiped = time(nextTokenPiped, 100000);
        console.info('Time using piped RE2 and Buffer: %d sec', timePiped);
        var timeSet = time(nextTokenSet, 100000);
        console.info('Time using RE2-Set and Buffer  : %d sec', timeSet);
        eval(t.TEST(`timeSet < timePiped`));
    }
]);


// utilities

function time(fn, iters) {
    var preIters = 0 | (iters / 10);
    for (var i = 0; i < preIters; ++i) {
        fn();
    }
    var hrtimeBefore = process.hrtime();
    for (var i = 0; i < iters; ++i) {
        fn();
    }
    var hrtimeAfter = process.hrtime();
    return hrtimeAfter[0] - hrtimeBefore[0] + (hrtimeAfter[1] - hrtimeBefore[1]) / 1e9;
}
