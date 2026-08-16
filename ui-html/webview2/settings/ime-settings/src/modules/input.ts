import { applyDropdownValue, setupDropdownMenu, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';
import { updateCandidatePreviewHelpcode } from './appearance';

type InputScheme = 'quanpin' | 'shuangpin' | 'wubi';

function updateInputConfig(path: string, value: string): void {
  window.chrome?.webview?.postMessage(JSON.stringify({
    type: 'configUpdate',
    data: { path, value }
  }));
}

export function applyInputConfig(
  schema: string | undefined,
  characterSet: string | undefined,
  shuangpinSchema: string | undefined,
  wubiSchema: string | undefined,
  defaultImeMode?: string | undefined,
  imeModeScope?: string | undefined
): void {
  if (schema === 'quanpin' || schema === 'shuangpin' || schema === 'wubi') {
    const radio = document.querySelector<HTMLInputElement>(`input[name="input-method"][value="${schema}"]`);
    if (radio) {
      radio.checked = true;
    }
    updateCandidatePreviewHelpcode({ input_schema: schema });
  }

  applyDropdownValue('characterSetBtn', 'characterSetMenu', characterSet);
  applyDropdownValue('shuangpinSchemeBtn', 'shuangpinSchemeMenu', shuangpinSchema);
  applyDropdownValue('wubiSchemeBtn', 'wubiSchemeMenu', wubiSchema);
  applyDropdownValue('defaultImeModeBtn', 'defaultImeModeMenu', defaultImeMode);
  applyDropdownValue('imeModeScopeBtn', 'imeModeScopeMenu', imeModeScope);
}

export function applyFrequencyConfig(config: any): void {
  applyDropdownValue('frequencyModeBtn', 'frequencyModeMenu', config?.mode);
  applyDropdownValue('frequencyTriggerCountBtn', 'frequencyTriggerCountMenu', String(config?.trigger_count ?? 1));
  applyDropdownValue('frequencyLinearStepBtn', 'frequencyLinearStepMenu', String(config?.linear_step ?? 1));
}

export function setupInput(): void {
  setupDropdownMenu('characterSetBtn', 'characterSetMenu', 'changeCharacterSet', true, 'input.character_set');
  setupDropdownMenu('defaultImeModeBtn', 'defaultImeModeMenu', '', true, 'input.default_ime_mode');
  setupDropdownMenu('imeModeScopeBtn', 'imeModeScopeMenu', '', true, 'input.ime_mode_scope');
  document.querySelectorAll<HTMLInputElement>('input[name="input-method"]').forEach((radio) => {
    radio.addEventListener('change', () => {
      if (!radio.checked || (radio.value !== 'quanpin' && radio.value !== 'shuangpin' && radio.value !== 'wubi')) {
        return;
      }
      const schema = radio.value as InputScheme;
      updateInputConfig('input.schema', schema);
      updateCandidatePreviewHelpcode({ input_schema: schema });
    });
  });

  setupDropdownMenu(
    'shuangpinSchemeBtn',
    'shuangpinSchemeMenu',
    'changeShuangpinScheme',
    true,
    'input.shuangpin_schema'
  );
  setupDropdownMenu('wubiSchemeBtn', 'wubiSchemeMenu', 'changeWubiScheme', true, 'input.wubi_schema');

  setupPageOptions();
  setupFrequencyOptions();
  setupToggleButton('wordToCharacterToggleBtn', (active) => {
    updateConfig('input.word_to_character', active);
  });
  setupToggleButton('smartPunctuationToggleBtn', (active) => {
    updateConfig('input.smart_punctuation', active);
  });
  setupToggleButton('smartPunctuationRepeatToChineseToggleBtn', (active) => {
    updateConfig('input.smart_punctuation_repeat_to_chinese', active);
  });
  setupToggleButton('pairedPunctuationToggleBtn', (active) => {
    updateConfig('input.paired_punctuation', active);
  });
  setupToggleButton('zhEnToggleBtn', (active) => {
    updateConfig('general.cn_en_mixed_input', active);
  });
  setupToggleButton('emojiMixedInputToggleBtn', (active) => {
    updateConfig('general.emoji_mixed_input', active);
  });
  setupToggleButton('cloudCandidatesToggleBtn', (active) => {
    updateConfig('general.cloud_candidates', active);
  });
}

function setupFrequencyOptions(): void {
  setupDropdownMenu('frequencyModeBtn', 'frequencyModeMenu', '', true, 'frequency_adjustment.mode');
  setupDropdownMenu('frequencyTriggerCountBtn', 'frequencyTriggerCountMenu', '', true,
    'frequency_adjustment.trigger_count', Number);
  setupDropdownMenu('frequencyLinearStepBtn', 'frequencyLinearStepMenu', '', true,
    'frequency_adjustment.linear_step', Number);
}

function setupPageOptions(): void {
  document.querySelectorAll<HTMLInputElement>('input[name="page-method"]').forEach((checkbox) => {
    checkbox.addEventListener('change', () => {
      const configPaths: Record<string, string> = {
        minus: 'general.paging_minus_equal',
        comma: 'general.paging_comma_period',
        tab: 'general.paging_tab',
        page: 'general.paging_page_up_down',
        arrow: 'general.candidate_arrow_navigation'
      };
      const path = configPaths[checkbox.value];
      if (path) updateConfig(path, checkbox.checked);
    });
  });
}
