import { applyToggleState, setupToggleButton } from './shared';
import { updateConfig } from './config-sync';

const fields: Record<string, string> = {
  aiProvider: 'provider', aiToken: 'token', aiEndpoint: 'endpoint', aiModel: 'model', aiPrompt: 'prompt'
};

export function setupAiSettings(): void {
  setupToggleButton('aiEnabled', value => updateConfig('ai_assistant.enabled', value));
  Object.entries(fields).forEach(([id, key]) => {
    const element = document.getElementById(id) as HTMLInputElement | HTMLTextAreaElement | null;
    element?.addEventListener('change', () => updateConfig(`ai_assistant.${key}`, element.value));
  });
  const limit = document.getElementById('aiCandidateLimit') as HTMLInputElement | null;
  limit?.addEventListener('change', () => {
    const value = Math.max(1, Math.min(10, Number.parseInt(limit.value, 10) || 3));
    limit.value = String(value);
    updateConfig('ai_assistant.candidate_limit', value);
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
}
