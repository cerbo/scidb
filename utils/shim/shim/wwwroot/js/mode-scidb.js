ace.define("ace/mode/scidb_highlight_rules",["require","exports","module","ace/lib/oop","ace/mode/text_highlight_rules"], function(require, exports, module) {
"use strict";

var oop = require("../lib/oop");
var TextHighlightRules = require("./text_highlight_rules").TextHighlightRules;

var aflHighlightRules = function() {

    var keywords = (
"aggregate|apply|between|build|cancel|cast|cross_join|cumulate|filter|gemm|gesvd|glm|index_lookup|insert|join|list|load_library|lookup|merge|project|quantile|rank|redimension|remove|rename|reshape|rng_uniform|save|scan|show|slice|sort|spgemm|store|subarray|substitute|transpose|tsvd|uniq|unfold|unpack|variable_window|window|upload|split|parse|op_count|op_sum|op_avg|op_stdev|op_min|op_max|op_range|op_distinct_count|op_now"
    );

    var builtinConstants = (
        "true|false|null|TRUE|FALSE|NULL|not|as|and|or|double|int8|int16|int32|int64|string|bool|date|float|uint8|uint16|uint32|uint64"
    );

    var builtinFunctions = (
"approxdc|avg|count|first_value|last_value|mad|max|median|min|prod|stdev|sum|top_five|var|binomcdf|chi2cdf|gcd_hf|gcd_slc|gcd_vif|geomcdf|hygecdf|hygepmf|hygequant|ibinomcdf|ichi2cdf|igeomcdf|ihygecdf|inegbinomcdf|inormcdf|itcdf|negbinomcdf|normcdf|tcdf|cos|sin|tan"
    );

    var keywordMapper = this.createKeywordMapper({
        "support.function": builtinFunctions,
        "keyword": keywords,
        "constant.language": builtinConstants
    }, "identifier", true);

    this.$rules = {
        "start" : [ {
            token : "comment",
            regex : "--.*$"
        }, {
            token : "string",           // ' string
            regex : "'.*?'"
        }, {
            token : "constant.numeric", // float
            regex : "[+-]?\\d+(?:(?:\\.\\d*)?(?:[eE][+-]?\\d+)?)?\\b"
        }, {
            token : keywordMapper,
            regex : "[a-zA-Z_$][a-zA-Z0-9_$]*\\b"
        }, {
            token : "keyword.operator",
            regex : "\\+|\\-|\\/|\\/\\/|%|\\[|\\]|<@|&|\\^|~|<|>|<=|=>|=|<>|:|\\*"
        }, {
            token : "paren.lparen",
            regex : "[\\(]"
        }, {
            token : "paren.rparen",
            regex : "[\\)]"
        }, {
            token : "text",
            regex : "\\s+"
        } ]
    };
    this.normalizeRules();
};

oop.inherits(aflHighlightRules, TextHighlightRules);

exports.aflHighlightRules = aflHighlightRules;
});

ace.define("ace/mode/scidb",["require","exports","module","ace/lib/oop","ace/mode/text","ace/mode/scidb_highlight_rules","ace/range"], function(require, exports, module) {
"use strict";

var oop = require("../lib/oop");
var TextMode = require("./text").Mode;
var aflHighlightRules = require("./scidb_highlight_rules").aflHighlightRules;
var Range = require("../range").Range;

var Mode = function() {
    this.HighlightRules = aflHighlightRules;
};
oop.inherits(Mode, TextMode);

(function() {

    this.lineCommentStart = "#";

    this.$id = "ace/mode/scidb";
}).call(Mode.prototype);

exports.Mode = Mode;

});
