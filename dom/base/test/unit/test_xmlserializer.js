/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function xmlEncode(aFile, aFlags, aCharset) {
    if(aFlags == undefined) aFlags = 0;
    if(aCharset == undefined) aCharset = "UTF-8";

    return do_parse_document(aFile, "text/xml").then(doc => {
      var encoder = SpecialPowers.Cu.createDocumentEncoder("text/xml");
      encoder.setCharset(aCharset);
      encoder.init(doc, "text/xml", aFlags);
      return encoder.encodeToString();
    });
}

function run_test()
{
    var result, expected;
    const de = Ci.nsIDocumentEncoder;

    xmlEncode("1_original.xml", de.OutputLFLineBreak).then(result => {
      expected = loadContentFile("1_result.xml");
      Assert.equal(expected, result);
    });

    xmlEncode("2_original.xml", de.OutputLFLineBreak).then(result => {
      expected = loadContentFile("2_result_1.xml");
      Assert.equal(expected, result);
    });

    xmlEncode("2_original.xml", de.OutputCRLineBreak).then(result => {
      expected = expected.replace(/\n/g, "\r");
      Assert.equal(expected, result);
    });

    xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputCRLineBreak).then(result => {
      expected = expected.replace(/\r/mg, "\r\n");
      Assert.equal(expected, result);
    });

    xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputFormatted).then(result => {
      expected = loadContentFile("2_result_2.xml");
      Assert.equal(expected, result);
    });

    xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap).then(result => {
      expected = loadContentFile("2_result_3.xml");
      Assert.equal(expected, result);
    });

    xmlEncode("2_original.xml", de.OutputLFLineBreak | de.OutputWrap).then(result => {
      expected = loadContentFile("2_result_4.xml");
      Assert.equal(expected, result);
    });

    xmlEncode("3_original.xml", de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap).then(result => {
      expected = loadContentFile("3_result.xml");
      Assert.equal(expected, result);
    });

    xmlEncode("3_original.xml", de.OutputLFLineBreak | de.OutputWrap).then(result => {
      expected = loadContentFile("3_result_2.xml");
      Assert.equal(expected, result);
    });

    // tests on namespaces
    do_parse_document("4_original.xml", "text/xml").then(run_namespace_tests);
}

function run_namespace_tests(doc) {
    const de = Ci.nsIDocumentEncoder;
    var encoder = SpecialPowers.Cu.createDocumentEncoder("text/xml");
    encoder.setCharset("UTF-8");
    encoder.init(doc, "text/xml", de.OutputLFLineBreak);

    result = encoder.encodeToString();
    expected = loadContentFile("4_result_1.xml");
    Assert.equal(expected, result);

    encoder.setNode(doc.documentElement.childNodes[9]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_2.xml");
    Assert.equal(expected, result);

    encoder.setNode(doc.documentElement.childNodes[7].childNodes[1]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_3.xml");
    Assert.equal(expected, result);

    encoder.setNode(doc.documentElement.childNodes[11].childNodes[1]);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_4.xml");
    Assert.equal(expected, result);

    encoder.setCharset("UTF-8");
    // it doesn't support this flags
    encoder.init(doc, "text/xml",  de.OutputLFLineBreak | de.OutputFormatted | de.OutputWrap);
    encoder.setWrapColumn(40);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_5.xml");
    Assert.equal(expected, result);

    encoder.init(doc, "text/xml",  de.OutputLFLineBreak | de.OutputWrap);
    encoder.setWrapColumn(40);
    result = encoder.encodeToString();
    expected = loadContentFile("4_result_6.xml");
    Assert.equal(expected, result);
}
