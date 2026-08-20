import { applyDropdownValue, applyToggleState, setupDropdownMenu, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

type ProviderDefaults = { endpoint: string; model: string };

const fields: Record<string, string> = {
  aiToken: 'token', aiEndpoint: 'endpoint', aiModel: 'model', aiPrompt: 'prompt'
};

const PROVIDER_DEFAULTS: Record<string, ProviderDefaults> = {
  deepseek: {
    endpoint: 'https://api.deepseek.com/chat/completions',
    model: 'deepseek-v4-flash'
  },
  openai: {
    endpoint: 'https://api.openai.com/v1/chat/completions',
    model: 'gpt-4o-mini'
  },
  siliconflow: {
    endpoint: 'https://api.siliconflow.cn/v1/chat/completions',
    model: 'Qwen/Qwen3-8B'
  },
  groq: {
    endpoint: 'https://api.groq.com/openai/v1/chat/completions',
    model: 'llama-3.3-70b-versatile'
  }
};

const PROVIDERS = ['deepseek', 'openai', 'siliconflow', 'groq'] as const;
let tokens: Record<string, string> = {};
let currentProvider = 'deepseek';

function knownValues(field: keyof ProviderDefaults): string[] {
  return Object.values(PROVIDER_DEFAULTS).map((item) => item[field]);
}

function fillIfDefault(id: string, value: string, known: string[], path: string): void {
  const input = document.getElementById(id) as HTMLInputElement | null;
  if (!input) return;
  const current = input.value.trim();
  if (current && !known.includes(current)) return;
  input.value = value;
  updateConfig(path, value);
}

function applyProviderDefaults(provider: string): void {
  const defaults = PROVIDER_DEFAULTS[provider];
  if (!defaults) return;
  const model = document.getElementById('aiModel') as HTMLInputElement | null;
  if (model) model.placeholder = defaults.model;
  fillIfDefault('aiEndpoint', defaults.endpoint, knownValues('endpoint'), 'ai_assistant.endpoint');
  fillIfDefault('aiModel', defaults.model, knownValues('model'), 'ai_assistant.model');
}

function readTokenMap(raw: unknown, provider: string, legacyToken: string): Record<string, string> {
  const result: Record<string, string> = {};
  if (raw && typeof raw === 'object') {
    Object.entries(raw as Record<string, unknown>).forEach(([key, value]) => {
      if (typeof value === 'string') result[key] = value;
    });
  }
  if (legacyToken && !result[provider]) result[provider] = legacyToken;
  return result;
}

function switchProvider(provider: string): void {
  const token = document.getElementById('aiToken') as HTMLInputElement | null;
  if (token) {
    tokens[currentProvider] = token.value.trim();
    updateConfig(`ai_assistant.token_${currentProvider}`, tokens[currentProvider]);
  }
  currentProvider = provider;
  if (token) token.value = tokens[provider] ?? '';
  updateConfig('ai_assistant.provider', provider);
  applyProviderDefaults(provider);
}

export function setupAiSettings(): void {
  setupToggleButton('aiEnabled', value => updateConfig('ai_assistant.enabled', value));
  setupTokenVisibilityToggle();
  setupDropdownMenu('aiProviderBtn', 'aiProviderMenu', 'changeAiProvider', true);
  document.getElementById('aiProviderMenu')?.addEventListener('click', (event) => {
    const item = (event.target as HTMLElement | null)?.closest('.dropdown-item') as HTMLElement | null;
    const provider = item?.dataset.value;
    if (provider) switchProvider(provider);
  });
  Object.entries(fields).forEach(([id, key]) => {
    const element = document.getElementById(id) as HTMLInputElement | HTMLTextAreaElement | null;
    element?.addEventListener('change', () => {
      if (id === 'aiToken') {
        tokens[currentProvider] = element.value.trim();
        updateConfig(`ai_assistant.token_${currentProvider}`, tokens[currentProvider]);
        return;
      }
      updateConfig(`ai_assistant.${key}`, element.value);
    });
  });
  const limit = document.getElementById('aiCandidateLimit') as HTMLInputElement | null;
  limit?.addEventListener('change', () => {
    const value = Math.max(1, Math.min(10, Number.parseInt(limit.value, 10) || 3));
    limit.value = String(value);
    updateConfig('ai_assistant.candidate_limit', value);
  });
}

function setupTokenVisibilityToggle(): void {
  const token = document.getElementById('aiToken') as HTMLInputElement | null;
  const toggle = document.getElementById('aiTokenVisibility') as HTMLButtonElement | null;
  if (!token || !toggle) return;

  toggle.addEventListener('click', () => {
    const shouldShow = token.type === 'password';
    token.type = shouldShow ? 'text' : 'password';
    toggle.setAttribute('aria-pressed', String(shouldShow));

    const label = shouldShow ? '隐藏 API Token' : '显示 API Token';
    toggle.setAttribute('aria-label', label);
    toggle.title = label;
  });
}

export function applyAiConfig(config: Record<string, unknown>): void {
  Object.entries(fields).forEach(([id, key]) => {
    const element = document.getElementById(id) as HTMLInputElement | HTMLTextAreaElement | null;
    if (element && typeof config[key] === 'string') element.value = config[key] as string;
  });
  const limit = document.getElementById('aiCandidateLimit') as HTMLInputElement | null;
  if (limit && typeof config.candidate_limit === 'number') limit.value = String(config.candidate_limit);
  if (typeof config.enabled === 'boolean') applyToggleState('aiEnabled', config.enabled);
  currentProvider = typeof config.provider === 'string' ? config.provider : 'deepseek';
  tokens = readTokenMap(config.tokens, currentProvider, typeof config.token === 'string' ? config.token : '');
  PROVIDERS.forEach((provider) => {
    const slot = config[`token_${provider}`];
    if (typeof slot === 'string' && slot) tokens[provider] = slot;
  });
  const token = document.getElementById('aiToken') as HTMLInputElement | null;
  if (token) token.value = tokens[currentProvider] ?? '';
  applyDropdownValue('aiProviderBtn', 'aiProviderMenu', currentProvider);
  const defaults = PROVIDER_DEFAULTS[currentProvider];
  const model = document.getElementById('aiModel') as HTMLInputElement | null;
  if (model && defaults) model.placeholder = defaults.model;
}
