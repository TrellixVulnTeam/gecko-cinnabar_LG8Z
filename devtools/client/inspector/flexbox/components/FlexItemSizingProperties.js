/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const Types = require("../types");

class FlexItemSizingProperties extends PureComponent {
  static get propTypes() {
    return {
      flexDirection: PropTypes.string.isRequired,
      flexItem: PropTypes.shape(Types.flexItem).isRequired,
    };
  }

  /**
   * Rounds some dimension in pixels and returns a string to be displayed to the user.
   * The string will end with 'px'. If the number is 0, the string "0" is returned.
   *
   * @param  {Number} value
   *         The number to be rounded
   * @return {String}
   *         Representation of the rounded number
   */
  getRoundedDimension(value) {
    if (value == 0) {
      return "0";
    }
    return (Math.round(value * 100) / 100) + "px";
  }

  /**
   * Format the flexibility value into a meaningful value for the UI.
   * If the item grew, then prepend a + sign, if it shrank, prepend a - sign.
   * If it didn't flex, return "0".
   *
   * @param  {Boolean} grew
   *         Whether the item grew or not
   * @param  {Number} value
   *         The amount of pixels the item flexed
   * @return {String}
   *         Representation of the flexibility value
   */
  getFlexibilityValueString(grew, mainDeltaSize) {
    const value = this.getRoundedDimension(mainDeltaSize);

    if (grew) {
      return "+" + value;
    }

    return value;
  }

  /**
   * Render an authored CSS property.
   *
   * @param  {String} name
   *         The name for this CSS property
   * @param  {String} value
   *         The property value
   * @param  {Booleam} isDefaultValue
   *         Whether the value come from the browser default style
   * @return {Object}
   *         The React component representing this CSS property
   */
  renderCssProperty(name, value, isDefaultValue) {
    return (
      dom.span({ className: "css-property-link" },
        dom.span({ className: "theme-fg-color5" }, name),
        ": ",
        dom.span({ className: "theme-fg-color1" }, value),
        ";"
      )
    );
  }

  /**
   * Render a list of sentences to be displayed in the UI as reasons why a certain sizing
   * value happened.
   *
   * @param  {Array} sentences
   *         The list of sentences as Strings
   * @return {Object}
   *         The React component representing these sentences
   */
  renderReasons(sentences) {
    return (
      dom.ul({ className: "reasons" },
        sentences.map(sentence => dom.li({}, sentence))
      )
    );
  }

  renderBaseSizeSection({ mainBaseSize, clampState }, properties, dimension) {
    const flexBasisValue = properties["flex-basis"];
    const dimensionValue = properties[dimension];

    let property = null;
    let reason = null;

    if (flexBasisValue) {
      // If flex-basis is defined, then that's what is used for the base size.
      property = this.renderCssProperty("flex-basis", flexBasisValue);
    } else if (dimensionValue) {
      // If not and width/height is defined, then that's what defines the base size.
      property = this.renderCssProperty(dimension, dimensionValue);
    } else {
      // Finally, if nothing is set, then the base size is the max-content size.
      reason = this.renderReasons(
        [getStr("flexbox.itemSizing.itemBaseSizeFromContent")]);
    }

    const className = "section base";
    return (
      dom.li({ className: className + (property ? "" : " no-property") },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.baseSizeSectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getRoundedDimension(mainBaseSize)
        ),
        property,
        reason
      )
    );
  }

  renderFlexibilitySection(flexItemSizing, properties, computedStyle) {
    const {
      mainDeltaSize,
      mainBaseSize,
      mainFinalSize,
      lineGrowthState,
      clampState,
    } = flexItemSizing;

    // Don't display anything if all interesting sizes are 0.
    if (!mainFinalSize && !mainBaseSize && !mainDeltaSize) {
      return null;
    }

    // Also don't display anything if the item did not grow or shrink.
    const grew = mainDeltaSize > 0;
    const shrank = mainDeltaSize < 0;
    if (!grew && !shrank) {
      return null;
    }

    const definedFlexGrow = properties["flex-grow"];
    const computedFlexGrow = computedStyle.flexGrow;
    const definedFlexShrink = properties["flex-shrink"];
    const computedFlexShrink = computedStyle.flexShrink;
    const wasClamped = clampState !== "unclamped";

    const reasons = [];

    // Tell users whether the item was set to grow or shrink.
    if (computedFlexGrow && lineGrowthState === "growing") {
      reasons.push(getStr("flexbox.itemSizing.setToGrow"));
    }
    if (computedFlexShrink && lineGrowthState === "shrinking") {
      reasons.push(getStr("flexbox.itemSizing.setToShrink"));
    }
    if (!computedFlexGrow && !grew && !shrank && lineGrowthState === "growing") {
      reasons.push(getStr("flexbox.itemSizing.notSetToGrow"));
    }
    if (!computedFlexShrink && !grew && !shrank && lineGrowthState === "shrinking") {
      reasons.push(getStr("flexbox.itemSizing.notSetToShrink"));
    }

    let property = null;

    if (grew) {
      // If the item grew.
      if (definedFlexGrow) {
        // It's normally because it was set to grow (flex-grow is non 0).
        property = this.renderCssProperty("flex-grow", definedFlexGrow);
      }

      if (wasClamped && clampState === "clamped_to_max") {
        // It may have wanted to grow more than it did, because it was later max-clamped.
        reasons.push(getStr("flexbox.itemSizing.growthAttemptButMaxClamped"));
      } else if (wasClamped && clampState === "clamped_to_min") {
        // Or it may have wanted to grow less, but was later min-clamped to a larger size.
        reasons.push(getStr("flexbox.itemSizing.growthAttemptButMinClamped"));
      }
    } else if (shrank) {
      // If the item shrank.
      if (definedFlexShrink && computedFlexShrink) {
        // It's either because flex-shrink is non 0.
        property = this.renderCssProperty("flex-shrink", definedFlexShrink);
      } else if (computedFlexShrink) {
        // Or also because it's default value is 1 anyway.
        property = this.renderCssProperty("flex-shrink", computedFlexShrink, true);
      }

      if (wasClamped) {
        // It might have wanted to shrink more (to accomodate all items) but couldn't
        // because it was later min-clamped.
        reasons.push(getStr("flexbox.itemSizing.shrinkAttemptWhenClamped"));
      }
    }

    // Don't display the section at all if there's nothing useful to show users.
    if (!property && !reasons.length) {
      return null;
    }

    const className = "section flexibility";
    return (
      dom.li({ className: className + (property ? "" : " no-property") },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.flexibilitySectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getFlexibilityValueString(grew, mainDeltaSize)
        ),
        property,
        this.renderReasons(reasons)
      )
    );
  }

  renderMinimumSizeSection({ clampState, mainMinSize }, properties, dimension) {
    // We only display the minimum size when the item actually violates that size during
    // layout & is clamped.
    if (clampState !== "clamped_to_min") {
      return null;
    }

    const minDimensionValue = properties[`min-${dimension}`];

    return (
      dom.li({ className: "section min" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.minSizeSectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getRoundedDimension(mainMinSize)
        ),
        this.renderCssProperty(`min-${dimension}`, minDimensionValue)
      )
    );
  }

  renderMaximumSizeSection({ clampState, mainMaxSize }, properties, dimension) {
    if (clampState !== "clamped_to_max") {
      return null;
    }

    const maxDimensionValue = properties[`max-${dimension}`];

    return (
      dom.li({ className: "section max" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.maxSizeSectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getRoundedDimension(mainMaxSize)
        ),
        this.renderCssProperty(`max-${dimension}`, maxDimensionValue)
      )
    );
  }

  renderFinalSizeSection({ mainFinalSize }) {
    return (
      dom.li({ className: "section final no-property" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.finalSizeSectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getRoundedDimension(mainFinalSize)
        )
      )
    );
  }

  render() {
    const {
      flexItem,
    } = this.props;
    const {
      computedStyle,
      flexItemSizing,
      properties,
    } = flexItem;
    const {
      mainAxisDirection,
      mainBaseSize,
      mainDeltaSize,
      mainMaxSize,
      mainMinSize,
    } = flexItemSizing;
    const dimension = mainAxisDirection.startsWith("horizontal") ? "width" : "height";

    // Calculate the final size. This is base + delta, then clamped by min or max.
    let mainFinalSize = mainBaseSize + mainDeltaSize;
    mainFinalSize = Math.max(mainFinalSize, mainMinSize);
    mainFinalSize = Math.min(mainFinalSize, mainMaxSize);
    flexItemSizing.mainFinalSize = mainFinalSize;

    return (
      dom.ul({ className: "flex-item-sizing" },
        this.renderBaseSizeSection(flexItemSizing, properties, dimension),
        this.renderFlexibilitySection(flexItemSizing, properties, computedStyle),
        this.renderMinimumSizeSection(flexItemSizing, properties, dimension),
        this.renderMaximumSizeSection(flexItemSizing, properties, dimension),
        this.renderFinalSizeSection(flexItemSizing)
      )
    );
  }
}

module.exports = FlexItemSizingProperties;
