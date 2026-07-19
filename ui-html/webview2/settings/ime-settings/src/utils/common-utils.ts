// HTML 加载工具
// Prefer bundling partials so they work in WebView2/file://
const partials = import.meta.glob<string>('/src/partials/**/*.html', {
  query: '?raw',
  import: 'default',
});

export async function loadHTML(url: string): Promise<string> {
  if (url.startsWith('/src/partials/')) {
    const loader = partials[url];
    if (!loader) {
      throw new Error(`Partial not found: ${url}`);
    }
    return await loader();
  }

  const response = await fetch(url);
  return await response.text();
}

// 只显示当前的模块，隐藏其他模块
export function showOnlyCurrentModule(moduleName: string): void {
  const modules = ['floating-toolbar', 'appearance', 'input', 'helpcode', 'dict', 'skin', 'voice', 'screenkb-settings', 'tools-settings', 'shortcut'];
  modules.forEach((module: string) => {
    if (module === moduleName) {
      document.getElementById(module)!.style.display = 'block';
    } else {
      document.getElementById(module)!.style.display = 'none';
    }
  });
}
