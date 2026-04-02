import { render, screen, waitFor } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import App from '../App';

class MockWebSocket {
  constructor() {
    this.onmessage = null;
    this.onerror = null;
  }

  close() {}
}

describe('App', () => {
  beforeEach(() => {
    global.fetch = vi.fn((url) => {
      if (String(url).includes('/api/services')) {
        return Promise.resolve({
          ok: true,
          json: async () => ({
            gateway_address: '127.0.0.1:7001',
            services: [{ name: 'gateway_service', port: 7001, label: 'Gateway', healthy: true }],
          }),
        });
      }
      return Promise.resolve({
        ok: true,
        json: async () => ({
          summary: {
            online_users: 2,
            stored_offline_messages: 0,
            registered_gateways: 1,
            active_transfers: 0,
          },
        }),
      });
    });
    global.WebSocket = MockWebSocket;
  });

  it('renders the main dashboard panels', async () => {
    render(<App />);
    expect(screen.getByText('可视化演示控制台')).toBeInTheDocument();
    expect(screen.getByRole('button', { name: '运行完整演示' })).toBeInTheDocument();
    expect(screen.getByText('双端会话演示')).toBeInTheDocument();
    expect(screen.getByText('分片上传与断点续传')).toBeInTheDocument();
    await waitFor(() => expect(screen.getByText('Gateway')).toBeInTheDocument());
  });
});