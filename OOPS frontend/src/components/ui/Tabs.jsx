import React, { createContext, useContext } from "react";
import { cn } from "../../lib/cn";

// A minimal shadcn-style Tabs (compound components, controlled value) --
// no Radix dependency, just a tiny context so TabsTrigger/TabsContent
// know the active value.
const TabsContext = createContext(null);

export function Tabs({ value, onValueChange, className, children }) {
  return (
    <TabsContext.Provider value={{ value, onValueChange }}>
      <div className={cn(className)}>{children}</div>
    </TabsContext.Provider>
  );
}

export function TabsList({ className, children }) {
  return (
    <div className={cn("inline-flex w-full gap-1 rounded-lg bg-bg p-1", className)}>
      {children}
    </div>
  );
}

export function TabsTrigger({ value, className, children }) {
  const ctx = useContext(TabsContext);
  const active = ctx?.value === value;
  return (
    <button
      type="button"
      onClick={() => ctx?.onValueChange(value)}
      className={cn(
        "flex-1 rounded-md px-3 py-1.5 text-xs font-semibold transition-colors",
        active ? "bg-white text-brand-green shadow-sm" : "text-muted hover:text-ink",
        className
      )}
    >
      {children}
    </button>
  );
}

export function TabsContent({ value, className, children }) {
  const ctx = useContext(TabsContext);
  if (ctx?.value !== value) return null;
  return <div className={cn(className)}>{children}</div>;
}
