// Project Seoul Development Harness - one-time worker setup.
//
// Panel behavior and storage access-level configuration. These settings persist
// across worker restarts, so they run on install/startup only and there is no
// boot-time session reconcile: a worker restart must not stop valid sessions.

// Disable auto-open so that clicking the toolbar action fires action.onClicked.
// That click is what confers the activeTab grant for the clicked tab; auto-open
// would open the panel without granting access. We open the panel ourselves.
export async function configurePanelBehavior(): Promise<void> {
  try {
    if (chrome.sidePanel && typeof chrome.sidePanel.setPanelBehavior === 'function') {
      await chrome.sidePanel.setPanelBehavior({ openPanelOnActionClick: false });
    }
  } catch {
    // Non-fatal on builds without setPanelBehavior.
  }
}

// Restrict extension storage to trusted contexts. Applies to both the durable
// control-session store (storage.local) and the browser-session invocation
// context (storage.session).
export async function restrictStorageAccess(): Promise<void> {
  const areas: Array<'local' | 'session'> = ['local', 'session'];
  for (const area of areas) {
    try {
      const store = chrome.storage?.[area] as unknown as {
        setAccessLevel?: (opts: { accessLevel: string }) => Promise<void>;
      };
      if (store && typeof store.setAccessLevel === 'function') {
        await store.setAccessLevel({ accessLevel: 'TRUSTED_CONTEXTS' });
      }
    } catch {
      // Older Chrome builds do not support storage access levels.
    }
  }
}
