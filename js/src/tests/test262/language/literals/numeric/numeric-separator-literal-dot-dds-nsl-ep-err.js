// |reftest| skip error:SyntaxError -- numeric-separator-literal is not supported
// Copyright (C) 2017 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: prod-NumericLiteralSeparator
description: NumericLiteralSeparator may not appear adjacent to ExponentPart
info: |
  NumericLiteralSeparator ::
    _

  DecimalLiteral ::
    . DecimalDigits ExponentPart_opt

  DecimalDigits ::
    ...
    DecimalDigits NumericLiteralSeparator DecimalDigit

  ExponentIndicator :: one of
    e E

negative:
  phase: parse
  type: SyntaxError

features: [numeric-separator-literal]
---*/

throw "Test262: This statement should not be evaluated.";

.0_e1
