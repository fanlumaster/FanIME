import { applyDropdownValue, setupDropdownMenu } from './shared';

type InputScheme = 'quanpin' | 'shuangpin' | 'wubi';

function updateInputConfig(path: string, value: string): void {
  window.chrome?.webview?.postMessage(JSON.stringify({
    type: 'configUpdate',
    data: { path, value }
  }));
}

export function applyInputConfig(
  schema: string | undefined,
  shuangpinSchema: string | undefined,
  wubiSchema: string | undefined
): void {
  if (schema === 'quanpin' || schema === 'shuangpin' || schema === 'wubi') {
    const radio = document.querySelector<HTMLInputElement>(`input[name="input-method"][value="${schema}"]`);
    if (radio) {
      radio.checked = true;
    }
  }

  applyDropdownValue('shuangpinSchemeBtn', 'shuangpinSchemeMenu', shuangpinSchema);
  applyDropdownValue('wubiSchemeBtn', 'wubiSchemeMenu', wubiSchema);
}

export function setupInput(): void {
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

}
