/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { editableItem } = require("devtools/client/shared/inplace-editor");

const LONG_TEXT_ROTATE_LIMIT = 3;

class BoxModelEditable extends PureComponent {
  static get propTypes() {
    return {
      box: PropTypes.string.isRequired,
      direction: PropTypes.string,
      focusable: PropTypes.bool.isRequired,
      level: PropTypes.string,
      onShowBoxModelEditor: PropTypes.func.isRequired,
      property: PropTypes.string.isRequired,
      textContent: PropTypes.oneOfType([PropTypes.string, PropTypes.number]).isRequired,
    };
  }

  componentDidMount() {
    const { property, onShowBoxModelEditor } = this.props;

    editableItem({
      element: this.boxModelEditable,
    }, (element, event) => {
      onShowBoxModelEditor(element, event, property);
    });
  }

  render() {
    const {
      box,
      direction,
      focusable,
      level,
      property,
      textContent,
    } = this.props;

    const rotate = direction &&
                 (direction == "left" || direction == "right") &&
                 box !== "position" &&
                 textContent.toString().length > LONG_TEXT_ROTATE_LIMIT;

    return (
      dom.p(
        {
          className: `boxmodel-${box}
                      ${direction ? " boxmodel-" + direction : "boxmodel-" + property}
                      ${rotate ? " boxmodel-rotate" : ""}`,
        },
        dom.span(
          {
            className: "boxmodel-editable",
            "data-box": box,
            tabIndex: box === level && focusable ? 0 : -1,
            title: property,
            ref: span => {
              this.boxModelEditable = span;
            },
          },
          textContent
        )
      )
    );
  }
}

module.exports = BoxModelEditable;
