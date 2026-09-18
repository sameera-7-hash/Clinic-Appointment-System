import React, { useEffect, useRef, useState } from "react";
import { cn } from "../../lib/cn";

// A minimal shadcn-style DropdownMenu: click the trigger to toggle a
// floating panel, click-outside or Escape to close. `children` can be a
// render prop (({ close }) => ...) when an item needs to close the menu
// after acting, or plain JSX otherwise.
export function DropdownMenu({ trigger, children, align = "right", className }) {
  const [open, setOpen] = useState(false);
  const ref = useRef(null);

  useEffect(() => {
    if (!open) return;
    const onClick = (e) => {
      if (ref.current && !ref.current.contains(e.target)) setOpen(false);
    };
    const onKey = (e) => {
      if (e.key === "Escape") setOpen(false);
    };
    document.addEventListener("mousedown", onClick);
    document.addEventListener("keydown", onKey);
    return () => {
      document.removeEventListener("mousedown", onClick);
      document.removeEventListener("keydown", onKey);
    };
  }, [open]);

  return (
    <div className="relative inline-flex" ref={ref}>
      <div onClick={() => setOpen((o) => !o)}>{trigger}</div>

      {open && (
        <div
          className={cn(
            "absolute top-full z-40 mt-2 max-h-96 w-72 overflow-y-auto rounded-lg border border-border bg-white p-1.5 shadow-2xl",
            align === "right" ? "right-0" : "left-0",
            className
          )}
        >
          {typeof children === "function" ? children({ close: () => setOpen(false) }) : children}
        </div>
      )}
    </div>
  );
}

export function DropdownMenuItem({ className, ...props }) {
  return (
    <button
      type="button"
      className={cn(
        "block w-full rounded-md px-3 py-2 text-left text-xs text-ink transition-colors hover:bg-bg",
        className
      )}
      {...props}
    />
  );
}

export function DropdownMenuLabel({ className, ...props }) {
  return (
    <div
      className={cn("px-3 pb-1 pt-2 text-[10px] font-bold uppercase tracking-wide text-muted", className)}
      {...props}
    />
  );
}
