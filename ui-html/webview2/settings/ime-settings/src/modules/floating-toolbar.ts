import { setupToggleButton } from './shared';
import { updateConfig } from './config-sync';
import ftbHTML from '../../../../ftb/default.html?raw';

type FloatingToolbarItem = 'fullwidth' | 'punctuation' | 'character_set' | 'emoji' | 'screen_keyboard' | 'settings';
type FloatingToolbarItemsConfig = Partial<Record<FloatingToolbarItem, boolean>>;

const toolbarItems: FloatingToolbarItem[] = [
  'fullwidth',
  'punctuation',
  'character_set',
  'emoji',
  'screen_keyboard',
  'settings'
];

const toolbarItemState: Record<FloatingToolbarItem, boolean> = {
  fullwidth: true,
  punctuation: true,
  character_set: true,
  emoji: true,
  screen_keyboard: false,
  settings: true
};

export function setupFloatingToolbar(): void {
  mountFloatingToolbarPreview();

  setupToggleButton('ftbToggleBtn', (active) => {
    updateConfig('general.floating_toolbar', active);
    document.getElementById('ftbToggleBtn')?.setAttribute('aria-checked', String(active));
  });

  document.querySelectorAll<HTMLInputElement>('.floating-toolbar-component-list input[data-toolbar-item]')
    .forEach((checkbox) => {
      checkbox.addEventListener('change', () => {
        const item = checkbox.dataset.toolbarItem as FloatingToolbarItem;
        toolbarItemState[item] = checkbox.checked;
        updatePreviewItems();
        updateConfig(`general.floating_toolbar_${item}`, checkbox.checked);
      });
    });
}

export function applyFloatingToolbarItemsConfig(config: FloatingToolbarItemsConfig | undefined): void {
  if (!config) return;
  toolbarItems.forEach((item) => {
    if (typeof config[item] !== 'boolean') return;
    toolbarItemState[item] = config[item]!;
    const checkbox = document.querySelector<HTMLInputElement>(
      `.floating-toolbar-component-list input[data-toolbar-item="${item}"]`
    );
    if (checkbox) checkbox.checked = toolbarItemState[item];
  });
  updatePreviewItems();
}

function mountFloatingToolbarPreview(): void {
  const host = document.getElementById('ftbPreviewHost');
  if (!host) return;

  const source = new DOMParser().parseFromString(ftbHTML, 'text/html');
  const statusBar = source.querySelector<HTMLElement>('.status-bar');
  if (!statusBar) return;

  statusBar.querySelectorAll('#en, #fullwidth, #puncEn').forEach((element) => element.remove());
  statusBar.querySelectorAll<HTMLElement>('[id]').forEach((element) => element.removeAttribute('id'));
  host.replaceChildren(statusBar);
  updatePreviewItems();
}

function updatePreviewItems(): void {
  const host = document.getElementById('ftbPreviewHost');
  if (!host) return;
  toolbarItems.forEach((item) => {
    const element = host.querySelector<HTMLElement>(`[data-toolbar-item="${item}"]`);
    if (element) element.style.display = toolbarItemState[item] ? 'flex' : 'none';
  });
}
