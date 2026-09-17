import React, { useEffect, useState } from "react";
import { getNotifications, markNotificationRead } from "../Services/api";

// Reminder bell for the top bar. Polls /api/notifications for the current
// role/user every 15s (appointment booking is what creates these, see
// pushNotification() in main.cpp) and lets the viewer mark one read.
function NotificationBell({ role, userId }) {
  const [items, setItems] = useState([]);
  const [open, setOpen] = useState(false);

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
    <div className="notification-bell">
      <button
        type="button"
        className="icon-btn notification-toggle"
        onClick={() => setOpen((o) => !o)}
      >
        🔔
        {unread > 0 && <span className="notification-badge">{unread}</span>}
      </button>

      {open && (
        <>
          <div className="notification-scrim" onClick={() => setOpen(false)} />
          <div className="notification-dropdown">
            <div className="notification-dropdown-header">Reminders</div>

            {items.length === 0 && (
              <p className="notification-empty">You're all caught up.</p>
            )}

            {items.map((n) => (
              <button
                type="button"
                key={n.id}
                className={`notification-item${n.read ? "" : " unread"}`}
                onClick={() => dismiss(n.id)}
              >
                <p>{n.message}</p>
                <small>{new Date(n.createdAt).toLocaleString()}</small>
              </button>
            ))}
          </div>
        </>
      )}
    </div>
  );
}

export default NotificationBell;
