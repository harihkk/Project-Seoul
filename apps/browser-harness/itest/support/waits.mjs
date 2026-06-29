// Bounded polling. No arbitrary readiness sleeps: every wait polls an explicit
// condition until it holds or a descriptive deadline error is thrown.

export async function waitFor(label, fn, { timeoutMs = 15000, intervalMs = 150 } = {}) {
  const deadline = Date.now() + timeoutMs;
  let lastErr = null;
  let attempts = 0;
  while (Date.now() < deadline) {
    attempts += 1;
    try {
      const value = await fn();
      if (value) return value;
    } catch (e) {
      lastErr = e;
    }
    await new Promise((r) => setTimeout(r, intervalMs));
  }
  throw new Error(
    `waitFor timed out: ${label} (after ${timeoutMs}ms, ${attempts} attempts)` +
      (lastErr ? ` lastError=${lastErr.message}` : ''),
  );
}

export async function waitGone(label, fn, opts = {}) {
  return waitFor(`${label} (to disappear)`, async () => !(await fn()), opts);
}
