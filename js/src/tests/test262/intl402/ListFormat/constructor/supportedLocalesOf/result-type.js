// |reftest| skip-if(!Intl.hasOwnProperty('ListFormat')) -- Intl.ListFormat is not enabled unconditionally
// Copyright 2018 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-Intl.ListFormat.supportedLocalesOf
description: Verifies the type of the return value of Intl.ListFormat.supportedLocalesOf().
info: |
    Intl.ListFormat.supportedLocalesOf ( locales [, options ])
includes: [propertyHelper.js]
features: [Intl.ListFormat]
---*/

const result = Intl.ListFormat.supportedLocalesOf("en");
assert.sameValue(Array.isArray(result), true,
  "Array.isArray() should return true");
assert.sameValue(Object.getPrototypeOf(result), Array.prototype,
  "The prototype should be Array.prototype");
assert.sameValue(Object.isExtensible(result), true,
  "Object.isExtensible() should return true");

assert.notSameValue(result.length, 0);
for (let i = 0; i < result.length; ++i) {
  verifyProperty(result, String(i), {
    "writable": false,
    "enumerable": true,
    "configurable": false,
  });
}

verifyProperty(result, "length", {
  "writable": false,
  "enumerable": false,
  "configurable": false,
});

reportCompare(0, 0);
