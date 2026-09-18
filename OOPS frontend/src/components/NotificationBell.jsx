import React, { useEffect, useState } from "react";
import { getNotifications, markNotificationRead } from "../Services/api";
import { DropdownMenu, DropdownMenuItem, DropdownMenuLabel } from "./ui/DropdownMenu";
import { cn } from "../lib/cn";

// Reminder bell for the top bar. Polls /api/notifications for the current
// role/user every 15s (appointment booking is what creates these, see
// pushNotification() in main.cpp) and lets the viewer mark one read.
function NotificationBell({ role, userId }) {
  const [items, setItems] = useState([]);

  useEffect(() => {
    if (!role) return;
    let cancelled = false;

    const load = () => {
      getNotifications(role, userId)
        .then((data) => {
          if (!cancelled) setItems(data);
        })
        .catch(() => {});
    };

    load();
    const interval = setInterval(load, 15000);
    return () => {
      cancelled = true;
      clearInterval(interval);
    };
  }, [role, userId]);

  const unread = items.filter((n) => !n.read).length;

  const dismiss = (id) => {
    markNotificationRead(id).catch(() => {});
    setItems((prev) => prev.map((n) => (n.id === id ? { ...n, read: true } : n)));
  };

  return (
    <DropdownMenu
      trigger={
        <button type="button" className="icon-btn notification-toggle">
          🔔
          {unread > 0 && <span className="notification-badge">{unread}</span>}
        </button>
      }
    >
      <DropdownMenuLabel>Reminders</DropdownMenuLabel>

      {items.length === 0 && (
        <p className="px-3 py-3 text-xs text-muted">You're all caught up.</p>
      )}

      {items.map((n) => (
        <DropdownMenuItem
          key={n.id}
          onClick={() => dismiss(n.id)}
          className={cn(!n.read && "bg-brand-green/5")}
        >
          <p className="text-ink">{n.message}</p>
          <small className="mt-0.5 block text-[10px] text-muted">
            {new Date(n.createdAt).toLocaleString()}
          </small>
        </DropdownMenuItem>
      ))}
    </DropdownMenu>
  );
}

export default NotificationBell;
