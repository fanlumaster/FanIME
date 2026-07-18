import { applyDropdownValue, setupDropdownMenu, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

const fields: Record<string, string> = {
  voiceAsrProvider: 'voice_input.asr_provider', voiceAsrToken: 'voice_input.asr_token',
  voiceAsrEndpoint: 'voice_input.asr_endpoint', voicePolishProvider: 'voice_input.polish_provider',
  voicePolishToken: 'voice_input.polish_token', voicePolishEndpoint: 'voice_input.polish_endpoint'
};

export function setupVoiceInput(): void {
  setupToggleButton('voiceEnabled', value => updateConfig('voice_input.voice_input', value));
  setupToggleButton('voicePolishText', value => updateConfig('voice_input.polish_text', value));
  setupToggleButton('voiceNotificationSound', value => updateConfig('voice_input.notification_sound', value));
  setupDropdownMenu('voiceLanguageBtn', 'voiceLanguageMenu', 'changeVoiceLanguage', true,
    'voice_input.language');
  Object.entries(fields).forEach(([id, path]) => {
    const element = document.getElementById(id) as HTMLInputElement | HTMLSelectElement | null;
    element?.addEventListener('change', () => updateConfig(path, element.value.trim()));
  });
}

export function applyVoiceConfig(config: Record<string, unknown>): void {
  Object.entries(fields).forEach(([id, path]) => {
    const key = path.split('.')[1];
    const element = document.getElementById(id) as HTMLInputElement | HTMLSelectElement | null;
    if (element && typeof config[key] === 'string') element.value = config[key] as string;
  });
  applyDropdownValue('voiceLanguageBtn', 'voiceLanguageMenu',
    typeof config.language === 'string' ? config.language : undefined);
}
