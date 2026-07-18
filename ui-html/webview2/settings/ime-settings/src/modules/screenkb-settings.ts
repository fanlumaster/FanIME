export function setupScreenKeyboardSettings(): void {
  document.getElementById('screenkbOpenButton')?.addEventListener('click', () => {
    window.chrome?.webview?.postMessage(JSON.stringify({ type: 'openKeyboardPanel' }));
  });
}
