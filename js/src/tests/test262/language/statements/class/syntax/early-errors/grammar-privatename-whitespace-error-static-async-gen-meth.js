// |reftest| skip error:SyntaxError -- class-static-methods-private is not supported
// This file was procedurally generated from the following sources:
// - src/class-elements/grammar-privatename-whitespace-error-static-async-gen-meth.case
// - src/class-elements/syntax/invalid/cls-decl-elements-invalid-syntax.template
/*---
description: No space allowed between sigil and IdentifierName (Static Async Generator Method) (class declaration)
esid: prod-ClassElement
features: [async-iteration, class-static-methods-private, class]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    Updated Productions

    ClassElementName :
      PropertyName
      PrivateName

    PrivateName ::
      # IdentifierName

---*/


throw "Test262: This statement should not be evaluated.";

class C {
  static async * # m() {}
}
