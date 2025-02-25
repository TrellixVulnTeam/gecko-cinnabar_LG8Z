// |reftest| skip error:SyntaxError -- class-static-methods-private is not supported
// This file was procedurally generated from the following sources:
// - src/async-generators/await-as-label-identifier-escaped.case
// - src/async-generators/syntax/async-class-expr-static-private-method.template
/*---
description: await is a reserved keyword within generator function bodies and may not be used as a label identifier. (Static async generator private method as a ClassExpression element)
esid: prod-AsyncGeneratorMethod
features: [async-iteration, class-static-methods-private]
flags: [generated]
negative:
  phase: parse
  type: SyntaxError
info: |
    ClassElement :
      static PrivateMethodDefinition

    MethodDefinition :
      AsyncGeneratorMethod

    Async Generator Function Definitions

    AsyncGeneratorMethod :
      async [no LineTerminator here] * # PropertyName ( UniqueFormalParameters ) { AsyncGeneratorBody }


    LabelIdentifier : Identifier

    It is a Syntax Error if this production has a [Await] parameter and
    StringValue of Identifier is "await".

---*/
throw "Test262: This statement should not be evaluated.";


var C = class { static async *#gen() {
    \u0061wait: ;
}};
