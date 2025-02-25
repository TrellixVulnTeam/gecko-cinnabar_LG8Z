/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flex item outline has a correct layout. This outline is built using css
// grid under the hood to position everything. So we want to check that the template for
// this grid has been correctly generated depending on the item that is currently
// selected.

const TEST_URI = URL_ROOT + "doc_flexbox_simple.html";

const TEST_DATA = [{
  selector: ".shrinking .item",
  expectedGridTemplate: "[final-start basis-start] 300fr [final-end delta-start] " +
                        "200fr [basis-end delta-end]",
}, {
  selector: ".shrinking.is-clamped .item",
  expectedGridTemplate: "[final-start basis-start] 300fr [delta-start] " +
                        "50fr [final-end min] 150fr [basis-end delta-end]",
}, {
  selector: ".growing .item",
  expectedGridTemplate: "[final-start basis-start] 200fr [basis-end delta-start] " +
                        "100fr [final-end delta-end]",
}, {
  selector: ".growing.is-clamped .item",
  expectedGridTemplate: "[final-start basis-start] 200fr [basis-end delta-start] " +
                        "50fr [final-end max] 50fr [delta-end]",
}];

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  for (const {selector, expectedGridTemplate} of TEST_DATA) {
    info(`Checking the grid template for the flex item outline for ${selector}`);

    const flexOutline = await selectNodeAndGetFlexOutline(selector, inspector, doc);

    is(flexOutline.style.gridTemplateColumns, expectedGridTemplate,
       "Grid template is correct");
  }
});

async function selectNodeAndGetFlexOutline(selector, inspector, doc) {
  const onFlexItemOutlineRendered = waitForDOM(doc, ".flex-outline");
  await selectNode(selector, inspector);
  const [flexOutlineContainer] = await onFlexItemOutlineRendered;
  return flexOutlineContainer;
}
