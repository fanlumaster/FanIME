import { applyCandidateArrange, applyDropdownValue, applyToggleState } from './shared';

function safeParseJson(value: string): unknown {
  try {
    return JSON.parse(value);
  } catch {
    return null;
  }
}

let lastSnapshot: Record<string, any> | null = null;
const readyModules = new Set<string>();

export function notifySettingsModuleReady(name: string): void {
  readyModules.add(name);
  if (lastSnapshot) applyConfigData(lastSnapshot);
}

function applyConfigData(data: Record<string, any>): void {
  lastSnapshot = data;

  applyCandidateArrange(data?.appearance?.candidate_window_layout);

  if (readyModules.has('appearance')) {
    void import('./appearance').then((module) => {
      module.applyAppearanceConfig(
        data?.appearance?.candidate_window_preedit_style,
        data?.appearance?.tsf_preedit_style,
        {
          theme_mode: data?.appearance?.theme_mode,
          theme_settings: data?.appearance?.theme_settings,
          theme_cand: data?.appearance?.theme_cand,
          theme_ftb: data?.appearance?.theme_ftb,
          theme_menu: data?.appearance?.theme_menu,
          theme_emoji: data?.appearance?.theme_emoji,
          theme_screen_keyboard: data?.appearance?.theme_screen_keyboard,
          theme_handwriting: data?.appearance?.theme_handwriting,
          theme_voice: data?.appearance?.theme_voice
        },
        {
          font: data?.appearance?.font,
          font_css_family: data?.appearance?.font_css_family,
          english_font: data?.appearance?.english_font,
          english_font_css_family: data?.appearance?.english_font_css_family,
          default_font: data?.appearance?.default_font,
          default_font_css_family: data?.appearance?.default_font_css_family,
          font_size: data?.appearance?.font_size,
          candidate_window_preedit_font_size: data?.appearance?.candidate_window_preedit_font_size,
          cand_text_color: data?.appearance?.cand_text_color,
          page_size: data?.appearance?.page_size,
          candidate_window_follow_cursor: data?.appearance?.candidate_window_follow_cursor,
          system_fonts: data?.appearance?.system_fonts
        }
      );
      module.updateCandidatePreviewHelpcode({
        input_schema: data?.input?.schema,
        shuangpin_helpcode: data?.helpcode?.shuangpin_helpcode,
        quanpin_helpcode: data?.helpcode?.quanpin_helpcode,
        show_sp_helpcode_in_candidate_window: data?.helpcode?.show_sp_helpcode_in_candidate_window,
        show_qp_helpcode_in_candidate_window: data?.helpcode?.show_qp_helpcode_in_candidate_window
      });
    });
  }

  if (readyModules.has('skin')) {
    void import('./skin').then((module) => {
      module.applyCandidateSkinCatalog(
        data?.appearance?.external_candidate_skins,
        data?.appearance?.candidate_skin_scan_issues,
        data?.appearance?.candidate_skin_directory,
        data?.appearance?.candidate_skin_catalog_scanned,
        data?.appearance?.candidate_skin_catalog_revision
      );
      module.applyCandidateSkin(data?.appearance?.candidate_skin);
    });
  }

  if (typeof data?.general?.floating_toolbar === 'boolean') {
    applyToggleState('ftbToggleBtn', data.general.floating_toolbar);
  }
  const diagnosticLog = data?.general?.diagnostic_log ?? data?.general?.candidate_window_diagnostic_log;
  if (typeof diagnosticLog === 'boolean') {
    applyToggleState('serverDiagnosticLogToggleBtn', diagnosticLog);
  }
  if (typeof data?.general?.tsf_diagnostic_log === 'boolean') {
    applyToggleState('tsfDiagnosticLogToggleBtn', data.general.tsf_diagnostic_log);
  }
  if (typeof data?.input?.word_to_character === 'boolean') {
    applyToggleState('wordToCharacterToggleBtn', data.input.word_to_character);
  }
  if (typeof data?.input?.smart_punctuation === 'boolean') {
    applyToggleState('smartPunctuationToggleBtn', data.input.smart_punctuation);
  }
  if (typeof data?.input?.smart_punctuation_repeat_to_chinese === 'boolean') {
    applyToggleState(
      'smartPunctuationRepeatToChineseToggleBtn',
      data.input.smart_punctuation_repeat_to_chinese
    );
  }
  if (typeof data?.input?.paired_punctuation === 'boolean') {
    applyToggleState('pairedPunctuationToggleBtn', data.input.paired_punctuation);
  }
  if (data?.input?.punctuation_lock === 'chinese' || data?.input?.punctuation_lock === 'english' ||
      data?.input?.punctuation_lock === 'follow') {
    applyToggleState('alwaysChinesePunctuationToggleBtn', data.input.punctuation_lock === 'chinese');
    applyToggleState('alwaysEnglishPunctuationToggleBtn', data.input.punctuation_lock === 'english');
  }
  if (typeof data?.general?.candidate_translations === 'boolean') {
    applyToggleState('candidateTranslationsToggleBtn', data.general.candidate_translations);
    document.getElementById('candidateTranslationApiOptions')?.classList.toggle(
      'is-disabled',
      !data.general.candidate_translations
    );
  }
  if (typeof data?.general?.emoji_mixed_input === 'boolean') {
    applyToggleState('emojiMixedInputToggleBtn', data.general.emoji_mixed_input);
  }
  if (typeof data?.general?.kaomoji_mixed_input === 'boolean') {
    applyToggleState('kaomojiMixedInputToggleBtn', data.general.kaomoji_mixed_input);
  }
  if (typeof data?.general?.cloud_candidates === 'boolean') {
    applyToggleState('cloudCandidatesToggleBtn', data.general.cloud_candidates);
  }
  if (typeof data?.utility?.unicode_mode === 'boolean') {
    applyToggleState('unicodeModeToggleBtn', data.utility.unicode_mode);
  }
  if (typeof data?.utility?.quick_phrase === 'boolean') {
    applyToggleState('quickPhraseToggleBtn', data.utility.quick_phrase);
  }
  if (typeof data?.utility?.date_time_mode === 'boolean') {
    applyToggleState('dateTimeModeToggleBtn', data.utility.date_time_mode);
  }
  if (typeof data?.utility?.emoji_mode === 'boolean') {
    applyToggleState('emojiModeToggleBtn', data.utility.emoji_mode);
  }
  if (typeof data?.utility?.kaomoji_mode === 'boolean') {
    applyToggleState('kaomojiModeToggleBtn', data.utility.kaomoji_mode);
  }
  if (typeof data?.utility?.jianpin_mode === 'boolean') {
    applyToggleState('jianpinModeToggleBtn', data.utility.jianpin_mode);
  }
  if (typeof data?.utility?.y_mode === 'boolean') {
    applyToggleState('yModeToggleBtn', data.utility.y_mode);
  }
  if (typeof data?.utility?.r_mode === 'boolean') {
    applyToggleState('rModeToggleBtn', data.utility.r_mode);
  }
  if (typeof data?.utility?.clipboard_history === 'boolean') {
    applyToggleState('clipboardHistoryToggleBtn', data.utility.clipboard_history);
  }
  if (typeof data?.general?.paging_minus_equal === 'boolean') {
    const checkbox = document.getElementById('pagingMinusEqualCheckbox') as HTMLInputElement | null;
    if (checkbox) checkbox.checked = data.general.paging_minus_equal;
  }
  if (typeof data?.general?.paging_tab === 'boolean') {
    const checkbox = document.getElementById('pagingTabCheckbox') as HTMLInputElement | null;
    if (checkbox) checkbox.checked = data.general.paging_tab;
  }
  if (typeof data?.general?.paging_comma_period === 'boolean') {
    const checkbox = document.getElementById('pagingCommaPeriodCheckbox') as HTMLInputElement | null;
    if (checkbox) checkbox.checked = data.general.paging_comma_period;
  }
  if (typeof data?.general?.paging_page_up_down === 'boolean') {
    const checkbox = document.getElementById('pagingPageUpDownCheckbox') as HTMLInputElement | null;
    if (checkbox) checkbox.checked = data.general.paging_page_up_down;
  }
  if (typeof data?.general?.candidate_arrow_navigation === 'boolean') {
    const checkbox = document.getElementById('candidateArrowNavigationCheckbox') as HTMLInputElement | null;
    if (checkbox) checkbox.checked = data.general.candidate_arrow_navigation;
  }
  if (typeof data?.helpcode?.show_sp_helpcode_in_candidate_window === 'boolean') {
    applyToggleState(
      'showShuangpinHelpcodeToggleBtn',
      data.helpcode.show_sp_helpcode_in_candidate_window
    );
  }
  if (typeof data?.helpcode?.shuangpin_helpcode === 'boolean') {
    applyToggleState('shuangpinHelpcodeToggleBtn', data.helpcode.shuangpin_helpcode);
  }
  applyDropdownValue(
    'shuangpinHelpcodeSchemeBtn',
    'shuangpinHelpcodeSchemeMenu',
    data?.helpcode?.shuangpin_helpcode_schema
  );
  if (typeof data?.helpcode?.quanpin_helpcode === 'boolean') {
    applyToggleState('quanpinHelpcodeToggleBtn', data.helpcode.quanpin_helpcode);
  }
  applyDropdownValue(
    'quanpinHelpcodeSchemeBtn',
    'quanpinHelpcodeSchemeMenu',
    data?.helpcode?.quanpin_helpcode_schema
  );
  if (typeof data?.helpcode?.show_qp_helpcode_in_candidate_window === 'boolean') {
    applyToggleState(
      'showQuanpinHelpcodeToggleBtn',
      data.helpcode.show_qp_helpcode_in_candidate_window
    );
  }
  if (data?.voice_input && typeof data.voice_input === 'object') {
    if (typeof data.voice_input.enabled === 'boolean') {
      applyToggleState('voiceEnabled', data.voice_input.enabled);
    }
    if (typeof data.voice_input.polish_text === 'boolean') {
      applyToggleState('voicePolishText', data.voice_input.polish_text);
    }
    if (typeof data.voice_input.stream_inline_preedit === 'boolean') {
      applyToggleState('voiceStreamInlinePreedit', data.voice_input.stream_inline_preedit);
    }
    if (typeof data.voice_input.mute_system_audio === 'boolean') {
      applyToggleState('voiceMuteSystemAudio', data.voice_input.mute_system_audio);
    }
  }
  if (data?.ai_assistant && typeof data.ai_assistant === 'object') {
    if (typeof data.ai_assistant.enabled === 'boolean') {
      applyToggleState('aiEnabled', data.ai_assistant.enabled);
    }
  }

  if (readyModules.has('input') || readyModules.has('helpcode') || readyModules.has('tools-settings')) {
    void import('./input').then((module) => {
      module.applyInputConfig(
        data?.input?.mode,
        data?.input?.schema,
        data?.input?.character_set,
        data?.input?.shuangpin_schema,
        data?.input?.wubi_schema,
        data?.input?.default_ime_mode,
        data?.input?.ime_mode_scope,
        data?.input?.japanese_schema
      );
      module.applyFrequencyConfig(data?.frequency_adjustment);
      module.applyZhEnMixedInputConfig(
        data?.general?.cn_en_mixed_input,
        data?.general?.cn_en_mixed_input_min_chars
      );
      module.applyTencentTmtConfig(data?.tencent_tmt);
      module.applyCustomTranslationConfig(data?.custom_translation);
    });
  }
  if (readyModules.has('voice') && data?.voice_input && typeof data.voice_input === 'object') {
    void import('./voice').then((module) => module.applyVoiceConfig(data.voice_input));
  }
  if (readyModules.has('ai-settings') && data?.ai_assistant && typeof data.ai_assistant === 'object') {
    void import('./ai-settings').then((module) => module.applyAiConfig(data.ai_assistant));
  }
  if (readyModules.has('floating-toolbar')) {
    void import('./floating-toolbar').then((module) => {
      module.applyFloatingToolbarItemsConfig({
        fullwidth: data?.general?.floating_toolbar_fullwidth,
        punctuation: data?.general?.floating_toolbar_punctuation,
        character_set: data?.general?.floating_toolbar_character_set,
        emoji: data?.general?.floating_toolbar_emoji,
        screen_keyboard: data?.general?.floating_toolbar_screen_keyboard,
        settings: data?.general?.floating_toolbar_settings
      });
      module.applyFloatingToolbarAppearanceConfig(
        data?.general?.floating_toolbar_scale,
        data?.general?.floating_toolbar_font_size
      );
    });
  }
  if (readyModules.has('shortcut')) {
    void import('./shortcut').then((module) => module.applyShortcutConfig(data?.keybindings));
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
    applyConfigData(payload.data ?? {});
  });

  window.chrome.webview.postMessage(JSON.stringify({ type: 'configRequest' }));
}

export function updateConfig(path: string, value: string | boolean | number): void {
  window.chrome?.webview?.postMessage(JSON.stringify({
    type: 'configUpdate',
    data: { path, value }
  }));
}
