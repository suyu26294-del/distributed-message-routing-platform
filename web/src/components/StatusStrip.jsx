export default function StatusStrip({ services, summary, lastUpdated, onRefresh, onRunDemo, demoRunning, gatewayAddress }) {
  return (
    <section className="hero-shell card">
      <div>
        <p className="eyebrow">Distributed Message Platform</p>
        <h1>可视化演示控制台</h1>
        <p className="hero-copy">
          同时展示多服务状态、双端消息流、离线补偿、文件分片上传和断点续传，适合本地演示、答辩和联调。
        </p>
      </div>
      <div className="hero-actions">
        <button className="primary-button" onClick={onRunDemo} disabled={demoRunning}>
          {demoRunning ? '演示进行中...' : '运行完整演示'}
        </button>
        <button className="secondary-button" onClick={onRefresh}>刷新状态</button>
        <div className="hero-meta">
          <span>网关地址 {gatewayAddress}</span>
          <span>最近刷新 {lastUpdated}</span>
        </div>
      </div>
      <div className="metric-grid">
        <div className="metric-card glass-card">
          <span>在线用户</span>
          <strong>{summary.online_users}</strong>
        </div>
        <div className="metric-card glass-card">
          <span>离线消息</span>
          <strong>{summary.stored_offline_messages}</strong>
        </div>
        <div className="metric-card glass-card">
          <span>已注册网关</span>
          <strong>{summary.registered_gateways}</strong>
        </div>
        <div className="metric-card glass-card">
          <span>活跃传输</span>
          <strong>{summary.active_transfers}</strong>
        </div>
      </div>
      <div className="service-row">
        {services.map((service) => (
          <div key={service.name} className={`service-pill ${service.healthy ? 'healthy' : 'unhealthy'}`}>
            <span>{service.label}</span>
            <small>{service.name}:{service.port}</small>
          </div>
        ))}
      </div>
    </section>
  );
}