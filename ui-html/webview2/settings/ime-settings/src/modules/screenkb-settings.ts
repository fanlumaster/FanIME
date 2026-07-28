import type { ResolvedTheme } from './theme';

export function applyScreenKeyboardPreviewTheme(theme: ResolvedTheme): void {
  const preview = document.querySelector<HTMLElement>('.screenkb-preview');
  preview?.classList.toggle('theme-light', theme === 'light');
  preview?.classList.toggle('theme-dark', theme === 'dark');

  const image = document.getElementById('screenKeyboardPreviewImage');
  if (image instanceof HTMLImageElement) {
    image.src = theme === 'light' ? '/assets/softkbd_light.png' : '/assets/softkbd.png';
  }
}

export function setupScreenKeyboardSettings(): void {
  document.getElementById('screenkbOpenButton')?.addEventListener('click', () => {
    window.chrome?.webview?.postMessage(JSON.stringify({ type: 'openKeyboardPanel' }));
  });
}
