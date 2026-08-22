import { applyDropdownValue, applyToggleState, setupDropdownMenu, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';
import { updateCandidatePreviewHelpcode } from './appearance';

type InputScheme = 'quanpin' | 'shuangpin' | 'wubi';
type InputMode = 'chinese' | 'japanese';

let applyingInputConfig = false;

function updateInputConfig(path: string, value: string): void {
  window.chrome?.webview?.postMessage(JSON.stringify({
    type: 'configUpdate',
    data: { path, value }
  }));
}

export function applyInputConfig(
  inputMode: string | undefined,
  schema: string | undefined,
  characterSet: string | undefined,
  shuangpinSchema: string | undefined,
  wubiSchema: string | undefined,
  defaultImeMode?: string | undefined,
  imeModeScope?: string | undefined,
  japaneseSchema?: string | undefined
): void {
  applyingInputConfig = true;
  try {
    const mode: InputMode = inputMode === 'japanese' ? 'japanese' : 'chinese';
  const modeRadio = document.querySelector<HTMLInputElement>(`input[name="input-mode"][value="${mode}"]`);
  if (modeRadio) modeRadio.checked = true;
  syncInputModeView(mode);

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
  const japaneseRadio = document.querySelector<HTMLInputElement>(
    `input[name="japanese-input-method"][value="${japaneseSchema === 'romaji' ? japaneseSchema : 'romaji'}"]`
  );
  if (japaneseRadio) japaneseRadio.checked = true;
  } finally {
    applyingInputConfig = false;
  }
}

function syncInputModeView(mode: InputMode): void {
  const japanese = mode === 'japanese';
  document.querySelectorAll<HTMLElement>('.chinese-scheme-settings').forEach((element) => {
    element.hidden = japanese;
  });
  document.querySelectorAll<HTMLElement>('.japanese-scheme-settings').forEach((element) => {
    element.hidden = !japanese;
  });
}

export function applyFrequencyConfig(config: any): void {
  applyDropdownValue('frequencyModeBtn', 'frequencyModeMenu', config?.mode);
  applyDropdownValue('frequencyTriggerCountBtn', 'frequencyTriggerCountMenu', String(config?.trigger_count ?? 1));
  applyDropdownValue('frequencyLinearStepBtn', 'frequencyLinearStepMenu', String(config?.linear_step ?? 1));
}

function syncZhEnMixedInputOptionsEnabled(enabled: boolean): void {
  document.getElementById('zhEnMixedInputOptions')?.classList.toggle('is-disabled', !enabled);
}

export function applyZhEnMixedInputConfig(enabled?: boolean, minChars?: number): void {
  if (typeof enabled === 'boolean') {
    applyToggleState('zhEnToggleBtn', enabled);
    syncZhEnMixedInputOptionsEnabled(enabled);
  }
  if (typeof minChars === 'number' && Number.isFinite(minChars)) {
    applyDropdownValue('zhEnTriggerLengthBtn', 'zhEnTriggerLengthMenu', String(minChars));
  }
}

function syncCandidateTranslationOptions(enabled: boolean): void {
  document.getElementById('candidateTranslationApiOptions')?.classList.toggle('is-disabled', !enabled);
}

function syncCandidateTranslationWarning(): void {
  const secretId = (document.getElementById('tencentTmtSecretId') as HTMLInputElement | null)?.value.trim();
  const secretKey = (document.getElementById('tencentTmtSecretKey') as HTMLInputElement | null)?.value.trim();
  document.getElementById('candidateTranslationApiWarning')?.classList.toggle(
    'is-hidden', Boolean(secretId && secretKey)
  );
}

export function applyTencentTmtConfig(config: Record<string, unknown> | undefined): void {
  const secretId = document.getElementById('tencentTmtSecretId') as HTMLInputElement | null;
  const secretKey = document.getElementById('tencentTmtSecretKey') as HTMLInputElement | null;
  if (secretId && typeof config?.secret_id === 'string') secretId.value = config.secret_id;
  if (secretKey && typeof config?.secret_key === 'string') secretKey.value = config.secret_key;
  syncCandidateTranslationWarning();
}

export function setupInput(): void {
  document.querySelectorAll<HTMLInputElement>('input[name="input-mode"]').forEach((radio) => {
    radio.addEventListener('change', () => {
      if (applyingInputConfig) return;
      if (!radio.checked || (radio.value !== 'chinese' && radio.value !== 'japanese')) return;
      const mode = radio.value as InputMode;
      syncInputModeView(mode);
      updateInputConfig('input.mode', mode);
    });
  });

  document.querySelectorAll<HTMLInputElement>('input[name="japanese-input-method"]').forEach((radio) => {
    radio.addEventListener('change', () => {
      if (radio.checked && radio.value === 'romaji' && !applyingInputConfig) {
        updateInputConfig('input.japanese_schema', radio.value);
      }
    });
  });

  setupDropdownMenu('characterSetBtn', 'characterSetMenu', 'changeCharacterSet', true, 'input.character_set');
  setupDropdownMenu('defaultImeModeBtn', 'defaultImeModeMenu', '', true, 'input.default_ime_mode');
  setupDropdownMenu('imeModeScopeBtn', 'imeModeScopeMenu', '', true, 'input.ime_mode_scope');
  document.querySelectorAll<HTMLInputElement>('input[name="input-method"]').forEach((radio) => {
    radio.addEventListener('change', () => {
      if (applyingInputConfig) return;
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
    syncZhEnMixedInputOptionsEnabled(active);
    updateConfig('general.cn_en_mixed_input', active);
  });
  setupToggleButton('candidateTranslationsToggleBtn', (active) => {
    syncCandidateTranslationOptions(active);
    updateConfig('general.candidate_translations', active);
  });
  const translationFields: Record<string, string> = {
    tencentTmtSecretId: 'tencent_tmt.secret_id',
    tencentTmtSecretKey: 'tencent_tmt.secret_key'
  };
  Object.entries(translationFields).forEach(([id, path]) => {
    const input = document.getElementById(id) as HTMLInputElement | null;
    input?.addEventListener('change', () => {
      input.value = input.value.trim();
      updateConfig(path, input.value);
      syncCandidateTranslationWarning();
    });
  });
  const secretKey = document.getElementById('tencentTmtSecretKey') as HTMLInputElement | null;
  const visibility = document.getElementById('tencentTmtSecretKeyVisibility') as HTMLButtonElement | null;
  visibility?.addEventListener('click', () => {
    if (!secretKey) return;
    const show = secretKey.type === 'password';
    secretKey.type = show ? 'text' : 'password';
    visibility.setAttribute('aria-pressed', String(show));
    const label = show ? '隐藏 SecretKey' : '显示 SecretKey';
    visibility.setAttribute('aria-label', label);
    visibility.title = label;
  });
  setupDropdownMenu(
    'zhEnTriggerLengthBtn',
    'zhEnTriggerLengthMenu',
    '',
    true,
    'general.cn_en_mixed_input_min_chars',
    Number
  );
  setupToggleButton('emojiMixedInputToggleBtn', (active) => {
    updateConfig('general.emoji_mixed_input', active);
  });
  setupToggleButton('kaomojiMixedInputToggleBtn', (active) => {
    updateConfig('general.kaomoji_mixed_input', active);
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
