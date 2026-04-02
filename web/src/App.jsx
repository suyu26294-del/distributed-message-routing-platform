import { useEffect, useState } from 'react';
import FileConsole from './components/FileConsole';
import MessageConsole from './components/MessageConsole';
import OpsTimeline from './components/OpsTimeline';
import StatusStrip from './components/StatusStrip';
import {
  createEventsSocket,
  createManifest,
  disconnectUser,
  downloadTransfer,
  getServices,
  getSummary,
  login,
  pullOffline,
  queryResume,
  sendMessage,
  uploadChunk,
} from './lib/api';

const defaultSummary = {
  online_users: 0,
  stored_offline_messages: 0,
  registered_gateways: 0,
  active_transfers: 0,
};

const defaultUsers = {
  alice: { loggedIn: false, sessionId: '', messages: [] },
  bob: { loggedIn: false, sessionId: '', messages: [] },
};

const initialFileState = {
  fileName: '',
  fileSize: 0,
  chunkSize: 65536,
  totalChunks: 0,
  sha256: '',
  transferId: '',
  chunks: [],
  missingChunks: [],
  progressPercent: 0,
  verified: false,
};

function formatTime(value) {
  return new Date(value ?? Date.now()).toLocaleTimeString('zh-CN', { hour12: false });
}

function bytesToBase64(bytes) {
  const sliceSize = 0x8000;
  let binary = '';
  for (let index = 0; index < bytes.length; index += sliceSize) {
    binary += String.fromCharCode(...bytes.subarray(index, index + sliceSize));
  }
  return btoa(binary);
}

function base64ToBytes(value) {
  const binary = atob(value);
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; index += 1) {
    bytes[index] = binary.charCodeAt(index);
  }
  return bytes;
}

async function sha256Hex(buffer) {
  const digest = await crypto.subtle.digest('SHA-256', buffer);
  return Array.from(new Uint8Array(digest)).map((item) => item.toString(16).padStart(2, '0')).join('');
}

async function prepareFileObject(file) {
  const buffer = await file.arrayBuffer();
  const chunkSize = 65536;
  const totalChunks = Math.ceil(buffer.byteLength / chunkSize);
  const bytes = new Uint8Array(buffer);
  const chunks = [];
  for (let index = 0; index < totalChunks; index += 1) {
    const start = index * chunkSize;
    const end = Math.min(bytes.length, start + chunkSize);
    chunks.push(bytesToBase64(bytes.subarray(start, end)));
  }

  return {
    fileName: file.name,
    fileSize: buffer.byteLength,
    chunkSize,
    totalChunks,
    sha256: await sha256Hex(buffer),
    chunks,
    transferId: '',
    missingChunks: [],
    progressPercent: 0,
    verified: false,
  };
}

function createSampleFile() {
  const content = 'dist-msg-platform visual demo '.repeat(8000);
  return new File([content], 'visual-demo.txt', { type: 'text/plain' });
}

export default function App() {
  const [services, setServices] = useState([]);
  const [summary, setSummary] = useState(defaultSummary);
  const [lastUpdated, setLastUpdated] = useState('--');
  const [gatewayAddress, setGatewayAddress] = useState('127.0.0.1:7001');
  const [users, setUsers] = useState(defaultUsers);
  const [compose, setCompose] = useState({ from_user: 'alice', to_user: 'bob', body: 'hello from the visual console' });
  const [events, setEvents] = useState([]);
  const [fileState, setFileState] = useState(initialFileState);
  const [demoRunning, setDemoRunning] = useState(false);

  function pushEvent(title, description, level = 'info') {
    setEvents((current) => [
      { id: `${Date.now()}-${Math.random()}`, title, description, level, time: formatTime() },
      ...current,
    ].slice(0, 18));
  }

  function appendMessage(userId, message) {
    setUsers((current) => ({
      ...current,
      [userId]: {
        ...current[userId],
        messages: [message, ...current[userId].messages].slice(0, 12),
      },
    }));
  }

  function syncConnectedUsers(connectedUsers) {
    if (!Array.isArray(connectedUsers)) {
      return;
    }
    setUsers((current) => ({
      ...current,
      alice: {
        ...current.alice,
        loggedIn: connectedUsers.includes('alice'),
        sessionId: connectedUsers.includes('alice') ? current.alice.sessionId || 'active' : '',
      },
      bob: {
        ...current.bob,
        loggedIn: connectedUsers.includes('bob'),
        sessionId: connectedUsers.includes('bob') ? current.bob.sessionId || 'active' : '',
      },
    }));
  }

  async function ensureUserConnected(userId, silent = false) {
    try {
      const response = await login(userId);
      setUsers((current) => ({
        ...current,
        [userId]: { ...current[userId], loggedIn: true, sessionId: response.user.session_id },
      }));
      if (!silent) {
        pushEvent('登录成功', `${userId} 已建立会话`, 'success');
      }
      return true;
    } catch (error) {
      pushEvent('登录失败', error.message, 'error');
      return false;
    }
  }

  async function refreshSystem() {
    try {
      const [servicesResponse, summaryResponse] = await Promise.all([getServices(), getSummary()]);
      setServices(servicesResponse.services ?? []);
      setGatewayAddress(servicesResponse.gateway_address ?? '127.0.0.1:7001');
      setSummary(summaryResponse.summary ?? defaultSummary);
      syncConnectedUsers(servicesResponse.connected_users ?? []);
      setLastUpdated(formatTime());
    } catch (error) {
      pushEvent('状态刷新失败', error.message, 'error');
    }
  }

  useEffect(() => {
    refreshSystem();
    const timer = window.setInterval(refreshSystem, 4000);
    return () => window.clearInterval(timer);
  }, []);

  useEffect(() => {
    const socket = createEventsSocket();
    socket.onmessage = (event) => {
      const payload = JSON.parse(event.data);
      if (payload.type === 'summary' && payload.summary) {
        setSummary(payload.summary);
        setLastUpdated(formatTime(payload.timestamp));
      }
      if (payload.type === 'services' && payload.services) {
        setServices(payload.services);
        setGatewayAddress(payload.gateway_address ?? '127.0.0.1:7001');
        syncConnectedUsers(payload.connected_users ?? []);
      }
      if (payload.type === 'login' && payload.user) {
        setUsers((current) => ({
          ...current,
          [payload.user.user_id]: {
            ...current[payload.user.user_id],
            loggedIn: true,
            sessionId: payload.user.session_id,
          },
        }));
        pushEvent('用户登录', `${payload.user.user_id} 已连接到 ${payload.user.gateway_instance_id}`);
      }
      if (payload.type === 'disconnect' && payload.user_id) {
        setUsers((current) => ({
          ...current,
          [payload.user_id]: { ...current[payload.user_id], loggedIn: false, sessionId: '' },
        }));
        pushEvent('用户断开', `${payload.user_id} 已从桥接层断开`);
      }
      if (payload.type === 'session_closed' && payload.user_id) {
        setUsers((current) => ({
          ...current,
          [payload.user_id]: { ...current[payload.user_id], loggedIn: false, sessionId: '' },
        }));
        pushEvent('会话失效', `${payload.user_id} 的连接已断开，后续操作会自动补登录`, 'warning');
      }
      if (payload.type === 'live_message' && payload.message) {
        appendMessage(payload.user_id, {
          id: payload.message.message_id,
          from: payload.message.from_user,
          to: payload.message.to_user,
          body: payload.message.body,
          direction: 'incoming',
          mode: 'live',
        });
        pushEvent('实时消息送达', `${payload.message.from_user} -> ${payload.message.to_user}：${payload.message.body}`);
      }
      if (payload.type === 'offline_batch') {
        (payload.messages ?? []).forEach((message, index) => {
          appendMessage(payload.user_id, {
            id: `${message.message_id ?? index}-offline`,
            from: message.from_user,
            to: message.to_user,
            body: message.body,
            direction: 'incoming',
            mode: 'offline',
          });
        });
        pushEvent('离线补偿返回', `${payload.user_id} 拉取到 ${(payload.messages ?? []).length} 条离线消息`);
      }
      if (payload.type === 'file_chunk' && payload.progress) {
        pushEvent('分片确认', `chunk ${payload.progress.chunk_index} 已确认，累计 ${payload.progress.received_chunks} 片`, 'success');
      }
      if (payload.type === 'resume_token' && payload.resume) {
        pushEvent('续传令牌更新', `仍缺少 ${payload.resume.missing_chunks.length} 个分片`);
      }
      if (payload.type === 'file_download' && payload.file) {
        pushEvent('文件下载完成', `已回读 ${payload.file.byte_size} bytes`, 'success');
      }
    };
    socket.onerror = () => pushEvent('事件流断开', 'WebSocket 暂时不可用，页面会继续轮询状态。', 'warning');
    return () => socket.close();
  }, []);

  async function handleLogin(userId) {
    await ensureUserConnected(userId, false);
    await refreshSystem();
  }

  async function handleDisconnect(userId) {
    try {
      await disconnectUser(userId);
      setUsers((current) => ({
        ...current,
        [userId]: { ...current[userId], loggedIn: false, sessionId: '' },
      }));
      pushEvent('已断开用户', `${userId} 现在处于离线状态`);
      await refreshSystem();
    } catch (error) {
      pushEvent('断开失败', error.message, 'error');
    }
  }

  async function handlePullOffline(userId) {
    const ready = await ensureUserConnected(userId, true);
    if (!ready) {
      return;
    }
    try {
      const response = await pullOffline(userId);
      response.messages.forEach((message, index) => {
        appendMessage(userId, {
          id: `${message.message_id ?? index}-manual`,
          from: message.from_user,
          to: message.to_user,
          body: message.body,
          direction: 'incoming',
          mode: 'offline',
        });
      });
      pushEvent('手动拉取完成', `${userId} 收到 ${response.messages.length} 条离线消息`, 'success');
      await refreshSystem();
    } catch (error) {
      pushEvent('拉取离线失败', error.message, 'error');
    }
  }

  async function handleSendMessage() {
    const ready = await ensureUserConnected(compose.from_user, true);
    if (!ready) {
      return;
    }
    try {
      const response = await sendMessage(compose);
      appendMessage(compose.from_user, {
        id: response.delivery.message_id ?? `${Date.now()}-outgoing`,
        from: compose.from_user,
        to: compose.to_user,
        body: compose.body,
        direction: 'outgoing',
        mode: response.delivery.delivered_live ? 'live' : 'offline',
      });
      pushEvent(
        response.delivery.delivered_live ? '消息实时送达' : '消息已转离线',
        `${compose.from_user} -> ${compose.to_user}：${compose.body}`,
        'success',
      );
      await refreshSystem();
    } catch (error) {
      pushEvent('发送失败', error.message, 'error');
    }
  }

  async function loadPreparedFile(file) {
    const prepared = await prepareFileObject(file);
    setFileState(prepared);
    pushEvent('文件已就绪', `${prepared.fileName} 已拆分为 ${prepared.totalChunks} 个分片`);
    return prepared;
  }

  async function ensureFileReady() {
    if (fileState.chunks.length > 0) {
      return fileState;
    }
    return loadPreparedFile(createSampleFile());
  }

  async function handleChooseFile(event) {
    const [file] = event.target.files ?? [];
    if (!file) {
      return;
    }
    try {
      await loadPreparedFile(file);
    } catch (error) {
      pushEvent('文件读取失败', error.message, 'error');
    }
  }

  async function handleUseSampleFile() {
    try {
      await loadPreparedFile(createSampleFile());
    } catch (error) {
      pushEvent('示例文件生成失败', error.message, 'error');
    }
  }

  async function handleCreateManifest() {
    const ready = await ensureUserConnected('alice', true);
    if (!ready) {
      return null;
    }
    try {
      const prepared = await ensureFileReady();
      const response = await createManifest({
        owner_user: 'alice',
        file_name: prepared.fileName,
        file_size: prepared.fileSize,
        chunk_size: prepared.chunkSize,
        total_chunks: prepared.totalChunks,
        sha256: prepared.sha256,
      });
      setFileState((current) => ({ ...current, transferId: response.transfer.transfer_id }));
      pushEvent('传输创建成功', `transfer id: ${response.transfer.transfer_id}`, 'success');
      await refreshSystem();
      return response.transfer.transfer_id;
    } catch (error) {
      pushEvent('创建传输失败', error.message, 'error');
      return null;
    }
  }

  async function handleUploadFirstChunk() {
    const ready = await ensureUserConnected('alice', true);
    if (!ready) {
      return;
    }
    try {
      const prepared = await ensureFileReady();
      const transferId = fileState.transferId || await handleCreateManifest();
      if (!transferId) {
        return;
      }
      await uploadChunk({
        user_id: 'alice',
        transfer_id: transferId,
        chunk_index: 0,
        data_base64: prepared.chunks[0],
        eof: prepared.totalChunks === 1,
      });
      const resume = await queryResume({ user_id: 'alice', transfer_id: transferId });
      const received = prepared.totalChunks - resume.resume.missing_chunks.length;
      setFileState((current) => ({
        ...current,
        transferId,
        missingChunks: resume.resume.missing_chunks,
        progressPercent: Math.round((received / prepared.totalChunks) * 100),
      }));
      pushEvent('首片上传完成', `缺失分片：${resume.resume.missing_chunks.join(', ')}`, 'success');
      await refreshSystem();
    } catch (error) {
      pushEvent('上传首片失败', error.message, 'error');
    }
  }

  async function handleResumeUpload() {
    const ready = await ensureUserConnected('alice', true);
    if (!ready) {
      return;
    }
    try {
      const prepared = await ensureFileReady();
      const transferId = fileState.transferId || await handleCreateManifest();
      if (!transferId) {
        return;
      }
      const resume = await queryResume({ user_id: 'alice', transfer_id: transferId });
      for (const index of resume.resume.missing_chunks) {
        await uploadChunk({
          user_id: 'alice',
          transfer_id: transferId,
          chunk_index: index,
          data_base64: prepared.chunks[index],
          eof: index === prepared.totalChunks - 1,
        });
      }
      setFileState((current) => ({
        ...current,
        transferId,
        missingChunks: [],
        progressPercent: 100,
      }));
      pushEvent('断点续传完成', `${prepared.fileName} 已全部写入`, 'success');
      await refreshSystem();
    } catch (error) {
      pushEvent('续传失败', error.message, 'error');
    }
  }

  async function handleDownloadVerify() {
    const ready = await ensureUserConnected('alice', true);
    if (!ready) {
      return;
    }
    try {
      const prepared = await ensureFileReady();
      const transferId = fileState.transferId || await handleCreateManifest();
      if (!transferId) {
        return;
      }
      const response = await downloadTransfer({ user_id: 'alice', transfer_id: transferId });
      const downloaded = base64ToBytes(response.file.data_base64);
      const digest = await sha256Hex(downloaded.buffer);
      const verified = digest === prepared.sha256;
      setFileState((current) => ({ ...current, verified, transferId }));
      pushEvent(verified ? '下载校验通过' : '下载校验失败', `SHA-256 ${verified ? '一致' : '不一致'}`, verified ? 'success' : 'error');
      await refreshSystem();
    } catch (error) {
      pushEvent('下载失败', error.message, 'error');
    }
  }

  async function runFullDemo() {
    setDemoRunning(true);
    try {
      const demoFile = await ensureFileReady();
      await ensureUserConnected('alice', true);
      await ensureUserConnected('bob', true);
      await sendMessage({ from_user: 'alice', to_user: 'bob', body: 'hello live from dashboard' });
      appendMessage('alice', { id: `${Date.now()}-live`, from: 'alice', to: 'bob', body: 'hello live from dashboard', direction: 'outgoing', mode: 'live' });
      await handleDisconnect('bob');
      await sendMessage({ from_user: 'alice', to_user: 'bob', body: 'hello offline from dashboard' });
      appendMessage('alice', { id: `${Date.now()}-offline`, from: 'alice', to: 'bob', body: 'hello offline from dashboard', direction: 'outgoing', mode: 'offline' });
      await handleLogin('bob');
      await handlePullOffline('bob');
      if (!fileState.fileName) {
        setFileState(demoFile);
      }
      await handleCreateManifest();
      await handleUploadFirstChunk();
      await handleResumeUpload();
      await handleDownloadVerify();
      pushEvent('完整演示完成', '消息路由、离线补偿和文件续传已经全部跑完。', 'success');
    } catch (error) {
      pushEvent('完整演示失败', error.message, 'error');
    } finally {
      setDemoRunning(false);
      await refreshSystem();
    }
  }

  return (
    <main className="app-shell">
      <StatusStrip
        services={services}
        summary={summary}
        lastUpdated={lastUpdated}
        onRefresh={refreshSystem}
        onRunDemo={runFullDemo}
        demoRunning={demoRunning}
        gatewayAddress={gatewayAddress}
      />
      <section className="content-grid">
        <MessageConsole
          users={users}
          compose={compose}
          onComposeChange={setCompose}
          onSendMessage={handleSendMessage}
          onLogin={handleLogin}
          onDisconnect={handleDisconnect}
          onPullOffline={handlePullOffline}
        />
        <FileConsole
          fileState={fileState}
          onChooseFile={handleChooseFile}
          onUseSampleFile={handleUseSampleFile}
          onCreateManifest={handleCreateManifest}
          onUploadFirstChunk={handleUploadFirstChunk}
          onResumeUpload={handleResumeUpload}
          onDownloadVerify={handleDownloadVerify}
        />
      </section>
      <OpsTimeline events={events} users={users} fileState={fileState} />
    </main>
  );
}