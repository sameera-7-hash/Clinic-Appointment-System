import React, { useEffect } from "react";
import { cn } from "../../lib/cn";

// A minimal shadcn-style Dialog -- overlay + centered panel, closes on
// Escape or an overlay click. No Radix/focus-trap dependency; fine for
// the app's short confirm/booking flows.
export function Dialog({ open, onClose, className, children }) {
  useEffect(() => {
    if (!open) return;
    const onKey = (e) => {
      if (e.key === "Escape") onClose?.();
    };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [open, onClose]);

  if (!open) return null;

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center bg-black/40 p-4 backdrop-blur-sm"
      onClick={onClose}
    >
      <div
        className={cn(
          "w-full max-w-md rounded-xl border border-border bg-white p-6 shadow-2xl animate-[dialog-in_0.18s_ease]",
          className
        )}
        onClick={(e) => e.stopPropagation()}
        role="dialog"
        aria-modal="true"
      >
        {children}
      </div>
    </div>
  );
}

export function DialogHeader({ className, ...props }) {
  return <div className={cn("mb-4", className)} {...props} />;
}

export function DialogTitle({ className, ...props }) {
  return <h2 className={cn("text-lg font-bold text-ink", className)} {...props} />;
}

export function DialogDescription({ className, ...props }) {
  return <p className={cn("mt-1 text-sm text-muted", className)} {...props} />;
}

export function DialogFooter({ className, ...props }) {
  return <div className={cn("mt-5 flex justify-end gap-2", className)} {...props} />;
}
