/* @ds-bundle: {"format":4,"namespace":"SungamDesignSystem_1cd7e7","components":[{"name":"Fader","sourcePath":"components/controls/Fader.jsx"},{"name":"Knob","sourcePath":"components/controls/Knob.jsx"},{"name":"Latch","sourcePath":"components/controls/Latch.jsx"},{"name":"Selector","sourcePath":"components/controls/Selector.jsx"},{"name":"Lamp","sourcePath":"components/feedback/Lamp.jsx"},{"name":"Meter","sourcePath":"components/feedback/Meter.jsx"},{"name":"SegmentMeter","sourcePath":"components/feedback/SegmentMeter.jsx"},{"name":"PanelFrame","sourcePath":"components/misc/PanelFrame.jsx"},{"name":"Terminal","sourcePath":"components/misc/Terminal.jsx"},{"name":"Wordmark","sourcePath":"components/misc/Wordmark.jsx"}],"sourceHashes":{"components/controls/Fader.jsx":"94951f2ff049","components/controls/Knob.jsx":"ccf0d271ca52","components/controls/Latch.jsx":"abdba8f1efab","components/controls/Selector.jsx":"5431c751a5a7","components/feedback/Lamp.jsx":"3f4f03c8a8cd","components/feedback/Meter.jsx":"b607bde7ce19","components/feedback/SegmentMeter.jsx":"5624182b173d","components/misc/PanelFrame.jsx":"80776438baab","components/misc/Terminal.jsx":"23658c8cabc0","components/misc/Wordmark.jsx":"54468b165678","ui_kits/cloudius/CloudiusPanel.jsx":"fcc0d087d1de","ui_kits/colacut/ColacutPanel.jsx":"2edb0d4fd2bd","ui_kits/lacze/LaczePanel.jsx":"7d73258d0efb","ui_kits/whoomp/WhoompPanel.jsx":"34f00262f35b"},"inlinedExternals":[],"unexposedExports":[]} */

(() => {

const __ds_ns = (window.SungamDesignSystem_1cd7e7 = window.SungamDesignSystem_1cd7e7 || {});

const __ds_scope = {};

(__ds_ns.__errors = __ds_ns.__errors || []);

// components/controls/Fader.jsx
try { (() => {
/**
 * Vertical fader — 6px pill track, 14x4 handle. Unipolar fills from the
 * bottom; a bipolar one fills from its centre tick instead, which is where
 * its zero is.
 */
function Fader(props) {
  const {
    value = 0,
    color = 'var(--coral)',
    label,
    valueText,
    bipolar = false,
    travel = 130,
    onChange
  } = props;
  const trackW = 6,
    handleW = 14,
    handleH = 4,
    width = 44;
  const top = 24,
    bot = top + travel;
  const norm = Math.max(0, Math.min(1, value));
  const y = bot - norm * travel;
  const origin = bipolar ? bot - 0.5 * travel : bot;
  return /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'inline-flex',
      flexDirection: 'column',
      alignItems: 'center',
      width,
      fontFamily: 'var(--font-mono)'
    }
  }, label && /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: 8.5,
      letterSpacing: '.02em',
      color: 'var(--ink-62)',
      textTransform: 'uppercase',
      marginBottom: 6
    }
  }, label), /*#__PURE__*/React.createElement("svg", {
    width: width,
    height: bot + 24,
    style: {
      cursor: onChange ? 'ns-resize' : 'default'
    }
  }, Array.from({
    length: 9
  }).map((_, i) => {
    const ty = top + travel * i / 8;
    const major = i % 4 === 0;
    return /*#__PURE__*/React.createElement("rect", {
      key: i,
      x: width / 2 - 15,
      y: ty - 0.5,
      width: major ? 6 : 3.5,
      height: 1,
      fill: "var(--ink-13)"
    });
  }), /*#__PURE__*/React.createElement("rect", {
    x: width / 2 - trackW / 2,
    y: top,
    width: trackW,
    height: travel,
    rx: trackW / 2,
    fill: "var(--ink-13)"
  }), Math.abs(origin - y) > 1 && /*#__PURE__*/React.createElement("rect", {
    x: width / 2 - trackW / 2,
    y: Math.min(y, origin),
    width: trackW,
    height: Math.abs(origin - y),
    rx: trackW / 2,
    fill: color
  }), bipolar && /*#__PURE__*/React.createElement("rect", {
    x: width / 2 + 9,
    y: origin - 0.5,
    width: 5,
    height: 1,
    fill: "var(--ink-32)"
  }), /*#__PURE__*/React.createElement("rect", {
    x: width / 2 - handleW / 2,
    y: y - handleH / 2,
    width: handleW,
    height: handleH,
    fill: "var(--ink)"
  })), valueText && /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: 9.5,
      fontWeight: 700,
      color,
      marginTop: 6
    }
  }, valueText));
}
Object.assign(__ds_scope, { Fader });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/controls/Fader.jsx", error: String((e && e.message) || e) }); }

// components/controls/Knob.jsx
try { (() => {
const TAU = Math.PI * 2;
const SWEEP = 158.6 * Math.PI / 180;
const angleFor = n => -SWEEP + 2 * SWEEP * Math.max(0, Math.min(1, n)) - Math.PI / 2;
function pt(cx, cy, r, a) {
  return [cx + r * Math.cos(a), cy + r * Math.sin(a)];
}
function arcPath(cx, cy, r, a, b) {
  if (Math.abs(b - a) < 1e-4) return '';
  const [x1, y1] = pt(cx, cy, r, a);
  const [x2, y2] = pt(cx, cy, r, b);
  const large = Math.abs(b - a) > Math.PI ? 1 : 0;
  return `M ${x1} ${y1} A ${r} ${r} 0 ${large} 1 ${x2} ${y2}`;
}

/**
 * One circle, one arc, one pointer — the house control. Unipolar knobs grow
 * their arc from the anticlockwise stop; bipolar ones grow from noon. An
 * optional trim renders as a small satellite knob on a dashed leg, the way a
 * modulation depth hangs off the control it moves.
 */
function Knob(props) {
  const {
    value = 0,
    radius = 20,
    color = 'var(--coral)',
    bipolar = false,
    label,
    valueText,
    below = false,
    trimValue,
    trimColor = 'var(--violet)',
    onChange
  } = props;
  const r = radius;
  const pad = 40;
  const size = r * 2 + pad * 2;
  const cx = size / 2,
    cy = size / 2;
  const norm = Math.max(0, Math.min(1, value));
  const aStart = angleFor(0),
    aEnd = angleFor(1),
    aNow = angleFor(norm);
  const track = r + 4.5;
  const w = r >= 24 ? 3.4 : r >= 16 ? 2.8 : 2.2;
  const from = bipolar ? -Math.PI / 2 : aStart;
  const [px, py] = pt(cx, cy, r * 0.78, aNow);
  const [tx1, ty1] = pt(cx, cy, r + 1.5, aStart);
  const [tx2, ty2] = pt(cx, cy, r + 6.5, aStart);
  const fs = r >= 30 ? 10 : r >= 18 ? 9 : 8;
  const trim = trimValue != null ? (() => {
    const tcx = cx + r * 0.72 + 26,
      tcy = cy - r * 0.72 - 8;
    const tr = 11;
    const tAngle = angleFor(trimValue);
    const [ttx, tty] = pt(tcx, tcy, tr * 0.78, tAngle);
    return {
      tcx,
      tcy,
      tr,
      tAngle,
      ttx,
      tty
    };
  })() : null;
  return /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'inline-flex',
      flexDirection: 'column',
      alignItems: 'center',
      fontFamily: 'var(--font-mono)'
    }
  }, !below && /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      marginBottom: 2,
      minHeight: 26
    }
  }, label && /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: fs,
      letterSpacing: '.02em',
      color: 'var(--ink-62)',
      textTransform: 'uppercase'
    }
  }, label), valueText && /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: fs + 2,
      fontWeight: 700,
      color
    }
  }, valueText)), /*#__PURE__*/React.createElement("svg", {
    width: size,
    height: size,
    style: {
      display: 'block',
      cursor: onChange ? 'ns-resize' : 'default'
    }
  }, /*#__PURE__*/React.createElement("circle", {
    cx: cx,
    cy: cy,
    r: r,
    fill: "var(--paper)",
    stroke: "var(--ink-38)",
    strokeWidth: 1.2
  }), /*#__PURE__*/React.createElement("path", {
    d: arcPath(cx, cy, track, aStart, aEnd),
    stroke: "var(--ink-13)",
    strokeWidth: w,
    fill: "none"
  }), /*#__PURE__*/React.createElement("path", {
    d: arcPath(cx, cy, track, from, aNow),
    stroke: color,
    strokeWidth: w,
    fill: "none"
  }), /*#__PURE__*/React.createElement("line", {
    x1: cx + r * 0.16 * Math.cos(aNow),
    y1: cy + r * 0.16 * Math.sin(aNow),
    x2: px,
    y2: py,
    stroke: "var(--ink-85)",
    strokeWidth: r >= 24 ? 2 : 1.5
  }), /*#__PURE__*/React.createElement("line", {
    x1: tx1,
    y1: ty1,
    x2: tx2,
    y2: ty2,
    stroke: "var(--ink-28)",
    strokeWidth: 1
  }), trim && /*#__PURE__*/React.createElement("g", null, /*#__PURE__*/React.createElement("line", {
    x1: cx + r * 0.72,
    y1: cy - r * 0.72,
    x2: trim.tcx - trim.tr,
    y2: trim.tcy + trim.tr,
    stroke: trimColor,
    strokeWidth: 1,
    strokeDasharray: "3,3",
    opacity: 0.5
  }), /*#__PURE__*/React.createElement("circle", {
    cx: trim.tcx,
    cy: trim.tcy,
    r: trim.tr,
    fill: "var(--paper)",
    stroke: "var(--ink-38)",
    strokeWidth: 1
  }), /*#__PURE__*/React.createElement("path", {
    d: arcPath(trim.tcx, trim.tcy, trim.tr + 3, -Math.PI / 2, trim.tAngle),
    stroke: trimColor,
    strokeWidth: 1.6,
    fill: "none"
  }), /*#__PURE__*/React.createElement("line", {
    x1: trim.tcx,
    y1: trim.tcy,
    x2: trim.ttx,
    y2: trim.tty,
    stroke: "var(--ink-85)",
    strokeWidth: 1.2
  }))), below && /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      marginTop: 2
    }
  }, label && /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: fs,
      letterSpacing: '.02em',
      color: 'var(--ink-62)',
      textTransform: 'uppercase'
    }
  }, label), valueText && /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: fs + 2,
      fontWeight: 700,
      color
    }
  }, valueText)));
}
Object.assign(__ds_scope, { Knob });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/controls/Knob.jsx", error: String((e && e.message) || e) }); }

// components/controls/Latch.jsx
try { (() => {
/**
 * A single latching button: filled and coloured when on, an outline of the
 * same colour when off. This is the panel's toggle — no pill switches, no
 * checkmarks.
 */
function Latch(props) {
  const {
    on = false,
    label,
    color = 'var(--amber)',
    onClick,
    size = 8,
    width = 60
  } = props;
  return /*#__PURE__*/React.createElement("div", {
    onClick: onClick,
    style: {
      width,
      height: 17,
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      fontFamily: 'var(--font-mono)',
      fontSize: size,
      letterSpacing: '.02em',
      background: on ? color : 'var(--paper)',
      color: on ? 'var(--paper)' : 'var(--ink-62)',
      border: `1.2px solid ${on ? color : 'var(--ink-28)'}`,
      cursor: onClick ? 'pointer' : 'default',
      userSelect: 'none'
    }
  }, label);
}
Object.assign(__ds_scope, { Latch });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/controls/Latch.jsx", error: String((e && e.message) || e) }); }

// components/controls/Selector.jsx
try { (() => {
/**
 * N-position selector. Every position stays labelled — nothing is
 * abbreviated to fit. Horizontal by default; set `vertical` to read the
 * choices top to bottom instead.
 */
function Selector(props) {
  const {
    options = [],
    selected = 0,
    color = 'var(--teal)',
    vertical = false,
    onChange,
    size = 9
  } = props;
  const segStyle = active => ({
    flex: 1,
    padding: vertical ? '5px 8px' : '5px 4px',
    textAlign: 'center',
    fontFamily: 'var(--font-mono)',
    fontSize: size,
    letterSpacing: '.02em',
    background: active ? color : 'transparent',
    color: active ? 'var(--paper)' : 'var(--ink-62)',
    cursor: onChange ? 'pointer' : 'default',
    userSelect: 'none'
  });
  return /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      flexDirection: vertical ? 'column' : 'row',
      border: '1px solid var(--ink-28)',
      width: 'fit-content'
    }
  }, options.map((opt, i) => /*#__PURE__*/React.createElement("div", {
    key: opt,
    style: {
      ...segStyle(i === selected),
      borderRight: !vertical && i < options.length - 1 ? '1px solid var(--ink-18)' : undefined,
      borderBottom: vertical && i < options.length - 1 ? '1px solid var(--ink-18)' : undefined
    },
    onClick: onChange ? () => onChange(i) : undefined
  }, opt)));
}
Object.assign(__ds_scope, { Selector });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/controls/Selector.jsx", error: String((e && e.message) || e) }); }

// components/feedback/Lamp.jsx
try { (() => {
/**
 * Lit when something is happening, outlined when it is not. The panel's
 * activity indicator — never a coloured dot, always a small square.
 */
function Lamp(props) {
  const {
    on = false,
    color = 'var(--ink)',
    size = 9
  } = props;
  return /*#__PURE__*/React.createElement("svg", {
    width: size + 2,
    height: size + 2
  }, /*#__PURE__*/React.createElement("rect", {
    x: 1,
    y: 1,
    width: size,
    height: size,
    fill: on ? color : 'var(--paper)',
    stroke: on ? 'none' : color,
    strokeOpacity: on ? 1 : 0.45,
    strokeWidth: 1
  }));
}
Object.assign(__ds_scope, { Lamp });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/feedback/Lamp.jsx", error: String((e && e.message) || e) }); }

// components/feedback/Meter.jsx
try { (() => {
/**
 * A bar meter: unipolar fills from the left, bipolar fills out from a
 * centre tick. Used for level readouts, buffer status and modulation
 * amount.
 */
function Meter(props) {
  const {
    value = 0,
    color = 'var(--steel)',
    bipolar = false,
    width = 180,
    height = 9
  } = props;
  const v = bipolar ? Math.max(-1, Math.min(1, value)) : Math.max(0, Math.min(1, value));
  return /*#__PURE__*/React.createElement("svg", {
    width: width,
    height: height
  }, /*#__PURE__*/React.createElement("rect", {
    x: 0,
    y: 0,
    width: width,
    height: height,
    fill: "var(--paper)",
    stroke: "var(--ink-30)",
    strokeWidth: 1
  }), bipolar ? /*#__PURE__*/React.createElement(React.Fragment, null, /*#__PURE__*/React.createElement("rect", {
    x: v >= 0 ? width / 2 : width / 2 + v * width / 2,
    y: 1,
    width: Math.abs(v) * width / 2,
    height: height - 2,
    fill: color,
    opacity: 0.55
  }), /*#__PURE__*/React.createElement("rect", {
    x: width / 2 - 0.5,
    y: -2,
    width: 1,
    height: height + 4,
    fill: color,
    opacity: 0.5
  })) : v > 0.02 && /*#__PURE__*/React.createElement("rect", {
    x: 1,
    y: 1,
    width: Math.max(0, v * width - 2),
    height: height - 2,
    fill: color,
    opacity: 0.75
  }));
}
Object.assign(__ds_scope, { Meter });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/feedback/Meter.jsx", error: String((e && e.message) || e) }); }

// components/feedback/SegmentMeter.jsx
try { (() => {
/**
 * A level meter stood on end, filling from the bottom in discrete segments.
 * The top segment reads amber regardless of section colour — it means you
 * are nearly out of room.
 */
function SegmentMeter(props) {
  const {
    level = 0,
    color = 'var(--steel)',
    segments = 7
  } = props;
  const w = 16,
    h = 6,
    gap = 3;
  const lit = Math.round(Math.max(0, Math.min(1, level)) * segments);
  return /*#__PURE__*/React.createElement("svg", {
    width: w,
    height: segments * (h + gap)
  }, Array.from({
    length: segments
  }).map((_, i) => {
    const on = segments - i <= lit;
    const c = i === 0 ? 'var(--amber)' : color;
    return /*#__PURE__*/React.createElement("rect", {
      key: i,
      x: 0,
      y: i * (h + gap),
      width: w,
      height: h,
      fill: on ? c : 'var(--paper)',
      stroke: on ? 'none' : 'var(--ink-22)',
      strokeWidth: on ? 0 : 1
    });
  }));
}
Object.assign(__ds_scope, { SegmentMeter });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/feedback/SegmentMeter.jsx", error: String((e && e.message) || e) }); }

// components/misc/PanelFrame.jsx
try { (() => {
/**
 * A section, drawn as the box the controls in it sit inside: a thin
 * hairline rule in the section colour with the title sitting in the rule
 * rather than floating above it.
 */
function PanelFrame(props) {
  const {
    title,
    color = 'var(--coral)',
    width = 400,
    height = 260,
    children
  } = props;
  return /*#__PURE__*/React.createElement("div", {
    style: {
      position: 'relative',
      width,
      height,
      border: `1px solid ${color}`,
      opacity: 1,
      boxSizing: 'border-box'
    }
  }, title && /*#__PURE__*/React.createElement("span", {
    style: {
      position: 'absolute',
      top: -8,
      left: 14,
      background: 'var(--paper)',
      padding: '0 6px',
      fontFamily: 'var(--font-mono)',
      fontSize: 8.5,
      letterSpacing: '.04em',
      color,
      textTransform: 'uppercase'
    }
  }, title), /*#__PURE__*/React.createElement("div", {
    style: {
      position: 'absolute',
      inset: 0
    }
  }, children));
}
Object.assign(__ds_scope, { PanelFrame });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/misc/PanelFrame.jsx", error: String((e && e.message) || e) }); }

// components/misc/Terminal.jsx
try { (() => {
/** An I/O jack: a paper circle on an ink ring, with its label above it. */
function Terminal(props) {
  const {
    label,
    size = 14,
    onClick
  } = props;
  const r = size / 2;
  return /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'inline-flex',
      flexDirection: 'column',
      alignItems: 'center',
      fontFamily: 'var(--font-mono)'
    },
    onClick: onClick
  }, label && /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: 8,
      color: 'var(--ink-62)',
      letterSpacing: '.02em',
      marginBottom: 3
    }
  }, label), /*#__PURE__*/React.createElement("svg", {
    width: size + 4,
    height: size + 4,
    style: {
      cursor: onClick ? 'pointer' : 'default'
    }
  }, /*#__PURE__*/React.createElement("circle", {
    cx: r + 2,
    cy: r + 2,
    r: r,
    fill: "var(--paper)",
    stroke: "var(--ink-55)",
    strokeWidth: 1.3
  })));
}
Object.assign(__ds_scope, { Terminal });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/misc/Terminal.jsx", error: String((e && e.message) || e) }); }

// components/misc/Wordmark.jsx
try { (() => {
/** Letter-spaced caps — the wordmark treatment, used for product names and section captions alike. */
function Wordmark(props) {
  const {
    children,
    size = 16,
    color = 'var(--ink-55)',
    tracking = '.28em'
  } = props;
  return /*#__PURE__*/React.createElement("span", {
    style: {
      fontFamily: 'var(--font-mono)',
      fontWeight: 700,
      fontSize: size,
      letterSpacing: tracking,
      color,
      textTransform: 'uppercase'
    }
  }, children);
}
Object.assign(__ds_scope, { Wordmark });
})(); } catch (e) { __ds_ns.__errors.push({ path: "components/misc/Wordmark.jsx", error: String((e && e.message) || e) }); }

// ui_kits/cloudius/CloudiusPanel.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
const {
  useState,
  useRef,
  useCallback
} = React;
const {
  Fader,
  Selector,
  Latch,
  Wordmark,
  Meter
} = window.SungamDesignSystem_1cd7e7;
function useDrag(value, onChange, travel = 130) {
  const start = useRef(null);
  return useCallback(e => {
    start.current = {
      y: e.clientY,
      v: value
    };
    const move = ev => {
      const dy = start.current.y - ev.clientY;
      onChange(Math.max(0, Math.min(1, start.current.v + dy / travel)));
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }, [value, onChange]);
}
function DFader({
  value,
  onChange,
  ...rest
}) {
  const onDown = useDrag(value, onChange, 130);
  return /*#__PURE__*/React.createElement("div", {
    onPointerDown: onDown
  }, /*#__PURE__*/React.createElement(Fader, _extends({
    value: value
  }, rest)));
}
const lerp = (a, b, n) => a + (b - a) * n;
function CloudiusPanel() {
  const [grain, setGrain] = useState({
    pitch: 0.5,
    position: 0.34,
    size: 0.46,
    density: 0.72,
    texture: 0.4
  });
  const [blend, setBlend] = useState({
    dryWet: 0.8,
    spread: 0.55,
    feedback: 0.3,
    reverb: 0.45
  });
  const [note, setNote] = useState(1);
  const [mode, setMode] = useState(0);
  const [quality, setQuality] = useState(0);
  const [freeze, setFreeze] = useState(false);
  const [lim, setLim] = useState(true);
  const setGrainV = id => v => setGrain(s => ({
    ...s,
    [id]: v
  }));
  const setBlendV = id => v => setBlend(s => ({
    ...s,
    [id]: v
  }));
  return /*#__PURE__*/React.createElement("div", {
    style: {
      background: 'var(--cloudius-paper)',
      padding: '22px 26px',
      border: '1px solid var(--ink-22)',
      color: 'var(--cloudius-ink)'
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'grid',
      gridTemplateColumns: '1.3fr 1fr 1fr',
      gap: 0
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      borderRight: '1px solid var(--ink-13)',
      paddingRight: 20
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      alignItems: 'center',
      marginBottom: 14
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: 9.5,
      letterSpacing: '.15em',
      color: 'var(--cloudius-grain)',
      fontWeight: 700
    }
  }, "GRAIN"), /*#__PURE__*/React.createElement("span", {
    style: {
      marginLeft: 20,
      fontSize: 8,
      color: 'var(--ink-45)'
    }
  }, "NOTE"), /*#__PURE__*/React.createElement("span", {
    style: {
      marginLeft: 8
    }
  }, /*#__PURE__*/React.createElement(Selector, {
    options: ['OFF', 'TRACK'],
    selected: note,
    color: "var(--cloudius-grain)",
    onChange: setNote,
    size: 8
  }))), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      gap: 18
    }
  }, /*#__PURE__*/React.createElement(DFader, {
    value: grain.pitch,
    onChange: setGrainV('pitch'),
    color: "var(--cloudius-grain)",
    bipolar: true,
    label: "PITCH",
    valueText: `${lerp(-24, 24, grain.pitch).toFixed(1)} st`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: grain.position,
    onChange: setGrainV('position'),
    color: "var(--cloudius-grain)",
    label: "POSITION",
    valueText: `${Math.round(grain.position * 100)}%`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: grain.size,
    onChange: setGrainV('size'),
    color: "var(--cloudius-grain)",
    label: "SIZE",
    valueText: `${Math.round(lerp(32, 512, grain.size))}ms`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: grain.density,
    onChange: setGrainV('density'),
    color: "var(--cloudius-grain)",
    label: "DENSITY",
    valueText: `RND ${Math.round(grain.density * 100)}%`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: grain.texture,
    onChange: setGrainV('texture'),
    color: "var(--cloudius-grain)",
    label: "TEXTURE",
    valueText: `${Math.round(grain.texture * 100)}%`
  }))), /*#__PURE__*/React.createElement("div", {
    style: {
      padding: '0 20px',
      borderRight: '1px solid var(--ink-13)'
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'space-between',
      marginBottom: 14
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: 9.5,
      letterSpacing: '.15em',
      color: 'var(--cloudius-engine)',
      fontWeight: 700
    }
  }, "ENGINE"), /*#__PURE__*/React.createElement(Latch, {
    on: freeze,
    label: "FREEZE",
    color: "var(--cloudius-engine)",
    onClick: () => setFreeze(!freeze),
    width: 64
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      gap: 12,
      marginBottom: 14
    }
  }, /*#__PURE__*/React.createElement("div", null, /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 7.5,
      color: 'var(--ink-45)',
      marginBottom: 4
    }
  }, "MODE"), /*#__PURE__*/React.createElement(Selector, {
    options: ['GRANULAR', 'STRETCH', 'DELAY', 'SPECTRAL'],
    selected: mode,
    color: "var(--cloudius-engine)",
    vertical: true,
    onChange: setMode,
    size: 8
  })), /*#__PURE__*/React.createElement("div", null, /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 7.5,
      color: 'var(--ink-45)',
      marginBottom: 4
    }
  }, "QUALITY"), /*#__PURE__*/React.createElement(Selector, {
    options: ['1s ST', '2s MO', '4s ST', '8s MO'],
    selected: quality,
    color: "var(--cloudius-engine)",
    vertical: true,
    onChange: setQuality,
    size: 8
  }))), /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 8,
      color: 'var(--ink-45)',
      marginBottom: 10
    }
  }, "32 kHz stereo, 16-bit"), /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 7.5,
      color: 'var(--ink-45)',
      marginBottom: 4
    }
  }, "BUFFER"), /*#__PURE__*/React.createElement("div", {
    style: {
      position: 'relative',
      height: 16,
      border: '1px solid var(--ink-22)',
      background: 'var(--cloudius-paper)'
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      position: 'absolute',
      left: 0,
      top: 0,
      bottom: 0,
      width: '40%',
      background: 'var(--cloudius-engine)',
      opacity: 0.5
    }
  }), /*#__PURE__*/React.createElement("div", {
    style: {
      position: 'absolute',
      left: '48%',
      top: 0,
      bottom: 0,
      width: 1,
      background: 'var(--ink)'
    }
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 7.5,
      color: 'var(--ink-45)',
      marginTop: 4,
      textAlign: 'center'
    }
  }, "1.00 s recorded")), /*#__PURE__*/React.createElement("div", {
    style: {
      paddingLeft: 20
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: 9.5,
      letterSpacing: '.15em',
      color: 'var(--cloudius-blend)',
      fontWeight: 700,
      display: 'block',
      marginBottom: 14
    }
  }, "BLEND"), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      gap: 18
    }
  }, /*#__PURE__*/React.createElement(DFader, {
    value: blend.dryWet,
    onChange: setBlendV('dryWet'),
    color: "var(--cloudius-blend)",
    label: "DRY/WET",
    valueText: `${Math.round(blend.dryWet * 100)}%`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: blend.spread,
    onChange: setBlendV('spread'),
    color: "var(--cloudius-blend)",
    label: "SPREAD",
    valueText: `${Math.round(blend.spread * 100)}%`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: blend.feedback,
    onChange: setBlendV('feedback'),
    color: "var(--cloudius-blend)",
    label: "FEEDBACK",
    valueText: `${Math.round(blend.feedback * 100)}%`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: blend.reverb,
    onChange: setBlendV('reverb'),
    color: "var(--cloudius-blend)",
    label: "REVERB",
    valueText: `${Math.round(blend.reverb * 100)}%`
  })))), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      alignItems: 'center',
      gap: 16,
      marginTop: 22,
      paddingTop: 14,
      borderTop: '1px solid var(--ink-13)',
      fontSize: 8.5
    }
  }, /*#__PURE__*/React.createElement("span", null, "VU"), /*#__PURE__*/React.createElement("span", null, "OUT"), /*#__PURE__*/React.createElement(Meter, {
    value: 0.62,
    color: "var(--cloudius-grain)",
    width: 140,
    height: 7
  }), /*#__PURE__*/React.createElement(Latch, {
    on: lim,
    label: "LIM",
    color: "var(--cloudius-grain)",
    onClick: () => setLim(!lim),
    width: 40
  }), /*#__PURE__*/React.createElement("span", null, "CLIP"), /*#__PURE__*/React.createElement("span", {
    style: {
      marginLeft: 10
    }
  }, "RATE ", /*#__PURE__*/React.createElement("b", null, "32 kHz from 48.0 kHz")), /*#__PURE__*/React.createElement("span", null, "LATENCY ", /*#__PURE__*/React.createElement("b", null, "2.7 ms")), /*#__PURE__*/React.createElement("span", {
    style: {
      marginLeft: 'auto',
      color: 'var(--ink-38)'
    }
  }, "after clouds"), /*#__PURE__*/React.createElement(Wordmark, {
    size: 13,
    color: "var(--ink-55)"
  }, "CLOUDIUS")));
}
ReactDOM.createRoot(document.getElementById('root')).render(/*#__PURE__*/React.createElement(CloudiusPanel, null));
})(); } catch (e) { __ds_ns.__errors.push({ path: "ui_kits/cloudius/CloudiusPanel.jsx", error: String((e && e.message) || e) }); }

// ui_kits/colacut/ColacutPanel.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
const {
  useState,
  useRef,
  useCallback
} = React;
const {
  Knob,
  Fader,
  Selector,
  Latch,
  Wordmark
} = window.SungamDesignSystem_1cd7e7;
function useDrag(value, onChange, travel = 90) {
  const start = useRef(null);
  return useCallback(e => {
    start.current = {
      y: e.clientY,
      v: value
    };
    const move = ev => {
      const dy = start.current.y - ev.clientY;
      onChange(Math.max(0, Math.min(1, start.current.v + dy / travel)));
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }, [value, onChange]);
}
function DFader({
  value,
  onChange,
  ...rest
}) {
  const onDown = useDrag(value, onChange, 130);
  return /*#__PURE__*/React.createElement("div", {
    onPointerDown: onDown
  }, /*#__PURE__*/React.createElement(Fader, _extends({
    value: value
  }, rest)));
}
function DKnob({
  value,
  onChange,
  ...rest
}) {
  const onDown = useDrag(value, onChange, 90);
  return /*#__PURE__*/React.createElement("div", {
    onPointerDown: onDown
  }, /*#__PURE__*/React.createElement(Knob, _extends({
    value: value
  }, rest)));
}
const lerp = (a, b, n) => a + (b - a) * n;
const ROWS = ['PITCH', 'STRETCH', 'THRESH', 'GRAIN', 'QUALITY', 'FEEDBK'];
function ColacutPanel() {
  const [pitch, setPitch] = useState(0.5);
  const [perf, setPerf] = useState({
    stretch: 0.55,
    thresh: 0.55,
    grain: 0.85,
    quality: 1,
    feedback: 0.05
  });
  const [source, setSource] = useState([1, 1, 1, 1, 1, 1]);
  const [depth, setDepth] = useState([0.5, 0.5, 0.5, 0.5, 0.5, 0.5]);
  const [character, setCharacter] = useState(0.85);
  const [voice, setVoice] = useState({
    smooth: 0.55,
    fade: 0.4,
    drive: 0.15,
    mix: 1,
    fbTone: 0.55
  });
  const setPerfV = id => v => setPerf(s => ({
    ...s,
    [id]: v
  }));
  const setVoiceV = id => v => setVoice(s => ({
    ...s,
    [id]: v
  }));
  return /*#__PURE__*/React.createElement("div", {
    style: {
      background: 'var(--colacut-paper)',
      padding: '26px 30px',
      border: '1px solid var(--ink-22)',
      color: 'var(--colacut-ink)'
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'grid',
      gridTemplateColumns: '1fr 1fr 1fr',
      gap: 28
    }
  }, /*#__PURE__*/React.createElement("div", null, /*#__PURE__*/React.createElement("div", {
    style: {
      textAlign: 'center',
      marginBottom: 8,
      fontSize: 8,
      letterSpacing: '.15em',
      color: 'var(--colacut-perform)'
    }
  }, "PITCH"), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      justifyContent: 'center',
      marginBottom: 24
    }
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: pitch,
    onChange: setPitch,
    radius: 26,
    bipolar: true,
    color: "var(--colacut-perform)",
    valueText: `${Math.round(lerp(-12, 12, pitch))}.0 st`
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      gap: 16,
      justifyContent: 'center'
    }
  }, /*#__PURE__*/React.createElement(DFader, {
    value: perf.stretch,
    onChange: setPerfV('stretch'),
    color: "var(--colacut-perform)",
    label: "STRETCH",
    valueText: `${lerp(1, 20, perf.stretch).toFixed(1)}x`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: perf.thresh,
    onChange: setPerfV('thresh'),
    color: "var(--colacut-perform)",
    label: "THRESH",
    valueText: lerp(0, 8, perf.thresh).toFixed(1)
  }), /*#__PURE__*/React.createElement(DFader, {
    value: perf.grain,
    onChange: setPerfV('grain'),
    color: "var(--colacut-perform)",
    label: "GRAIN",
    valueText: Math.round(lerp(32, 4096, perf.grain))
  }), /*#__PURE__*/React.createElement(DFader, {
    value: perf.quality,
    onChange: setPerfV('quality'),
    color: "var(--colacut-perform)",
    label: "QUALITY",
    valueText: `${Math.round(perf.quality * 100)}%`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: perf.feedback,
    onChange: setPerfV('feedback'),
    color: "var(--colacut-perform)",
    label: "FEEDBK",
    valueText: perf.feedback.toFixed(2)
  }))), /*#__PURE__*/React.createElement("div", null, /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      fontSize: 8,
      color: 'var(--ink-45)',
      marginBottom: 6,
      paddingLeft: 76
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      flex: 1
    }
  }, "SOURCE"), /*#__PURE__*/React.createElement("span", {
    style: {
      width: 60
    }
  }, "DEPTH")), ROWS.map((label, i) => /*#__PURE__*/React.createElement("div", {
    key: label,
    style: {
      display: 'flex',
      alignItems: 'center',
      gap: 10,
      marginBottom: 14
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      width: 60,
      fontSize: 8.5
    }
  }, label), /*#__PURE__*/React.createElement(Selector, {
    options: ['IN', 'OUT', 'MOD'],
    selected: source[i],
    color: "var(--colacut-mod)",
    size: 8,
    onChange: idx => setSource(s => s.map((v, j) => j === i ? idx : v))
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: depth[i],
    onChange: v => setDepth(s => s.map((x, j) => j === i ? v : x)),
    radius: 13,
    bipolar: true,
    color: "var(--colacut-mod)",
    valueText: depth[i] === 0.5 ? 'OFF' : depth[i].toFixed(2)
  })))), /*#__PURE__*/React.createElement("div", null, /*#__PURE__*/React.createElement("div", {
    style: {
      textAlign: 'center',
      marginBottom: 8,
      fontSize: 8,
      letterSpacing: '.15em',
      color: 'var(--colacut-voice)'
    }
  }, "CHARACTER"), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      flexDirection: 'column',
      alignItems: 'center',
      marginBottom: 24
    }
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: character,
    onChange: setCharacter,
    radius: 26,
    color: "var(--colacut-voice)",
    valueText: "SINC"
  }), /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 7.5,
      color: 'var(--ink-38)',
      marginTop: 4
    }
  }, "quake / clean / sinc")), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      gap: 16,
      justifyContent: 'center'
    }
  }, /*#__PURE__*/React.createElement(DFader, {
    value: voice.smooth,
    onChange: setVoiceV('smooth'),
    color: "var(--colacut-voice)",
    label: "SMOOTH",
    valueText: `${Math.round(lerp(1, 60, voice.smooth))}Hz`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: voice.fade,
    onChange: setVoiceV('fade'),
    color: "var(--colacut-voice)",
    label: "FADE",
    valueText: `${lerp(10, 250, voice.fade).toFixed(1)}ms`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: voice.drive,
    onChange: setVoiceV('drive'),
    color: "var(--colacut-voice)",
    label: "DRIVE",
    valueText: lerp(0.5, 4, voice.drive).toFixed(2)
  }), /*#__PURE__*/React.createElement(DFader, {
    value: voice.mix,
    onChange: setVoiceV('mix'),
    color: "var(--colacut-voice)",
    label: "MIX",
    valueText: `${Math.round(voice.mix * 100)}%`
  }), /*#__PURE__*/React.createElement(DFader, {
    value: voice.fbTone,
    onChange: setVoiceV('fbTone'),
    color: "var(--colacut-voice)",
    label: "FB TONE",
    valueText: `${Math.round(lerp(100, 2000, voice.fbTone))}Hz`
  })))), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      alignItems: 'center',
      gap: 20,
      marginTop: 30,
      paddingTop: 16,
      borderTop: '1px solid var(--ink-18)',
      fontSize: 8.5
    }
  }, /*#__PURE__*/React.createElement("span", null, "LATENCY ", /*#__PURE__*/React.createElement("b", null, "20.0 ms")), /*#__PURE__*/React.createElement("div", {
    style: {
      marginLeft: 'auto',
      display: 'flex',
      alignItems: 'center',
      gap: 14
    }
  }, /*#__PURE__*/React.createElement(Latch, {
    on: true,
    label: "SLICE",
    color: "var(--colacut-perform)",
    width: 70
  }), /*#__PURE__*/React.createElement(Wordmark, {
    size: 13,
    color: "var(--ink-55)"
  }, "COLACUT"))));
}
ReactDOM.createRoot(document.getElementById('root')).render(/*#__PURE__*/React.createElement(ColacutPanel, null));
})(); } catch (e) { __ds_ns.__errors.push({ path: "ui_kits/colacut/ColacutPanel.jsx", error: String((e && e.message) || e) }); }

// ui_kits/lacze/LaczePanel.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
const {
  useState,
  useRef,
  useCallback
} = React;
const {
  Knob,
  Latch,
  Terminal,
  Wordmark,
  SegmentMeter
} = window.SungamDesignSystem_1cd7e7;
function useDrag(value, onChange, travel = 100) {
  const start = useRef(null);
  return useCallback(e => {
    start.current = {
      y: e.clientY,
      v: value
    };
    const move = ev => {
      const dy = start.current.y - ev.clientY;
      onChange(Math.max(0, Math.min(1, start.current.v + dy / travel * (ev.shiftKey ? 0.22 : 1))));
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }, [value, onChange]);
}
function DKnob({
  value,
  onChange,
  ...rest
}) {
  const onDown = useDrag(value, onChange, rest.radius >= 30 ? 100 : 80);
  return /*#__PURE__*/React.createElement("div", {
    onPointerDown: onDown
  }, /*#__PURE__*/React.createElement(Knob, _extends({
    value: value
  }, rest)));
}
const lerp = (a, b, n) => a + (b - a) * n;
function LaczePanel() {
  const [k, setK] = useState({
    bassIn: 0.6,
    inDrive: 0.38,
    howl: 0.51,
    howlIntensity: 0.4,
    pitch: 0.4,
    lively: false,
    normalize: 0.75,
    bias: 0.32,
    biasIntensity: 0.5,
    spread: 0.44,
    drive: 1,
    driveIntensity: 0.5,
    tone: 0.35,
    notch: false,
    feedback: 0.46,
    fbTime: 0.4,
    fbTone: 0.55,
    fbReso: 0.9,
    bassOut: 0,
    mix: 0.8,
    wander: 0.3,
    lvl1: 0.5,
    lvl2: 0.45,
    lvl3: 0.5,
    split1: 0.35,
    split2: 0.6
  });
  const [lim, setLim] = useState(false);
  const [solo, setSolo] = useState([false, false, false]);
  const [mute, setMute] = useState([false, false, false]);
  const set = id => v => setK(s => ({
    ...s,
    [id]: v
  }));
  const row = {
    display: 'flex',
    gap: 30,
    alignItems: 'flex-end'
  };
  const toggleArr = (arr, setArr, i) => setArr(arr.map((v, j) => j === i ? !v : v));
  return /*#__PURE__*/React.createElement("div", {
    style: {
      background: 'var(--paper)',
      padding: '30px 34px',
      border: '1px solid var(--ink-22)'
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      ...row,
      marginBottom: 30
    }
  }, /*#__PURE__*/React.createElement(Terminal, {
    label: "IN"
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.bassIn,
    onChange: set('bassIn'),
    radius: 15,
    color: "var(--lilac)",
    label: "BASS IN",
    valueText: `${lerp(-12, 12, k.bassIn).toFixed(1)} dB`,
    trimValue: k.howlIntensity,
    trimColor: "var(--violet)"
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.inDrive,
    onChange: set('inDrive'),
    radius: 26,
    color: "var(--coral)",
    label: "IN DRIVE",
    valueText: `${Math.round(k.inDrive * 100)}%`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.howl,
    onChange: set('howl'),
    radius: 38,
    color: "var(--coral)",
    label: "HOWL",
    valueText: `${Math.round(k.howl * 100)}%`,
    trimValue: 0.3
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.pitch,
    onChange: set('pitch'),
    radius: 15,
    color: "var(--lilac)",
    label: "PITCH",
    valueText: `${Math.round(lerp(40, 400, k.pitch))} Hz`,
    below: true
  }), /*#__PURE__*/React.createElement(Latch, {
    on: k.lively,
    label: "LIVELY",
    color: "var(--coral)",
    onClick: () => setK(s => ({
      ...s,
      lively: !s.lively
    }))
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.normalize,
    onChange: set('normalize'),
    radius: 26,
    color: "var(--coral)",
    label: "NORMALIZE",
    valueText: `${Math.round(k.normalize * 100)}%`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.bias,
    onChange: set('bias'),
    radius: 26,
    color: "var(--coral)",
    label: "BIAS",
    valueText: `${Math.round(lerp(-180, 180, k.bias))}°`,
    trimValue: 0.5,
    bipolar: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.spread,
    onChange: set('spread'),
    radius: 15,
    color: "var(--lilac)",
    label: "SPREAD",
    valueText: `${Math.round(k.spread * 100)}%`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.drive,
    onChange: set('drive'),
    radius: 38,
    color: "var(--coral)",
    label: "DRIVE",
    valueText: `${Math.round(k.drive * 100)}%`,
    trimValue: 0.4
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      ...row,
      marginBottom: 30,
      paddingLeft: 40
    }
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.tone,
    onChange: set('tone'),
    radius: 20,
    color: "var(--coral)",
    label: "TONE",
    valueText: `${lerp(0.2, 12, k.tone).toFixed(2)} kHz`
  }), /*#__PURE__*/React.createElement(Latch, {
    on: k.notch,
    label: "NOTCH",
    color: "var(--coral)",
    onClick: () => setK(s => ({
      ...s,
      notch: !s.notch
    }))
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.feedback,
    onChange: set('feedback'),
    radius: 38,
    color: "var(--coral)",
    label: "FEEDBACK",
    valueText: `${Math.round(k.feedback * 100)}%`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fbTime,
    onChange: set('fbTime'),
    radius: 15,
    color: "var(--lilac)",
    label: "FB TIME",
    valueText: `${Math.round(lerp(10, 250, k.fbTime))} ms`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fbTone,
    onChange: set('fbTone'),
    radius: 15,
    color: "var(--lilac)",
    label: "FB TONE",
    valueText: `${lerp(0.2, 5, k.fbTone).toFixed(2)} kHz`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fbReso,
    onChange: set('fbReso'),
    radius: 15,
    color: "var(--lilac)",
    label: "FB RESO",
    valueText: `${Math.round(k.fbReso * 100)}%`
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      gap: 18,
      alignItems: 'center',
      marginBottom: 30,
      borderTop: '1px solid var(--ink-13)',
      paddingTop: 24
    }
  }, ['LOW', 'MID', 'HIGH'].map((band, i) => /*#__PURE__*/React.createElement("div", {
    key: band,
    style: {
      display: 'flex',
      alignItems: 'center',
      gap: 8,
      fontSize: 8.5,
      color: 'var(--ink-62)'
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      width: 32
    }
  }, band), /*#__PURE__*/React.createElement("span", {
    style: {
      width: 50,
      border: '1px solid var(--ink-18)',
      padding: '3px 6px',
      color: 'var(--teal)'
    }
  }, lerp(-24, 12, k['lvl' + (i + 1)]).toFixed(1), " dB"), /*#__PURE__*/React.createElement(Latch, {
    width: 18,
    size: 7,
    on: mute[i],
    label: "M",
    color: "var(--teal)",
    onClick: () => toggleArr(mute, setMute, i)
  }), /*#__PURE__*/React.createElement(Latch, {
    width: 18,
    size: 7,
    on: solo[i],
    label: "S",
    color: "var(--teal)",
    onClick: () => toggleArr(solo, setSolo, i)
  }), /*#__PURE__*/React.createElement("span", {
    style: {
      background: 'var(--teal)',
      color: 'var(--paper)',
      padding: '3px 8px'
    }
  }, band === 'LOW' ? 'SUB' : 'HISS'), band !== 'LOW' && /*#__PURE__*/React.createElement("span", {
    style: {
      border: '1px solid var(--teal)',
      padding: '3px 8px'
    }
  }, "VERB")))), /*#__PURE__*/React.createElement("div", {
    style: {
      ...row,
      justifyContent: 'flex-end'
    }
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.bassOut,
    onChange: set('bassOut'),
    radius: 15,
    color: "var(--lilac)",
    label: "BASS OUT",
    valueText: k.bassOut === 0 ? 'OFF' : `${lerp(-12, 12, k.bassOut).toFixed(1)} dB`
  }), /*#__PURE__*/React.createElement(Latch, {
    on: lim,
    label: "LIM",
    color: "var(--amber)",
    onClick: () => setLim(!lim)
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.mix,
    onChange: set('mix'),
    radius: 38,
    color: "var(--coral)",
    label: "MIX",
    valueText: `${Math.round(k.mix * 100)}% wet`
  }), /*#__PURE__*/React.createElement(Terminal, {
    label: "OUT"
  }), /*#__PURE__*/React.createElement(SegmentMeter, {
    level: 0.5,
    color: "var(--coral)",
    segments: 6
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      textAlign: 'right',
      marginTop: 24
    }
  }, /*#__PURE__*/React.createElement(Wordmark, {
    size: 16
  }, "LACZE")));
}
ReactDOM.createRoot(document.getElementById('root')).render(/*#__PURE__*/React.createElement(LaczePanel, null));
})(); } catch (e) { __ds_ns.__errors.push({ path: "ui_kits/lacze/LaczePanel.jsx", error: String((e && e.message) || e) }); }

// ui_kits/whoomp/WhoompPanel.jsx
try { (() => {
function _extends() { return _extends = Object.assign ? Object.assign.bind() : function (n) { for (var e = 1; e < arguments.length; e++) { var t = arguments[e]; for (var r in t) ({}).hasOwnProperty.call(t, r) && (n[r] = t[r]); } return n; }, _extends.apply(null, arguments); }
const {
  useState,
  useRef,
  useCallback
} = React;
const {
  Knob,
  Selector,
  Latch,
  Terminal,
  Wordmark,
  SegmentMeter,
  PanelFrame
} = window.SungamDesignSystem_1cd7e7;
function useDrag(value, onChange, travel = 100) {
  const start = useRef(null);
  const onDown = useCallback(e => {
    start.current = {
      y: e.clientY,
      v: value
    };
    const move = ev => {
      const dy = start.current.y - ev.clientY;
      const sens = ev.shiftKey ? 0.22 : 1;
      onChange(Math.max(0, Math.min(1, start.current.v + dy / travel * sens)));
    };
    const up = () => {
      window.removeEventListener('pointermove', move);
      window.removeEventListener('pointerup', up);
    };
    window.addEventListener('pointermove', move);
    window.addEventListener('pointerup', up);
  }, [value, onChange]);
  return onDown;
}
function DKnob({
  value,
  onChange,
  ...rest
}) {
  const onDown = useDrag(value, onChange, rest.radius >= 24 ? 100 : 90);
  return /*#__PURE__*/React.createElement("div", {
    onPointerDown: onDown,
    onDoubleClick: () => onChange(rest.def ?? 0.5)
  }, /*#__PURE__*/React.createElement(Knob, _extends({
    value: value,
    onChange: onChange
  }, rest)));
}
const fmtHz = n => n >= 1000 ? (n / 1000).toFixed(2) + ' kHz' : Math.round(n) + ' Hz';
const lerp = (a, b, n) => a + (b - a) * n;
function WhoompPanel() {
  const [k, setK] = useState({
    tune: 0.42,
    bend: 0.55,
    fall: 0.31,
    vel: 0.72,
    sine: 1,
    tri: 0.34,
    saw: 0.12,
    fold: 0.46,
    foldDepth: 0.42,
    foldEnv: 0.7,
    cutoff: 0.55,
    filtEnv: 0.65,
    reso: 0.38,
    subA: 0.05,
    subD: 0.41,
    subS: 0,
    subR: 0.18,
    op2ratio: 0.3,
    op1ratio: 0.1,
    fmIndex: 0.6,
    fmIndexEnv: 0.5,
    fmA: 0.05,
    fmD: 0.062,
    fmS: 0,
    fmR: 0.06,
    subLevel: 0.7,
    fmLevel: 0.4,
    drive: 0.66,
    hiss: 0.58,
    cabMix: 0.72,
    roomSize: 0.3,
    roomDamp: 0.62,
    roomMix: 0.22,
    loCut: 0.1,
    b1f: 0.3,
    b1g: 0.6,
    b1q: 0.3,
    b2f: 0.4,
    b2g: 0.4,
    b2q: 0.4,
    b3f: 0.5,
    b3g: 0.55,
    b3q: 0.3,
    hiCut: 0.85,
    output: 0.55
  });
  const [op1, setOp1] = useState(0),
    [op2, setOp2] = useState(1);
  const [cab, setCab] = useState(1);
  const [lim, setLim] = useState(false);
  const [hitLevel, setHitLevel] = useState(0);
  const set = id => v => setK(s => ({
    ...s,
    [id]: v
  }));
  const hit = () => {
    setHitLevel(1);
    setTimeout(() => setHitLevel(0.15), 90);
    setTimeout(() => setHitLevel(0), 500);
  };
  const tuneHz = lerp(20, 200, k.tune);
  const row = {
    display: 'flex',
    gap: 34,
    alignItems: 'flex-end'
  };
  const sectionGap = {
    marginBottom: 30
  };
  return /*#__PURE__*/React.createElement("div", {
    style: {
      background: 'var(--paper)',
      padding: '28px 36px',
      border: '1px solid var(--ink-22)'
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      ...row,
      marginBottom: 34
    }
  }, /*#__PURE__*/React.createElement(Terminal, {
    label: "MIDI",
    onClick: hit
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.tune,
    onChange: set('tune'),
    radius: 32,
    color: "var(--ink-78)",
    label: "TUNE",
    valueText: fmtHz(tuneHz)
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.bend,
    onChange: set('bend'),
    radius: 24,
    color: "var(--ink-78)",
    label: "BEND",
    valueText: `+${Math.round(lerp(0, 48, k.bend))} st`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fall,
    onChange: set('fall'),
    radius: 24,
    color: "var(--ink-78)",
    label: "FALL",
    valueText: `${Math.round(lerp(5, 400, k.fall))} ms`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.vel,
    onChange: set('vel'),
    radius: 16,
    color: "var(--ink-78)",
    label: "VEL",
    valueText: `${Math.round(k.vel * 100)}%`
  }), /*#__PURE__*/React.createElement("div", {
    style: {
      marginLeft: 20,
      fontSize: 9,
      color: 'var(--ink-62)'
    }
  }, "NOTE ", /*#__PURE__*/React.createElement("b", {
    style: {
      color: 'var(--ink-78)'
    }
  }, "C1\xA0\xA0", fmtHz(tuneHz))), /*#__PURE__*/React.createElement("button", {
    onClick: hit,
    style: {
      marginLeft: 'auto',
      fontFamily: 'var(--font-mono)',
      fontSize: 9,
      letterSpacing: '.05em',
      padding: '8px 18px',
      border: '1.2px solid var(--coral)',
      background: 'var(--paper)',
      color: 'var(--ink)',
      cursor: 'pointer'
    }
  }, "HIT")), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      gap: 24,
      marginBottom: 34
    }
  }, /*#__PURE__*/React.createElement(PanelFrame, {
    title: "SUBTRACTIVE",
    color: "var(--coral)",
    width: 700,
    height: 330
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      padding: '26px 24px'
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      ...row,
      marginBottom: 26
    }
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.sine,
    onChange: set('sine'),
    radius: 18,
    color: "var(--coral)",
    label: "SINE",
    valueText: `${Math.round(k.sine * 100)}%`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.tri,
    onChange: set('tri'),
    radius: 18,
    color: "var(--coral)",
    label: "TRI",
    valueText: `${Math.round(k.tri * 100)}%`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.saw,
    onChange: set('saw'),
    radius: 18,
    color: "var(--coral)",
    label: "SAW",
    valueText: `${Math.round(k.saw * 100)}%`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fold,
    onChange: set('fold'),
    radius: 18,
    color: "var(--coral)",
    label: "FOLD",
    valueText: `${Math.round(k.fold * 100)}%`
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      ...row,
      marginBottom: 26
    }
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.foldDepth,
    onChange: set('foldDepth'),
    radius: 26,
    color: "var(--coral)",
    label: "FOLD DEPTH",
    valueText: `${Math.round(k.foldDepth * 100)}%`,
    trimValue: k.foldEnv
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.cutoff,
    onChange: set('cutoff'),
    radius: 26,
    color: "var(--coral)",
    label: "CUTOFF",
    valueText: `${lerp(0.1, 8, k.cutoff).toFixed(2)} kHz`,
    trimValue: k.filtEnv
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.reso,
    onChange: set('reso'),
    radius: 18,
    color: "var(--coral)",
    label: "RESO",
    valueText: `${Math.round(k.reso * 100)}%`
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 8,
      color: 'var(--coral)',
      opacity: 0.85,
      marginBottom: 8
    }
  }, "AMP ENVELOPE"), /*#__PURE__*/React.createElement("div", {
    style: row
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.subA,
    onChange: set('subA'),
    radius: 15,
    color: "var(--coral)",
    label: "ATTACK",
    valueText: `${(k.subA * 20).toFixed(2)} ms`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.subD,
    onChange: set('subD'),
    radius: 15,
    color: "var(--coral)",
    label: "DECAY",
    valueText: `${Math.round(k.subD * 1000)} ms`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.subS,
    onChange: set('subS'),
    radius: 15,
    color: "var(--coral)",
    label: "SUSTAIN",
    valueText: `${Math.round(k.subS * 100)}%`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.subR,
    onChange: set('subR'),
    radius: 15,
    color: "var(--coral)",
    label: "RELEASE",
    valueText: `${Math.round(k.subR * 1000)} ms`
  })))), /*#__PURE__*/React.createElement(PanelFrame, {
    title: "FM",
    color: "var(--teal)",
    width: 556,
    height: 330
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      padding: '26px 24px'
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      alignItems: 'center',
      gap: 14,
      marginBottom: 18
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: 8.5,
      color: 'var(--teal)',
      width: 34
    }
  }, "OP 2"), /*#__PURE__*/React.createElement(Selector, {
    options: ['SIN', 'TRI', 'NSE'],
    selected: op2,
    color: "var(--teal)",
    onChange: setOp2,
    size: 7.5
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.op2ratio,
    onChange: set('op2ratio'),
    radius: 18,
    color: "var(--teal)",
    label: "RATIO",
    valueText: `${lerp(0.25, 16, k.op2ratio).toFixed(2)}:1`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fmIndex,
    onChange: set('fmIndex'),
    radius: 26,
    color: "var(--teal)",
    label: "INDEX",
    valueText: `${(k.fmIndex * 6).toFixed(2)} rad`,
    trimValue: k.fmIndexEnv
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      display: 'flex',
      alignItems: 'center',
      gap: 14,
      marginBottom: 26
    }
  }, /*#__PURE__*/React.createElement("span", {
    style: {
      fontSize: 8.5,
      color: 'var(--teal)',
      width: 34
    }
  }, "OP 1"), /*#__PURE__*/React.createElement(Selector, {
    options: ['SIN', 'TRI', 'NSE'],
    selected: op1,
    color: "var(--teal)",
    onChange: setOp1,
    size: 7.5
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.op1ratio,
    onChange: set('op1ratio'),
    radius: 18,
    color: "var(--teal)",
    label: "RATIO",
    valueText: `${lerp(0.25, 16, k.op1ratio).toFixed(2)}:1`
  })), /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 8,
      color: 'var(--teal)',
      opacity: 0.85,
      marginBottom: 8
    }
  }, "AMP ENVELOPE"), /*#__PURE__*/React.createElement("div", {
    style: row
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.fmA,
    onChange: set('fmA'),
    radius: 15,
    color: "var(--teal)",
    label: "ATTACK",
    valueText: `${(k.fmA * 20).toFixed(2)} ms`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fmD,
    onChange: set('fmD'),
    radius: 15,
    color: "var(--teal)",
    label: "DECAY",
    valueText: `${Math.round(k.fmD * 1000)} ms`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fmS,
    onChange: set('fmS'),
    radius: 15,
    color: "var(--teal)",
    label: "SUSTAIN",
    valueText: `${Math.round(k.fmS * 100)}%`
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fmR,
    onChange: set('fmR'),
    radius: 15,
    color: "var(--teal)",
    label: "RELEASE",
    valueText: `${Math.round(k.fmR * 1000)} ms`
  }))))), /*#__PURE__*/React.createElement("div", {
    style: {
      ...row,
      marginBottom: 30,
      justifyContent: 'center'
    }
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.subLevel,
    onChange: set('subLevel'),
    radius: 24,
    color: "var(--steel)",
    label: "SUB LEVEL",
    valueText: `${lerp(-24, 6, k.subLevel).toFixed(1)} dB`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.fmLevel,
    onChange: set('fmLevel'),
    radius: 24,
    color: "var(--steel)",
    label: "FM LEVEL",
    valueText: `${lerp(-24, 6, k.fmLevel).toFixed(1)} dB`,
    below: true
  })), /*#__PURE__*/React.createElement(PanelFrame, {
    title: "CHAIN",
    color: "var(--steel)",
    width: 1170,
    height: 190
  }, /*#__PURE__*/React.createElement("div", {
    style: {
      padding: '30px 24px',
      display: 'flex',
      flexDirection: 'column',
      gap: 26
    }
  }, /*#__PURE__*/React.createElement("div", {
    style: row
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.drive,
    onChange: set('drive'),
    radius: 26,
    color: "var(--steel)",
    label: "TAPE DRIVE",
    valueText: `${Math.round(k.drive * 100)}%`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.hiss,
    onChange: set('hiss'),
    radius: 16,
    color: "var(--steel)",
    label: "HISS",
    valueText: `${Math.round(k.hiss * 100)}%`,
    below: true
  }), /*#__PURE__*/React.createElement("div", null, /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 8,
      color: 'var(--ink-62)',
      marginBottom: 4
    }
  }, "CAB"), /*#__PURE__*/React.createElement(Selector, {
    options: ['12"', '15"', '18"'],
    selected: cab,
    color: "var(--steel)",
    onChange: setCab
  })), /*#__PURE__*/React.createElement(DKnob, {
    value: k.cabMix,
    onChange: set('cabMix'),
    radius: 20,
    color: "var(--steel)",
    label: "CAB MIX",
    valueText: `${Math.round(k.cabMix * 100)}%`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.roomSize,
    onChange: set('roomSize'),
    radius: 20,
    color: "var(--steel)",
    label: "ROOM SIZE",
    valueText: `${Math.round(k.roomSize * 100)}%`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.roomDamp,
    onChange: set('roomDamp'),
    radius: 18,
    color: "var(--steel)",
    label: "DAMPING",
    valueText: `${Math.round(k.roomDamp * 100)}%`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.roomMix,
    onChange: set('roomMix'),
    radius: 20,
    color: "var(--steel)",
    label: "ROOM MIX",
    valueText: `${Math.round(k.roomMix * 100)}%`,
    below: true
  })), /*#__PURE__*/React.createElement("div", {
    style: row
  }, /*#__PURE__*/React.createElement(DKnob, {
    value: k.loCut,
    onChange: set('loCut'),
    radius: 20,
    color: "var(--steel)",
    label: "LOW CUT",
    valueText: `${Math.round(lerp(20, 400, k.loCut))} Hz`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.b1f,
    onChange: set('b1f'),
    radius: 15,
    color: "var(--steel)",
    label: "1 FREQ",
    valueText: `${Math.round(lerp(60, 400, k.b1f))} Hz`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.b1g,
    onChange: set('b1g'),
    radius: 18,
    color: "var(--steel)",
    bipolar: true,
    label: "1 GAIN",
    valueText: `${lerp(-12, 12, k.b1g).toFixed(1)} dB`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.b2f,
    onChange: set('b2f'),
    radius: 15,
    color: "var(--steel)",
    label: "2 FREQ",
    valueText: `${Math.round(lerp(200, 2000, k.b2f))} Hz`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.b2g,
    onChange: set('b2g'),
    radius: 18,
    color: "var(--steel)",
    bipolar: true,
    label: "2 GAIN",
    valueText: `${lerp(-12, 12, k.b2g).toFixed(1)} dB`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.b3f,
    onChange: set('b3f'),
    radius: 15,
    color: "var(--steel)",
    label: "3 FREQ",
    valueText: `${lerp(1, 8, k.b3f).toFixed(2)} kHz`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.b3g,
    onChange: set('b3g'),
    radius: 18,
    color: "var(--steel)",
    bipolar: true,
    label: "3 GAIN",
    valueText: `${lerp(-12, 12, k.b3g).toFixed(1)} dB`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.hiCut,
    onChange: set('hiCut'),
    radius: 20,
    color: "var(--steel)",
    label: "HIGH CUT",
    valueText: `${lerp(2, 18, k.hiCut).toFixed(1)} kHz`,
    below: true
  }), /*#__PURE__*/React.createElement(DKnob, {
    value: k.output,
    onChange: set('output'),
    radius: 26,
    color: "var(--steel)",
    label: "OUTPUT",
    valueText: `${lerp(-24, 6, k.output).toFixed(1)} dB`,
    below: true
  }), /*#__PURE__*/React.createElement(Latch, {
    on: lim,
    label: "LIM",
    color: "var(--amber)",
    onClick: () => setLim(!lim)
  }), /*#__PURE__*/React.createElement(Terminal, {
    label: "OUT",
    onClick: hit
  }), /*#__PURE__*/React.createElement(SegmentMeter, {
    level: hitLevel,
    color: "var(--steel)",
    segments: 7
  })))), /*#__PURE__*/React.createElement("div", {
    style: {
      textAlign: 'right',
      marginTop: 20
    }
  }, /*#__PURE__*/React.createElement(Wordmark, {
    size: 16
  }, "WHOOMP"), /*#__PURE__*/React.createElement("div", {
    style: {
      fontSize: 7.5,
      color: 'var(--ink-38)',
      letterSpacing: '.1em',
      marginTop: 2
    }
  }, "KICK SYNTHESISER")));
}
ReactDOM.createRoot(document.getElementById('root')).render(/*#__PURE__*/React.createElement(WhoompPanel, null));
})(); } catch (e) { __ds_ns.__errors.push({ path: "ui_kits/whoomp/WhoompPanel.jsx", error: String((e && e.message) || e) }); }

__ds_ns.Fader = __ds_scope.Fader;

__ds_ns.Knob = __ds_scope.Knob;

__ds_ns.Latch = __ds_scope.Latch;

__ds_ns.Selector = __ds_scope.Selector;

__ds_ns.Lamp = __ds_scope.Lamp;

__ds_ns.Meter = __ds_scope.Meter;

__ds_ns.SegmentMeter = __ds_scope.SegmentMeter;

__ds_ns.PanelFrame = __ds_scope.PanelFrame;

__ds_ns.Terminal = __ds_scope.Terminal;

__ds_ns.Wordmark = __ds_scope.Wordmark;

})();
