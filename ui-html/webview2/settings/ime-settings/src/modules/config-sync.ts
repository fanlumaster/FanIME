import { applyCandidateArrange, applyToggleState } from './shared';

function safeParseJson(value: string): unknown {
  try {
    return JSON.parse(value);
  } catch {
    return null;
  }
}

export function setupConfigSync(): void {
  if (!window.chrome?.webview) {
    return;
  }

  window.chrome.webview.addEventListener('message', (event: Event & { data?: any }) => {
    const payload = typeof event.data === 'string' ? safeParseJson(event.data) : event.data;
    if (!payload || typeof payload !== 'object' || payload.type !== 'configSnapshot') {
      return;
    }

    applyCandidateArrange(payload.data?.appearance?.candidate_window_layout);
    if (typeof payload.data?.general?.floating_toolbar === 'boolean') {
      applyToggleState('ftbToggleBtn', payload.data.general.floating_toolbar);
    }
  });

  window.chrome.webview.postMessage(JSON.stringify({ type: 'configRequest' }));
}

export function updateConfig(path: string, value: string | boolean | number): void {
  window.chrome?.webview?.postMessage(JSON.stringify({
    type: 'configUpdate',
    data: { path, value }
  }));
}
