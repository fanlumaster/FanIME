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
    if (typeof payload.data?.general?.paging_minus_equal === 'boolean') {
      const checkbox = document.getElementById('pagingMinusEqualCheckbox') as HTMLInputElement | null;
      if (checkbox) checkbox.checked = payload.data.general.paging_minus_equal;
    }
    if (typeof payload.data?.general?.paging_tab === 'boolean') {
      const checkbox = document.getElementById('pagingTabCheckbox') as HTMLInputElement | null;
      if (checkbox) checkbox.checked = payload.data.general.paging_tab;
    }
    if (typeof payload.data?.helpcode?.show_sp_helpcode_in_candidate_window === 'boolean') {
      applyToggleState(
        'showShuangpinHelpcodeToggleBtn',
        payload.data.helpcode.show_sp_helpcode_in_candidate_window
      );
    }
    if (typeof payload.data?.helpcode?.shuangpin_helpcode === 'boolean') {
      applyToggleState('shuangpinHelpcodeToggleBtn', payload.data.helpcode.shuangpin_helpcode);
    }
    if (typeof payload.data?.helpcode?.quanpin_helpcode === 'boolean') {
      applyToggleState('quanpinHelpcodeToggleBtn', payload.data.helpcode.quanpin_helpcode);
    }
    if (typeof payload.data?.helpcode?.show_qp_helpcode_in_candidate_window === 'boolean') {
      applyToggleState(
        'showQuanpinHelpcodeToggleBtn',
        payload.data.helpcode.show_qp_helpcode_in_candidate_window
      );
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
