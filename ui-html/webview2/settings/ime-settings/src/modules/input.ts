import { applyDropdownValue, setupDropdownMenu, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

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
  wubiSchema: string | undefined
): void {
  if (schema === 'quanpin' || schema === 'shuangpin' || schema === 'wubi') {
    const radio = document.querySelector<HTMLInputElement>(`input[name="input-method"][value="${schema}"]`);
    if (radio) {
      radio.checked = true;
    }
  }

  applyDropdownValue('characterSetBtn', 'characterSetMenu', characterSet);
  applyDropdownValue('shuangpinSchemeBtn', 'shuangpinSchemeMenu', shuangpinSchema);
  applyDropdownValue('wubiSchemeBtn', 'wubiSchemeMenu', wubiSchema);
}

export function setupInput(): void {
  setupDropdownMenu('characterSetBtn', 'characterSetMenu', 'changeCharacterSet', true, 'input.character_set');
  document.querySelectorAll<HTMLInputElement>('input[name="input-method"]').forEach((radio) => {
    radio.addEventListener('change', () => {
      if (!radio.checked || (radio.value !== 'quanpin' && radio.value !== 'shuangpin' && radio.value !== 'wubi')) {
        return;
      }
      const schema = radio.value as InputScheme;
      updateInputConfig('input.schema', schema);
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
  setupToggleButton('zhEnToggleBtn', (active) => {
    updateConfig('general.cn_en_mixed_input', active);
  });
  setupToggleButton('cloudCandidatesToggleBtn', (active) => {
    updateConfig('general.cloud_candidates', active);
  });
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
