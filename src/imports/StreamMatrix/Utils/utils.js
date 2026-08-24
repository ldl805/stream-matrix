// Helpers
function currentModel() {
    return layoutsCollectionModel.get(stackLayout.currentIndex);
}

function currentLayout() {
    return layoutRepeater.itemAt(stackLayout.currentIndex);
}

function tokenizeArgs(str) {
    if (!str || typeof str !== "string") return [];
    var tokens = [];
    var current = "";
    var inQuotes = false;
    var quoteChar = "";

    for (var i = 0; i < str.length; i++) {
        var c = str[i];
        if ((c === '"' || c === "'") && !inQuotes) {
            inQuotes = true;
            quoteChar = c;
        } else if (c === quoteChar && inQuotes) {
            inQuotes = false;
            quoteChar = "";
        } else if (/\s/.test(c) && !inQuotes) {
            if (current.length > 0) {
                tokens.push(current);
                current = "";
            }
        } else {
            current += c;
        }
    }
    if (current.length > 0) {
        tokens.push(current);
    }
    return tokens;
}

function parseOptions(str) {
    var obj = {};
    var tokens = tokenizeArgs(str);

    for (var i = 0; i < tokens.length; i++) {
        var token = tokens[i];
        if (token.startsWith("-")) {
            var key = token.slice(1);
            if (i + 1 < tokens.length && !tokens[i + 1].startsWith("-")) {
                obj[key] = tokens[i + 1];
                i++;
            } else {
                obj[key] = "true";
            }
        }
    }

    return obj;
}

function stringifyOptions(obj) {
    var str = "";

    for (var key in obj) {
        if (obj[key] === undefined || obj[key] === null) continue;
        var val = String(obj[key]);
        if (val === "true") {
            str += "-%1 ".arg(key);
        } else if (val.indexOf(" ") >= 0) {
            str += "-%1 \"%2\" ".arg(key).arg(val);
        } else {
            str += "-%1 %2 ".arg(key).arg(val);
        }
    }

    return str.trim();
}

function ifLeftToRight(leftToRight, rightToLeft) {
    if (rightToLeft === undefined) {
        leftToRight = false;
    }

    return (Qt.application.layoutDirection == Qt.LeftToRight) ? leftToRight : rightToLeft;
}

function ifRightToLeft(rightToLeft, leftToRight) {
    if (leftToRight === undefined) {
        leftToRight = false;
    }

    return (Qt.application.layoutDirection == Qt.RightToLeft) ? rightToLeft : leftToRight;
}

// Objects
// NOTE: Shallow, not recursive "cloning"!
Object.defineProperty(Object, "assignDefault", {
                          enumerable: false,
                          configurable: true,
                          writable: true,
                          value: function(target) {
                              if (!(target instanceof Object)) {
                                  throw new TypeError("Cannot convert first argument to object");
                              }

                              for (var i = 1; i < arguments.length; ++i) {
                                  var source = arguments[i];

                                  if (!(source instanceof Object)) {
                                      continue;
                                  }

                                  for (var key in source) {
                                      if (target[key] === undefined) {
                                        target[key] = source[key];
                                      }
                                  }
                              }

                              return target;
                          }
                      });

// Strings
String.prototype.isEmpty = function() {
    return this.length === 0 || !this.trim();
};

String.prototype.leadingChars = function(fieldWidth, fillChar) {
    var chars = "";

    if (fillChar === undefined) {
        fillChar = " ";
    }

    for (var i = 0; i < fieldWidth - this.length; ++i) {
        chars += fillChar;
    }

    return chars + this;
}

// Math
function isNumeric(n) {
  return !isNaN(parseFloat(n)) && isFinite(n);
}

Number.prototype.clamp = function(min, max) {
    return Math.min(Math.max(this, min), max);
}

Number.prototype.inRange = function(min, max) {
    return (this >= min && this <= max) ? true : false;
}

// Debug
function log_info(message) {
    console.log(message);
}

function log_error(message) {
    console.log("ERROR: " + message);
}
