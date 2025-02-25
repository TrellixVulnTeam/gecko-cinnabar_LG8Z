// |reftest| skip error:SyntaxError -- class-fields-public is not supported
// This file was procedurally generated from the following sources:
// - src/class-elements/init-err-contains-arguments.case
// - src/class-elements/initializer-error/cls-expr-fields-arrow-fnc.template
/*---
description: Syntax error if `arguments` used in class field (arrow function expression)
esid: sec-class-definitions-static-semantics-early-errors
features: [class, class-fields-public, arrow-function]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Static Semantics: Early Errors

      FieldDefinition:
        PropertyNameInitializeropt

      - It is a Syntax Error if ContainsArguments of Initializer is true.

    Static Semantics: ContainsArguments
      IdentifierReference : Identifier

      1. If the StringValue of Identifier is "arguments", return true.
      ...
      For all other grammatical productions, recurse on all nonterminals. If any piece returns true, then return true. Otherwise return false.

---*/


throw "Test262: This statement should not be evaluated.";

var C = class {
  x = () => arguments;
}
