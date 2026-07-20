import { applyCandidateArrange, applyDropdownValue, applyToggleState } from './shared';
import { applyInputConfig } from './input';
import { applyVoiceConfig } from './voice';
import { applyAiConfig } from './ai-settings';

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
    applyInputConfig(
      payload.data?.input?.schema,
      payload.data?.input?.character_set,
      payload.data?.input?.shuangpin_schema,
      payload.data?.input?.wubi_schema
    );
    if (payload.data?.voice_input && typeof payload.data.voice_input === 'object') {
      applyVoiceConfig(payload.data.voice_input);
      if (typeof payload.data.voice_input.enabled === 'boolean') applyToggleState('voiceEnabled', payload.data.voice_input.enabled);
      if (typeof payload.data.voice_input.polish_text === 'boolean') applyToggleState('voicePolishText', payload.data.voice_input.polish_text);
      if (typeof payload.data.voice_input.notification_sound === 'boolean') applyToggleState('voiceNotificationSound', payload.data.voice_input.notification_sound);
    }
    if (payload.data?.ai_assistant && typeof payload.data.ai_assistant === 'object') {
      applyAiConfig(payload.data.ai_assistant);
      if (typeof payload.data.ai_assistant.enabled === 'boolean') {
        applyToggleState('aiEnabled', payload.data.ai_assistant.enabled);
      }
    }
    if (typeof payload.data?.general?.floating_toolbar === 'boolean') {
      applyToggleState('ftbToggleBtn', payload.data.general.floating_toolbar);
    }
    if (typeof payload.data?.general?.cn_en_mixed_input === 'boolean') {
      applyToggleState('zhEnToggleBtn', payload.data.general.cn_en_mixed_input);
    }
    if (typeof payload.data?.general?.cloud_candidates === 'boolean') {
      applyToggleState('cloudCandidatesToggleBtn', payload.data.general.cloud_candidates);
    }
    if (typeof payload.data?.utility?.unicode_mode === 'boolean') {
      applyToggleState('unicodeModeToggleBtn', payload.data.utility.unicode_mode);
    }
    if (typeof payload.data?.utility?.quick_phrase === 'boolean') {
      applyToggleState('quickPhraseToggleBtn', payload.data.utility.quick_phrase);
    }
    if (typeof payload.data?.general?.paging_minus_equal === 'boolean') {
      const checkbox = document.getElementById('pagingMinusEqualCheckbox') as HTMLInputElement | null;
      if (checkbox) checkbox.checked = payload.data.general.paging_minus_equal;
    }
    if (typeof payload.data?.general?.paging_tab === 'boolean') {
      const checkbox = document.getElementById('pagingTabCheckbox') as HTMLInputElement | null;
      if (checkbox) checkbox.checked = payload.data.general.paging_tab;
    }
    if (typeof payload.data?.general?.paging_comma_period === 'boolean') {
      const checkbox = document.getElementById('pagingCommaPeriodCheckbox') as HTMLInputElement | null;
      if (checkbox) checkbox.checked = payload.data.general.paging_comma_period;
    }
    if (typeof payload.data?.general?.paging_page_up_down === 'boolean') {
      const checkbox = document.getElementById('pagingPageUpDownCheckbox') as HTMLInputElement | null;
      if (checkbox) checkbox.checked = payload.data.general.paging_page_up_down;
    }
    if (typeof payload.data?.general?.candidate_arrow_navigation === 'boolean') {
      const checkbox = document.getElementById('candidateArrowNavigationCheckbox') as HTMLInputElement | null;
      if (checkbox) checkbox.checked = payload.data.general.candidate_arrow_navigation;
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
    applyDropdownValue('shuangpinHelpcodeSchemeBtn', 'shuangpinHelpcodeSchemeMenu',
      payload.data?.helpcode?.shuangpin_helpcode_schema);
    if (typeof payload.data?.helpcode?.quanpin_helpcode === 'boolean') {
      applyToggleState('quanpinHelpcodeToggleBtn', payload.data.helpcode.quanpin_helpcode);
    }
    applyDropdownValue('quanpinHelpcodeSchemeBtn', 'quanpinHelpcodeSchemeMenu',
      payload.data?.helpcode?.quanpin_helpcode_schema);
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
