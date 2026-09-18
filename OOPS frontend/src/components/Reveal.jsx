import React, { useEffect, useRef, useState } from "react";

// Wraps children in an element that fades/slides into place the first time
// it scrolls into view (or immediately, if it's already on screen at
// mount -- e.g. above-the-fold content on Login/RoleSelection). `delay`
// (ms) staggers a group of siblings; `as` picks the wrapper tag so this
// can wrap a <div>, a grid item, etc. without adding an extra layout box
// where a specific tag is expected.
function Reveal({ children, delay = 0, className = "", as = "div" }) {
  const ref = useRef(null);
  const [visible, setVisible] = useState(false);
  const Tag = as;

  useEffect(() => {
    const el = ref.current;
    if (!el) return;

    if (typeof IntersectionObserver === "undefined") {
      setVisible(true);
      return;
    }

    const observer = new IntersectionObserver(
      ([entry]) => {
        if (entry.isIntersecting) {
          setVisible(true);
          observer.disconnect();
        }
      },
      { threshold: 0.15 }
    );
    observer.observe(el);
    return () => observer.disconnect();
  }, []);

  return (
    <Tag
      ref={ref}
      className={`reveal${visible ? " reveal-visible" : ""}${className ? ` ${className}` : ""}`}
      style={{ transitionDelay: `${delay}ms` }}
    >
      {children}
    </Tag>
  );
}

export default Reveal;
