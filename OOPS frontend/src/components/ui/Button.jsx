import React from "react";
import { cn } from "../../lib/cn";

// A small shadcn-style Button (variant + size props, no shadcn/Radix
// dependency) built directly on Tailwind utilities and the app's brand
// theme tokens (see the @theme block in App.css).
const variants = {
  primary: "bg-brand-green text-white hover:bg-brand-green-dark",
  accent: "bg-brand-lime text-brand-green-dark hover:brightness-95",
  outline: "border border-border bg-white text-ink hover:bg-bg",
  ghost: "bg-transparent text-ink hover:bg-black/5",
  destructive: "bg-danger text-white hover:brightness-95",
};

const sizes = {
  sm: "px-3 py-1.5 text-xs rounded-md",
  md: "px-4 py-2 text-sm rounded-lg",
  lg: "px-6 py-3 text-sm rounded-lg",
};

export function Button({ variant = "primary", size = "md", className, ...props }) {
  return (
    <button
      className={cn(
        "inline-flex items-center justify-center gap-2 font-semibold transition-all duration-150 active:scale-[0.98] disabled:opacity-50 disabled:pointer-events-none",
        variants[variant],
        sizes[size],
        className
      )}
      {...props}
    />
  );
}
