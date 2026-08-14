import { applyDropdownValue, setupDropdownMenu, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

const fields: Record<string, string> = {
  voiceAsrProvider: 'voice_input.asr_provider', voiceAsrAppKey: 'voice_input.asr_app_key',
  voiceAsrToken: 'voice_input.asr_token',
  voiceAsrEndpoint: 'voice_input.asr_endpoint', voicePolishProvider: 'voice_input.polish_provider',
  voicePolishToken: 'voice_input.polish_token', voicePolishEndpoint: 'voice_input.polish_endpoint'
};

const doubaoEndpoint = 'wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async';
const siliconflowEndpoint = 'https://api.siliconflow.cn/v1/audio/transcriptions';

function updateAsrProvider(providerInput: HTMLInputElement | HTMLSelectElement): void {
  const provider = providerInput.value.trim().toLowerCase();
  updateConfig('voice_input.asr_provider', provider);
  const endpointInput = document.getElementById('voiceAsrEndpoint') as HTMLInputElement | null;
  if (!endpointInput) return;
  const endpoint = endpointInput.value.trim();
  const replacement = provider === 'doubao' && (!endpoint || endpoint === siliconflowEndpoint)
    ? doubaoEndpoint
    : provider === 'siliconflow' && (!endpoint || endpoint === doubaoEndpoint)
      ? siliconflowEndpoint
      : null;
  if (replacement) {
    endpointInput.value = replacement;
    updateConfig('voice_input.asr_endpoint', replacement);
  }
}

export function setupVoiceInput(): void {
  setupToggleButton('voiceEnabled', value => updateConfig('voice_input.voice_input', value));
  setupToggleButton('voicePolishText', value => updateConfig('voice_input.polish_text', value));
  setupToggleButton('voiceNotificationSound', value => updateConfig('voice_input.notification_sound', value));
  const hotkeyPaths: Record<string, string> = {
    ralt: 'voice_input.hotkey_ralt',
    'ctrl-f9': 'voice_input.hotkey_ctrl_f9',
    'ctrl-win': 'voice_input.hotkey_ctrl_win',
    'rctrl-ralt': 'voice_input.hotkey_rctrl_ralt',
    'hold-space-lock': 'voice_input.hotkey_hold_space_lock'
  };
  document.querySelectorAll<HTMLInputElement>('input[name="voice-hotkey"]').forEach((checkbox) => {
    checkbox.addEventListener('change', () => {
      const path = hotkeyPaths[checkbox.value];
      if (path) updateConfig(path, checkbox.checked);
    });
  });
  setupDropdownMenu('voiceLanguageBtn', 'voiceLanguageMenu', 'changeVoiceLanguage', true,
    'voice_input.language');
  setupDropdownMenu('voiceCommitModeBtn', 'voiceCommitModeMenu', 'changeVoiceCommitMode', true,
    'voice_input.commit_mode');
  Object.entries(fields).forEach(([id, path]) => {
    const element = document.getElementById(id) as HTMLInputElement | HTMLSelectElement | null;
    element?.addEventListener('change', () => {
      if (id === 'voiceAsrProvider') updateAsrProvider(element);
      else updateConfig(path, element.value.trim());
    });
  });
}

export function applyVoiceConfig(config: Record<string, unknown>): void {
  const hotkeyCheckboxes: Record<string, string> = {
    voiceHotkeyRAltCheckbox: 'hotkey_ralt',
    voiceHotkeyCtrlF9Checkbox: 'hotkey_ctrl_f9',
    voiceHotkeyCtrlWinCheckbox: 'hotkey_ctrl_win',
    voiceHotkeyRCtrlRAltCheckbox: 'hotkey_rctrl_ralt',
    voiceHotkeyHoldSpaceLockCheckbox: 'hotkey_hold_space_lock'
  };
  Object.entries(hotkeyCheckboxes).forEach(([id, key]) => {
    const checkbox = document.getElementById(id) as HTMLInputElement | null;
    if (checkbox && typeof config[key] === 'boolean') checkbox.checked = config[key] as boolean;
  });
  Object.entries(fields).forEach(([id, path]) => {
    const key = path.split('.')[1];
    const element = document.getElementById(id) as HTMLInputElement | HTMLSelectElement | null;
    if (element && typeof config[key] === 'string') element.value = config[key] as string;
  });
  applyDropdownValue('voiceLanguageBtn', 'voiceLanguageMenu',
    typeof config.language === 'string' ? config.language : undefined);
  applyDropdownValue('voiceCommitModeBtn', 'voiceCommitModeMenu',
    typeof config.commit_mode === 'string' ? config.commit_mode : undefined);
}
