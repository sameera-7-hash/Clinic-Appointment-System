import React from "react";
import { cn } from "../../lib/cn";

const variants = {
  default: "bg-brand-green/10 text-brand-green",
  success: "bg-emerald-100 text-emerald-700",
  warning: "bg-amber-100 text-amber-700",
  danger: "bg-red-100 text-red-700",
  outline: "border border-border text-ink bg-transparent",
};

export function Badge({ variant = "default", className, ...props }) {
  return (
    <span
      className={cn(
        "inline-flex items-center rounded-full px-2.5 py-1 text-[10px] font-bold uppercase tracking-wide",
        variants[variant],
        className
      )}
      {...props}
    />
  );
}
